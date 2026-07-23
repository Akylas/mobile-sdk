# CompositeVectorTileLayer — Detailed Design & Implementation Plan

Branch: `feat/composite-vector-tile-layer`

## Goal

A layer that renders a **master vector-tile style** but lets you weave **external named
data sources** (raster, hillshade, extra vector, contour) into the style's layer order.
Placement, visibility and per-source settings are driven from CartoCSS with zoom and
`nuti::` expressions:

```css
/* raster overlay, half-opaque, only mid-zoom, multiply blend */
#satellite [zoom>=8][zoom<14] {
  raster-opacity: 0.6;
  raster-comp-op: multiply;
}

/* hillshade woven between fills and roads; exaggeration ramps with zoom;
   toggled by a runtime nuti parameter */
#hillshade [zoom>5][zoom<=15][nuti::show_relief=true] {
  hillshade-exaggeration: linear(zoom, [5, 0.3], [12, 1.0]);
  hillshade-illumination-direction: 315;
  hillshade-shadow-color: #003040;
  raster-opacity: 0.8;
}

/* on-the-fly contour lines — this is a MERGED vector source, so it is styled
   with the normal line/text symbolizers, no special config symbolizer needed */
#contour [zoom>=12] {
  line-width: 1;
  line-color: #a06010;
  text-name: [ele];
  text-face-name: "Noto Sans Regular";
}
```

## Confirmed decisions

| Topic | Choice |
|-------|--------|
| Class name | `CompositeVectorTileLayer` (extends `VectorTileLayer`) |
| Config mechanism | Extend libs-carto: real config symbolizers in cartocss+mapnikvt |
| Terrain | Must work with 3D terrain (draped children, painter-order model) |
| Extra vector source | Merged into master decoder/style |
| Contour source | Merged vector (styled by master CSS); params driven by an optional config symbolizer |
| Dynamic | add/remove external sources at runtime |

---

## Key architectural insight

The property system already does the hard part. From `Properties.h`:

- `FloatFunctionProperty`, `ColorFunctionProperty`, `ColorProperty`, `FloatProperty` all
  parse a CartoCSS expression and can be **evaluated per-frame** against an
  `ExpressionContext` + `ViewState` (`getStaticValue(context)` returns the concrete value
  for the current `view::zoom`).
- `nuti::` parameters and `zoom` are already resolved by `ExpressionContext`.
- Rule visibility (`[zoom>5]`, `[nuti::x=true]`) is already encoded as `mvt::Rule`
  min/max-zoom + a `Filter` predicate (`CartoCSSMapnikTranslator::buildRule`,
  `CartoCSSMapnikTranslator.cpp:29`).

So the **config symbolizers emit no geometry**. They exist only to (a) let CartoCSS parse
and validate `hillshade-*` / `raster-*` properties, and (b) act as a typed, evaluable
container. `CompositeVectorTileLayer` reads their evaluated values every frame via a small
decoder helper — no changes to the decode/geometry path, no injection into `vt::Tile`.

Zoom-range and nuti visibility fall out for free: if no rule matches the current
view-zoom / nuti map, the helper returns "no config" ⇒ that external source is not drawn
this frame.

---

## Part A — libs-carto (submodule `develop`)

> Submodule gotcha: `libs-carto` is often in **detached HEAD**; `git status -sb` there and
> commit on `develop` before bumping the pointer.

### A.1 New symbolizer base: `LayerConfigSymbolizer`

A symbolizer whose `createFeatureProcessor` returns an empty processor (never touches
geometry). It just holds evaluable properties.

`libs-carto/mapnikvt/src/mapnikvt/LayerConfigSymbolizer.h`:

```cpp
#ifndef _CARTO_MAPNIKVT_LAYERCONFIGSYMBOLIZER_H_
#define _CARTO_MAPNIKVT_LAYERCONFIGSYMBOLIZER_H_

#include "Symbolizer.h"

namespace carto::mvt {
    // Base for "external source config" symbolizers (raster / hillshade / contour).
    // Produces NO geometry. Its properties are read out-of-band by the SDK layer via
    // Symbolizer::getProperty(name) + Property::getExpression(), evaluated per frame.
    class LayerConfigSymbolizer : public Symbolizer {
    public:
        // Never emits geometry.
        FeatureProcessor createFeatureProcessor(const ExpressionContext&, const SymbolizerContext&) const override {
            return FeatureProcessor();
        }

    protected:
        explicit LayerConfigSymbolizer(std::shared_ptr<Logger> logger) : Symbolizer(std::move(logger)) {
            // 'visible' lets a style force-hide independent of zoom predicates.
            bindProperty("visible", &_visible);
            bindProperty("opacity", &_opacity);
        }

        BoolProperty        _visible = BoolProperty(true);
        FloatFunctionProperty _opacity = FloatFunctionProperty(1.0f);
    };
}
#endif
```

### A.2 `RasterConfigSymbolizer` and `HillshadeConfigSymbolizer`

`libs-carto/mapnikvt/src/mapnikvt/RasterConfigSymbolizer.h`:

```cpp
#include "LayerConfigSymbolizer.h"

namespace carto::mvt {
    class RasterConfigSymbolizer : public LayerConfigSymbolizer {
    public:
        explicit RasterConfigSymbolizer(std::shared_ptr<Logger> logger) : LayerConfigSymbolizer(std::move(logger)) {
            bindProperty("filter-mode", &_filterMode);   // nearest | bilinear | bicubic
            bindProperty("comp-op",     &_compOp);        // already bound by Symbolizer, rebind is fine
        }
    protected:
        StringProperty _filterMode = StringProperty("bilinear");
    };
}
```

`libs-carto/mapnikvt/src/mapnikvt/HillshadeConfigSymbolizer.h`:

```cpp
#include "LayerConfigSymbolizer.h"

namespace carto::mvt {
    class HillshadeConfigSymbolizer : public LayerConfigSymbolizer {
    public:
        explicit HillshadeConfigSymbolizer(std::shared_ptr<Logger> logger) : LayerConfigSymbolizer(std::move(logger)) {
            bindProperty("exaggeration",           &_exaggeration);
            bindProperty("height-scale",           &_heightScale);
            bindProperty("contrast",               &_contrast);
            bindProperty("illumination-direction", &_illumDir);
            bindProperty("shadow-color",           &_shadowColor);
            bindProperty("highlight-color",        &_highlightColor);
            bindProperty("accent-color",           &_accentColor);
            bindProperty("method",                 &_method);        // standard|combined|igor|...
            bindProperty("contour-interval",       &_contourInterval);
            bindProperty("contour-color",          &_contourColor);
            bindProperty("contour-width",          &_contourWidth);
        }
    protected:
        FloatFunctionProperty _exaggeration    = FloatFunctionProperty(1.0f);
        FloatFunctionProperty _heightScale     = FloatFunctionProperty(1.0f);
        FloatFunctionProperty _contrast        = FloatFunctionProperty(0.5f);
        FloatFunctionProperty _illumDir        = FloatFunctionProperty(335.0f);
        ColorProperty         _shadowColor     = ColorProperty("#000000");
        ColorProperty         _highlightColor  = ColorProperty("#ffffff");
        ColorProperty         _accentColor     = ColorProperty("#000000");
        StringProperty        _method          = StringProperty("standard");
        FloatFunctionProperty _contourInterval = FloatFunctionProperty(0.0f); // 0 = off
        ColorProperty         _contourColor    = ColorProperty("#804000");
        FloatFunctionProperty _contourWidth    = FloatFunctionProperty(1.0f);
    };
}
```

`.cpp` files: only needed if any property needs custom parsing; the above all use
existing property types, so a trivial `.cpp` (or header-only) suffices.

### A.3 CartoCSS translator wiring (`CartoCSSMapnikTranslator.cpp`)

Three edits, all in existing tables/switch:

1. Add symbolizer ids to `_symbolizerList` (order matters: longest-prefix first, matching
   the existing `"line-pattern"` before `"line"` convention):

```cpp
const std::vector<std::string> CartoCSSMapnikTranslator::_symbolizerList = {
    "line-pattern", "line", "polygon-pattern", "polygon", "point",
    "text", "marker", "shield", "building",
    "hillshade", "raster", "contour"          // NEW
};
```

2. Add property → mapnik-property rows to `_symbolizerPropertyMap`:

```cpp
{ "raster-opacity",      "opacity" },
{ "raster-comp-op",      "comp-op" },
{ "raster-filter-mode",  "filter-mode" },

{ "hillshade-opacity",               "opacity" },
{ "hillshade-exaggeration",          "exaggeration" },
{ "hillshade-height-scale",          "height-scale" },
{ "hillshade-contrast",              "contrast" },
{ "hillshade-illumination-direction","illumination-direction" },
{ "hillshade-shadow-color",          "shadow-color" },
{ "hillshade-highlight-color",       "highlight-color" },
{ "hillshade-accent-color",          "accent-color" },
{ "hillshade-method",                "method" },
{ "hillshade-contour-interval",      "contour-interval" },
{ "hillshade-contour-color",         "contour-color" },
{ "hillshade-contour-width",         "contour-width" },
{ "hillshade-comp-op",               "comp-op" },
// contour datasource params (optional; drives ContourTileDataSource setters, not per-frame)
{ "contour-base-interval",           "base-interval" },
{ "contour-resolution",              "resolution" },
```

3. Add construction cases to `createSymbolizer` (mirrors the `polygon`/`line` cases):

```cpp
else if (symbolizerType == "raster") {
    mapnikSymbolizer = std::make_shared<mvt::RasterConfigSymbolizer>(_logger);
}
else if (symbolizerType == "hillshade") {
    mapnikSymbolizer = std::make_shared<mvt::HillshadeConfigSymbolizer>(_logger);
}
else if (symbolizerType == "contour") {
    mapnikSymbolizer = std::make_shared<mvt::ContourConfigSymbolizer>(_logger);
}
```

Result: `#hillshade { hillshade-exaggeration: … }` compiles into a `Layer("hillshade")`
holding a `HillshadeConfigSymbolizer`, with zoom/nuti predicates preserved on its
`mvt::Rule`s — exactly like every other CartoCSS layer.

### A.4 Decoder-side evaluation helper (mapnikvt `Map` or a free function)

Add a helper that, without decoding a tile, returns the evaluated config for a named layer
at a given view zoom + nuti map. It walks the layer's styles→rules, honoring the rule
zoom range and filter predicate, and reads the config symbolizer's properties.

`libs-carto/mapnikvt/src/mapnikvt/LayerConfigResolver.h` (sketch):

```cpp
namespace carto::mvt {
    struct ResolvedLayerConfig {
        bool   visible = false;                       // false => no matching rule / hidden
        std::map<std::string, Value> values;          // "opacity", "exaggeration", ...
    };

    // viewZoom is fractional (view::zoom). nutiValues comes from the decoder's parameter map.
    ResolvedLayerConfig resolveLayerConfig(const Map& map,
                                           const std::string& layerName,
                                           float viewZoom,
                                           const std::map<std::string, Value>& nutiValues);
}
```

Implementation outline:

```cpp
ResolvedLayerConfig resolveLayerConfig(const Map& map, const std::string& layerName,
                                       float viewZoom, const std::map<std::string, Value>& nutiValues) {
    ResolvedLayerConfig out;
    std::shared_ptr<const Layer> layer = map.getLayer(layerName);   // add getter if missing
    if (!layer) return out;

    ExpressionContext ctx;
    ctx.setAdjustedZoom(static_cast<int>(viewZoom));    // predicate zoom
    ctx.setNutiParameterValueMap(nutiValues);
    vt::ViewState viewState; viewState.zoom = viewZoom; // for FloatFunction eval

    for (const auto& styleName : layer->getStyleNames()) {
        std::shared_ptr<const Style> style = map.getStyle(styleName);
        for (const std::shared_ptr<const Rule>& rule : style->getRules()) {
            if (viewZoom < rule->getMinZoom() || viewZoom >= rule->getMaxZoom()) continue;
            if (rule->getFilter() && !rule->getFilter()->evaluate(ctx)) continue;   // zoom/nuti predicates
            for (const auto& sym : rule->getSymbolizers()) {
                auto cfg = std::dynamic_pointer_cast<const LayerConfigSymbolizer>(sym);
                if (!cfg) continue;
                out.visible = true;
                for (const std::string& name : cfg->getPropertyNames()) {
                    if (const Property* p = cfg->getProperty(name); p && p->isDefined()) {
                        // Reuse the property's own evaluation. For FloatFunctionProperty this is
                        // getStaticValue(ctx); a small visitor maps each Property kind to a Value.
                        out.values[name] = evaluatePropertyValue(*p, ctx, viewState);
                    }
                }
            }
        }
    }
    return out;
}
```

`evaluatePropertyValue` is a thin switch over the concrete `Property` subtype (Float/
Color/String/FloatFunction) calling its existing `getValue`/`getStaticValue`. Some
`Map`/`Layer`/`Style`/`Rule` getters (`getLayer`, `getStyle`, `getRules`, `getMinZoom`,
`getFilter`) may need to be added/exposed — verify exact names during impl.

---

## Part B — all/native (main repo)

### B.1 Decoder passthrough (`MBVectorTileDecoder`)

Expose two things the composite needs (thin wrappers over the compiled `_map`):

```cpp
// MBVectorTileDecoder.h additions
public:
    // Ordered master style layer names (the #name order in the CSS).
    std::vector<std::string> getStyleLayerNames() const;
    // Evaluate a config layer's properties for the given view zoom (uses current nuti params).
    mvt::ResolvedLayerConfig resolveLayerConfig(const std::string& layerName, float viewZoom) const;
```

```cpp
// MBVectorTileDecoder.cpp
std::vector<std::string> MBVectorTileDecoder::getStyleLayerNames() const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::string> names;
    if (_map) for (const auto& layer : _map->getLayers()) names.push_back(layer->getName());
    return names;
}

mvt::ResolvedLayerConfig MBVectorTileDecoder::resolveLayerConfig(const std::string& layerName, float viewZoom) const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_map) return {};
    return mvt::resolveLayerConfig(*_map, layerName, viewZoom, _parameterValueMap);  // _parameterValueMap already held
}
```

### B.2 `CompositeVectorTileLayer` — header

`all/native/layers/CompositeVectorTileLayer.h`:

```cpp
#ifndef _CARTO_COMPOSITEVECTORTILELAYER_H_
#define _CARTO_COMPOSITEVECTORTILELAYER_H_

#include "layers/VectorTileLayer.h"

namespace carto {
    class HillshadeRasterTileLayer;
    class ElevationDecoder;

    namespace CompositeSourceType {
        enum CompositeSourceType {
            COMPOSITE_SOURCE_RASTER,     // drawn as RasterTileLayer
            COMPOSITE_SOURCE_HILLSHADE,  // drawn as HillshadeRasterTileLayer
            COMPOSITE_SOURCE_VECTOR,     // merged into master decoder (incl. ContourTileDataSource)
        };
    }

    /**
     * A VectorTileLayer that can weave named external data sources (raster, hillshade,
     * merged vector / contour) into the master CartoCSS style's layer order. The slot and
     * per-source settings are taken from a matching '#name { ... }' block in the style.
     */
    class CompositeVectorTileLayer : public VectorTileLayer {
    public:
        CompositeVectorTileLayer(const std::shared_ptr<TileDataSource>& dataSource,
                                 const std::shared_ptr<VectorTileDecoder>& decoder);
        virtual ~CompositeVectorTileLayer();

        // Raster / hillshade: rendered as its own draped child at the '#name' slot.
        void addExternalDataSource(const std::string& name,
                                   const std::shared_ptr<TileDataSource>& dataSource,
                                   CompositeSourceType::CompositeSourceType type,
                                   const std::shared_ptr<ElevationDecoder>& elevationDecoder = nullptr);
        // Merged vector (incl. contour): folded into the master source, styled by master CSS.
        void addVectorDataSource(const std::string& name,
                                 const std::shared_ptr<TileDataSource>& dataSource);

        bool removeExternalDataSource(const std::string& name);
        std::vector<std::string> getExternalDataSourceNames() const;

    protected:
        virtual bool onDrawFrame(float deltaSeconds, const ViewState& viewState);   // 2D
        virtual bool onDrawFrame3D(float deltaSeconds, const ViewState& viewState); // terrain

        virtual void setComponents(const std::shared_ptr<CancelableThreadPool>& envelopeThreadPool,
                                   const std::shared_ptr<CancelableThreadPool>& tileThreadPool,
                                   const std::weak_ptr<Options>& options,
                                   const std::weak_ptr<MapRenderer>& mapRenderer,
                                   const std::weak_ptr<TouchHandler>& touchHandler);

    private:
        struct ExternalSource {
            std::string name;
            CompositeSourceType::CompositeSourceType type;
            std::shared_ptr<TileDataSource> dataSource;
            std::shared_ptr<Layer> childLayer;   // Raster/Hillshade layer; null for merged vector
        };

        static std::shared_ptr<ElevationDecoder> resolveElevationDecoder(const std::shared_ptr<TileDataSource>& ds);
        void rebuildMergedDataSource();          // recompute the MergedMBVT chain for the master source
        void recomputeSegments();                // order external names against getStyleLayerNames()
        void applyConfig(const ExternalSource& src, const ViewState& viewState); // push evaluated props
        std::vector<std::optional<std::regex>> buildGroupFilters() const;        // regex per vt segment
        bool renderSegmented(float dt, const ViewState& viewState, bool terrain);

        std::vector<ExternalSource> _externalSources;   // registration order
        std::vector<std::string>    _rasterHillshadeOrder; // subset that occupy slots, in style order
        std::shared_ptr<TileDataSource> _baseVectorSource; // the original master source (pre-merge)
        mutable std::recursive_mutex _sourceMutex;
    };
}
#endif
```

### B.3 `CompositeVectorTileLayer` — key implementation

**Add / remove (dynamic):**

```cpp
void CompositeVectorTileLayer::addExternalDataSource(const std::string& name,
        const std::shared_ptr<TileDataSource>& ds, CompositeSourceType::CompositeSourceType type,
        const std::shared_ptr<ElevationDecoder>& elevDecoder) {
    std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
    ExternalSource src{ name, type, ds, nullptr };
    if (type == CompositeSourceType::COMPOSITE_SOURCE_RASTER) {
        src.childLayer = std::make_shared<RasterTileLayer>(ds);
    } else if (type == CompositeSourceType::COMPOSITE_SOURCE_HILLSHADE) {
        // If no decoder is supplied, resolve it from the data source's 'encoding' metadata
        // (mirrors ContourTileDataSource::resolveDecoder). Building it explicitly (vs the
        // single-arg HillshadeRasterTileLayer ctor) keeps decoder-based heightScale semantics.
        std::shared_ptr<ElevationDecoder> decoder = elevDecoder;
        if (!decoder) decoder = resolveElevationDecoder(ds);   // see helper below
        src.childLayer = decoder ? std::make_shared<HillshadeRasterTileLayer>(ds, decoder)
                                 : std::make_shared<HillshadeRasterTileLayer>(ds); // lazy-infer fallback
    } else {
        addVectorDataSource(name, ds);   // merged path
        return;
    }
    // Wire the child into the same map context so it fetches + drapes on terrain.
    if (auto mr = _mapRenderer.lock())
        src.childLayer->setComponents(_envelopeThreadPool, _tileThreadPool, _options, _mapRenderer, _touchHandler);
    _externalSources.push_back(std::move(src));
    recomputeSegments();
    refresh();
}

void CompositeVectorTileLayer::addVectorDataSource(const std::string& name,
        const std::shared_ptr<TileDataSource>& ds) {
    std::lock_guard<std::recursive_mutex> lock(_sourceMutex);
    _externalSources.push_back({ name, CompositeSourceType::COMPOSITE_SOURCE_VECTOR, ds, nullptr });
    rebuildMergedDataSource();       // fold into master TileDataSource
    recomputeSegments();
    refresh();
}

void CompositeVectorTileLayer::rebuildMergedDataSource() {
    std::shared_ptr<TileDataSource> merged = _baseVectorSource;
    for (const auto& s : _externalSources)
        if (s.type == CompositeSourceType::COMPOSITE_SOURCE_VECTOR)
            merged = std::make_shared<MergedMBVTTileDataSource>(merged, s.dataSource);
    setDataSource(merged);           // TileLayer setter; triggers tile reload
}
```

**Elevation decoder resolution from encoding** (used when the hillshade decoder arg is null):

```cpp
// Resolve an ElevationDecoder from a data source's 'encoding' (TileDataSource::getEncoding()).
// "mapbox" -> MapBoxElevationDataDecoder, "terrarium"/default -> TerrariumElevationDataDecoder.
// Returns null if encoding is unset, so the caller can fall back to lazy per-tile inference.
std::shared_ptr<ElevationDecoder> CompositeVectorTileLayer::resolveElevationDecoder(
        const std::shared_ptr<TileDataSource>& ds) {
    std::string encoding = ds ? ds->getEncoding() : std::string();
    if (encoding.empty())   return nullptr;                 // let HillshadeRasterTileLayer infer per tile
    if (encoding == "mapbox") return std::make_shared<MapBoxElevationDataDecoder>();
    return std::make_shared<TerrariumElevationDataDecoder>();  // terrarium is the default
}
```

> `ContourTileDataSource` is registered via `addVectorDataSource("contour", contourDS)`:
> it emits MVT, so it merges and is styled by the master `#contour {}` block. No child
> layer, no config symbolizer required for basic use. (An optional `#contour {
> contour-base-interval: …; contour-resolution: … }` config symbolizer can push the
> datasource setters on style/nuti change — applied in `applyConfig`, NOT per frame,
> since changing them re-generates tiles.)

**Segment ordering** — split master style layers at external slots:

```cpp
void CompositeVectorTileLayer::recomputeSegments() {
    auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder());
    if (!decoder) return;
    // Order comes from the style PROJECT JSON's "layers" array (CartoCSSMapLoader::loadMapProject,
    // CartoCSSMapLoader.cpp:86) -> mvt::Map::getLayers() -> getStyleLayerNames(). The "layers" array
    // is what defines both draw order AND which layers are visible; an external source is placed by
    // adding its name to that array. Names not listed there get no slot (skipped + warned).
    std::vector<std::string> order = decoder->getStyleLayerNames();

    // Which registered raster/hillshade sources actually have a slot in the style, in style order.
    std::set<std::string> registered;
    for (const auto& s : _externalSources)
        if (s.type != CompositeSourceType::COMPOSITE_SOURCE_VECTOR) registered.insert(s.name);

    _rasterHillshadeOrder.clear();
    for (const std::string& layerName : order)
        if (registered.count(layerName)) _rasterHillshadeOrder.push_back(layerName);
    // A registered-but-unreferenced source has no slot -> not drawn (log a warning).
}
```

**Segmented render** — the core. For each frame: render master vt style-layers up to the
next external slot (via a transient `rendererLayerFilter`), draw the child, continue.

```cpp
bool CompositeVectorTileLayer::renderSegmented(float dt, const ViewState& viewState, bool terrain) {
    auto decoder = std::dynamic_pointer_cast<MBVectorTileDecoder>(getTileDecoder());
    if (!decoder || _rasterHillshadeOrder.empty()) {
        // No interleaving needed — behave as a plain VectorTileLayer.
        return terrain ? VectorTileLayer::onDrawFrame3D(dt, viewState)
                       : VectorTileLayer::onDrawFrame(dt, viewState);
    }
    std::vector<std::string> order = decoder->getStyleLayerNames();

    bool refresh = false;
    std::size_t i = 0;
    std::string prevBoundary;    // exclusive lower bound (last drawn style layer)
    for (const std::string& slot : _rasterHillshadeOrder) {
        // 1) master vt: style layers in (prevBoundary, slot)  via regex filter
        std::optional<std::regex> filter = buildRangeFilter(order, prevBoundary, /*upTo=*/slot);
        _tileRenderer->setRendererLayerFilter(filter);
        refresh |= terrain ? _tileRenderer->onDrawFrame3D(dt, viewState)
                           : _tileRenderer->onDrawFrame(dt, viewState);

        // 2) the external child at this slot, with per-frame evaluated config
        const ExternalSource* src = findSource(slot);
        mvt::ResolvedLayerConfig cfg = decoder->resolveLayerConfig(slot, viewState.getZoom());
        if (src && cfg.visible) {
            applyConfig(*src, cfg, viewState);                       // opacity/exaggeration/...
            refresh |= renderChild(*src, dt, viewState, terrain);    // child->onDrawFrame(3D)
        }
        prevBoundary = slot;
    }
    // 3) master vt: remaining style layers after the last slot
    _tileRenderer->setRendererLayerFilter(buildRangeFilter(order, prevBoundary, /*upTo=*/""));
    refresh |= terrain ? _tileRenderer->onDrawFrame3D(dt, viewState)
                       : _tileRenderer->onDrawFrame(dt, viewState);

    _tileRenderer->setRendererLayerFilter(std::nullopt);   // restore
    return refresh;
}

bool CompositeVectorTileLayer::onDrawFrame(float dt, const ViewState& vs)   { return renderSegmented(dt, vs, false); }
bool CompositeVectorTileLayer::onDrawFrame3D(float dt, const ViewState& vs) { return renderSegmented(dt, vs, true); }
```

`buildRangeFilter` builds an ECMA regex matching the qualified names of the master style
layers strictly between two boundaries — reusing the existing `rendererLayerFilter`
mechanism (`TileRenderer.cpp:227` re-applies it every `onDrawFrame`). The child layers
are separate `RasterTileLayer`/`HillshadeRasterTileLayer` instances whose own
`onDrawFrame`/`onDrawFrame3D` already drape on terrain today, so cross-pass stacking is
pure painter order — consistent with the established terrain depth model (content never
writes depth; only per-layer surface pre-pass). **No new depth work.**

**Config application** — map resolved values onto the child's setters:

```cpp
void CompositeVectorTileLayer::applyConfig(const ExternalSource& src,
        const mvt::ResolvedLayerConfig& cfg, const ViewState& vs) {
    auto getF = [&](const char* k, float d){ auto it = cfg.values.find(k);
        return it==cfg.values.end()? d : (float)mvt::ValueConverter<double>::convert(it->second); };
    auto getC = [&](const char* k, Color d){ auto it = cfg.values.find(k);
        return it==cfg.values.end()? d : colorFromValue(it->second); };

    if (src.type == CompositeSourceType::COMPOSITE_SOURCE_RASTER) {
        src.childLayer->setOpacity(getF("opacity", 1.0f));
        // comp-op / filter-mode -> RasterTileLayer setters
    } else if (src.type == CompositeSourceType::COMPOSITE_SOURCE_HILLSHADE) {
        auto hs = std::static_pointer_cast<HillshadeRasterTileLayer>(src.childLayer);
        hs->setOpacity(getF("opacity", 1.0f));
        hs->setHeightScale(getF("exaggeration", 1.0f));   // maps to exaggeration/height scale
        hs->setContrast(getF("contrast", 0.5f));
        hs->setShadowColor(getC("shadow-color", Color(0,0,0,255)));
        hs->setHighlightColor(getC("highlight-color", Color(255,255,255,255)));
        // illumination-direction, method, contour-* -> matching HillshadeRasterTileLayer setters
    }
}
```

### B.4 `Layer::update` / lifecycle forwarding

Children are real `TileLayer`s: forward `update(cullState)`, `setComponents`, `refresh`,
`offsetLayerHorizontally`, and teardown to each `childLayer` so their fetch threads/caches
run. Do this in the composite's overrides of `Layer::update` and `setComponents`.

---

## Part C — SWIG (`all/modules/`)

`all/modules/layers/CompositeVectorTileLayer.i`:

```cpp
#ifndef _COMPOSITEVECTORTILELAYER_I
#define _COMPOSITEVECTORTILELAYER_I

%module CompositeVectorTileLayer

!proxy_imports(carto::CompositeVectorTileLayer, datasources.TileDataSource, layers.VectorTileLayer, vectortiles.VectorTileDecoder, rastertiles.ElevationDecoder)

%{
#include "layers/CompositeVectorTileLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_vector.i>
%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "layers/VectorTileLayer.i"
%import "datasources/TileDataSource.i"
%import "rastertiles/ElevationDecoder.i"

!enum(carto::CompositeSourceType::CompositeSourceType)
!polymorphic_shared_ptr(carto::CompositeVectorTileLayer, layers.CompositeVectorTileLayer)

%std_exceptions(carto::CompositeVectorTileLayer::CompositeVectorTileLayer)

%include "layers/CompositeVectorTileLayer.h"

#endif
```

Regenerate proxies manually (gradle does NOT run swig):

```sh
cd scripts && python3 swigpp-java.py \
  --profile "standard+valhalla+geocoding+routing+packagemanager" \
  --swig /Volumes/dev/carto/mobile-swig/swig
```

Add the module to the swig module list / `all/modules/carto_mobile_sdk.i` include set the
same way `VectorTileLayer.i` is registered.

---

## Part D — Usage example (target API)

```java
// master style declares #hillshade, #satellite, #contour blocks (see top of doc)
MBVectorTileDecoder decoder = new MBVectorTileDecoder(new CartoCSSStyleSet(cartoCss));
CompositeVectorTileLayer layer = new CompositeVectorTileLayer(baseMvtSource, decoder);

// names "satellite", "hillshade", "contour" MUST appear in the style project JSON "layers"
// array (defines order + visibility). The #name blocks supply per-source settings.
layer.addExternalDataSource("satellite", rasterSource, CompositeSourceType.COMPOSITE_SOURCE_RASTER);
// hillshade decoder arg omitted -> resolved from demSource.getEncoding() ("terrarium"/"mapbox")
demSource.setEncoding("terrarium");
layer.addExternalDataSource("hillshade", demSource, CompositeSourceType.COMPOSITE_SOURCE_HILLSHADE);
layer.addVectorDataSource("contour", new ContourTileDataSource(demSource));  // merged, styled by #contour

mapView.getLayers().add(layer);

// runtime toggle via nuti parameter used in the #hillshade predicate
decoder.setStyleParameter("show_relief", "false");   // hides hillshade live, no re-add
layer.removeExternalDataSource("satellite");         // dynamic remove
```

---

## Final rendering architecture (after device testing)

The `rendererLayerFilter` is applied at **tile-build time** (`GLTileRenderer::setVisibleTiles`),
not per draw call, so a single renderer cannot be re-filtered per frame. The layer therefore
renders **one stable-filtered layer per style-layer group**:

- Group 0 (style layers before the first external slot) renders on the composite itself.
- Each later group renders on an internal `VectorTileLayer` over the base source with a fixed
  filter. Empty intermediate groups create no layer (and empty filters use a true never-match
  `[^\s\S]`, because `regex_match` full-matches `""` — the per-tile background layer's name — so
  a naive empty filter would paint the opaque map background over earlier groups).
- Each external source is a child at its named slot: raster → `RasterTileLayer`, hillshade →
  `HillshadeRasterTileLayer`, vector/contour → its own `VectorTileLayer` over its own source
  (master decoder, filtered to its layer name, carrying its `MaxOverzoomLevel`). Vector children
  are **not merged**, so e.g. contours overzoom (z13+ from z12 DEM) via the child layer.
- Draw order per frame: group 0, then the draw items in style order (child / group / child ...),
  pure painter order (consistent with the terrain depth model). Raster/hillshade children gate on
  their config symbolizer's zoom/nuti visibility; vector children draw always (zoom-filtered by
  their own decode).

Cost: one vt **decode per group + per vector child**; share network with a cache on the source.

## Milestones

1. **[DONE]** libs-carto config symbolizers (A.1–A.3) + `resolveLayerConfig` (A.4).
   Files added under `libs-carto/mapnikvt/src/mapnikvt/`: `LayerConfigSymbolizer.h`,
   `RasterConfigSymbolizer.h`, `HillshadeConfigSymbolizer.h`, `ContourConfigSymbolizer.h`,
   `LayerConfigResolver.{h,cpp}`. Translator wired in `CartoCSSMapnikTranslator.cpp`
   (`_symbolizerList`, `_symbolizerPropertyMap`, `createSymbolizer`, includes). Both TUs
   pass `clang -fsyntax-only`; CMake globs the new `.cpp`. TODO: runtime assertion (CSS
   with `#hillshade{}` → `resolveLayerConfig` across a zoom sweep + nuti toggle) deferred
   to a linked build / the demo in Milestone 5.
2. **[DONE]** CompositeVectorTileLayer 2D + 3D segmented render (B). Added:
   `MBVectorTileDecoder::getStyleLayerNames()` + `resolveLayerConfig()` (SWIG-ignored);
   `datasources/DynamicMergedMBVTTileDataSource.{h,cpp}` (mutable N-source MBVT merge —
   needed because `TileLayer::_dataSource` is `const`, so the master source is wrapped once
   and vector sources are added/removed inside the wrapper); `layers/CompositeVectorTileLayer.{h,cpp}`
   (external source registry, raster/hillshade children, `rebuildRenderSteps` splitting the
   master style-layer order at child slots, `renderSegmented` = one filtered vt pass per
   group + child draw between, `applyConfig` mapping resolved props to child setters,
   lifecycle forwarding); `layers/CompositeVectorTileLayer.i`; `friend class
   CompositeVectorTileLayer;` added to `Layer.h` so the composite can drive child protected
   virtuals (setComponents/loadData/onDrawFrame/register listeners). All five TUs pass
   `clang -fsyntax-only`. TODO (needs a real build/device): runtime verify slot ordering,
   per-frame opacity/exaggeration, zoom/nuti visibility; illumination-direction mapping;
   interaction of the composite's own rendererLayerFilter with segment filters.
3. **[DONE]** Merged vector + contour. `addVectorDataSource` folds any MBVT source
   (incl. `ContourTileDataSource`) into the master via `DynamicMergedMBVTTileDataSource`,
   styled by the master CSS. `applyVectorSourceConfigs()` (called from `loadData`, with
   per-source change tracking to avoid setter→reload loops) applies `ContourConfigSymbolizer`
   values (`contour-base-interval`/`resolution`/`min-visible-zoom`/`simplify-tolerance`,
   nuti-aware, evaluated at a neutral zoom) to a `ContourTileDataSource`. Compiles.
4. **Terrain** (`onDrawFrame3D`): renderComposite already forwards `onDrawFrame3D` to group 0,
   the internal group layers and the external children in painter order (the terrain depth model
   is cross-layer painter order, so this should hold). NEEDS on-device validation: enable terrain
   in the demo (TerrainOptions on Options) alongside `addCompositeMap`, without the separate
   `addTerrain` hillshade layer. Watch for depth/see-through between groups (emulator vs device).
5. **[DEMO PREPPED]** SWIG + demo (C, D). `addCompositeMap(dataPath)` added to
   `SecondFragment.java` (uncommitted, per the never-commit-SecondFragment rule): a
   self-contained CartoCSS over the OpenMapTiles schema (openfreemap) with `#hillshade`
   (zoom-ramped exaggeration), `#satellite` (raster, faint, z>=13) and `#contour`
   (line + `contour-base-interval`) woven by first-reference order; hillshade decoder
   resolved from the DEM `encoding`; contour merged. Build steps below. Runtime verify +
   nuti (needs a project-bundle style) still pending on device.

### Build / run the demo

```sh
# 1. Regenerate SWIG proxies (gradle does NOT run swig)
cd scripts && python3 swigpp-java.py \
  --profile "standard+valhalla+geocoding+routing+packagemanager" \
  --swig /Volumes/dev/carto/mobile-swig/swig
# 2. Build the native libs + APK
cd scripts/android-dev && ./gradlew :app:assembleDebug -x lint
# 3. Install; the demo entry is proceedWithSdCard -> addCompositeMap.
#    To go back to the normal map, restore addMap/addTerrain in proceedWithSdCard.
```

Expected: base fills (water/landcover) → hillshade shading → faint satellite (z>=13) →
roads → buildings + contour lines (z>=12). Panning/zooming should keep the hillshade under
the roads; exaggeration should increase with zoom.
6. **Single-pass segmented renderer** (perf, optional, NOT STARTED). Current cost: the master
   style is decoded once per group (groups = external raster/hillshade/vector slots + 1), because
   each group is a separate `VectorTileLayer` (the `rendererLayerFilter` is build-time, so one
   renderer cannot draw disjoint ranges per frame). For typical styles with a few slots this is a
   few decodes; it grows with the number of interleaved sources.
   The fix is a **render-time** (not build-time) layer-index gate in `vt::GLTileRenderer`: let one
   renderer hold all decoded layers and draw a `[minLayerIdx, maxLayerIdx)` range per draw call,
   with children between. Integration points (all must honor the range consistently):
   `renderGeometry`/`renderLabels` loops over `renderTile.renderLayers`
   (GLTileRenderer.cpp:663/734/814/1362/1507/1666/1710) AND the blend-node construction
   (~1362-1400). Style layer boundaries come from the `layerIdx = layerNum*65536+...` encoding
   (TileReader.cpp:36), so a slot at style position p bounds a group at `layerIdx < p*65536`.
   Keep it behind `setSinglePassRenderingEnabled` (already stubbed, default off) and A/B on device
   before making it default. **Requires on-device validation** (this is the GL hot path; the
   terrain history shows emulator passes do not guarantee device correctness) - do not merge from
   a headless/emulator-only check.

**nuti-parameter visibility** works today with **project-bundle styles** (a zip/asset package whose
project JSON declares `nutiparameters`, e.g. the app's osm.zip): `resolveLayerConfig` evaluates
`[nuti::x=...]` predicates against the decoder's nuti value map, and `decoder.setStyleParameter`
toggles them live. It is NOT available with a **raw CartoCSS string** decoder, because
`CartoCSSMapLoader::loadMap` passes no `nutiParameters` (only `loadMapProject` reads them,
CartoCSSMapLoader.cpp:133). The demo uses a raw string for self-containment, so it demos zoom-based
config only. A future enhancement could let raw CartoCSS / the decoder declare nuti params
programmatically.

## Status: feature complete (2D + 3D terrain), verified on device

Milestones 1-5 done and device-verified (base fills, hillshade woven under roads, satellite raster
with zoom gating, contour lines with overzoom, style background, 3D terrain). Remaining is the
optional single-pass perf project (6) and the raw-string nuti enhancement, both above.

## Open points (leaning defaults)

1. Placement is by the style **project JSON `layers` array** (order + visibility), not by
   CSS first-reference. An external source is drawn only if its name is listed there;
   registered-but-unlisted → **skip + warn**. The `#name { … }` CSS block supplies the
   per-source settings. (`loadMapProject` reads `layers`, CartoCSSMapLoader.cpp:86.)
2. Config value plumbing → **typed switch** in `evaluatePropertyValue`/`applyConfig`
   (safer than a generic map across the SWIG bridge).
3. Segmented label placement: the group filter must include a slot-group's label layers so
   `VTLabelPlacementWorker` still culls per group — **verify no cross-group label loss**.
4. Nuti-driven **reorder** unsupported: order is static per style; nuti drives properties
   and visibility only. Documented limitation.
5. Perf: N slots ⇒ N+1 master vt passes/frame over the same GPU tiles. Fine for small N.
   Milestone 6 adds an optional single-pass segmented renderer (toggle, this layer only)
   to A/B compare against the N+1-pass path and choose the default on device.
6. Exact `mvt::Map`/`Layer`/`Style`/`Rule` getter names (`getLayer`, `getStyle`,
   `getRules`, `getMinZoom`, `getFilter`) to be confirmed/added in A.4.
```
