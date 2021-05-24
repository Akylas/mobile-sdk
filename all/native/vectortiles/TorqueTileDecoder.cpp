#include "TorqueTileDecoder.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "styles/CartoCSSStyleSet.h"
#include "vectortiles/utils/MapnikVTLogger.h"
#include "vectortiles/utils/VTBitmapLoader.h"
#include "vectortiles/utils/CartoCSSAssetLoader.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <vt/Tile.h>
#include <mapnikvt/Value.h>
#include <mapnikvt/SymbolizerContext.h>
#include <mapnikvt/TorqueFeatureDecoder.h>
#include <mapnikvt/TorqueTileReader.h>
#include <cartocss/TorqueCartoCSSMapLoader.h>

#include <boost/lexical_cast.hpp>

namespace carto {
    
    TorqueTileDecoder::TorqueTileDecoder(const std::shared_ptr<CartoCSSStyleSet>& styleSet) :
        _logger(std::make_shared<MapnikVTLogger>("TorqueTileDecoder")),
        _fallbackFonts(),
        _map(),
        _mapSettings(),
        _symbolizerContext(),
        _styleSet(),
        _mutex()
    {
        if (!styleSet) {
            throw NullArgumentException("Null styleSet");
        }

        updateCurrentStyleSet(styleSet);
    }
    
    TorqueTileDecoder::~TorqueTileDecoder() {
    }

    int TorqueTileDecoder::getFrameCount() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _map->getTorqueSettings().frameCount;
    }

    int TorqueTileDecoder::getResolution() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _map->getTorqueSettings().resolution;
    }
    
    std::shared_ptr<CartoCSSStyleSet> TorqueTileDecoder::getStyleSet() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _styleSet;
    }
    
    void TorqueTileDecoder::setStyleSet(const std::shared_ptr<CartoCSSStyleSet>& styleSet) {
        if (!styleSet) {
            throw NullArgumentException("Null styleSet");
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            updateCurrentStyleSet(styleSet);
        }
        notifyDecoderChanged();
    }

    std::shared_ptr<mvt::Map::Settings> TorqueTileDecoder::getMapSettings() const  {
        std::lock_guard<std::mutex> lock(_mutex);
        return _mapSettings;
    }
    
    void TorqueTileDecoder::addFallbackFont(const std::shared_ptr<BinaryData>& fontData) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (fontData) {
                _fallbackFonts.push_back(fontData);
                updateCurrentStyleSet(_styleSet);
            }
        }
        notifyDecoderChanged();
    }

    int TorqueTileDecoder::getMinZoom() const {
        return 0;
    }
    
    int TorqueTileDecoder::getMaxZoom() const {
        return Const::MAX_SUPPORTED_ZOOM_LEVEL;
    }
        
    std::shared_ptr<VectorTileFeature> TorqueTileDecoder::decodeFeature(long long id, const vt::TileId& tile, const std::shared_ptr<BinaryData>& tileData, const MapBounds& tileBounds) const {
        Log::Warn("TorqueTileDecoder::decodeFeature: Not implemented");
        return std::shared_ptr<VectorTileFeature>();
    }

    std::shared_ptr<VectorTileFeatureCollection> TorqueTileDecoder::decodeFeatures(const vt::TileId& tile, const std::shared_ptr<BinaryData>& tileData, const MapBounds& tileBounds) const {
        Log::Warn("TorqueTileDecoder::decodeFeatures: Not implemented");
        return std::shared_ptr<VectorTileFeatureCollection>();
    }

    std::shared_ptr<TorqueTileDecoder::TileMap> TorqueTileDecoder::decodeTile(const vt::TileId& tile, const vt::TileId& targetTile, const std::shared_ptr<vt::TileTransformer>& tileTransformer, const std::shared_ptr<BinaryData>& tileData) const {
        if (!tileData) {
            Log::Warn("TorqueTileDecoder::decodeTile: Null tile data");
            return std::shared_ptr<TileMap>();
        }
        if (tileData->empty()) {
            return std::shared_ptr<TileMap>();
        }

        int resolution;
        std::shared_ptr<mvt::TorqueMap> map;
        std::shared_ptr<mvt::SymbolizerContext> symbolizerContext;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            int settingsResolution = (_map ? _map->getTorqueSettings().resolution : -1);
            resolution = DEFAULT_TILE_SIZE / (settingsResolution > 0 ? settingsResolution : 1);
            map = _map;
            symbolizerContext = _symbolizerContext;
        }
    
        try {
            mvt::TorqueFeatureDecoder decoder(*tileData->getDataPtr(), resolution, _logger);
            decoder.setTransform(calculateTileTransform(tile, targetTile));

            auto tileMap = std::make_shared<TileMap>();
            for (int frame = 0; frame < map->getTorqueSettings().frameCount; frame++) {
                mvt::TorqueTileReader reader(map, frame, true, tileTransformer, *symbolizerContext, decoder);
                if (std::shared_ptr<vt::Tile> tile = reader.readTile(targetTile)) {
                    (*tileMap)[frame] = tile;
                }
            }
            return tileMap;
        }
        catch (const std::exception& ex) {
            Log::Errorf("TorqueTileDecoder::decodeTile: Exception while decoding: %s", ex.what());
        }
        return std::shared_ptr<TileMap>();
    }

    void TorqueTileDecoder::updateCurrentStyleSet(const std::shared_ptr<CartoCSSStyleSet>& styleSet) {
        std::shared_ptr<mvt::TorqueMap> map;
        try {
            auto assetLoader = std::make_shared<CartoCSSAssetLoader>("", std::shared_ptr<AssetPackage>());
            css::TorqueCartoCSSMapLoader mapLoader(assetLoader, _logger);
            mapLoader.setIgnoreLayerPredicates(true);
            map = mapLoader.loadMap(styleSet->getCartoCSS());
        }
        catch (const std::exception& ex) {
            throw ParseException(std::string("Style parsing failed: ") + ex.what(), styleSet->getCartoCSS());
        }

        auto fontManager = std::make_shared<vt::FontManager>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);
        auto bitmapLoader = std::make_shared<VTBitmapLoader>("", std::shared_ptr<AssetPackage>());
        auto bitmapManager = std::make_shared<vt::BitmapManager>(bitmapLoader);
        auto strokeMap = std::make_shared<vt::StrokeMap>(1, 1);
        auto glyphMap = std::make_shared<vt::GlyphMap>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);

        std::shared_ptr<vt::Font> fallbackFont;
        for (auto it = _fallbackFonts.rbegin(); it != _fallbackFonts.rend(); it++) {
            std::shared_ptr<BinaryData> fontData = *it;
            std::string fontName = fontManager->loadFontData(*fontData->getDataPtr());
            fallbackFont = fontManager->getFont(fontName, fallbackFont);
        }
        mvt::SymbolizerContext::Settings settings(DEFAULT_TILE_SIZE, std::map<std::string, mvt::Value>(), fallbackFont);
        auto symbolizerContext = std::make_shared<mvt::SymbolizerContext>(bitmapManager, fontManager, strokeMap, glyphMap, settings);

        _map = map;
        _mapSettings = std::make_shared<mvt::Map::Settings>(map->getSettings());
        _mapSettings->backgroundColor = _map->getTorqueSettings().clearColor;
        _symbolizerContext = symbolizerContext;
        _styleSet = styleSet;
    }
    
    const int TorqueTileDecoder::DEFAULT_TILE_SIZE = 256;
    const int TorqueTileDecoder::GLYPHMAP_SIZE = 2048;
}
