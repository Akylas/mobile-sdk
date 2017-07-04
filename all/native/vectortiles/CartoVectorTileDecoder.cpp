#include "CartoVectorTileDecoder.h"
#include "core/MapTile.h"
#include "core/MapBounds.h"
#include "core/BinaryData.h"
#include "core/Variant.h"
#include "components/Exceptions.h"
#include "geometry/Feature.h"
#include "geometry/Geometry.h"
#include "geometry/PointGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PolygonGeometry.h"
#include "geometry/MultiPointGeometry.h"
#include "geometry/MultiLineGeometry.h"
#include "geometry/MultiPolygonGeometry.h"
#include "geometry/VectorTileFeature.h"
#include "geometry/VectorTileFeatureCollection.h"
#include "graphics/Bitmap.h"
#include "styles/CartoCSSStyleSet.h"
#include "vectortiles/utils/MapnikVTLogger.h"
#include "vectortiles/utils/GeometryConverter.h"
#include "vectortiles/utils/ValueConverter.h"
#include "vectortiles/utils/VTBitmapLoader.h"
#include "vectortiles/utils/CartoCSSAssetLoader.h"
#include "utils/AssetPackage.h"
#include "utils/FileUtils.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <vt/Tile.h>
#include <mapnikvt/Value.h>
#include <mapnikvt/SymbolizerParser.h>
#include <mapnikvt/SymbolizerContext.h>
#include <mapnikvt/MBVTFeatureDecoder.h>
#include <mapnikvt/MBVTTileReader.h>
#include <mapnikvt/MapParser.h>
#include <cartocss/CartoCSSMapLoader.h>

#include <functional>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace carto {
    
    CartoVectorTileDecoder::CartoVectorTileDecoder(const std::vector<std::string>& layerIds, const std::map<std::string, std::shared_ptr<CartoCSSStyleSet> >& layerStyleSets) :
        _logger(std::make_shared<MapnikVTLogger>("CartoVectorTileDecoder")),
        _layerIds(layerIds),
        _layerInvisibleSet(),
        _layerStyleSets(),
        _layerMaps(),
        _layerSymbolizerContexts(),
        _assetPackageSymbolizerContexts(),
        _backgroundColor(),
        _backgroundPattern()
    {
        for (auto it = layerStyleSets.begin(); it != layerStyleSets.end(); it++) {
            updateLayerStyleSet(it->first, it->second);
        }
    }
    
    CartoVectorTileDecoder::~CartoVectorTileDecoder() {
    }

    std::vector<std::string> CartoVectorTileDecoder::getLayerIds() const {
        return _layerIds;
    }

    bool CartoVectorTileDecoder::isLayerVisible(const std::string& layerId) const {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _layerStyleSets.find(layerId);
        if (it == _layerStyleSets.end()) {
            throw OutOfRangeException("Invalid layer id");
        }
        return _layerInvisibleSet.count(layerId) == 0;
    }

    void CartoVectorTileDecoder::setLayerVisible(const std::string& layerId, bool visible) {
        {
            std::lock_guard<std::mutex> lock(_mutex);

            auto it = _layerStyleSets.find(layerId);
            if (it == _layerStyleSets.end()) {
                throw OutOfRangeException("Invalid layer id");
            }
            if (visible) {
                _layerInvisibleSet.erase(layerId);
            } else {
                _layerInvisibleSet.insert(layerId);
            }
        }
        notifyDecoderChanged();
    }

    std::shared_ptr<CartoCSSStyleSet> CartoVectorTileDecoder::getLayerStyleSet(const std::string& layerId) const {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _layerStyleSets.find(layerId);
        if (it == _layerStyleSets.end()) {
            throw OutOfRangeException("Invalid layer id");
        }
        return it->second;
    }
    
    void CartoVectorTileDecoder::setLayerStyleSet(const std::string& layerId, const std::shared_ptr<CartoCSSStyleSet>& styleSet) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _layerStyleSets.find(layerId);
            if (it == _layerStyleSets.end()) {
                throw OutOfRangeException("Invalid layer id");
            }
            updateLayerStyleSet(layerId, styleSet);
        }
        notifyDecoderChanged();
    }

    Color CartoVectorTileDecoder::getBackgroundColor() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _backgroundColor;
    }
    
    std::shared_ptr<const vt::BitmapPattern> CartoVectorTileDecoder::getBackgroundPattern() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _backgroundPattern;
    }
        
    int CartoVectorTileDecoder::getMinZoom() const {
        return 0;
    }
    
    int CartoVectorTileDecoder::getMaxZoom() const {
        return Const::MAX_SUPPORTED_ZOOM_LEVEL;
    }

    std::shared_ptr<VectorTileFeature> CartoVectorTileDecoder::decodeFeature(long long id, const vt::TileId& tile, const std::shared_ptr<BinaryData>& tileData, const MapBounds& tileBounds) const {
        if (!tileData) {
            Log::Warn("CartoVectorTileDecoder::decodeFeature: Null tile data");
            return std::shared_ptr<VectorTileFeature>();
        }
        if (tileData->empty()) {
            return std::shared_ptr<VectorTileFeature>();
        }

        try {
            std::shared_ptr<mvt::MBVTFeatureDecoder> decoder;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                if (_cachedFeatureDecoder.first != tileData) {
                    lock.unlock();
                    decoder = std::make_shared<mvt::MBVTFeatureDecoder>(*tileData->getDataPtr(), _logger);
                    lock.lock();
                    _cachedFeatureDecoder = std::make_pair(tileData, decoder);
                }
                else {
                    decoder = _cachedFeatureDecoder.second;
                }
            }

            std::string mvtLayerName;
            std::shared_ptr<const mvt::Feature> mvtFeature = decoder->getFeature(id, mvtLayerName);
            if (!mvtFeature) {
                return std::shared_ptr<VectorTileFeature>();
            }

            std::shared_ptr<const mvt::Geometry> mvtGeometry = mvtFeature->getGeometry();
            if (!mvtGeometry) {
                return std::shared_ptr<VectorTileFeature>();
            }

            std::map<std::string, Variant> featureData;
            if (std::shared_ptr<const mvt::FeatureData> mvtFeatureData = mvtFeature->getFeatureData()) {
                for (const std::string& varName : mvtFeatureData->getVariableNames()) {
                    mvt::Value mvtValue;
                    mvtFeatureData->getVariable(varName, mvtValue);
                    featureData[varName] = boost::apply_visitor(ValueConverter(), mvtValue);
                }
            }

            auto convertFn = [&tileBounds](const cglib::vec2<float>& pos) {
                return MapPos(tileBounds.getMin().getX() + pos(0) * tileBounds.getDelta().getX(), tileBounds.getMax().getY() - pos(1) * tileBounds.getDelta().getY(), 0);
            };

            return std::make_shared<VectorTileFeature>(mvtFeature->getId(), MapTile(tile.x, tile.y, tile.zoom, 0), mvtLayerName, convertGeometry(convertFn, mvtGeometry), Variant(featureData));
        } catch (const std::exception& ex) {
            Log::Errorf("CartoVectorTileDecoder::decodeFeature: Exception while decoding: %s", ex.what());
        }
        return std::shared_ptr<VectorTileFeature>();
    }

    std::shared_ptr<VectorTileFeatureCollection> CartoVectorTileDecoder::decodeFeatures(const vt::TileId& tile, const std::shared_ptr<BinaryData>& tileData, const MapBounds& tileBounds) const {
        if (!tileData) {
            Log::Warn("CartoVectorTileDecoder::decodeFeatures: Null tile data");
            return std::shared_ptr<VectorTileFeatureCollection>();
        }
        if (tileData->empty()) {
            return std::shared_ptr<VectorTileFeatureCollection>();
        }

        std::vector<std::shared_ptr<VectorTileFeature> > tileFeatures;
        try {
            std::shared_ptr<mvt::MBVTFeatureDecoder> decoder;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                if (_cachedFeatureDecoder.first != tileData) {
                    lock.unlock();
                    decoder = std::make_shared<mvt::MBVTFeatureDecoder>(*tileData->getDataPtr(), _logger);
                    lock.lock();
                    _cachedFeatureDecoder = std::make_pair(tileData, decoder);
                }
                else {
                    decoder = _cachedFeatureDecoder.second;
                }
            }

            for (const std::string& mvtLayerName : decoder->getLayerNames()) {
                for (std::shared_ptr<mvt::FeatureDecoder::FeatureIterator> mvtIt = decoder->createLayerFeatureIterator(mvtLayerName); mvtIt->valid(); mvtIt->advance()) {
                    std::shared_ptr<const mvt::Geometry> mvtGeometry = mvtIt->getGeometry();
                    if (!mvtGeometry) {
                        continue;
                    }

                    std::map<std::string, Variant> featureData;
                    if (std::shared_ptr<const mvt::FeatureData> mvtFeatureData = mvtIt->getFeatureData()) {
                        for (const std::string& varName : mvtFeatureData->getVariableNames()) {
                            mvt::Value mvtValue;
                            mvtFeatureData->getVariable(varName, mvtValue);
                            featureData[varName] = boost::apply_visitor(ValueConverter(), mvtValue);
                        }
                    }

                    auto convertFn = [&tileBounds](const cglib::vec2<float>& pos) {
                        return MapPos(tileBounds.getMin().getX() + pos(0) * tileBounds.getDelta().getX(), tileBounds.getMax().getY() - pos(1) * tileBounds.getDelta().getY(), 0);
                    };

                    auto feature = std::make_shared<VectorTileFeature>(mvtIt->getGlobalId(), MapTile(tile.x, tile.y, tile.zoom, 0), mvtLayerName, convertGeometry(convertFn, mvtGeometry), Variant(featureData));
                    tileFeatures.push_back(feature);
                }
            }
        } catch (const std::exception& ex) {
            Log::Errorf("CartoVectorTileDecoder::decodeFeatures: Exception while decoding: %s", ex.what());
            return std::shared_ptr<VectorTileFeatureCollection>();
        }
        return std::make_shared<VectorTileFeatureCollection>(tileFeatures);
    }

    std::shared_ptr<CartoVectorTileDecoder::TileMap> CartoVectorTileDecoder::decodeTile(const vt::TileId& tile, const vt::TileId& targetTile, const std::shared_ptr<BinaryData>& tileData) const {
        if (!tileData) {
            Log::Warn("CartoVectorTileDecoder::decodeTile: Null tile data");
            return std::shared_ptr<TileMap>();
        }
        if (tileData->empty()) {
            return std::shared_ptr<TileMap>();
        }

        std::set<std::string> layerInvisibleSet;
        std::map<std::string, std::shared_ptr<mvt::Map> > layerMaps;
        std::map<std::string, std::shared_ptr<mvt::SymbolizerContext> > layerSymbolizerContexts;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            layerInvisibleSet = _layerInvisibleSet;
            layerMaps = _layerMaps;
            layerSymbolizerContexts = _layerSymbolizerContexts;
        }
    
        try {
            mvt::MBVTFeatureDecoder decoder(*tileData->getDataPtr(), _logger);
            decoder.setTransform(calculateTileTransform(tile, targetTile));
            decoder.setGlobalIdOverride(true, MapTile(tile.x, tile.y, tile.zoom, 0).getTileId());

            std::vector<std::shared_ptr<vt::Tile> > tiles(_layerIds.size());
            for (auto it = layerMaps.begin(); it != layerMaps.end(); it++) {
                if (layerInvisibleSet.count(it->first) > 0) {
                    continue;
                }

                std::size_t index = std::distance(_layerIds.begin(), std::find(_layerIds.begin(), _layerIds.end(), it->first));
                if (index >= tiles.size()) {
                    continue;
                }

                mvt::MBVTTileReader reader(it->second, *layerSymbolizerContexts[it->first], decoder);
                reader.setLayerNameOverride(it->first);
                tiles[index] = reader.readTile(targetTile);
            }

            std::vector<std::shared_ptr<vt::TileLayer> > tileLayers;
            for (std::size_t i = 0; i < tiles.size(); i++) {
                if (std::shared_ptr<vt::Tile> tile = tiles[i]) {
                    for (const std::shared_ptr<vt::TileLayer>& tileLayer : tile->getLayers()) {
                        int layerIdx = static_cast<int>(i * 65536) + tileLayer->getLayerIndex();
                        tileLayers.push_back(std::make_shared<vt::TileLayer>(layerIdx, tileLayer->getCompOp(), tileLayer->getOpacityFunc(), tileLayer->getBitmaps(), tileLayer->getGeometries(), tileLayer->getLabels()));
                    }
                }
            }

            auto tileMap = std::make_shared<TileMap>();
            (*tileMap)[0] = std::make_shared<vt::Tile>(targetTile, tileLayers);
            return tileMap;
        } catch (const std::exception& ex) {
            Log::Errorf("CartoVectorTileDecoder::decodeTile: Exception while decoding: %s", ex.what());
        }
        return std::shared_ptr<TileMap>();
    }

    void CartoVectorTileDecoder::updateLayerStyleSet(const std::string& layerId, const std::shared_ptr<CartoCSSStyleSet>& styleSet) {
        if (!styleSet) {
            throw NullArgumentException("Null styleset");
        }

        std::shared_ptr<AssetPackage> assetPackage = styleSet->getAssetPackage();
        std::shared_ptr<mvt::SymbolizerContext>& symbolizerContext = _assetPackageSymbolizerContexts[assetPackage];
        if (!symbolizerContext) {
            mvt::SymbolizerContext::Settings settings(DEFAULT_TILE_SIZE, std::map<std::string, mvt::Value>());
            auto fontManager = std::make_shared<vt::FontManager>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);
            auto bitmapLoader = std::make_shared<VTBitmapLoader>("", assetPackage);
            auto bitmapManager = std::make_shared<vt::BitmapManager>(bitmapLoader);
            auto strokeMap = std::make_shared<vt::StrokeMap>(STROKEMAP_SIZE, STROKEMAP_SIZE);
            auto glyphMap = std::make_shared<vt::GlyphMap>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);
            symbolizerContext = std::make_shared<mvt::SymbolizerContext>(bitmapManager, fontManager, strokeMap, glyphMap, settings);

            if (assetPackage) {
                std::string fontPrefix = FileUtils::NormalizePath("fonts/");

                for (const std::string& assetName : assetPackage->getAssetNames()) {
                    if (assetName.size() > fontPrefix.size() && assetName.substr(0, fontPrefix.size()) == fontPrefix) {
                        if (std::shared_ptr<BinaryData> fontData = assetPackage->loadAsset(assetName)) {
                            fontManager->loadFontData(*fontData->getDataPtr());
                        }
                    }
                }
            }
        }

        std::shared_ptr<mvt::Map> map;
        try {
            auto assetLoader = std::make_shared<CartoCSSAssetLoader>("", assetPackage);
            css::CartoCSSMapLoader mapLoader(assetLoader, _logger);
            mapLoader.setIgnoreLayerPredicates(true);
            map = mapLoader.loadMap(styleSet->getCartoCSS());
        }
        catch (const std::exception& ex) {
            throw ParseException("CartoCSS style parsing failed", ex.what());
        }

        if (!_layerIds.empty() && _layerIds.front() == layerId) {
            _backgroundColor = Color(map->getSettings().backgroundColor.value());

            std::shared_ptr<const vt::BitmapPattern> backgroundPattern;
            if (!map->getSettings().backgroundImage.empty()) {
                auto bitmapLoader = std::make_shared<VTBitmapLoader>("", assetPackage);
                auto bitmapManager = std::make_shared<vt::BitmapManager>(bitmapLoader);
                backgroundPattern = bitmapManager->loadBitmapPattern(map->getSettings().backgroundImage, 1.0f, 1.0f);
            }
            _backgroundPattern = backgroundPattern;
        }

        _layerStyleSets[layerId] = styleSet;
        _layerMaps[layerId] = map;
        _layerSymbolizerContexts[layerId] = symbolizerContext;
    }

    const int CartoVectorTileDecoder::DEFAULT_TILE_SIZE = 256;
    const int CartoVectorTileDecoder::STROKEMAP_SIZE = 512;
    const int CartoVectorTileDecoder::GLYPHMAP_SIZE = 2048;
}
