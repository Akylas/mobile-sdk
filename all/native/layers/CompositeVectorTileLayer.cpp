#include "CompositeVectorTileLayer.h"
#include "datasources/ContourTileDataSource.h"
#include "layers/RasterTileLayer.h"
#include "layers/HillshadeRasterTileLayer.h"
#include "vectortiles/MBVectorTileDecoder.h"
#include "renderers/TileRenderer.h"
#include "rastertiles/ElevationDecoder.h"
#include "rastertiles/TerrariumElevationDataDecoder.h"
#include "rastertiles/MapBoxElevationDataDecoder.h"
#include "renderers/MapRenderer.h"
#include "graphics/ViewState.h"
#include "graphics/Color.h"
#include "core/MapRange.h"
#include "core/MapVec.h"
#include "components/Exceptions.h"
#include "utils/Log.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
        VectorTileLayer(dataSource, decoder),
        _externalSources(),
        _drawItems(),
        _lastVectorConfig(),
        _singlePassRenderingEnabled(false),
        _componentsSet(false),
        _childOptions(),
        _childMapRenderer(),
        _childTouchHandler(),
        _sourceMutex()
    {
        rebuildDrawItems();
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
            std::shared_ptr<ElevationDecoder> elevDecoder = elevationDecoder;
            if (!elevDecoder) {
                elevDecoder = resolveElevationDecoder(dataSource);
            }
            childLayer = elevDecoder ? std::make_shared<HillshadeRasterTileLayer>(dataSource, elevDecoder)
                                     : std::make_shared<HillshadeRasterTileLayer>(dataSource);
        }

        {
            std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
            removeExternalDataSource(name); // replace if it exists
            _externalSources.push_back({ name, type, dataSource, childLayer });
            if (_componentsSet) {
                wireChild(childLayer);
            }
            rebuildDrawItems();
        }
        refresh();
    }

    void CompositeVectorTileLayer::addVectorDataSource(const std::string& name, const std::shared_ptr<TileDataSource>& dataSource) {
        if (!dataSource) {
            throw NullArgumentException("Null dataSource");
        }

        // A vector source is drawn as its own child VectorTileLayer over its own source, using the
        // master decoder and filtered to its own layer name. Kept separate (not merged) so it can
        // overzoom independently - e.g. a ContourTileDataSource renders z13+ from z12 DEM data via
        // the child layer's MaxOverzoomLevel, without needing the DEM at the target zoom.
        auto childVectorLayer = std::make_shared<VectorTileLayer>(dataSource, getTileDecoder());
        childVectorLayer->setRendererLayerFilter("^(" + name + ")$");
        childVectorLayer->setMaxOverzoomLevel(dataSource->getMaxOverzoomLevel());
        childVectorLayer->setLabelRenderOrder(getLabelRenderOrder());
        childVectorLayer->setBuildingRenderOrder(getBuildingRenderOrder());
        std::shared_ptr<Layer> childLayer = childVectorLayer;

        {
            std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
            removeExternalDataSource(name);
            _externalSources.push_back({ name, CompositeSourceType::COMPOSITE_SOURCE_TYPE_VECTOR, dataSource, childLayer });
            if (_componentsSet) {
                wireChild(childLayer);
            }
            rebuildDrawItems();
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
                if (it->childLayer && _componentsSet) {
                    unwireChild(it->childLayer);
                }
                _externalSources.erase(it);
                _lastVectorConfig.erase(name);
                _lastChildConfig.erase(name);
                rebuildDrawItems();
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
        {
            std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
            if (_singlePassRenderingEnabled == enabled) {
                return;
            }
            _singlePassRenderingEnabled = enabled;
            rebuildDrawItems(); // switch between per-group layers and single-pass segments
        }
        refresh();
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

    std::string CompositeVectorTileLayer::buildFilterString(const std::vector<std::string>& group, bool includeBackground) {
        // The filter is tested with std::regex_match (full match) and the per-tile background layer
        // has an EMPTY name (TileReader). So "^$" matches ONLY the background, and a trailing empty
        // alternative "(...|)" additionally matches it. Non-bottom groups must NOT match "" or they
        // would paint the opaque background over earlier groups.
        if (group.empty()) {
            // Bottom group with no style layers still draws the background; other empty groups match
            // nothing ("[^\\s\\S]" requires one impossible char, so it matches no string, not even "").
            return includeBackground ? "^$" : "[^\\s\\S]";
        }
        std::string pattern = "^(";
        for (std::size_t i = 0; i < group.size(); i++) {
            pattern += (i ? "|" : "") + group[i];
        }
        if (includeBackground) {
            pattern += "|"; // empty alternative -> also matches the empty-named background layer
        }
        pattern += ")$";
        return pattern;
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

    std::shared_ptr<Layer> CompositeVectorTileLayer::makeGroupLayer(const std::string& filter) {
        // Internal group layer: same merged source + decoder as this layer, with a fixed
        // rendererLayerFilter. Shares the master data source so a source change reloads all groups.
        auto groupLayer = std::make_shared<VectorTileLayer>(getDataSource(), getTileDecoder());
        groupLayer->setRendererLayerFilter(filter);
        groupLayer->setLabelRenderOrder(getLabelRenderOrder());
        groupLayer->setBuildingRenderOrder(getBuildingRenderOrder());
        std::shared_ptr<Layer> child = groupLayer;
        if (_componentsSet) {
            wireChild(child);
        }
        return child;
    }

    void CompositeVectorTileLayer::applyExternalChildZoomRange(const ExternalSource& source) {
        auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder());
        if (!decoder || !source.childLayer) {
            return;
        }
        std::vector<int> range = decoder->getStyleLayerZoomRange(source.name);
        if (range.size() == 2) {
            source.childLayer->setVisibleZoomRange(MapRange(static_cast<float>(range[0]), static_cast<float>(range[1])));
        }
    }

    void CompositeVectorTileLayer::rebuildDrawItems() {
        // Caller holds _sourceMutex (or is the constructor).

        // Drop previous internal group layers.
        for (const DrawItem& item : _drawItems) {
            if (item.groupLayer && _componentsSet) {
                unwireChild(item.groupLayer);
            }
        }
        _drawItems.clear();

        auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder());
        if (!decoder) {
            VectorTileLayer::setRendererLayerFilter("");
            return;
        }
        std::vector<std::string> order = decoder->getStyleLayerNames();

        // Constrain each external child's visible zoom range to the style's config-rule range.
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer) {
                applyExternalChildZoomRange(s);
            }
        }

        _singlePassSegments.clear();
        if (_singlePassRenderingEnabled) {
            // Single-pass: no per-group layers; this layer renders all groups via layer-index ranges.
            VectorTileLayer::setRendererLayerFilter(""); // no build-time filter - the range gate selects
            rebuildSinglePassSegments(order);
            return;
        }

        auto isChildSlot = [&](const std::string& layerName) {
            const ExternalSource* s = findExternalSource(layerName);
            return s && s->childLayer; // raster / hillshade / vector children all occupy a slot
        };

        std::vector<std::string> group;
        bool firstSlotSeen = false;
        for (const std::string& layerName : order) {
            if (isChildSlot(layerName)) {
                if (!firstSlotSeen) {
                    // Group 0 renders on this layer itself. It also draws the style background
                    // (includeBackground), so the Map background-color appears once at the bottom.
                    VectorTileLayer::setRendererLayerFilter(buildFilterString(group, /*includeBackground=*/true));
                    firstSlotSeen = true;
                } else if (!group.empty()) {
                    // A non-empty intermediate group gets its own stable-filtered layer. Empty
                    // intermediate groups are skipped entirely (no layer to fetch/decode/overpaint).
                    _drawItems.push_back({ DRAW_ITEM_VT_GROUP, std::string(), makeGroupLayer(buildFilterString(group)) });
                }
                _drawItems.push_back({ DRAW_ITEM_EXTERNAL, layerName, std::shared_ptr<Layer>() });
                group.clear();
            } else {
                group.push_back(layerName);
            }
        }
        if (!firstSlotSeen) {
            // No external child slots: render everything on this layer, no interleaving.
            VectorTileLayer::setRendererLayerFilter("");
        } else if (!group.empty()) {
            _drawItems.push_back({ DRAW_ITEM_VT_GROUP, std::string(), makeGroupLayer(buildFilterString(group)) });
        }

        // Warn about registered sources that have no slot in the style order.
        for (const ExternalSource& s : _externalSources) {
            if (s.childLayer && std::find(order.begin(), order.end(), s.name) == order.end()) {
                Log::Warnf("CompositeVectorTileLayer: external source '%s' is not listed in the style 'layers' - it will not be drawn", s.name.c_str());
            }
        }
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
        for (const DrawItem& item : _drawItems) {
            if (item.groupLayer) {
                wireChild(item.groupLayer);
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
        for (const DrawItem& item : _drawItems) {
            if (item.groupLayer) {
                item.groupLayer->loadData(cullState);
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
        for (const DrawItem& item : _drawItems) {
            if (item.groupLayer) {
                item.groupLayer->offsetLayerHorizontally(offset);
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
        for (const DrawItem& item : _drawItems) {
            if (item.groupLayer && item.groupLayer->isUpdateInProgress()) {
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
        for (const DrawItem& item : _drawItems) {
            if (item.groupLayer) {
                item.groupLayer->calculateRayIntersectedElements(ray, viewState, results);
            }
        }
    }

    bool CompositeVectorTileLayer::onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState) {
        return renderComposite(deltaSeconds, billboardSorter, viewState, false);
    }

    bool CompositeVectorTileLayer::onDrawFrame3D(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState) {
        return renderComposite(deltaSeconds, billboardSorter, viewState, true);
    }

    const CompositeVectorTileLayer::ExternalSource* CompositeVectorTileLayer::findExternalSource(const std::string& name) const {
        auto it = std::find_if(_externalSources.begin(), _externalSources.end(), [&](const ExternalSource& s) { return s.name == name; });
        return it != _externalSources.end() ? &(*it) : nullptr;
    }

    void CompositeVectorTileLayer::applyConfig(const ExternalSource& source, const mvt::ResolvedLayerConfig& config, const ViewState& viewState) {
        auto getValue = [&](const std::string& key) -> const mvt::Value* {
            auto it = config.values.find(key);
            return it != config.values.end() ? &it->second : nullptr;
        };
        // Some hillshade setters bake into the normal map and re-decode the tile
        // (setHeightScale/setContrast/setContourEnabled call updateTiles). Applying those every frame
        // with a zoom-interpolated value would re-decode continuously and the tiles would never
        // settle, so the value appears static. Instead they are applied only when the INTEGER zoom
        // changes - which is exactly when the hillshade tiles reload for the new zoom level anyway, so
        // the re-decode is aligned and free. Result: per-zoom-level animation of exaggeration/contrast.
        // Cheap redraw-only setters (opacity, colors, method, illumination, contour width/color, filter
        // mode) are applied every frame so they stay smooth.
        std::map<std::string, float>& applied = _lastChildConfig[source.name];
        int intZoom = static_cast<int>(std::floor(viewState.getZoom()));
        bool decodeZoomChanged = (applied.find("__izoom") == applied.end()) || (static_cast<int>(applied["__izoom"]) != intZoom);
        applied["__izoom"] = static_cast<float>(intZoom);

        if (source.type == CompositeSourceType::COMPOSITE_SOURCE_TYPE_RASTER) {
            auto raster = std::static_pointer_cast<RasterTileLayer>(source.childLayer);
            if (const mvt::Value* v = getValue("opacity")) { raster->setOpacity(valueToFloat(*v, 1.0f)); }
            if (const mvt::Value* v = getValue("filter-mode")) {
                if (auto str = std::get_if<std::string>(v)) { raster->setTileFilterMode(parseFilterMode(*str)); }
            }
        } else if (source.type == CompositeSourceType::COMPOSITE_SOURCE_TYPE_HILLSHADE) {
            auto hillshade = std::static_pointer_cast<HillshadeRasterTileLayer>(source.childLayer);
            if (const mvt::Value* v = getValue("opacity")) { hillshade->setOpacity(valueToFloat(*v, 1.0f)); }
            // exaggeration is a per-frame shader uniform (no re-decode) -> animates smoothly with zoom.
            if (const mvt::Value* v = getValue("exaggeration")) { hillshade->setExaggeration(valueToFloat(*v, 1.0f)); }

            if (decodeZoomChanged) { // decode-bound: only at integer zoom crossings (aligned with tile reload)
                // height-scale is the raw normal-map height scale (baked at decode; steps with zoom).
                if (const mvt::Value* v = getValue("height-scale")) { hillshade->setHeightScale(valueToFloat(*v, 1.0f)); }
                if (const mvt::Value* v = getValue("contrast")) { hillshade->setContrast(valueToFloat(*v, 0.5f)); }
                if (const mvt::Value* v = getValue("contour-interval")) {
                    float interval = valueToFloat(*v, 0.0f);
                    hillshade->setContourEnabled(interval > 0.0f);
                    if (interval > 0.0f) { hillshade->setContourInterval(interval); }
                }
            }

            if (const mvt::Value* v = getValue("shadow-color"))    { hillshade->setShadowColor(parseColorValue(*v, Color(0, 0, 0, 255))); }
            if (const mvt::Value* v = getValue("highlight-color")) { hillshade->setHighlightColor(parseColorValue(*v, Color(255, 255, 255, 255))); }
            if (const mvt::Value* v = getValue("accent-color"))    { hillshade->setAccentColor(parseColorValue(*v, Color(0, 0, 0, 255))); }
            if (const mvt::Value* v = getValue("method")) {
                if (auto str = std::get_if<std::string>(v)) { hillshade->setHillshadeMethod(parseHillshadeMethod(*str)); }
            }
            if (const mvt::Value* v = getValue("contour-color")) { hillshade->setContourColor(parseColorValue(*v, Color(0xC5, 0x60, 0x08, 0xff))); }
            if (const mvt::Value* v = getValue("contour-width")) { hillshade->setContourWidth(valueToFloat(*v, 1.0f)); }
            if (const mvt::Value* v = getValue("illumination-direction")) {
                // Azimuth in degrees (0 = north, clockwise) at a fixed 45 deg altitude, matching the
                // HillshadeRasterTileLayer default direction convention (sin az, cos az, -sin alt).
                double azimuth = valueToFloat(*v, 335.0f) * M_PI / 180.0;
                hillshade->setIlluminationDirection(MapVec(std::sin(azimuth), std::cos(azimuth), -0.70710678));
            }
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

    bool CompositeVectorTileLayer::drawExternalChild(const std::string& slot, float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState, bool terrain) {
        const ExternalSource* source = findExternalSource(slot);
        if (!source || !source->childLayer) {
            return false;
        }
        bool visible = true;
        // Raster/hillshade children are gated by their config symbolizer's zoom/nuti visibility.
        // Vector children have no config symbolizer (they are styled by normal line/text rules,
        // which the child's own decode already zoom-filters), so they always draw.
        if (source->type != CompositeSourceType::COMPOSITE_SOURCE_TYPE_VECTOR) {
            if (auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder())) {
                mvt::ResolvedLayerConfig config = decoder->resolveLayerConfig(slot, viewState.getZoom());
                applyConfig(*source, config, viewState);
                visible = config.visible;
            }
        }
        if (!visible) {
            return false;
        }
        return terrain ? source->childLayer->onDrawFrame3D(deltaSeconds, billboardSorter, viewState)
                       : source->childLayer->onDrawFrame(deltaSeconds, billboardSorter, viewState);
    }

    bool CompositeVectorTileLayer::renderComposite(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState, bool terrain) {
        std::lock_guard<std::recursive_mutex> lock(_sourceMutex);

        if (_singlePassRenderingEnabled) {
            return renderSinglePass(deltaSeconds, billboardSorter, viewState, terrain);
        }

        // Multi-layer: group 0 renders on this layer itself (with the group-0 rendererLayerFilter set
        // in rebuildDrawItems), later groups on internal layers, external children between.
        bool refresh = terrain ? VectorTileLayer::onDrawFrame3D(deltaSeconds, billboardSorter, viewState)
                               : VectorTileLayer::onDrawFrame(deltaSeconds, billboardSorter, viewState);

        for (const DrawItem& item : _drawItems) {
            if (item.kind == DRAW_ITEM_VT_GROUP) {
                if (item.groupLayer) {
                    refresh = (terrain ? item.groupLayer->onDrawFrame3D(deltaSeconds, billboardSorter, viewState)
                                       : item.groupLayer->onDrawFrame(deltaSeconds, billboardSorter, viewState)) || refresh;
                }
                continue;
            }
            refresh = drawExternalChild(item.slot, deltaSeconds, billboardSorter, viewState, terrain) || refresh;
        }
        return refresh;
    }

    bool CompositeVectorTileLayer::renderSinglePass(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState, bool terrain) {
        // Caller holds _sourceMutex. This layer decodes once and draws each segment as a layer-index
        // range on its own renderer (the render-time gate), with external children between. Labels are
        // rendered once, on the last segment, on top (this layer's labelRenderOrder is hidden on the
        // other segments). The frame delta is passed only to the first segment so blend/animation state
        // advances once per frame rather than once per segment.
        if (_singlePassSegments.empty()) {
            return terrain ? VectorTileLayer::onDrawFrame3D(deltaSeconds, billboardSorter, viewState)
                           : VectorTileLayer::onDrawFrame(deltaSeconds, billboardSorter, viewState);
        }

        VectorTileRenderOrder::VectorTileRenderOrder savedLabelOrder = getLabelRenderOrder();
        bool refresh = false;
        for (std::size_t i = 0; i < _singlePassSegments.size(); i++) {
            const SinglePassSegment& segment = _singlePassSegments[i];
            bool lastSegment = (i + 1 == _singlePassSegments.size());

            _tileRenderer->setRendererLayerIndexRange(std::make_pair(segment.loIndex, segment.hiIndex));
            setLabelRenderOrder(lastSegment ? savedLabelOrder : VectorTileRenderOrder::VECTOR_TILE_RENDER_ORDER_HIDDEN);

            float segmentDelta = (i == 0) ? deltaSeconds : 0.0f;
            refresh = (terrain ? VectorTileLayer::onDrawFrame3D(segmentDelta, billboardSorter, viewState)
                               : VectorTileLayer::onDrawFrame(segmentDelta, billboardSorter, viewState)) || refresh;

            if (!segment.childSlot.empty()) {
                refresh = drawExternalChild(segment.childSlot, deltaSeconds, billboardSorter, viewState, terrain) || refresh;
            }
        }
        _tileRenderer->setRendererLayerIndexRange(std::nullopt);
        setLabelRenderOrder(savedLabelOrder);
        return refresh;
    }

    void CompositeVectorTileLayer::rebuildSinglePassSegments(const std::vector<std::string>& order) {
        // Caller holds _sourceMutex. Style layer at position i has tile layerIndex in
        // [i*65536, (i+1)*65536) (TileReader: styleLayerIdx = layerNum*65536 + ...). A group covering
        // positions [a, b) is the layer-index range [a*65536, b*65536); segment 0 starts at INT_MIN so
        // the empty-named background layer (layerIndex -1) is drawn in the bottom segment.
        _singlePassSegments.clear();

        std::vector<std::pair<int, std::string> > slots; // (style position, external source name)
        for (int i = 0; i < static_cast<int>(order.size()); i++) {
            const ExternalSource* source = findExternalSource(order[i]);
            if (source && source->childLayer) {
                slots.emplace_back(i, order[i]);
            }
        }

        int lo = std::numeric_limits<int>::min();
        for (const std::pair<int, std::string>& slot : slots) {
            int hi = slot.first * 65536;
            _singlePassSegments.push_back({ lo, hi, slot.second });
            lo = hi;
        }
        // Trailing segment: everything after the last slot; carries the single label pass.
        _singlePassSegments.push_back({ lo, std::numeric_limits<int>::max(), std::string() });
    }

}
