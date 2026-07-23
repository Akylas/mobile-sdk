#include "CompositeVectorTileLayer.h"
#include "datasources/DynamicMergedMBVTTileDataSource.h"
#include "datasources/ContourTileDataSource.h"
#include "layers/RasterTileLayer.h"
#include "layers/HillshadeRasterTileLayer.h"
#include "vectortiles/MBVectorTileDecoder.h"
#include "rastertiles/ElevationDecoder.h"
#include "rastertiles/TerrariumElevationDataDecoder.h"
#include "rastertiles/MapBoxElevationDataDecoder.h"
#include "renderers/TileRenderer.h"
#include "renderers/MapRenderer.h"
#include "graphics/ViewState.h"
#include "graphics/Color.h"
#include "components/Exceptions.h"
#include "utils/Log.h"

#include <algorithm>
#include <variant>

#include <mapnikvt/Value.h>
#include <mapnikvt/LayerConfigResolver.h>

namespace carto {

    namespace {
        float valueToFloat(const mvt::Value& value, float defaultValue) {
            if (auto v = std::get_if<double>(&value))     { return static_cast<float>(*v); }
            if (auto v = std::get_if<long long>(&value))  { return static_cast<float>(*v); }
            if (auto v = std::get_if<bool>(&value))       { return *v ? 1.0f : 0.0f; }
            return defaultValue;
        }

        Color parseColorValue(const mvt::Value& value, const Color& defaultValue) {
            const std::string* str = std::get_if<std::string>(&value);
            if (!str || str->empty() || (*str)[0] != '#') {
                return defaultValue;
            }
            std::string hex = str->substr(1);
            unsigned long packed = 0;
            try {
                packed = std::stoul(hex, nullptr, 16);
            } catch (const std::exception&) {
                return defaultValue;
            }
            if (hex.size() == 6) {
                return Color(static_cast<unsigned char>((packed >> 16) & 0xff),
                             static_cast<unsigned char>((packed >> 8) & 0xff),
                             static_cast<unsigned char>(packed & 0xff), 255);
            }
            if (hex.size() == 8) {
                return Color(static_cast<unsigned char>((packed >> 24) & 0xff),
                             static_cast<unsigned char>((packed >> 16) & 0xff),
                             static_cast<unsigned char>((packed >> 8) & 0xff),
                             static_cast<unsigned char>(packed & 0xff));
            }
            return defaultValue;
        }

        RasterTileFilterMode::RasterTileFilterMode parseFilterMode(const std::string& mode) {
            if (mode == "nearest") { return RasterTileFilterMode::RASTER_TILE_FILTER_MODE_NEAREST; }
            if (mode == "bicubic") { return RasterTileFilterMode::RASTER_TILE_FILTER_MODE_BICUBIC; }
            return RasterTileFilterMode::RASTER_TILE_FILTER_MODE_BILINEAR;
        }

        HillshadeMethod::HillshadeMethod parseHillshadeMethod(const std::string& method) {
            if (method == "combined")         { return HillshadeMethod::HillshadeMethod::COMBINED; }
            if (method == "igor")             { return HillshadeMethod::HillshadeMethod::IGOR; }
            if (method == "multidirectional") { return HillshadeMethod::HillshadeMethod::MULTIDIRECTIONAL; }
            if (method == "basic")            { return HillshadeMethod::HillshadeMethod::BASIC; }
            return HillshadeMethod::HillshadeMethod::STANDARD;
        }
    }

    CompositeVectorTileLayer::CompositeVectorTileLayer(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<VectorTileDecoder>& decoder) :
        VectorTileLayer(std::make_shared<DynamicMergedMBVTTileDataSource>(dataSource), decoder),
        _mergedDataSource(),
        _externalSources(),
        _renderSteps(),
        _singlePassRenderingEnabled(false),
        _componentsSet(false),
        _childOptions(),
        _childMapRenderer(),
        _childTouchHandler(),
        _sourceMutex()
    {
        _mergedDataSource = std::static_pointer_cast<DynamicMergedMBVTTileDataSource>(getDataSource());
        rebuildRenderSteps();
    }

    CompositeVectorTileLayer::~CompositeVectorTileLayer() {
    }

    void CompositeVectorTileLayer::addExternalDataSource(const std::string& name, const std::shared_ptr<TileDataSource>& dataSource, CompositeSourceType::CompositeSourceType type, const std::shared_ptr<ElevationDecoder>& elevationDecoder) {
        if (!dataSource) {
            throw NullArgumentException("Null dataSource");
        }
        if (type == CompositeSourceType::COMPOSITE_SOURCE_TYPE_VECTOR) {
            addVectorDataSource(name, dataSource);
            return;
        }

        std::shared_ptr<Layer> childLayer;
        if (type == CompositeSourceType::COMPOSITE_SOURCE_TYPE_RASTER) {
            childLayer = std::make_shared<RasterTileLayer>(dataSource);
        } else { // HILLSHADE
            std::shared_ptr<ElevationDecoder> decoder = elevationDecoder;
            if (!decoder) {
                decoder = resolveElevationDecoder(dataSource);
            }
            childLayer = decoder ? std::make_shared<HillshadeRasterTileLayer>(dataSource, decoder)
                                 : std::make_shared<HillshadeRasterTileLayer>(dataSource);
        }

        {
            std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
            removeExternalDataSource(name); // replace if it exists
            _externalSources.push_back({ name, type, dataSource, childLayer });
            if (_componentsSet) {
                wireChild(childLayer);
            }
            rebuildRenderSteps();
        }
        refresh();
    }

    void CompositeVectorTileLayer::addVectorDataSource(const std::string& name, const std::shared_ptr<TileDataSource>& dataSource) {
        if (!dataSource) {
            throw NullArgumentException("Null dataSource");
        }
        {
            std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
            removeExternalDataSource(name);
            _externalSources.push_back({ name, CompositeSourceType::COMPOSITE_SOURCE_TYPE_VECTOR, dataSource, std::shared_ptr<Layer>() });
            _mergedDataSource->addDataSource(name, dataSource); // notifies -> master tiles reload
            rebuildRenderSteps();
        }
        applyVectorSourceConfigs();
        refresh();
    }

    bool CompositeVectorTileLayer::removeExternalDataSource(const std::string& name) {
        bool removed = false;
        {
            std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
            auto it = std::find_if(_externalSources.begin(), _externalSources.end(), [&](const ExternalSource& s) { return s.name == name; });
            if (it != _externalSources.end()) {
                if (it->type == CompositeSourceType::COMPOSITE_SOURCE_TYPE_VECTOR) {
                    _mergedDataSource->removeDataSource(name);
                } else if (it->childLayer) {
                    if (_componentsSet) {
                        unwireChild(it->childLayer);
                    }
                }
                _externalSources.erase(it);
                _lastVectorConfig.erase(name);
                rebuildRenderSteps();
                removed = true;
            }
        }
        if (removed) {
            refresh();
        }
        return removed;
    }

    std::vector<std::string> CompositeVectorTileLayer::getExternalDataSourceNames() const {
        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        std::vector<std::string> names;
        for (const ExternalSource& s : _externalSources) {
            names.push_back(s.name);
        }
        return names;
    }

    bool CompositeVectorTileLayer::isSinglePassRenderingEnabled() const {
        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        return _singlePassRenderingEnabled;
    }

    void CompositeVectorTileLayer::setSinglePassRenderingEnabled(bool enabled) {
        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        _singlePassRenderingEnabled = enabled;
    }

    std::shared_ptr<ElevationDecoder> CompositeVectorTileLayer::resolveElevationDecoder(const std::shared_ptr<TileDataSource>& dataSource) {
        std::string encoding = dataSource ? dataSource->getEncoding() : std::string();
        if (encoding.empty()) {
            return std::shared_ptr<ElevationDecoder>(); // let HillshadeRasterTileLayer infer per tile
        }
        if (encoding == "mapbox") {
            return std::make_shared<MapBoxElevationDataDecoder>();
        }
        return std::make_shared<TerrariumElevationDataDecoder>();
    }

    void CompositeVectorTileLayer::wireChild(const std::shared_ptr<Layer>& child) {
        if (!child) {
            return;
        }
        child->setComponents(_envelopeThreadPool, _tileThreadPool, _childOptions, _childMapRenderer, _childTouchHandler);
        child->registerDataSourceListener();
    }

    void CompositeVectorTileLayer::unwireChild(const std::shared_ptr<Layer>& child) {
        if (!child) {
            return;
        }
        child->unregisterDataSourceListener();
    }

    void CompositeVectorTileLayer::setComponents(const std::shared_ptr<CancelableThreadPool>& envelopeThreadPool,
                                                 const std::shared_ptr<CancelableThreadPool>& tileThreadPool,
                                                 const std::weak_ptr<Options>& options,
                                                 const std::weak_ptr<MapRenderer>& mapRenderer,
                                                 const std::weak_ptr<TouchHandler>& touchHandler) {
        VectorTileLayer::setComponents(envelopeThreadPool, tileThreadPool, options, mapRenderer, touchHandler);

        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        _childOptions = options;
        _childMapRenderer = mapRenderer;
        _childTouchHandler = touchHandler;
        _componentsSet = true;
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer) {
                wireChild(s.childLayer);
            }
        }
    }

    void CompositeVectorTileLayer::loadData(const std::shared_ptr<CullState>& cullState) {
        VectorTileLayer::loadData(cullState);

        // Re-apply merged-vector (contour) generation parameters in case the style / nuti
        // parameters changed (a change triggers a decoder update -> tile reload -> loadData).
        applyVectorSourceConfigs();

        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer) {
                s.childLayer->loadData(cullState);
            }
        }
    }

    void CompositeVectorTileLayer::offsetLayerHorizontally(double offset) {
        VectorTileLayer::offsetLayerHorizontally(offset);

        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer) {
                s.childLayer->offsetLayerHorizontally(offset);
            }
        }
    }

    bool CompositeVectorTileLayer::isUpdateInProgress() const {
        if (VectorTileLayer::isUpdateInProgress()) {
            return true;
        }
        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer && s.childLayer->isUpdateInProgress()) {
                return true;
            }
        }
        return false;
    }

    void CompositeVectorTileLayer::calculateRayIntersectedElements(const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<RayIntersectedElement>& results) const {
        VectorTileLayer::calculateRayIntersectedElements(ray, viewState, results);

        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer) {
                s.childLayer->calculateRayIntersectedElements(ray, viewState, results);
            }
        }
    }

    bool CompositeVectorTileLayer::onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState) {
        return renderSegmented(deltaSeconds, billboardSorter, viewState, false);
    }

    bool CompositeVectorTileLayer::onDrawFrame3D(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState) {
        return renderSegmented(deltaSeconds, billboardSorter, viewState, true);
    }

    const CompositeVectorTileLayer::ExternalSource* CompositeVectorTileLayer::findExternalSource(const std::string& name) const {
        auto it = std::find_if(_externalSources.begin(), _externalSources.end(), [&](const ExternalSource& s) { return s.name == name; });
        return it != _externalSources.end() ? &(*it) : nullptr;
    }

    void CompositeVectorTileLayer::rebuildRenderSteps() {
        // Caller holds _sourceMutex (or is the constructor).
        _renderSteps.clear();

        auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder());
        if (!decoder) {
            return;
        }
        std::vector<std::string> order = decoder->getStyleLayerNames();

        auto isChildSlot = [&](const std::string& layerName) {
            const ExternalSource* s = findExternalSource(layerName);
            return s && s->childLayer && s->type != CompositeSourceType::COMPOSITE_SOURCE_TYPE_VECTOR;
        };

        auto buildFilter = [](const std::vector<std::string>& group) -> std::optional<std::regex> {
            if (group.empty()) {
                return std::nullopt;
            }
            std::string pattern = "^(";
            for (std::size_t i = 0; i < group.size(); i++) {
                pattern += (i ? "|" : "") + group[i];
            }
            pattern += ")";
            return std::regex(pattern, std::regex::ECMAScript);
        };

        std::vector<std::string> group;
        for (const std::string& layerName : order) {
            if (isChildSlot(layerName)) {
                RenderStep step;
                step.vtFilter = buildFilter(group);
                step.hasVtGroup = !group.empty();
                step.childSlot = layerName;
                _renderSteps.push_back(std::move(step));
                group.clear();
            } else {
                group.push_back(layerName);
            }
        }
        RenderStep tail;
        tail.vtFilter = buildFilter(group);
        tail.hasVtGroup = !group.empty();
        _renderSteps.push_back(std::move(tail));

        // Warn about registered raster/hillshade sources that have no slot in the style order.
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer && std::find(order.begin(), order.end(), s.name) == order.end()) {
                Log::Warnf("CompositeVectorTileLayer: external source '%s' is not listed in the style 'layers' - it will not be drawn", s.name.c_str());
            }
        }
    }

    void CompositeVectorTileLayer::applyConfig(const ExternalSource& source, const mvt::ResolvedLayerConfig& config, const ViewState& viewState) {
        auto getValue = [&](const std::string& key) -> const mvt::Value* {
            auto it = config.values.find(key);
            return it != config.values.end() ? &it->second : nullptr;
        };

        if (source.type == CompositeSourceType::COMPOSITE_SOURCE_TYPE_RASTER) {
            auto raster = std::static_pointer_cast<RasterTileLayer>(source.childLayer);
            if (const mvt::Value* v = getValue("opacity")) { raster->setOpacity(valueToFloat(*v, 1.0f)); }
            if (const mvt::Value* v = getValue("filter-mode")) {
                if (auto str = std::get_if<std::string>(v)) { raster->setTileFilterMode(parseFilterMode(*str)); }
            }
        } else if (source.type == CompositeSourceType::COMPOSITE_SOURCE_TYPE_HILLSHADE) {
            auto hillshade = std::static_pointer_cast<HillshadeRasterTileLayer>(source.childLayer);
            if (const mvt::Value* v = getValue("opacity"))      { hillshade->setOpacity(valueToFloat(*v, 1.0f)); }
            if (const mvt::Value* v = getValue("exaggeration"))  { hillshade->setHeightScale(valueToFloat(*v, 1.0f)); }
            else if (const mvt::Value* v = getValue("height-scale")) { hillshade->setHeightScale(valueToFloat(*v, 1.0f)); }
            if (const mvt::Value* v = getValue("contrast"))     { hillshade->setContrast(valueToFloat(*v, 0.5f)); }
            if (const mvt::Value* v = getValue("shadow-color"))    { hillshade->setShadowColor(parseColorValue(*v, Color(0, 0, 0, 255))); }
            if (const mvt::Value* v = getValue("highlight-color")) { hillshade->setHighlightColor(parseColorValue(*v, Color(255, 255, 255, 255))); }
            if (const mvt::Value* v = getValue("accent-color"))    { hillshade->setAccentColor(parseColorValue(*v, Color(0, 0, 0, 255))); }
            if (const mvt::Value* v = getValue("method")) {
                if (auto str = std::get_if<std::string>(v)) { hillshade->setHillshadeMethod(parseHillshadeMethod(*str)); }
            }
            if (const mvt::Value* v = getValue("contour-interval")) {
                float interval = valueToFloat(*v, 0.0f);
                hillshade->setContourEnabled(interval > 0.0f);
                if (interval > 0.0f) { hillshade->setContourInterval(interval); }
            }
            if (const mvt::Value* v = getValue("contour-color")) { hillshade->setContourColor(parseColorValue(*v, Color(0xC5, 0x60, 0x08, 0xff))); }
            if (const mvt::Value* v = getValue("contour-width")) { hillshade->setContourWidth(valueToFloat(*v, 1.0f)); }
            // TODO: illumination-direction (azimuth degrees) -> setIlluminationDirection(MapVec).
        }
    }

    void CompositeVectorTileLayer::applyVectorSourceConfigs() {
        auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder());
        if (!decoder) {
            return;
        }

        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
        for (const ExternalSource& s : _externalSources) {
            if (s.type != CompositeSourceType::COMPOSITE_SOURCE_TYPE_VECTOR) {
                continue;
            }
            auto contour = std::dynamic_pointer_cast<ContourTileDataSource>(s.dataSource);
            if (!contour) {
                continue;
            }
            // Contour generation parameters are not per-frame (changing them regenerates
            // tiles), so evaluate at a neutral zoom; nuti parameters still apply.
            mvt::ResolvedLayerConfig config = decoder->resolveLayerConfig(s.name, 0.0f);
            std::map<std::string, float>& applied = _lastVectorConfig[s.name];

            auto changed = [&](const std::string& key, float& outValue) {
                auto it = config.values.find(key);
                if (it == config.values.end()) {
                    return false;
                }
                float value = valueToFloat(it->second, 0.0f);
                auto ait = applied.find(key);
                if (ait != applied.end() && ait->second == value) {
                    return false;
                }
                applied[key] = value;
                outValue = value;
                return true;
            };

            float value = 0.0f;
            if (changed("base-interval", value))      { contour->setBaseInterval(value); }
            if (changed("resolution", value))         { contour->setResolution(static_cast<int>(value)); }
            if (changed("min-visible-zoom", value))   { contour->setMinVisibleZoom(static_cast<int>(value)); }
            if (changed("simplify-tolerance", value)) { contour->setSimplifyTolerance(value); }
        }
    }

    bool CompositeVectorTileLayer::renderSegmented(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState, bool terrain) {
        auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder());

        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);

        bool hasChildSteps = std::any_of(_renderSteps.begin(), _renderSteps.end(), [](const RenderStep& s) { return !s.childSlot.empty(); });
        if (!decoder || !hasChildSteps) {
            // No interleaving needed - behave exactly as a plain VectorTileLayer.
            return terrain ? VectorTileLayer::onDrawFrame3D(deltaSeconds, billboardSorter, viewState)
                           : VectorTileLayer::onDrawFrame(deltaSeconds, billboardSorter, viewState);
        }

        auto mapRenderer = getMapRenderer();
        if (!mapRenderer) {
            return false;
        }

        float opacity = getOpacity();
        if (opacity < 1.0f) {
            mapRenderer->clearAndBindScreenFBO(Color(0, 0, 0, 0), true, true);
        }

        _tileRenderer->setLabelOrder(static_cast<int>(getLabelRenderOrder()));
        _tileRenderer->setBuildingOrder(static_cast<int>(getBuildingRenderOrder()));
        _tileRenderer->setLayerBlendingSpeed(getLayerBlendingSpeed());
        _tileRenderer->setLabelBlendingSpeed(getLabelBlendingSpeed());

        bool refresh = false;
        for (const RenderStep& step : _renderSteps) {
            if (step.hasVtGroup) {
                _tileRenderer->setRendererLayerFilter(step.vtFilter);
                refresh = (terrain ? _tileRenderer->onDrawFrame3D(deltaSeconds, viewState)
                                   : _tileRenderer->onDrawFrame(deltaSeconds, viewState)) || refresh;
            }
            if (!step.childSlot.empty()) {
                const ExternalSource* source = findExternalSource(step.childSlot);
                if (source && source->childLayer) {
                    mvt::ResolvedLayerConfig config = decoder->resolveLayerConfig(step.childSlot, viewState.getZoom());
                    applyConfig(*source, config, viewState);
                    if (config.visible) {
                        refresh = (terrain ? source->childLayer->onDrawFrame3D(deltaSeconds, billboardSorter, viewState)
                                           : source->childLayer->onDrawFrame(deltaSeconds, billboardSorter, viewState)) || refresh;
                    }
                }
            }
        }

        _tileRenderer->setRendererLayerFilter(std::nullopt);

        if (opacity < 1.0f) {
            mapRenderer->blendAndUnbindScreenFBO(opacity);
        }
        return refresh;
    }

}
