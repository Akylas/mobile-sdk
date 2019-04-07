#include "MBVectorTileDecoder.h"
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
#include "styles/CompiledStyleSet.h"
#include "styles/CartoCSSStyleSet.h"
#include "vectortiles/utils/GeometryConverter.h"
#include "vectortiles/utils/ValueConverter.h"
#include "vectortiles/utils/MapnikVTLogger.h"
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
    
    MBVectorTileDecoder::MBVectorTileDecoder(const std::shared_ptr<CompiledStyleSet>& compiledStyleSet) :
        _logger(std::make_shared<MapnikVTLogger>("MBVectorTileDecoder")),
        _featureIdOverride(false),
        _cartoCSSLayerNamesIgnored(false),
        _layerNameOverride(),
        _fallbackFonts(),
        _styleSet(),
        _map(),
        _mapSettings(),
        _parameterValueMap(),
        _updatedParameters(),
        _symbolizerContext(),
        _assetPackageSymbolizerContexts()
    {
        if (!compiledStyleSet) {
            throw NullArgumentException("Null compiledStyleSet");
        }

        updateCurrentStyleSet(compiledStyleSet);
    }
    
    MBVectorTileDecoder::MBVectorTileDecoder(const std::shared_ptr<CartoCSSStyleSet>& cartoCSSStyleSet) :
        _logger(std::make_shared<MapnikVTLogger>("MBVectorTileDecoder")),
        _featureIdOverride(false),
        _cartoCSSLayerNamesIgnored(false),
        _layerNameOverride(),
        _fallbackFonts(),
        _styleSet(),
        _map(),
        _parameterValueMap(),
        _updatedParameters(),
        _symbolizerContext()
    {
        if (!cartoCSSStyleSet) {
            throw NullArgumentException("Null cartoCSSStyleSet");
        }

        updateCurrentStyleSet(cartoCSSStyleSet);
    }
    
    MBVectorTileDecoder::~MBVectorTileDecoder() {
    }
        
    std::shared_ptr<CompiledStyleSet> MBVectorTileDecoder::getCompiledStyleSet() const {
        std::lock_guard<std::mutex> lock(_mutex);
        if (auto compiledStyleSet = boost::get<std::shared_ptr<CompiledStyleSet> >(&_styleSet)) {
            return *compiledStyleSet;
        }
        return std::shared_ptr<CompiledStyleSet>();
    }
    
    void MBVectorTileDecoder::setCompiledStyleSet(const std::shared_ptr<CompiledStyleSet>& styleSet) {
        if (!styleSet) {
            throw NullArgumentException("Null styleSet");
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            updateCurrentStyleSet(styleSet);
        }
        notifyDecoderChanged();
    }

    std::shared_ptr<CartoCSSStyleSet> MBVectorTileDecoder::getCartoCSSStyleSet() const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (auto cartoCSSStyleSet = boost::get<std::shared_ptr<CartoCSSStyleSet> >(&_styleSet)) {
            return *cartoCSSStyleSet;
        }
        return std::shared_ptr<CartoCSSStyleSet>();
    }
    
    void MBVectorTileDecoder::setCartoCSSStyleSet(const std::shared_ptr<CartoCSSStyleSet>& styleSet) {
        if (!styleSet) {
            throw NullArgumentException("Null styleSet");
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            updateCurrentStyleSet(styleSet);
        }
        notifyDecoderChanged();
    }

    std::vector<std::string> MBVectorTileDecoder::getStyleParameters() const {
        std::lock_guard<std::mutex> lock(_mutex);
    
        std::vector<std::string> params;
        for (auto it = _map->getNutiParameterMap().begin(); it != _map->getNutiParameterMap().end(); it++) {
            params.push_back(it->first);
        }
        return params;
    }

    std::string MBVectorTileDecoder::getStyleParameter(const std::string& param) const {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _map->getNutiParameterMap().find(param);
        if (it == _map->getNutiParameterMap().end()) {
            throw InvalidArgumentException("Could not find parameter");
        }
        const mvt::NutiParameter& nutiParam = it->second;
        
        mvt::Value value = nutiParam.getDefaultValue();
        {
            auto it2 = _parameterValueMap.find(param);
            if (it2 != _parameterValueMap.end()) {
                value = it2->second;
            }
        }

        if (!nutiParam.getEnumMap().empty()) {
            for (auto it2 = nutiParam.getEnumMap().begin(); it2 != nutiParam.getEnumMap().end(); it2++) {
                if (it2->second == value) {
                    return it2->first;
                }
            }
        } else {
            if (auto val = boost::get<bool>(&value)) {
                return boost::lexical_cast<std::string>(*val);
            } else if (auto val = boost::get<long long>(&value)) {
                return boost::lexical_cast<std::string>(*val);
            } else if (auto val = boost::get<double>(&value)) {
                return boost::lexical_cast<std::string>(*val);
            } else if (auto val = boost::get<std::string>(&value)) {
                return *val;
            }
        }
        return std::string();
    }

    bool MBVectorTileDecoder::setStyleParameter(const std::string& param, const std::string& value) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
    
            auto it = _map->getNutiParameterMap().find(param);
            if (it == _map->getNutiParameterMap().end()) {
                Log::Infof("MBVectorTileDecoder::setStyleParameter: Could not find parameter: %s", param.c_str());
                return false;
            }
            const mvt::NutiParameter& nutiParam = it->second;

            if (!nutiParam.getEnumMap().empty()) {
                auto it2 = nutiParam.getEnumMap().find(boost::lexical_cast<std::string>(value));
                if (it2 == nutiParam.getEnumMap().end()) {
                    Log::Infof("MBVectorTileDecoder::setStyleParameter: Illegal enum value for parameter: %s/%s", param.c_str(), value.c_str());
                    return false;
                }
                _parameterValueMap[param] = it2->second;
                _updatedParameters.insert(param);
            } else {
                mvt::Value val = nutiParam.getDefaultValue();
                if (boost::get<bool>(&val)) {
                    if (value == "true") {
                        val = mvt::Value(true);
                    } else if (value == "false") {
                        val = mvt::Value(false);
                    } else {
                        val = mvt::Value(boost::lexical_cast<bool>(value));
                    }
                } else if (boost::get<long long>(&val)) {
                    val = mvt::Value(boost::lexical_cast<long long>(value));
                } else if (boost::get<double>(&val)) {
                    val = mvt::Value(boost::lexical_cast<double>(value));
                } else if (boost::get<std::string>(&val)) {
                    val = value;
                }
                _parameterValueMap[param] = val;
                _updatedParameters.insert(param);
            }
    
            mvt::SymbolizerContext::Settings settings(DEFAULT_TILE_SIZE, _parameterValueMap);
            _symbolizerContext = std::make_shared<mvt::SymbolizerContext>(_symbolizerContext->getBitmapManager(), _symbolizerContext->getFontManager(), _symbolizerContext->getStrokeMap(), _symbolizerContext->getGlyphMap(), settings);
        }
        notifyDecoderChanged();
        return true;
    }

    bool MBVectorTileDecoder::isFeatureIdOverride() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _featureIdOverride;
    }

    void MBVectorTileDecoder::setFeatureIdOverride(bool idOverride) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _featureIdOverride = idOverride;
        }
        notifyDecoderChanged();
    }
        
    bool MBVectorTileDecoder::isCartoCSSLayerNamesIgnored() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _cartoCSSLayerNamesIgnored;
    }

    void MBVectorTileDecoder::setCartoCSSLayerNamesIgnored(bool ignore) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _cartoCSSLayerNamesIgnored = ignore;
            updateCurrentStyleSet(_styleSet);
        }
        notifyDecoderChanged();
    }
        
    std::string MBVectorTileDecoder::getLayerNameOverride() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _layerNameOverride;
    }

    void MBVectorTileDecoder::setLayerNameOverride(const std::string& name) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _layerNameOverride = name;
        }
        notifyDecoderChanged();
    }

    std::shared_ptr<mvt::Map::Settings> MBVectorTileDecoder::getMapSettings() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _mapSettings;
    }

    void MBVectorTileDecoder::addFallbackFont(const std::shared_ptr<BinaryData>& fontData) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (fontData) {
                _fallbackFonts.push_back(fontData);
                _assetPackageSymbolizerContexts.clear();
                updateCurrentStyleSet(_styleSet);
            }
        }
        notifyDecoderChanged();
    }
    
    int MBVectorTileDecoder::getMinZoom() const {
        return 0;
    }
    
    int MBVectorTileDecoder::getMaxZoom() const {
        return Const::MAX_SUPPORTED_ZOOM_LEVEL;
    }

    std::shared_ptr<VectorTileFeature> MBVectorTileDecoder::decodeFeature(long long id, const vt::TileId& tile, const std::shared_ptr<BinaryData>& tileData, const MapBounds& tileBounds) const {
        if (!tileData) {
            Log::Warn("MBVectorTileDecoder::decodeFeature: Null tile data");
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
                } else {
                    decoder = _cachedFeatureDecoder.second;
                }
            }

            std::string mvtLayerName;
            mvt::Feature mvtFeature;
            if (!decoder->findFeature(id, mvtLayerName, mvtFeature)) {
                return std::shared_ptr<VectorTileFeature>();
            }

            std::shared_ptr<const mvt::Geometry> mvtGeometry = mvtFeature.getGeometry();
            if (!mvtGeometry) {
                return std::shared_ptr<VectorTileFeature>();
            }

            std::map<std::string, Variant> featureData;
            if (std::shared_ptr<const mvt::FeatureData> mvtFeatureData = mvtFeature.getFeatureData()) {
                for (const std::string& varName : mvtFeatureData->getVariableNames()) {
                    mvt::Value mvtValue;
                    if (mvtFeatureData->getVariable(varName, mvtValue)) {
                        featureData[varName] = boost::apply_visitor(ValueConverter(), mvtValue);
                    }
                }
            }

            auto convertFn = [&tileBounds](const cglib::vec2<float>& pos) {
                return MapPos(tileBounds.getMin().getX() + pos(0) * tileBounds.getDelta().getX(), tileBounds.getMax().getY() - pos(1) * tileBounds.getDelta().getY(), 0);
            };

            return std::make_shared<VectorTileFeature>(mvtFeature.getId(), MapTile(tile.x, tile.y, tile.zoom, 0), mvtLayerName, convertGeometry(convertFn, mvtGeometry), Variant(featureData));
        } catch (const std::exception& ex) {
            Log::Errorf("MBVectorTileDecoder::decodeFeature: Exception while decoding: %s", ex.what());
        }
        return std::shared_ptr<VectorTileFeature>();
    }

    std::shared_ptr<VectorTileFeatureCollection> MBVectorTileDecoder::decodeFeatures(const vt::TileId& tile, const std::shared_ptr<BinaryData>& tileData, const MapBounds& tileBounds) const {
        if (!tileData) {
            Log::Warn("MBVectorTileDecoder::decodeFeatures: Null tile data");
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
                } else {
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
                            if (mvtFeatureData->getVariable(varName, mvtValue)) {
                                featureData[varName] = boost::apply_visitor(ValueConverter(), mvtValue);
                            }
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
            Log::Errorf("MBVectorTileDecoder::decodeFeatures: Exception while decoding: %s", ex.what());
            return std::shared_ptr<VectorTileFeatureCollection>();
        }
        return std::make_shared<VectorTileFeatureCollection>(tileFeatures);
    }

    std::shared_ptr<MBVectorTileDecoder::TileMap> MBVectorTileDecoder::decodeTile(const vt::TileId& tile, const vt::TileId& targetTile, const std::shared_ptr<vt::TileTransformer>& tileTransformer, const std::shared_ptr<BinaryData>& tileData) const {
        if (!tileData) {
            Log::Warn("MBVectorTileDecoder::decodeTile: Null tile data");
            return std::shared_ptr<TileMap>();
        }

        std::shared_ptr<mvt::Map> map;
        std::shared_ptr<mvt::SymbolizerContext> symbolizerContext;
        bool featureIdOverride;
        std::string layerNameOverride;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            map = _map;
            symbolizerContext = _symbolizerContext;
            featureIdOverride = _featureIdOverride;
            layerNameOverride = _layerNameOverride;
        }
    
        try {
            mvt::MBVTFeatureDecoder decoder(*tileData->getDataPtr(), _logger);
            decoder.setTransform(calculateTileTransform(tile, targetTile));
            decoder.setGlobalIdOverride(featureIdOverride, MapTile(tile.x, tile.y, tile.zoom, 0).getTileId());
            
            mvt::MBVTTileReader reader(map, tileTransformer, *symbolizerContext, decoder);
            reader.setLayerNameOverride(layerNameOverride);

            if (std::shared_ptr<vt::Tile> tile = reader.readTile(targetTile)) {
                auto tileMap = std::make_shared<TileMap>();
                (*tileMap)[0] = tile;
                return tileMap;
            }
        } catch (const std::exception& ex) {
            Log::Errorf("MBVectorTileDecoder::decodeTile: Exception while decoding: %s", ex.what());
        }
        return std::shared_ptr<TileMap>();
    }

    void MBVectorTileDecoder::updateCurrentStyleSet(const boost::variant<std::shared_ptr<CompiledStyleSet>, std::shared_ptr<CartoCSSStyleSet> >& styleSet) {
        std::string styleAssetName;
        std::shared_ptr<AssetPackage> assetPackage;
        std::shared_ptr<mvt::Map> map;

        if (auto cartoCSSStyleSet = boost::get<std::shared_ptr<CartoCSSStyleSet> >(&styleSet)) {
            styleAssetName = "";
            assetPackage = (*cartoCSSStyleSet)->getAssetPackage();

            try {
                auto assetLoader = std::make_shared<CartoCSSAssetLoader>("", (*cartoCSSStyleSet)->getAssetPackage());
                css::CartoCSSMapLoader mapLoader(assetLoader, _logger);
                mapLoader.setIgnoreLayerPredicates(_cartoCSSLayerNamesIgnored);
                map = mapLoader.loadMap((*cartoCSSStyleSet)->getCartoCSS());
            } catch (const std::exception& ex) {
                throw ParseException(std::string("CartoCSS style parsing failed: ") + ex.what(), (*cartoCSSStyleSet)->getCartoCSS());
            }
        } else if (auto compiledStyleSet = boost::get<std::shared_ptr<CompiledStyleSet> >(&styleSet)) {
            styleAssetName = (*compiledStyleSet)->getStyleAssetName();
            if (styleAssetName.empty()) {
                throw InvalidArgumentException("Could not find any styles in the style set");
            }

            assetPackage = (*compiledStyleSet)->getAssetPackage();

            std::shared_ptr<BinaryData> styleData;
            if (assetPackage) {
                styleData = assetPackage->loadAsset(styleAssetName);
            }
            if (!styleData) {
                throw GenericException("Failed to load style description asset");
            }

            if (boost::algorithm::ends_with(styleAssetName, ".xml")) {
                pugi::xml_document doc;
                if (!doc.load_buffer(styleData->data(), styleData->size())) {
                    throw ParseException("Style element XML parsing failed");
                }
                try {
                    auto symbolizerParser = std::make_shared<mvt::SymbolizerParser>(_logger);
                    mvt::MapParser mapParser(symbolizerParser, _logger);
                    map = mapParser.parseMap(doc);
                } catch (const std::exception& ex) {
                    throw ParseException(std::string("XML style processing failed: ") + ex.what());
                }
            } else if (boost::algorithm::ends_with(styleAssetName, ".json")) {
                try {
                    auto assetLoader = std::make_shared<CartoCSSAssetLoader>(FileUtils::GetFilePath(styleAssetName), assetPackage);
                    css::CartoCSSMapLoader mapLoader(assetLoader, _logger);
                    mapLoader.setIgnoreLayerPredicates(_cartoCSSLayerNamesIgnored);
                    map = mapLoader.loadMapProject(styleAssetName);
                } catch (const std::exception& ex) {
                    throw GenericException(std::string("CartoCSS style loading failed: ") + ex.what());
                }
            } else {
                throw GenericException("Failed to detect style asset type");
            }
        } else {
            throw InvalidArgumentException("Invalid style set");
        }

        if (_assetPackageSymbolizerContexts.find(assetPackage) == _assetPackageSymbolizerContexts.end() && _assetPackageSymbolizerContexts.size() >= MAX_ASSETPACKAGE_SYMBOLIZER_CONTEXTS) {
            _assetPackageSymbolizerContexts.clear();
        }
        std::shared_ptr<mvt::SymbolizerContext>& symbolizerContext = _assetPackageSymbolizerContexts[assetPackage];
        if (!symbolizerContext) {
            auto fontManager = std::make_shared<vt::FontManager>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);
            auto bitmapLoader = std::make_shared<VTBitmapLoader>(FileUtils::GetFilePath(styleAssetName), assetPackage);
            auto bitmapManager = std::make_shared<vt::BitmapManager>(bitmapLoader);
            auto strokeMap = std::make_shared<vt::StrokeMap>(STROKEMAP_SIZE, STROKEMAP_SIZE);
            auto glyphMap = std::make_shared<vt::GlyphMap>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);

            std::shared_ptr<vt::Font> fallbackFont;
            for (auto it = _fallbackFonts.rbegin(); it != _fallbackFonts.rend(); it++) {
                std::shared_ptr<BinaryData> fontData = *it;
                std::string fontName = fontManager->loadFontData(*fontData->getDataPtr());
                fallbackFont = fontManager->getFont(fontName, fallbackFont);
            }
            mvt::SymbolizerContext::Settings settings(DEFAULT_TILE_SIZE, parameterValueMap, fallbackFont);
            symbolizerContext = std::make_shared<mvt::SymbolizerContext>(bitmapManager, fontManager, strokeMap, glyphMap, settings);

            if (assetPackage) {
                std::string fontPrefix = map->getSettings().fontDirectory;
                fontPrefix = FileUtils::NormalizePath(FileUtils::GetFilePath(styleAssetName) + fontPrefix + "/");

                for (const std::string& assetName : assetPackage->getAssetNames()) {
                    if (assetName.size() > fontPrefix.size() && assetName.substr(0, fontPrefix.size()) == fontPrefix) {
                        if (std::shared_ptr<BinaryData> fontData = assetPackage->loadAsset(assetName)) {
                            fontManager->loadFontData(*fontData->getDataPtr());
                        }
                    }
                }
            }
        }

        std::map<std::string, mvt::Value> parameterValueMap;
        std::set<std::string> updatedParameters;
        for (auto it = map->getNutiParameterMap().begin(); it != map->getNutiParameterMap().end(); it++) {
            if (_updatedParameters.find(it->first) != _updatedParameters.end()) {
                auto it2 = _parameterValueMap.find(it->first);
                if (it2 != _parameterValueMap.end()) {
                    bool valid = it->second.getDefaultValue().which() == it2->second.which();
                    if (!it->second.getEnumMap().empty()) {
                        valid = false;
                        for (const std::pair<std::string, mvt::Value>& enumValue : it->second.getEnumMap()) {
                            if (enumValue.second == it2->second) {
                                valid = true;
                            }
                        }
                    }
                    if (valid) {
                        parameterValueMap[it->first] = it2->second;
                        updatedParameters.insert(it->first);
                        continue;
                    }
                }
            }

            parameterValueMap[it->first] = it->second.getDefaultValue();
        }

        _map = map;
        _mapSettings = std::make_shared<mvt::Map::Settings>(_map->getSettings());
        _parameterValueMap = parameterValueMap;
        _updatedParameters = updatedParameters;
        _symbolizerContext = symbolizerContext;
        _styleSet = styleSet;
        _cachedFeatureDecoder.first.reset();
        _cachedFeatureDecoder.second.reset();
    }

    const int MBVectorTileDecoder::DEFAULT_TILE_SIZE = 256;
    const int MBVectorTileDecoder::STROKEMAP_SIZE = 512;
    const int MBVectorTileDecoder::GLYPHMAP_SIZE = 2048;
    const int MBVectorTileDecoder::MAX_ASSETPACKAGE_SYMBOLIZER_CONTEXTS = 2;
}
