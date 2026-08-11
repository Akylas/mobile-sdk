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
#include "vectortiles/utils/MVTGeometryConverter.h"
#include "vectortiles/utils/MVTValueConverter.h"
#include "vectortiles/utils/MVTLogger.h"
#include "vectortiles/utils/VTBitmapLoader.h"
#include "vectortiles/utils/CartoCSSAssetLoader.h"
#include "utils/AssetPackage.h"
#include "utils/FileUtils.h"
#include "utils/SystemFontUtils.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <vt/Tile.h>
#include <mapnikvt/Value.h>
#include <mapnikvt/SymbolizerParser.h>
#include <mapnikvt/SymbolizerContext.h>
#include <mapnikvt/MBVTFeatureDecoder.h>
#include <mapnikvt/MBVTTileReader.h>
#include <mapnikvt/MapParser.h>
#include <mapnikvt/NutiParameterResolver.h>
#include <cartocss/CartoCSSMapLoader.h>

#include <functional>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace carto {

    namespace {
        // A style parameter may hold an object or an array (a table the style reads with get()),
        // and the public API passes parameter values as strings - so those are carried as JSON.
        mvt::Value convertJSONValue(const picojson::value& value) {
            if (value.is<std::string>()) {
                return mvt::Value(value.get<std::string>());
            }
            if (value.is<bool>()) {
                return mvt::Value(value.get<bool>());
            }
            if (value.is<std::int64_t>()) {
                return mvt::Value(static_cast<long long>(value.get<std::int64_t>()));
            }
            if (value.is<double>()) {
                return mvt::Value(value.get<double>());
            }
            if (value.is<picojson::object>()) {
                std::map<std::string, mvt::Value> members;
                const picojson::object& obj = value.get<picojson::object>();
                for (auto it = obj.begin(); it != obj.end(); it++) {
                    members[it->first] = convertJSONValue(it->second);
                }
                return mvt::Value(std::make_shared<const mvt::ValueObject>(std::move(members)));
            }
            if (value.is<picojson::array>()) {
                std::vector<mvt::Value> elements;
                const picojson::array& arr = value.get<picojson::array>();
                for (auto it = arr.begin(); it != arr.end(); it++) {
                    elements.push_back(convertJSONValue(*it));
                }
                return mvt::Value(std::make_shared<const mvt::ValueArray>(std::move(elements)));
            }
            return mvt::Value();
        }

        picojson::value convertValueToJSON(const mvt::Value& value) {
            if (auto val = std::get_if<bool>(&value)) {
                return picojson::value(*val);
            }
            if (auto val = std::get_if<long long>(&value)) {
                return picojson::value(static_cast<std::int64_t>(*val));
            }
            if (auto val = std::get_if<double>(&value)) {
                return picojson::value(*val);
            }
            if (auto val = std::get_if<std::string>(&value)) {
                return picojson::value(*val);
            }
            if (auto val = std::get_if<std::shared_ptr<const mvt::ValueObject>>(&value)) {
                picojson::object obj;
                if (*val) {
                    for (auto it = (*val)->members.begin(); it != (*val)->members.end(); it++) {
                        obj[it->first] = convertValueToJSON(it->second);
                    }
                }
                return picojson::value(obj);
            }
            if (auto val = std::get_if<std::shared_ptr<const mvt::ValueArray>>(&value)) {
                picojson::array arr;
                if (*val) {
                    for (auto it = (*val)->elements.begin(); it != (*val)->elements.end(); it++) {
                        arr.push_back(convertValueToJSON(*it));
                    }
                }
                return picojson::value(arr);
            }
            return picojson::value();
        }

        bool isContainerValue(const mvt::Value& value) {
            return std::get_if<std::shared_ptr<const mvt::ValueObject>>(&value) || std::get_if<std::shared_ptr<const mvt::ValueArray>>(&value);
        }

        // Compiled maps, shared between decoders. Parsing and compiling a style is 0.5-0.7 s for a
        // 23-layer project, and an app that switches between two styles of one asset package - day
        // and night - or builds several layers from the same style, pays it every time otherwise.
        // A compiled map is read-only, and the values a decoder sets live in its own parameter
        // store, so sharing one is safe.
        struct MapCacheKey {
            const AssetPackage* assetPackage = nullptr;
            std::string styleAssetName;
            std::string cartoCSS;
            bool ignoreLayerPredicates = false;

            bool operator == (const MapCacheKey& other) const {
                return assetPackage == other.assetPackage && styleAssetName == other.styleAssetName && cartoCSS == other.cartoCSS && ignoreLayerPredicates == other.ignoreLayerPredicates;
            }
        };

        std::mutex g_mapCacheMutex;
        std::vector<std::pair<MapCacheKey, std::weak_ptr<const mvt::Map>>> g_mapCache;
        std::vector<std::shared_ptr<const mvt::Map>> g_mapCacheRetained; // keeps the last few alive

        std::shared_ptr<const mvt::Map> findCachedMap(const MapCacheKey& key) {
            std::lock_guard<std::mutex> lock(g_mapCacheMutex);
            for (auto it = g_mapCache.begin(); it != g_mapCache.end(); ) {
                std::shared_ptr<const mvt::Map> map = it->second.lock();
                if (!map) {
                    it = g_mapCache.erase(it);
                    continue;
                }
                if (it->first == key) {
                    return map;
                }
                it++;
            }
            return std::shared_ptr<const mvt::Map>();
        }

        void storeCachedMap(const MapCacheKey& key, const std::shared_ptr<const mvt::Map>& map) {
            std::lock_guard<std::mutex> lock(g_mapCacheMutex);
            g_mapCache.emplace_back(key, map);
            g_mapCacheRetained.push_back(map);
            if (g_mapCacheRetained.size() > 2) {
                g_mapCacheRetained.erase(g_mapCacheRetained.begin());
            }
        }
    }

    MBVectorTileDecoder::MBVectorTileDecoder(const std::shared_ptr<CompiledStyleSet>& compiledStyleSet) :
        _logger(std::make_shared<MVTLogger>("MBVectorTileDecoder")),
        _pixelScale(1.0f),
        _featureIdOverride(false),
        _cartoCSSLayerNamesIgnored(false),
        _layerNameOverride(),
        _parameterValueMap(),
        _fallbackFonts(),
        _styleSet(),
        _map(),
        _mapSettings(),
        _symbolizerContext(),
        _symbolizerContextSettings(),
        _assetPackageSymbolizerContexts()
    {
        if (!compiledStyleSet) {
            throw NullArgumentException("Null compiledStyleSet");
        }

        updateCurrentStyleSet(compiledStyleSet);
    }
    
    MBVectorTileDecoder::MBVectorTileDecoder(const std::shared_ptr<CartoCSSStyleSet>& cartoCSSStyleSet) :
        _logger(std::make_shared<MVTLogger>("MBVectorTileDecoder")),
        _pixelScale(1.0f),
        _featureIdOverride(false),
        _cartoCSSLayerNamesIgnored(false),
        _layerNameOverride(),
        _parameterValueMap(),
        _fallbackFonts(),
        _styleSet(),
        _map(),
        _symbolizerContext(),
        _assetPackageSymbolizerContexts()
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
        if (auto compiledStyleSet = std::get_if<std::shared_ptr<CompiledStyleSet> >(&_styleSet)) {
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

        if (auto cartoCSSStyleSet = std::get_if<std::shared_ptr<CartoCSSStyleSet> >(&_styleSet)) {
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

        if (isContainerValue(value)) {
            return convertValueToJSON(value).serialize(); // a table parameter reads back as JSON
        }

        if (!nutiParam.getEnumMap().empty()) {
            for (auto it2 = nutiParam.getEnumMap().begin(); it2 != nutiParam.getEnumMap().end(); it2++) {
                if (it2->second == value) {
                    return it2->first;
                }
            }
        } else {
            if (auto val = std::get_if<bool>(&value)) {
                return boost::lexical_cast<std::string>(*val);
            } else if (auto val = std::get_if<long long>(&value)) {
                return boost::lexical_cast<std::string>(*val);
            } else if (auto val = std::get_if<double>(&value)) {
                return boost::lexical_cast<std::string>(*val);
            } else if (auto val = std::get_if<std::string>(&value)) {
                return *val;
            }
        }
        return std::string();
    }

    std::vector<std::string> MBVectorTileDecoder::getStyleLayerNames() const {
        std::lock_guard<std::mutex> lock(_mutex);

        std::vector<std::string> names;
        if (_map) {
            for (const std::shared_ptr<mvt::Layer>& layer : _map->getLayers()) {
                names.push_back(layer->getName());
            }
        }
        return names;
    }

    mvt::ResolvedLayerConfig MBVectorTileDecoder::resolveLayerConfig(const std::string& layerName, float viewZoom) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_map) {
            return mvt::ResolvedLayerConfig();
        }
        return mvt::resolveLayerConfig(*_map, layerName, viewZoom, _parameterStore);
    }

    std::vector<int> MBVectorTileDecoder::getStyleLayerZoomRange(const std::string& layerName) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_map) {
            return { 0, 24 };
        }
        std::pair<int, int> range = mvt::resolveLayerZoomRange(*_map, layerName);
        return { range.first, range.second };
    }

    void MBVectorTileDecoder::updateParameterStore() {
        // The style defaults, overlaid with whatever the app has set. The store is never replaced,
        // only its values are - the decoded tiles read through it.
        std::map<std::string, mvt::Value> parameterValues;
        for (auto it = _map->getNutiParameterMap().begin(); it != _map->getNutiParameterMap().end(); it++) {
            parameterValues[it->first] = it->second.getDefaultValue();
        }
        for (auto it = _parameterValueMap.begin(); it != _parameterValueMap.end(); it++) {
            parameterValues[it->first] = it->second;
        }
        _parameterStore->setValues(std::move(parameterValues));
    }

    void MBVectorTileDecoder::updateSymbolizer() {
        updateParameterStore();

        // Settings snapshot the parameters that scale geometry and glyphs, so they are rebuilt
        // whenever a parameter changes structurally.
        _symbolizerContextSettings = std::make_shared<mvt::SymbolizerContext::Settings>(_symbolizerContextSettings->getTileSize(), _parameterStore, _symbolizerContextSettings->getFallbackFont(), _pixelScale);
        _symbolizerContext = std::make_shared<mvt::SymbolizerContext>(_symbolizerContext->getBitmapManager(), _symbolizerContext->getFontManager(), _symbolizerContext->getStrokeMap(), _symbolizerContext->getGlyphMap(), *_symbolizerContextSettings);
    }

    bool MBVectorTileDecoder::areParametersLive(const std::vector<std::string>& params) const {
        if (params.empty() || _liveParameters.empty()) {
            return false;
        }
        for (const std::string& param : params) {
            if (_liveParameters.find(param) == _liveParameters.end()) {
                return false;
            }
        }
        return true;
    }

    bool MBVectorTileDecoder::setStyleParameterInternal(const std::string& param, const std::string& value) {
        auto it = _map->getNutiParameterMap().find(param);
        if (it == _map->getNutiParameterMap().end()) {
            Log::Errorf("MBVectorTileDecoder::setStyleParameter: Could not find parameter: %s", param.c_str());
            return false;
        }
        const mvt::NutiParameter& nutiParam = it->second;

        if (!nutiParam.getEnumMap().empty()) {
            auto it2 = nutiParam.getEnumMap().find(value);
            if (it2 == nutiParam.getEnumMap().end()) {
                Log::Errorf("MBVectorTileDecoder::setStyleParameter: Illegal enum value for parameter: %s/%s", param.c_str(), value.c_str());
                return false;
            }
            _parameterValueMap[param] = it2->second;
        } else if (isContainerValue(nutiParam.getDefaultValue())) {
            // An object/array parameter is set as JSON, and its shape must match what the style
            // declared - a style reading get(table, key) must not be handed a scalar.
            picojson::value jsonValue;
            std::string err = picojson::parse(jsonValue, value);
            if (!err.empty()) {
                Log::Errorf("MBVectorTileDecoder::setStyleParameter: Could not parse JSON for parameter %s: %s", param.c_str(), err.c_str());
                return false;
            }
            mvt::Value val = convertJSONValue(jsonValue);
            if (val.index() != nutiParam.getDefaultValue().index()) {
                Log::Errorf("MBVectorTileDecoder::setStyleParameter: Value of parameter %s does not match the declared object/array type", param.c_str());
                return false;
            }
            _parameterValueMap[param] = val;
        } else {
            try {
                mvt::Value val = nutiParam.getDefaultValue();
                if (std::get_if<bool>(&val)) {
                    if (value == "true") {
                        val = mvt::Value(true);
                    } else if (value == "false") {
                        val = mvt::Value(false);
                    } else {
                        val = mvt::Value(boost::lexical_cast<bool>(value));
                    }
                } else if (std::get_if<long long>(&val)) {
                    val = mvt::Value(boost::lexical_cast<long long>(value));
                } else if (std::get_if<double>(&val)) {
                    val = mvt::Value(boost::lexical_cast<double>(value));
                } else if (std::get_if<std::string>(&val)) {
                    val = value;
                }
                _parameterValueMap[param] = val;
            }
            catch (const std::exception& ex) {
                Log::Errorf("MBVectorTileDecoder::setStyleParameter: Exception while converting parameter %s/%s: %s", param.c_str(), value.c_str(), ex.what());
                return false;
            }
        }
        return true;
    }

    bool MBVectorTileDecoder::setStyleParameter(const std::string& param, const std::string& value) {
        bool live = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            setStyleParameterInternal(param, value);

            live = areParametersLive({ param });
            if (live) {
                updateParameterStore();
            } else {
                updateSymbolizer();
            }
        }
        // A parameter that nothing but a per-frame colour or width reads is already visible to the
        // decoded tiles through the store: they only have to be drawn again.
        if (live) {
            notifyDecoderRefreshed();
        } else {
            notifyDecoderChanged();
        }
        return true;
    }
    void MBVectorTileDecoder::setJSONStyleParameters(const std::string& params) {
        try
        {
            picojson::value val;
            std::string err = picojson::parse(val, params);
            if (!err.empty())
            {
                throw ParseException(std::string("JSON parsing failed: ") + err, params);
            }
            const picojson::object &jsonValues = val.get<picojson::object>();
            bool live = false;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                std::vector<std::string> params;
                for (auto it = jsonValues.begin(); it != jsonValues.end(); it++) {
                    setStyleParameterInternal(it->first, it->second.get<std::string>());
                    params.push_back(it->first);
                }
                live = areParametersLive(params);
                if (live) {
                    updateParameterStore();
                } else {
                    updateSymbolizer();
                }
            }
            if (live) {
                notifyDecoderRefreshed();
            } else {
                notifyDecoderChanged();
            }
        }
        catch (const std::exception &ex)
        {
            Log::Errorf("MBVectorTileDecoder::setJSONStyleParameters: Failed to set parameters: %s", ex.what());
            throw GenericException("Failed to set style parameters", ex.what());
        }
    }
    void MBVectorTileDecoder::setStyleParameters(const std::map<std::string, std::string>& params) {
        bool live = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            std::vector<std::string> paramNames;
            for (auto p = params.begin(); p != params.end(); ++p)  {
                setStyleParameterInternal(p->first, p->second);
                paramNames.push_back(p->first);
            }
            live = areParametersLive(paramNames);
            if (live) {
                updateParameterStore();
            } else {
                updateSymbolizer();
            }
        }
        if (live) {
            notifyDecoderRefreshed();
        } else {
            notifyDecoderChanged();
        }
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
            _assetPackageSymbolizerContexts.clear();
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

    std::shared_ptr<const mvt::Map::Settings> MBVectorTileDecoder::getMapSettings() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _mapSettings;
    }

    std::shared_ptr<const mvt::SymbolizerContext::Settings> MBVectorTileDecoder::getSymbolizerContextSettings() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _symbolizerContextSettings;
    }

    void MBVectorTileDecoder::addFallbackFont(const std::shared_ptr<BinaryData>& fontData) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (fontData) {
                _fallbackFonts.push_back(fontData);
                // Fonts live in the symbolizer context, not in the compiled map: rebuild only that.
                _assetPackageSymbolizerContexts.clear();
                updateSymbolizerContext();
            }
        }
        notifyDecoderChanged();
    }

    void MBVectorTileDecoder::setPixelScale(float pixelScale) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!(pixelScale > 0.0f) || _pixelScale == pixelScale) {
                return;
            }
            _pixelScale = pixelScale;
            // The glyph render size is picked from it when a tile is decoded, so the glyph/stroke
            // maps have to go. The compiled map does not depend on it - reloading the style here
            // cost a full parse + compile (~0.5 s for a 23-layer style) on every layer that joins
            // a map, which is where this fires.
            _assetPackageSymbolizerContexts.clear();
            updateSymbolizerContext();
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
            std::shared_ptr<Geometry> geometry = std::visit(MVTGeometryConverter(tileBounds), *mvtGeometry);

            Variant propertiesVariant;
            std::map<std::string, Variant> featureData;
            if (std::shared_ptr<const mvt::FeatureData> mvtFeatureData = mvtFeature.getFeatureData()) {
                mvt::Value value;
                if (mvtFeatureData->getVariable("$$properties$$", value)) {
                    propertiesVariant = Variant::FromString(std::get<std::string>( value));
                } else {
                    for (const std::pair<std::string, mvt::Value>& var : mvtFeatureData->getVariables()) {
                        featureData[var.first] = std::visit(MVTValueConverter(), var.second);
                    }
                    propertiesVariant = Variant(featureData);
                }

            }

            auto feature = std::make_shared<VectorTileFeature>(mvtFeature.getId(), MapTile(tile.x, tile.y, tile.zoom, 0), mvtLayerName, geometry, propertiesVariant);
            return feature;
        }
        catch (const std::exception& ex) {
            Log::Errorf("MBVectorTileDecoder::decodeFeature: Exception while decoding: %s", ex.what());
        }
        return std::shared_ptr<VectorTileFeature>();
    }

    std::shared_ptr<VectorTileFeatureCollection> MBVectorTileDecoder::decodeFeatures(const vt::TileId& tile, const std::shared_ptr<BinaryData>& tileData, const MapBounds& tileBounds, const std::vector<std::string>& onlyLayers) const {
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
            std::vector<std::string> layers = decoder->getLayerNames();
            if (onlyLayers.size() > 0) {
                std::vector<std::string> result;
                std::copy_if(onlyLayers.begin(), onlyLayers.end(), std::back_inserter(result), [&layers](std::string str) {
                    return std::find(layers.begin(), layers.end(), str) != layers.end();
                });
                layers = result;
            }
            for (const std::string& mvtLayerName : layers) {
                for (std::shared_ptr<mvt::FeatureDecoder::FeatureIterator> mvtIt = decoder->createLayerFeatureIterator(mvtLayerName, nullptr); mvtIt->valid(); mvtIt->advance()) {
                    std::shared_ptr<const mvt::Geometry> mvtGeometry = mvtIt->getGeometry();
                    if (!mvtGeometry) {
                        continue;
                    }
                    std::shared_ptr<Geometry> geometry = std::visit(MVTGeometryConverter(tileBounds), *mvtGeometry);

                    std::map<std::string, Variant> featureData;
                    if (std::shared_ptr<const mvt::FeatureData> mvtFeatureData = mvtIt->getFeatureData(false, nullptr)) {
                        for (const std::pair<std::string, mvt::Value>& var : mvtFeatureData->getVariables()) {
                            featureData[var.first] = std::visit(MVTValueConverter(), var.second);
                        }
                    }

                    auto feature = std::make_shared<VectorTileFeature>(mvtIt->getFeatureId(), MapTile(tile.x, tile.y, tile.zoom, 0), mvtLayerName, geometry, Variant(featureData));
                    tileFeatures.push_back(feature);
                }
            }
        }
        catch (const std::exception& ex) {
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

        std::shared_ptr<const mvt::Map> map;
        std::shared_ptr<const mvt::SymbolizerContext> symbolizerContext;
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
            decoder.setFeatureIdOverride(featureIdOverride, MapTile(tile.x, tile.y, tile.zoom, 0).getTileId());
            
            mvt::MBVTTileReader reader(map, tileTransformer, *symbolizerContext, decoder, _logger);
            reader.setLayerNameOverride(layerNameOverride);

            if (std::shared_ptr<vt::Tile> tile = reader.readTile(targetTile)) {
                auto tileMap = std::make_shared<TileMap>();
                (*tileMap)[0] = tile;
                return tileMap;
            }
        }
        catch (const std::exception& ex) {
            Log::Errorf("MBVectorTileDecoder::decodeTile: Exception while decoding: %s", ex.what());
        }
        return std::shared_ptr<TileMap>();
    }

    void MBVectorTileDecoder::updateCurrentStyleSet(const std::variant<std::shared_ptr<CompiledStyleSet>, std::shared_ptr<CartoCSSStyleSet> >& styleSet) {
        std::string styleAssetName;
        std::string cartoCSS;
        std::shared_ptr<AssetPackage> assetPackage;
        std::shared_ptr<const mvt::Map> map;

        // What identifies the compiled map: the asset package it came from, which style of it, and
        // whether layer predicates were ignored (that one changes how the style compiles).
        if (auto cartoCSSStyleSet = std::get_if<std::shared_ptr<CartoCSSStyleSet> >(&styleSet)) {
            assetPackage = (*cartoCSSStyleSet)->getAssetPackage();
            cartoCSS = (*cartoCSSStyleSet)->getCartoCSS();
        } else if (auto compiledStyleSet = std::get_if<std::shared_ptr<CompiledStyleSet> >(&styleSet)) {
            styleAssetName = (*compiledStyleSet)->getStyleAssetName();
            assetPackage = (*compiledStyleSet)->getAssetPackage();
        }
        MapCacheKey mapCacheKey { assetPackage.get(), styleAssetName, cartoCSS, _cartoCSSLayerNamesIgnored };
        map = findCachedMap(mapCacheKey);
        bool mapWasCached = static_cast<bool>(map);

        if (map) {
            // Already compiled - by this decoder before, or by another layer using the same style.
        } else if (auto cartoCSSStyleSet = std::get_if<std::shared_ptr<CartoCSSStyleSet> >(&styleSet)) {
            try {
                auto assetLoader = std::make_shared<CartoCSSAssetLoader>("", (*cartoCSSStyleSet)->getAssetPackage());
                css::CartoCSSMapLoader mapLoader(assetLoader, _logger);
                mapLoader.setIgnoreLayerPredicates(_cartoCSSLayerNamesIgnored);
                map = mapLoader.loadMap((*cartoCSSStyleSet)->getCartoCSS());
            }
            catch (const std::exception& ex) {
                throw ParseException(std::string("CartoCSS style parsing failed: ") + ex.what(), (*cartoCSSStyleSet)->getCartoCSS());
            }
        } else if (auto compiledStyleSet = std::get_if<std::shared_ptr<CompiledStyleSet> >(&styleSet)) {
            if (styleAssetName.empty()) {
                throw InvalidArgumentException("Could not find any styles in the style set");
            }

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
                }
                catch (const std::exception& ex) {
                    throw ParseException(std::string("XML style processing failed: ") + ex.what());
                }
            } else if (boost::algorithm::ends_with(styleAssetName, ".json")) {
                try {
                    auto assetLoader = std::make_shared<CartoCSSAssetLoader>(FileUtils::GetFilePath(styleAssetName), assetPackage);
                    css::CartoCSSMapLoader mapLoader(assetLoader, _logger);
                    mapLoader.setIgnoreLayerPredicates(_cartoCSSLayerNamesIgnored);
                    map = mapLoader.loadMapProject(styleAssetName);
                }
                catch (const std::exception& ex) {
                    throw GenericException(std::string("CartoCSS style loading failed: ") + ex.what());
                }
            } else {
                throw GenericException("Failed to detect style asset type");
            }
        } else {
            throw InvalidArgumentException("Invalid style set");
        }

        if (!mapWasCached) {
            storeCachedMap(mapCacheKey, map);
        }

        _styleSet = styleSet;
        _styleAssetName = styleAssetName;
        _styleAssetPackage = assetPackage;
        _map = map;
        _mapSettings = std::make_shared<mvt::Map::Settings>(_map->getSettings());

        updateSymbolizerContext();
    }

    void MBVectorTileDecoder::updateSymbolizerContext() {
        const std::string& styleAssetName = _styleAssetName;
        const std::shared_ptr<AssetPackage>& assetPackage = _styleAssetPackage;
        const std::shared_ptr<const mvt::Map>& map = _map;

        if (_assetPackageSymbolizerContexts.find(std::make_pair(styleAssetName, assetPackage)) == _assetPackageSymbolizerContexts.end() && _assetPackageSymbolizerContexts.size() >= MAX_ASSETPACKAGE_SYMBOLIZER_CONTEXTS) {
            _assetPackageSymbolizerContexts.clear();
        }
        std::shared_ptr<const mvt::SymbolizerContext>& symbolizerContext = _assetPackageSymbolizerContexts[std::make_pair(styleAssetName, assetPackage)];
        if (!symbolizerContext) {
            auto fontManager = std::make_shared<vt::FontManager>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);
            auto bitmapLoader = std::make_shared<VTBitmapLoader>(FileUtils::GetFilePath(styleAssetName), assetPackage);
            auto bitmapManager = std::make_shared<vt::BitmapManager>(bitmapLoader);
            auto strokeMap = std::make_shared<vt::StrokeMap>(STROKEMAP_SIZE, STROKEMAP_SIZE);
            auto glyphMap = std::make_shared<vt::GlyphMap>(GLYPHMAP_SIZE, GLYPHMAP_SIZE);

            // Register the fonts of the style first, they take precedence over the system fonts
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

            // Fonts the style asks for but does not provide are loaded from the system
            fontManager->setFontDataLoader([](const std::string& name) {
                if (std::shared_ptr<BinaryData> fontData = SystemFontUtils::LoadFont(name)) {
                    return *fontData->getDataPtr();
                }
                return std::vector<unsigned char>();
            });

            std::shared_ptr<const vt::Font> fallbackFont;
            for (auto it = _fallbackFonts.rbegin(); it != _fallbackFonts.rend(); it++) {
                std::shared_ptr<BinaryData> fontData = *it;
                std::string fontName = fontManager->loadFontData(*fontData->getDataPtr());
                fallbackFont = fontManager->getFont(fontName, fallbackFont);
            }
            if (!fallbackFont) {
                // Styles without any font (inline CartoCSS, for example) still need a font for their labels
                fallbackFont = fontManager->getFont(DEFAULT_FALLBACK_FONT_NAME, fallbackFont);
            }
            mvt::SymbolizerContext::Settings settings(DEFAULT_TILE_SIZE, std::make_shared<mvt::NutiParameterStore>(), fallbackFont, _pixelScale);
            symbolizerContext = std::make_shared<mvt::SymbolizerContext>(bitmapManager, fontManager, strokeMap, glyphMap, settings);
        }

        for (auto it = _parameterValueMap.begin(); it != _parameterValueMap.end(); ) {
            auto it2 = map->getNutiParameterMap().find(it->first);
            if (it2 == map->getNutiParameterMap().end()) {
                it = _parameterValueMap.erase(it);
                continue;
            }
            const mvt::NutiParameter& nutiParam = it2->second;

            bool valid = nutiParam.getDefaultValue().index() == it->second.index();
            if (!nutiParam.getEnumMap().empty()) {
                valid = false;
                for (std::pair<std::string, mvt::Value> enumValue : nutiParam.getEnumMap()) {
                    if (enumValue.second == it->second) {
                        valid = true;
                        break;
                    }
                }
            }
            if (!valid) {
                it = _parameterValueMap.erase(it);
                continue;
            }

            it++;
        }

        if (!_parameterStore) {
            _parameterStore = std::make_shared<mvt::NutiParameterStore>();
        }
        updateParameterStore();
        _liveParameters = mvt::resolveLiveNutiParameters(*_map);

        _symbolizerContextSettings = std::make_shared<mvt::SymbolizerContext::Settings>(symbolizerContext->getSettings().getTileSize(), _parameterStore, symbolizerContext->getSettings().getFallbackFont(), _pixelScale);
        _symbolizerContext = std::make_shared<mvt::SymbolizerContext>(symbolizerContext->getBitmapManager(), symbolizerContext->getFontManager(), symbolizerContext->getStrokeMap(), symbolizerContext->getGlyphMap(), *_symbolizerContextSettings);
        _cachedFeatureDecoder.first.reset();
        _cachedFeatureDecoder.second.reset();
    }

    const std::string MBVectorTileDecoder::DEFAULT_FALLBACK_FONT_NAME = "Arial";
    const int MBVectorTileDecoder::DEFAULT_TILE_SIZE = 256;
    const int MBVectorTileDecoder::STROKEMAP_SIZE = 512;
    const int MBVectorTileDecoder::GLYPHMAP_SIZE = 2048;
    const std::size_t MBVectorTileDecoder::MAX_ASSETPACKAGE_SYMBOLIZER_CONTEXTS = 2;

}
