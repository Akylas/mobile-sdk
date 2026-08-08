package com.akylas.cartotest.demo;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.carto.components.LightOptions;
import com.carto.components.Options;
import com.carto.components.SkyOptions;
import com.carto.components.TerrainOptions;
import com.carto.core.MapPos;
import com.carto.core.MapPosVector;
import com.carto.core.MapVec;
import com.carto.core.StringMap;
import com.carto.core.StringVector;
import com.carto.core.Variant;
import com.carto.datasources.ContourTileDataSource;
import com.carto.datasources.GeoJSONVectorTileDataSource;
import com.carto.datasources.HTTPTileDataSource;
import com.carto.datasources.LocalVectorDataSource;
import com.carto.datasources.MBTilesTileDataSource;
import com.carto.datasources.MultiTileDataSource;
import com.carto.datasources.PersistentCacheTileDataSource;
import com.carto.datasources.TileDataSource;
import com.carto.graphics.Color;
import com.carto.layers.CompositeSourceType;
import com.carto.layers.CompositeVectorTileLayer;
import com.carto.layers.CustomRasterTileLayer;
import com.carto.layers.HillshadeMethod;
import com.carto.layers.HillshadeRasterTileLayer;
import com.carto.layers.Layer;
import com.carto.layers.LayerVector;
import com.carto.layers.RasterTileFilterMode;
import com.carto.layers.RasterTileLayer;
import com.carto.layers.TileSubstitutionPolicy;
import com.carto.layers.VectorLayer;
import com.carto.layers.VectorTileEventListener;
import com.carto.layers.VectorTileLayer;
import com.carto.layers.VectorTileRenderOrder;
import com.carto.projections.Projection;
import com.carto.rastertiles.MapBoxElevationDataDecoder;
import com.carto.rastertiles.TerrariumElevationDataDecoder;
import com.carto.rastertiles.ElevationDecoder;
import com.carto.renderers.MapRenderer;
import com.carto.renderers.PostProcessEffect;
import com.carto.styles.CartoCSSStyleSet;
import com.carto.styles.LineStyleBuilder;
import com.carto.styles.MarkerStyleBuilder;
import com.carto.ui.MapView;
import com.carto.ui.VectorTileClickInfo;
import com.carto.vectorelements.Line;
import com.carto.vectorelements.Marker;
import com.carto.vectortiles.MBVectorTileDecoder;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * The whole demo map, as ONE composable configuration instead of a set of separate examples.
 *
 * MODEL
 *  - {@link DemoConfig} says what should exist;
 *  - this class owns the map objects and makes reality match the config: {@link #build()} once,
 *    then {@link #rebuildLayers()} / apply*() whenever something changes;
 *  - the panel (DemoPanel) only writes DemoConfig fields and calls back into here.
 *
 * LAYERS
 *  Each entry of {@link Feature} is an independent layer that can be switched on or off at any
 *  time. They are (re)built lazily and cached in {@link #layers}, and always re-added in
 *  {@link #LAYER_ORDER}, so toggling one never changes the draw order of the others.
 *
 * SOURCES
 *  Tile sources are created ONCE and shared: the DEM feeds the 3D terrain, the hillshade, the
 *  contours and the hypsometric tint at the same time, so a tile is downloaded and decoded once.
 */
public class DemoMap {

    private static final String TAG = "DemoMap";

    /** One switchable layer of the demo. */
    public enum Feature {
        CELESTIAL, STARS, BASE, SATELLITE, HILLSHADE, HYPSO, CONTOUR, CONTOUR_TILES, ROUTES, ROUTE_TEST, ELEMENTS, PEAKS
    }

    /** Bottom -> top draw order. Toggling a layer never reorders the others. */
    private static final Feature[] LAYER_ORDER = {
        // The sky goes FIRST, so the map and the terrain draw over it and a ridge hides what is
        // behind it - which is what a body in the sky should do.
        Feature.CELESTIAL, Feature.STARS,
        Feature.BASE, Feature.SATELLITE, Feature.HILLSHADE, Feature.HYPSO,
        Feature.CONTOUR, Feature.CONTOUR_TILES, Feature.ROUTES, Feature.ROUTE_TEST, Feature.ELEMENTS,
        // Last: the summit names go over everything the map draws.
        Feature.PEAKS
    };

    /** Sun, moon and their daily paths - demo content built on the generic celestial API. */
    public final DemoCelestial celestial = new DemoCelestial();
    /** The bright-star catalogue, the constellation figures and the planets - same API. */
    public final DemoStars stars = new DemoStars();
    /** Device orientation driving the camera, for the star sky mode. */
    private DemoOrientation orientation;
    /** Live camera preview behind the transparent map, for the star sky mode. */
    private DemoCameraPreview cameraPreview;

    private final Context context;
    public final MapView mapView;
    public final String dataPath;

    // --- map objects the panel needs to reach ---------------------------------------------------
    public TerrainOptions terrainOptions;
    public LightOptions lightOptions;
    public SkyOptions skyOptions;
    public HillshadeRasterTileLayer hillshadeLayer;      // stand-alone hillshade layer, when built
    public VectorTileLayer baseLayer;                    // base map layer, whatever the mode
    public CompositeVectorTileLayer compositeLayer;      // same object as baseLayer in COMPOSITE mode
    public MBVectorTileDecoder baseDecoder;              // decoder of the base layer
    public ContourTileDataSource contourSource;          // shared by the layer and the composite slot
    public PostProcessEffect reliefEffect;               // the attached relief outline effect, if any
    // What the peak-finder mode switched off, so leaving it puts the map back as it was.
    private boolean savedLayerBase, savedLayerHillshade, savedLayerContour, savedLayerContourTiles, savedLayerSatellite, savedLayerHypso;
    private float savedTilt;
    private float savedOcclusionTolerance;
    private float savedViewDistance = 1;
    /** Result of the last {@link #checkCompositeSlots()}: which slots the style really has. */
    public String compositeStatus = "";

    private final Map<Feature, Layer> layers = new LinkedHashMap<Feature, Layer>();

    // --- shared, lazily created tile sources -----------------------------------------------------
    private TileDataSource cachedDem;
    private TileDataSource cachedVector;
    private TileDataSource cachedRaster;
    private TileDataSource cachedContourTiles;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean nutiParameterOn = true;

    public DemoMap(Context context, MapView mapView, String dataPath) {
        this.context = context;
        this.mapView = mapView;
        this.dataPath = dataPath;
    }

    // =============================================================================================
    // BUILD
    // =============================================================================================

    /** Applies the whole {@link DemoConfig} to a fresh map. */
    public void build() {
        // Options first: layers created afterwards pick up the terrain/light state immediately.
        mapView.getOptions().setTileThreadPoolSize(DemoConfig.TILE_THREAD_POOL_SIZE);
        mapView.getOptions().setTileLODFactor(DemoConfig.TILE_LOD_FACTOR);
        applyDebugConfig();
        applyTerrainOptions();
        applyLightOptions();
        applySkyOptions();

        rebuildLayers();
        applyCamera();

        if (DemoConfig.RELIEF_OUTLINE) {
            // Attaching a post-process effect before the GL surface exists leaves the offscreen
            // colour buffer unwritten and the screen black, so wait for the first frames.
            handler.postDelayed(new Runnable() {
                public void run() {
                    setReliefOutlineEnabled(true);
                }
            }, (long) DemoConfig.RELIEF_OUTLINE_DELAY_MS);
        }
        if (DemoConfig.PEAK_FINDER) {
            // After the same delay as the effect: attaching it before the GL surface exists
            // leaves the offscreen colour buffer unwritten.
            handler.postDelayed(new Runnable() {
                public void run() { setPeakFinderMode(true); }
            }, (long) DemoConfig.RELIEF_OUTLINE_DELAY_MS);
        }
        if (DemoConfig.DAY_CYCLE) {
            applyDayCycle(DemoConfig.DAY_CYCLE_HOUR);
        }
        if (DemoConfig.STAR_SKY) {
            // Already built without the map layers (isEnabled), so there is nothing to fade out.
            saveMapAppearance();
            enterStarSky();
        }
        startScriptedAnimation();
    }

    // =============================================================================================
    // LAYERS
    // =============================================================================================

    /** True if the feature is currently switched on in the config. */
    public boolean isEnabled(Feature feature) {
        // Star sky mode: every map layer is left OUT of the layer list rather than hidden, so no
        // tile is fetched, decoded or drawn - the mode costs what an empty map costs.
        if (DemoConfig.STAR_SKY && feature != Feature.CELESTIAL && feature != Feature.STARS) {
            return false;
        }
        switch (feature) {
            case CELESTIAL: return DemoConfig.CELESTIAL;
            case STARS: return DemoConfig.STARS;
            case BASE: return DemoConfig.LAYER_BASE;
            case SATELLITE: return DemoConfig.LAYER_SATELLITE;
            case HILLSHADE: return DemoConfig.LAYER_HILLSHADE;
            case HYPSO: return DemoConfig.LAYER_HYPSO;
            case CONTOUR: return DemoConfig.LAYER_CONTOUR;
            case CONTOUR_TILES: return DemoConfig.LAYER_CONTOUR_TILES;
            case ROUTES: return DemoConfig.LAYER_ROUTES;
            case ROUTE_TEST: return DemoConfig.LAYER_ROUTE_TEST;
            case ELEMENTS: return DemoConfig.LAYER_ELEMENTS;
            case PEAKS: return DemoConfig.LAYER_PEAKS;
            default: return false;
        }
    }

    public void setEnabled(Feature feature, boolean enabled) {
        switch (feature) {
            case CELESTIAL: DemoConfig.CELESTIAL = enabled; break;
            case STARS: DemoConfig.STARS = enabled; break;
            case BASE: DemoConfig.LAYER_BASE = enabled; break;
            case SATELLITE: DemoConfig.LAYER_SATELLITE = enabled; break;
            case HILLSHADE: DemoConfig.LAYER_HILLSHADE = enabled; break;
            case HYPSO: DemoConfig.LAYER_HYPSO = enabled; break;
            case CONTOUR: DemoConfig.LAYER_CONTOUR = enabled; break;
            case CONTOUR_TILES: DemoConfig.LAYER_CONTOUR_TILES = enabled; break;
            case ROUTES: DemoConfig.LAYER_ROUTES = enabled; break;
            case ROUTE_TEST: DemoConfig.LAYER_ROUTE_TEST = enabled; break;
            case ELEMENTS: DemoConfig.LAYER_ELEMENTS = enabled; break;
            case PEAKS: DemoConfig.LAYER_PEAKS = enabled; break;
        }
        rebuildLayers();
    }

    /** Drops the cached instance of a layer so the next rebuild constructs it from the config. */
    public void invalidate(Feature feature) {
        layers.remove(feature);
    }

    /**
     * Rebuilds the layer list from the config. Layer objects are cached, so switching one off and
     * on again does not refetch anything; use {@link #invalidate} when a layer's own settings
     * changed in a way that needs a new object (style source, base mode).
     */
    public void rebuildLayers() {
        LayerVector vector = new LayerVector();
        for (Feature feature : LAYER_ORDER) {
            if (!isEnabled(feature)) {
                continue;
            }
            Layer layer = layers.get(feature);
            if (layer == null) {
                layer = createLayer(feature);
                if (layer == null) {
                    continue; // missing data file etc - already logged
                }
                layers.put(feature, layer);
            }
            vector.add(layer);
        }
        mapView.getLayers().setAll(vector);
        // The sky objects are built with their layer, which happens here, so place them now that
        // they exist.
        updateSky();
        mapView.requestRender();
    }

    /** Puts every sky object where it really is for the configured date, hour and position. */
    public void updateSky() {
        celestial.update();
        double n = DemoAstro.daysSinceJ2000(DemoConfig.SUN_YEAR, DemoConfig.SUN_MONTH, DemoConfig.SUN_DAY, DemoConfig.currentHourUtc());
        stars.update(n, DemoConfig.START_LAT, DemoConfig.START_LON);
        mapView.requestRender();
    }

    private Layer createLayer(Feature feature) {
        switch (feature) {
            case CELESTIAL: return celestial.createLayer(mapView);
            case STARS: return stars.createLayer(mapView);
            case BASE: return createBaseLayer();
            case SATELLITE: return new RasterTileLayer(rasterSource());
            case HILLSHADE: return createHillshadeLayer();
            case HYPSO: return createHypsoLayer();
            case CONTOUR: return createContourLayer();
            case CONTOUR_TILES: return createContourTilesLayer();
            case ROUTES: return createRoutesLayer();
            case ROUTE_TEST: return createRouteTestLayer();
            case ELEMENTS: return createElementsLayer();
            case PEAKS: return createPeaksLayer();
            default: return null;
        }
    }

    // --- base map --------------------------------------------------------------------------------

    /**
     * The base map: either a plain VectorTileLayer or a CompositeVectorTileLayer that weaves
     * hillshade / satellite / contour sources into the style's own layer order.
     */
    private Layer createBaseLayer() {
        baseDecoder = DemoStyles.create(DemoConfig.STYLE_SOURCE, dataPath);
        // see BASE_TILE_CACHE_MB: the SDK default (10MB) is what makes a zoom step blank the map
        if (DemoConfig.BASE_MODE == DemoConfig.BaseMode.PLAIN) {
            compositeLayer = null;
            baseLayer = new VectorTileLayer(vectorSource(), baseDecoder);
            baseLayer.setTileCacheCapacity(DemoConfig.BASE_TILE_CACHE_MB * 1024L * 1024L);
            return baseLayer;
        }

        CompositeVectorTileLayer layer = new CompositeVectorTileLayer(vectorSource(), baseDecoder);
        layer.setLabelRenderOrder(VectorTileRenderOrder.VECTOR_TILE_RENDER_ORDER_LAST);
        layer.setSinglePassRenderingEnabled(DemoConfig.COMPOSITE_SINGLE_PASS);
        compositeLayer = layer;
        baseLayer = layer;
        layer.setTileCacheCapacity(DemoConfig.BASE_TILE_CACHE_MB * 1024L * 1024L);
        syncCompositeSources();
        if (DemoConfig.STYLE_SOURCE == DemoConfig.StyleSource.NUTI && DemoConfig.NUTI_TOGGLE_INTERVAL_MS > 0) {
            startNutiToggleLoop();
        }
        return layer;
    }

    /** Adds/removes the composite slots to match the config. Safe to call at any time. */
    public void syncCompositeSources() {
        if (compositeLayer == null) {
            return;
        }
        // hillshade: the elevation decoder is resolved from the source 'encoding' metadata.
        if (DemoConfig.COMPOSITE_HILLSHADE) {
            compositeLayer.addExternalDataSource("hillshade", demSource(), CompositeSourceType.COMPOSITE_SOURCE_TYPE_HILLSHADE);
            if (DemoConfig.COMPOSITE_HILLSHADE_ZOOM_BIAS != 0) {
                compositeLayer.setExternalDataSourceZoomLevelBias("hillshade", DemoConfig.COMPOSITE_HILLSHADE_ZOOM_BIAS);
            }
        } else {
            compositeLayer.removeExternalDataSource("hillshade");
        }
        // satellite: a raster source drawn at the '#satellite' slot with the style's opacity.
        if (DemoConfig.COMPOSITE_SATELLITE) {
            compositeLayer.addExternalDataSource("satellite", rasterSource(), CompositeSourceType.COMPOSITE_SOURCE_TYPE_RASTER);
        } else {
            compositeLayer.removeExternalDataSource("satellite");
        }
        // contour: merged INTO the master source, styled by the '#contour' rules.
        if (DemoConfig.COMPOSITE_CONTOUR) {
            compositeLayer.addVectorDataSource("contour", contourSource());
        } else {
            compositeLayer.removeExternalDataSource("contour");
        }
        checkCompositeSlots();
        mapView.requestRender();
    }

    /**
     * WHY A COMPOSITE SLOT SILENTLY DOES NOTHING - the check to run first.
     *
     * A slot is the position of a style layer with the source's name. If the style does not
     * DECLARE a layer called 'hillshade' / 'satellite' / 'contour' (project.json "layers", or the
     * Layer elements of a Mapnik XML style), the source has nowhere to be drawn and the SDK only
     * warns in the log. The osm/alpimaps style, for instance, declares 'contour' but neither
     * 'hillshade' nor 'satellite', so those two slots do nothing until the style adds them.
     *
     * The result goes to the log AND to {@link #compositeStatus}, which the panel shows.
     */
    public void checkCompositeSlots() {
        if (compositeLayer == null || baseDecoder == null) {
            compositeStatus = "";
            return;
        }
        StringVector styleLayers = baseDecoder.getStyleLayerNames();
        java.util.List<String> declared = new java.util.ArrayList<String>();
        for (int i = 0; i < styleLayers.size(); i++) {
            declared.add(styleLayers.get(i));
        }

        StringVector registered = compositeLayer.getExternalDataSourceNames();
        StringBuilder status = new StringBuilder();
        boolean missing = false;
        for (int i = 0; i < registered.size(); i++) {
            String name = registered.get(i);
            boolean ok = declared.contains(name);
            missing |= !ok;
            status.append(status.length() > 0 ? ", " : "").append(name).append(ok ? " OK" : " MISSING in style");
        }
        compositeStatus = status.length() > 0 ? "slots: " + status : "slots: none";
        Log.i(TAG, compositeStatus + " | style layers: " + declared);
        if (missing) {
            // A COMPILED Mapnik XML style (what a packaged osm.zip / osm folder usually contains)
            // cannot declare these slots at all: the XML symbolizer set has no hillshade/raster
            // config symbolizer, only CartoCSS has. Either ship the style as a CartoCSS PROJECT
            // (project.json "layers" + '#hillshade { hillshade-... }' in the .mss, which
            // DirAssetPackage reads straight from the folder), or use the inline style to test.
            Log.w(TAG, "a slot is missing: the style declares no layer with that name. Compiled "
                    + "Mapnik XML styles cannot declare hillshade/satellite slots - use a CartoCSS "
                    + "project style (project.json + .mss) or switch the panel style to 'inline'");
        }
    }

    /** Rebuilds the base layer: needed after a style-source or base-mode change. */
    public void rebuildBaseLayer() {
        invalidate(Feature.BASE);
        rebuildLayers();
    }

    /**
     * The peak-finder view, in one switch. The pieces are independent SDK features, but each one
     * on its own looks like nothing happens: the shaded surface only shows where NO tile layer
     * paints, and summit names need a view that has summits in it - which a top-down city camera
     * has not. So the mode turns the map layers off, the relief and the names on, and tilts the
     * camera to a panorama (in this SDK tilt 90 is straight down).
     */
    public void setPeakFinderMode(boolean enabled) {
        DemoConfig.PEAK_FINDER = enabled;
        if (enabled) {
            savedLayerBase = DemoConfig.LAYER_BASE;
            savedLayerHillshade = DemoConfig.LAYER_HILLSHADE;
            savedLayerContour = DemoConfig.LAYER_CONTOUR;
            savedLayerContourTiles = DemoConfig.LAYER_CONTOUR_TILES;
            savedLayerSatellite = DemoConfig.LAYER_SATELLITE;
            savedLayerHypso = DemoConfig.LAYER_HYPSO;
            savedTilt = mapView.getTilt();
            DemoConfig.LAYER_BASE = false;
            DemoConfig.LAYER_HILLSHADE = false;
            DemoConfig.LAYER_CONTOUR = false;
            DemoConfig.LAYER_CONTOUR_TILES = false;
            DemoConfig.LAYER_SATELLITE = false;
            DemoConfig.LAYER_HYPSO = false;
            DemoConfig.LAYER_PEAKS = true;
            DemoConfig.RELIEF_SURFACE = true;
            savedOcclusionTolerance = DemoConfig.TERRAIN_OCCLUSION_TOLERANCE;
            // A summit sitting ON a ridge, or a metre behind it, is exactly what the view is for,
            // so the label occlusion is deliberately generous here.
            DemoConfig.TERRAIN_OCCLUSION_TOLERANCE = DemoConfig.PEAK_FINDER_OCCLUSION_TOLERANCE;
            // And a panorama wants the far ranges: tangram's rule stops the ground a few kilometres
            // out, which is most of what the view is about (see TerrainOptions.ViewDistanceFactor).
            savedViewDistance = DemoConfig.VIEW_DISTANCE_FACTOR;
            DemoConfig.VIEW_DISTANCE_FACTOR = DemoConfig.PEAK_FINDER_VIEW_DISTANCE;
            applyTerrainOptions();
            rebuildLayers();
            applyReliefSurface();
            setReliefOutlineEnabled(true);
            mapView.setTilt(DemoConfig.PEAK_FINDER_TILT, 0.6f);
            DemoPanel.setElevationWidgetVisible(true);
        } else {
            DemoConfig.LAYER_BASE = savedLayerBase;
            DemoConfig.LAYER_HILLSHADE = savedLayerHillshade;
            DemoConfig.LAYER_CONTOUR = savedLayerContour;
            DemoConfig.LAYER_CONTOUR_TILES = savedLayerContourTiles;
            DemoConfig.LAYER_SATELLITE = savedLayerSatellite;
            DemoConfig.LAYER_HYPSO = savedLayerHypso;
            DemoConfig.LAYER_PEAKS = false;
            DemoConfig.RELIEF_SURFACE = false;
            DemoConfig.TERRAIN_OCCLUSION_TOLERANCE = savedOcclusionTolerance;
            DemoConfig.VIEW_DISTANCE_FACTOR = savedViewDistance;
            DemoConfig.PEAK_FINDER_ELEVATION = 0;
            applyTerrainOptions();
            applyViewpointElevation();
            rebuildLayers();
            applyReliefSurface();
            setReliefOutlineEnabled(false);
            if (savedTilt > 0) {
                mapView.setTilt(savedTilt, 0.6f);
            }
            DemoPanel.setElevationWidgetVisible(false);
        }
        mapView.requestRender();
    }

    /**
     * Lifts the viewpoint by {@link DemoConfig#PEAK_FINDER_ELEVATION} metres. The focus position
     * carries a height and the camera rides on it, so raising the focus raises the eye - which is
     * what a peak-finder view wants: see over the ridge in front of you.
     * The z of a MapPos is in INTERNAL units, and one metre is worth more of them the further from
     * the equator (mercator), hence the latitude term.
     */
    public void applyViewpointElevation() {
        Projection proj = mapView.getOptions().getBaseProjection();
        MapPos wgs = proj.toWgs84(mapView.getFocusPos());
        double groundElevation = 0;
        if (terrainOptions != null) {
            double sample = terrainOptions.getElevation(wgs);
            if (sample > -100000) {
                groundElevation = sample;
            }
        }
        // The projection converts metres to internal units itself (toInternal scales z with x/y),
        // so this stays in METRES - with the mercator stretch the terrain heights also carry, or
        // the viewpoint would sit lower than the mountains it is measured against.
        double meters = (groundElevation + DemoConfig.PEAK_FINDER_ELEVATION) / Math.cos(Math.toRadians(wgs.getY()));
        mapView.setFocusPos(proj.fromWgs84(new MapPos(wgs.getX(), wgs.getY(), meters)), 0.3f);
        mapView.requestRender();
    }

    /** The peak labels are style-driven, so every callout knob needs a new decoder. */
    public void rebuildPeaksLayer() {
        invalidate(Feature.PEAKS);
        rebuildLayers();
    }

    // --- hillshade -------------------------------------------------------------------------------

    private Layer createHillshadeLayer() {
        HillshadeRasterTileLayer layer = new HillshadeRasterTileLayer(demSource(), elevationDecoder());
        layer.setPreloading(true);
        layer.setTileSubstitutionPolicy(TileSubstitutionPolicy.TILE_SUBSTITUTION_POLICY_VISIBLE);
        layer.setTileFilterMode(RasterTileFilterMode.RASTER_TILE_FILTER_MODE_BILINEAR);
        hillshadeLayer = layer;
        applyHillshadeConfig();
        return layer;
    }

    /** Pushes every HILLSHADE_* config value onto the stand-alone hillshade layer. */
    /** Debug overlays (DEBUG section of the panel). */
    public void applyDebugConfig() {
        mapView.getOptions().setDebugTileBorders(DemoConfig.DEBUG_TILE_BORDERS);
    }

    public void applyHillshadeConfig() {
        HillshadeRasterTileLayer layer = hillshadeLayer;
        if (layer == null) {
            return;
        }
        layer.setHillshadeMethod(hillshadeMethod());
        layer.setContrast(DemoConfig.HILLSHADE_CONTRAST);
        layer.setHeightScale(DemoConfig.HILLSHADE_HEIGHT_SCALE);
        layer.setExaggeration(DemoConfig.HILLSHADE_EXAGGERATION);
        layer.setIlluminationMapRotationEnabled(DemoConfig.HILLSHADE_ILLUMINATION_FOLLOWS_MAP);
        // The illumination direction is a vector; the panel/config express it in degrees.
        double rad = Math.toRadians(DemoConfig.HILLSHADE_ILLUMINATION_DEGREES);
        layer.setIlluminationDirection(new MapVec(Math.sin(rad), Math.cos(rad), layer.getIlluminationDirection().getZ()));
        layer.setShadowColor(color(DemoConfig.HILLSHADE_SHADOW_COLOR_ARGB));
        layer.setHighlightColor(color(DemoConfig.HILLSHADE_HIGHLIGHT_COLOR_ARGB));
        layer.setAccentColor(color(DemoConfig.HILLSHADE_ACCENT_COLOR_ARGB));

        // GPU contour lines drawn in the hillshade pass: the normal map then encodes absolute
        // elevation, and the fragment shader draws anti-aliased lines from it. Unlike the geometry
        // contours below, these are per-fragment at a fixed metre interval (no labels, no CartoCSS).
        layer.setContourEnabled(DemoConfig.HILLSHADE_CONTOUR_LINES);
        layer.setContourInterval(DemoConfig.HILLSHADE_CONTOUR_INTERVAL);
        layer.setContourWidth(DemoConfig.HILLSHADE_CONTOUR_WIDTH);
        layer.setContourColor(color(DemoConfig.HILLSHADE_CONTOUR_COLOR_ARGB));

        // Slope colouring replaces the lighting shader entirely.
        layer.setExagerateHeightScaleEnabled(!DemoConfig.HILLSHADE_SLOPES_SHADER);
        layer.setNormalMapLightingShader(DemoConfig.HILLSHADE_SLOPES_SHADER ? DemoStyles.slopesShader() : "");
        mapView.requestRender();
    }

    private HillshadeMethod hillshadeMethod() {
        try {
            return HillshadeMethod.valueOf(DemoConfig.HILLSHADE_METHOD);
        } catch (Exception e) {
            return HillshadeMethod.IGOR;
        }
    }

    // --- other layers ----------------------------------------------------------------------------

    /** CustomRasterTileLayer: any filter shader over any raster source (here: hypsometric tint). */
    private Layer createHypsoLayer() {
        CustomRasterTileLayer layer = new CustomRasterTileLayer(demSource());
        layer.setShaderSource(DemoStyles.hypsometricShader());
        return layer;
    }

    /**
     * Stand-alone contour layer: vector tiles GENERATED on the fly from the shared DEM, styled by
     * its own CartoCSS. This is the geometry path (labels possible, styled per 'div'), as opposed
     * to the shader contours of the hillshade layer.
     */
    private Layer createContourLayer() {
        MBVectorTileDecoder decoder = new MBVectorTileDecoder(new CartoCSSStyleSet(DemoStyles.contourStyle()));
        VectorTileLayer layer = new VectorTileLayer(contourSource(), decoder);
        // A stand-in tile of another zoom carries a coarser grid AND a coarser interval, so while the
        // right tile loads the map shows angular chords instead of contours. "none" trades that for
        // empty space until the tile is there.
        layer.setMaxStandInLevel(DemoConfig.CONTOUR_MAX_OVERZOOM_STANDIN);
        layer.setTileCacheCapacity(DemoConfig.CONTOUR_TILE_CACHE_MB * 1024L * 1024L);
        if ("none".equalsIgnoreCase(DemoConfig.CONTOUR_TILE_SUBSTITUTION)) {
            layer.setTileSubstitutionPolicy(TileSubstitutionPolicy.TILE_SUBSTITUTION_POLICY_NONE);
        } else if ("visible".equalsIgnoreCase(DemoConfig.CONTOUR_TILE_SUBSTITUTION)) {
            layer.setTileSubstitutionPolicy(TileSubstitutionPolicy.TILE_SUBSTITUTION_POLICY_VISIBLE);
        }
        return layer;
    }

    /**
     * PRE-BAKED contour tiles over HTTP, styled with the '#contour' rules of the real style
     * (shared/terrain.less, variables inlined). This is the A/B reference: same rules, same
     * 'ele'/'div' attributes, but geometry baked at zoom 11..14 instead of traced from the DEM.
     */
    private Layer createContourTilesLayer() {
        MBVectorTileDecoder decoder = new MBVectorTileDecoder(new CartoCSSStyleSet(DemoStyles.contourTilesStyle()));
        return new VectorTileLayer(contourTilesSource(), decoder);
    }

    /** Offline vector tiles + packaged style; skipped (with a log) when the files are missing. */
    private Layer createRoutesLayer() {
        try {
            MBTilesTileDataSource source = new MBTilesTileDataSource(dataPath + "/" + DemoConfig.ROUTES_MBTILES_NAME);
            MBVectorTileDecoder decoder = DemoStyles.createZipDecoder(dataPath + "/" + DemoConfig.ROUTES_STYLE_ZIP_NAME);
            if (decoder == null) {
                return null;
            }
            MultiTileDataSource multi = new MultiTileDataSource();
            multi.add(source);
            return new VectorTileLayer(multi, decoder);
        } catch (Exception e) {
            Log.w(TAG, "routes layer unavailable: " + e.getMessage());
            return null;
        }
    }

    /**
     * The LINE JOIN bench: a synthetic mountain road served as GeoJSON vector tiles and styled with
     * CartoCSS, so it takes the SAME tesselator, shaders and drape path as the base map's roads - a
     * Line vector element would not (LineDrawData is a second, independent tesselator).
     *
     * Casing + fill, as a navigation app draws a route. What to look at, zooming OUT: the outside
     * of a sharp turn (miter needles), the inside of a turn with line-opacity below 1 (the join
     * blends twice where the triangles overlap) and the switchback ends (cap / split joins).
     */
    private Layer createRouteTestLayer() {
        String geoJSON = readRouteTestGeoJSON();
        if (geoJSON == null) {
            return null; // already logged
        }
        MBVectorTileDecoder decoder = new MBVectorTileDecoder(new CartoCSSStyleSet(DemoStyles.routeTestStyle()));
        GeoJSONVectorTileDataSource source = new GeoJSONVectorTileDataSource(0, 24);
        source.setSimplifyTolerance(DemoConfig.ROUTE_TEST_SIMPLIFY);
        try {
            int layerIndex = source.createLayer("route");
            source.addGeoJSONStringFeature(layerIndex, geoJSON);
        } catch (IOException e) {
            Log.w(TAG, "route test geojson rejected: " + e.getMessage());
            return null;
        }
        return new VectorTileLayer(source, decoder);
    }

    /**
     * A real Valhalla route (1200 points around Grenoble): dense source geometry, switchbacks,
     * roundabouts and a handful of near-reversals - which is what a join has to survive, and what
     * a synthetic test line does not reproduce.
     *
     * The DATA DIRECTORY wins over the APK asset, so another route can be tried without rebuilding:
     *   adb push my-route.geojson /sdcard/alpimaps_mbtiles/route-test.geojson
     */
    private String readRouteTestGeoJSON() {
        String name = DemoConfig.ROUTE_TEST_GEOJSON_NAME;
        File file = new File(dataPath + "/" + name);
        if (file.exists()) {
            try {
                return readStream(new FileInputStream(file));
            } catch (Exception e) {
                Log.w(TAG, "route test geojson not readable (" + file + "): " + e.getMessage());
            }
        }
        try {
            return readStream(context.getAssets().open(name));
        } catch (Exception e) {
            Log.w(TAG, "route test asset unavailable (" + name + "): " + e.getMessage());
            return null;
        }
    }

    private static String readStream(InputStream stream) throws IOException {
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] chunk = new byte[16384];
            int read;
            while ((read = stream.read(chunk)) > 0) {
                out.write(chunk, 0, read);
            }
            return out.toString("UTF-8");
        } finally {
            stream.close();
        }
    }

    /**
     * Summit names as callout labels: their own vector tile layer on the base source, with a
     * peaks-only style (see DemoStyles.peaksStyle). Clicking one reports it through the standard
     * vector tile click path - a callout label is picked where it is DRAWN, at the end of its
     * leader line.
     */
    private Layer createPeaksLayer() {
        VectorTileLayer layer = new VectorTileLayer(vectorSource(), new MBVectorTileDecoder(new CartoCSSStyleSet(DemoStyles.peaksStyle())));
        layer.setLabelRenderOrder(VectorTileRenderOrder.VECTOR_TILE_RENDER_ORDER_LAST);
        // Out of the post-process pass: the relief effect reads the terrain depth but paints over
        // the whole frame, so a ridge line drawn after the names crosses them. Opting the layer out
        // draws it AFTER the effect has resolved (Layer.setPostProcessed), which is also what keeps
        // the glyphs crisp - they are not resampled by the effect's half-resolution buffer.
        layer.setPostProcessed(false);
        layer.setVectorTileEventListener(new VectorTileEventListener() {
            @Override
            public boolean onVectorTileClicked(VectorTileClickInfo clickInfo) {
                Variant properties = clickInfo.getFeature().getProperties();
                final String name = properties.getObjectElement("name").getString()
                        + " " + properties.getObjectElement("ele").toString() + " m";
                Log.i(TAG, "peak clicked: " + name);
                mapView.post(new Runnable() {
                    public void run() { android.widget.Toast.makeText(context, name, android.widget.Toast.LENGTH_SHORT).show(); }
                });
                return true;
            }
        });
        return layer;
    }

    /**
     * Test elements draped on the terrain: markers on summits (billboard occlusion test - orbit
     * around a ridge and watch them disappear) and a line crossing the Isere valley (drape-lines
     * and element-vs-terrain depth test).
     */
    private Layer createElementsLayer() {
        Projection proj = mapView.getOptions().getBaseProjection();
        LocalVectorDataSource source = new LocalVectorDataSource(proj);

        MarkerStyleBuilder markerStyle = new MarkerStyleBuilder();
        markerStyle.setSize(24);
        markerStyle.setColor(new Color((short) 255, (short) 0, (short) 0, (short) 255));
        double[][] peaks = {
                { 5.7869, 45.2876 }, // Chamechaude
                { 5.9207, 45.2989 }, // Dent de Crolles
                { 5.5433, 45.1861 }, // Le Moucherotte
                { 5.7247, 45.1988 }, // Bastille above Grenoble
        };
        for (double[] p : peaks) {
            source.add(new Marker(proj.fromWgs84(new MapPos(p[0], p[1])), markerStyle.buildStyle()));
        }

        LineStyleBuilder lineStyle = new LineStyleBuilder();
        lineStyle.setWidth(8);
        lineStyle.setColor(new Color((short) 0, (short) 90, (short) 255, (short) 255));
        MapPosVector linePoses = new MapPosVector();
        linePoses.add(proj.fromWgs84(new MapPos(5.6800, 45.1600)));
        linePoses.add(proj.fromWgs84(new MapPos(5.7247, 45.1927)));
        linePoses.add(proj.fromWgs84(new MapPos(5.7869, 45.2876)));
        source.add(new Line(linePoses, lineStyle.buildStyle()));

        return new VectorLayer(source);
    }

    // =============================================================================================
    // SHARED TILE SOURCES (created once, used by several layers)
    // =============================================================================================

    /** Master vector tiles of the base map, persistently cached. */
    public TileDataSource vectorSource() {
        if (cachedVector == null) {
            HTTPTileDataSource source = new HTTPTileDataSource(DemoConfig.VECTOR_MIN_ZOOM, DemoConfig.VECTOR_MAX_ZOOM, DemoConfig.VECTOR_URL);
            source.setHTTPHeaders(userAgentHeaders());
            cachedVector = new PersistentCacheTileDataSource(source, cacheDbPath(DemoConfig.VECTOR_CACHE_DB));
        }
        return cachedVector;
    }

    /**
     * The elevation source. EVERYTHING elevation-related uses this one instance (3D terrain,
     * hillshade, contours, hypsometric tint), so each DEM tile is fetched and decoded once.
     */
    public TileDataSource demSource() {
        if (cachedDem == null) {
            HTTPTileDataSource source = new HTTPTileDataSource(DemoConfig.DEM_MIN_ZOOM, DemoConfig.DEM_MAX_ZOOM, DemoConfig.DEM_URL);
            source.setEncoding(DemoConfig.DEM_ENCODING);
            cachedDem = new PersistentCacheTileDataSource(source, cacheDbPath(DemoConfig.DEM_CACHE_DB));
        }
        return cachedDem;
    }

    public TileDataSource rasterSource() {
        if (cachedRaster == null) {
            HTTPTileDataSource source = new HTTPTileDataSource(DemoConfig.RASTER_MIN_ZOOM, DemoConfig.RASTER_MAX_ZOOM, DemoConfig.RASTER_URL);
            source.setHTTPHeaders(userAgentHeaders());
            cachedRaster = new PersistentCacheTileDataSource(source, cacheDbPath(DemoConfig.RASTER_CACHE_DB));
        }
        return cachedRaster;
    }

    /** Pre-baked contour vector tiles, persistently cached (Feature.CONTOUR_TILES only). */
    public TileDataSource contourTilesSource() {
        if (cachedContourTiles == null) {
            HTTPTileDataSource source = new HTTPTileDataSource(DemoConfig.CONTOUR_TILES_MIN_ZOOM, DemoConfig.CONTOUR_TILES_MAX_ZOOM, DemoConfig.CONTOUR_TILES_URL);
            source.setHTTPHeaders(userAgentHeaders());
            cachedContourTiles = new PersistentCacheTileDataSource(source, cacheDbPath(DemoConfig.CONTOUR_TILES_CACHE_DB));
        }
        return cachedContourTiles;
    }

    /** Contours generated from the shared DEM; the same instance feeds layer and composite slot. */
    public ContourTileDataSource contourSource() {
        if (contourSource == null) {
            contourSource = new ContourTileDataSource(demSource());
            contourSource.setEncoding(DemoConfig.DEM_ENCODING);
            applyContourConfig();
        }
        return contourSource;
    }

    /** Pushes every CONTOUR_* config value onto the shared contour source. */
    public void applyContourConfig() {
        if (contourSource == null) {
            return;
        }
        contourSource.setBaseInterval(DemoConfig.CONTOUR_BASE_INTERVAL);
        // Perf knobs: the DEM is subsampled to at most 'resolution' samples/side before tracing,
        // and geometry is simplified by 'simplifyTolerance' tile pixels.
        contourSource.setResolution(DemoConfig.CONTOUR_RESOLUTION);
        contourSource.setSimplifyTolerance(DemoConfig.CONTOUR_SIMPLIFY_TOLERANCE);
        // Fetch neighbour DEM tiles so lines meet across tile boundaries (removes seams).
        contourSource.setSeamlessEdgesEnabled(DemoConfig.CONTOUR_SEAMLESS_EDGES);
        // NOTE: in CartoCSS 'zoom' is the TILE zoom. Contour tiles are generated at the DEM zoom,
        // so per-zoom style rules only fire if the DEM source max zoom is high enough.
        contourSource.setMinVisibleZoom(DemoConfig.CONTOUR_MIN_VISIBLE_ZOOM);
        // Per-zoom detail: how fine the traced interval is, and how big the tracing grid is. Both
        // are cost knobs and both must match what the style actually draws - see contourWidthByDiv.
        applyContourLadders();
        contourSource.setMaxOverzoomLevel(DemoConfig.CONTOUR_MAX_OVERZOOM);
        // Labels only: the lines come from the hillshade shader, so the tile carries a handful of
        // stubs to lay the text along instead of the traced geometry.
        contourSource.setLabelStubsEnabled(DemoConfig.CONTOUR_LABEL_STUBS);
        contourSource.setLabelInterval(DemoConfig.CONTOUR_LABEL_INTERVAL);
        // Stubs off the terrain's own elevation: no DEM tile of the contour source's own to fetch
        // and decode, which is tangram's arrangement. Same DEM source on both sides, so the labels
        // state the heights the terrain draws.
        contourSource.setTerrainOptions(DemoConfig.CONTOUR_STUBS_FROM_TERRAIN ? terrainOptions : null);
        mapView.requestRender();
    }

    /** "maxZoom:value,..." rungs onto the source. -1 as a zoom means every zoom above the others. */
    private void applyContourLadders() {
        if (!DemoConfig.CONTOUR_INTERVAL_LADDER.isEmpty()) {
            contourSource.clearIntervalMultipliers();
            for (String rung : DemoConfig.CONTOUR_INTERVAL_LADDER.split(",")) {
                String[] parts = rung.split(":");
                if (parts.length == 2) {
                    contourSource.setIntervalMultiplier(Integer.parseInt(parts[0].trim()), Float.parseFloat(parts[1].trim()));
                }
            }
        }
        if (!DemoConfig.CONTOUR_RESOLUTION_LADDER.isEmpty()) {
            contourSource.clearResolutionsForZoom();
            for (String rung : DemoConfig.CONTOUR_RESOLUTION_LADDER.split(",")) {
                String[] parts = rung.split(":");
                if (parts.length == 2) {
                    contourSource.setResolutionForZoom(Integer.parseInt(parts[0].trim()), Integer.parseInt(parts[1].trim()));
                }
            }
        }
    }

    private ElevationDecoder elevationDecoder() {
        return "mapbox".equals(DemoConfig.DEM_ENCODING)
                ? new MapBoxElevationDataDecoder()
                : new TerrariumElevationDataDecoder();
    }

    private StringMap userAgentHeaders() {
        StringMap headers = new StringMap();
        headers.set("User-Agent", DemoConfig.HTTP_USER_AGENT);
        return headers;
    }

    private String cacheDbPath(String name) {
        return context.getExternalFilesDir(null) + "/" + name;
    }

    // =============================================================================================
    // OPTIONS: TERRAIN / LIGHT / SKY
    // =============================================================================================

    /** Creates the TerrainOptions on first call, then pushes every terrain value onto it. */
    public void applyTerrainOptions() {
        if (terrainOptions == null) {
            terrainOptions = new TerrainOptions(demSource(), elevationDecoder());
            mapView.getOptions().setTerrainOptions(terrainOptions);
        }
        terrainOptions.setEnabled(DemoConfig.TERRAIN_ENABLED);
        terrainOptions.setExaggeration(DemoConfig.TERRAIN_EXAGGERATION);
        terrainOptions.setMeshResolution(DemoConfig.TERRAIN_MESH_RESOLUTION);
        terrainOptions.setCameraClearance(DemoConfig.TERRAIN_CAMERA_CLEARANCE);
        terrainOptions.setPainterOrderDepthEnabled(DemoConfig.TERRAIN_PAINTER_ORDER_DEPTH);
        terrainOptions.setDrapeFillsEnabled(DemoConfig.TERRAIN_DRAPE_FILLS);
        terrainOptions.setDrapeLinesEnabled(DemoConfig.TERRAIN_DRAPE_LINES);
        terrainOptions.setDrapeResolution(DemoConfig.TERRAIN_DRAPE_RESOLUTION);
        terrainOptions.setTileEdgeStitchingEnabled(DemoConfig.TERRAIN_TILE_EDGE_STITCHING);
        terrainOptions.setSeamlessTileEdgesEnabled(DemoConfig.TERRAIN_SEAMLESS_TILE_EDGES);
        terrainOptions.setElevationPrefetchEnabled(DemoConfig.TERRAIN_ELEVATION_PREFETCH);
        terrainOptions.setBillboardOcclusionEnabled(DemoConfig.TERRAIN_BILLBOARD_OCCLUSION);
        terrainOptions.setBillboardOcclusionTolerance(DemoConfig.TERRAIN_OCCLUSION_TOLERANCE);
        terrainOptions.setBackgroundBitmapEnabled(DemoConfig.TERRAIN_BACKGROUND_BITMAP);
        if (DemoConfig.TERRAIN_MAX_TILE_ZOOM_OFFSET_ENABLED) {
            terrainOptions.setMaxTileZoomOffset(DemoConfig.TERRAIN_MAX_TILE_ZOOM_OFFSET);
        }
        // Fog and view distance go together: the distance ENDS the ground, the fog is what makes
        // it fade out instead of being cut off.
        terrainOptions.setFogColor(new Color(DemoConfig.FOG_ENABLED ? DemoConfig.FOG_COLOR_ARGB : 0));
        terrainOptions.setFogStartDistance(DemoConfig.FOG_START_DISTANCE);
        terrainOptions.setFogDistance(DemoConfig.FOG_DISTANCE);
        terrainOptions.setViewDistanceFactor(DemoConfig.VIEW_DISTANCE_FACTOR);
        terrainOptions.setViewDistance(DemoConfig.VIEW_DISTANCE_METERS);
        terrainOptions.setMaxTileZoomCoarsening(DemoConfig.TERRAIN_MAX_TILE_ZOOM_COARSENING);
        applyReliefSurface();
        mapView.requestRender();
    }

    /**
     * The shaded terrain surface of the relief look. The surface shader replaces the terrain
     * background fill, so it is what shows wherever no tile layer paints - switch the base map
     * off to see it.
     */
    public void applyReliefSurface() {
        if (terrainOptions == null) {
            return;
        }
        terrainOptions.setSurfaceShaderSource(DemoConfig.RELIEF_SURFACE ? DemoStyles.reliefSurfaceShader() : "");
        terrainOptions.setSurfaceColorParameter("uPaperColor", color(DemoConfig.RELIEF_DARK ? 0xff10131a : 0xfff7f7f4));
        terrainOptions.setSurfaceColorParameter("uShadeColor", color(DemoConfig.RELIEF_DARK ? 0xff5a6070 : 0xff6c7280));
        terrainOptions.setSurfaceParameter("uShadeStrength", DemoConfig.RELIEF_SHADE_STRENGTH);
        terrainOptions.setSurfaceParameter("uAmbient", DemoConfig.RELIEF_AMBIENT);
        terrainOptions.setSurfaceParameter("uHaze", DemoConfig.RELIEF_HAZE);
        terrainOptions.setSurfaceParameter("uHazeDistance", DemoConfig.RELIEF_HAZE_DISTANCE);
        mapView.requestRender();
    }

    /** Creates the LightOptions on first call, then pushes every sun/shadow value onto it. */
    public void applyLightOptions() {
        if (lightOptions == null) {
            lightOptions = new LightOptions();
            mapView.getOptions().setLightOptions(lightOptions);
        }
        if (DemoConfig.SUN_HOUR_UTC >= 0) {
            int hour = (int) DemoConfig.SUN_HOUR_UTC;
            int minute = (int) ((DemoConfig.SUN_HOUR_UTC - hour) * 60);
            lightOptions.setSunPositionFromTime(DemoConfig.SUN_YEAR, DemoConfig.SUN_MONTH, DemoConfig.SUN_DAY,
                    hour, minute, DemoConfig.START_LAT, DemoConfig.START_LON);
        } else {
            lightOptions.setSunAzimuth(DemoConfig.SUN_AZIMUTH);
            lightOptions.setSunAltitude(DemoConfig.SUN_ALTITUDE);
        }
        lightOptions.setSunIntensity(DemoConfig.SUN_INTENSITY);
        lightOptions.setAmbientIntensity(DemoConfig.AMBIENT_INTENSITY);
        lightOptions.setTerrainLightingEnabled(DemoConfig.TERRAIN_LIGHTING);
        lightOptions.setShadowStrength(DemoConfig.SHADOW_STRENGTH);
        lightOptions.setShadowSoftness(DemoConfig.SHADOW_SOFTNESS);
        lightOptions.setShadowMapSize(DemoConfig.SHADOW_MAP_SIZE);
        lightOptions.setShadowCascades(DemoConfig.SHADOW_CASCADES);
        lightOptions.setShadowBias(DemoConfig.SHADOW_BIAS);
        lightOptions.setShadowDistance(DemoConfig.SHADOW_DISTANCE);
        lightOptions.setShadowCasterMargin(DemoConfig.SHADOW_CASTER_MARGIN);
        updateSky();
    }

    /** The sky is always attached so the panel can toggle it live; disabled = no sky at all. */
    public void applySkyOptions() {
        if (skyOptions == null) {
            skyOptions = new SkyOptions();
            mapView.getOptions().setSkyOptions(skyOptions);
        }
        skyOptions.setEnabled(DemoConfig.SKY_ENABLED);
        // How much of the sky the terrain haze takes: FogBlend is the fade width, FogHorizon the
        // angle it is still at full strength at (negative = from the terrain, 0 = from the horizon).
        skyOptions.setFogBlend(DemoConfig.SKY_FOG_BLEND);
        skyOptions.setFogHorizon(DemoConfig.SKY_FOG_HORIZON);
        mapView.requestRender();
    }

    /**
     * Day cycle: one hour value drives sun position, sky colours, shadow strength and a generated
     * sky shader. Turning it off restores the plain sky.
     */
    public void applyDayCycle(float hourUtc) {
        DemoConfig.DAY_CYCLE_HOUR = hourUtc;
        updateSky(); // the hour is also what places the sun, the moon and the stars
        if (!DemoConfig.DAY_CYCLE) {
            if (skyOptions != null) {
                skyOptions.setShaderSource("");
            }
            return;
        }
        DemoConfig.TERRAIN_LIGHTING = true;
        DemoConfig.SKY_ENABLED = true;
        lightOptions.setTerrainLightingEnabled(true);
        skyOptions.setEnabled(true);
        // The sun is computed for the CURRENT map centre, not the start position.
        MapPos centre = mapView.getOptions().getBaseProjection().toWgs84(mapView.getFocusPos());
        DemoSky.applyHour(lightOptions, skyOptions, hourUtc, centre.getY(), centre.getX());
        mapView.requestRender();
    }

    // =============================================================================================
    // CAMERA / DEBUG HELPERS
    // =============================================================================================

    /** Note: setFocusPos expects BASE PROJECTION coordinates, so WGS84 must be converted first. */
    public void applyCamera() {
        Projection proj = mapView.getOptions().getBaseProjection();
        mapView.setFocusPos(proj.fromWgs84(new MapPos(DemoConfig.START_LON, DemoConfig.START_LAT)), 0);
        mapView.setZoom(DemoConfig.START_ZOOM, 0);
        applyLookRange();
        mapView.setTilt(DemoConfig.START_TILT, 0);
        mapView.setMapRotation(DemoConfig.START_ROTATION, 0);
    }

    /**
     * Free roam and how far above the horizon the view may look.
     *
     * A NEGATIVE tilt is the look up: the camera stays where the tilt geometry put it and only the
     * view pitches, so nothing about zoom or the visible tiles changes. A map stops at the horizon
     * by default (tilt range 0..90), which is why this has to be asked for.
     */
    public void applyLookRange() {
        Options options = mapView.getOptions();
        options.setFreeRoamMode(freeRoamMode(DemoConfig.FREE_ROAM_MODE));
        options.setPanningSpeedMode(panningSpeedMode(DemoConfig.PANNING_SPEED_MODE));
        options.setFreeRoamLookSensitivity(DemoConfig.FREE_ROAM_LOOK_SENSITIVITY);
        options.setFreeRoamMoveSpeed(DemoConfig.FREE_ROAM_MOVE_SPEED);
        options.setTiltRange(new com.carto.core.MapRange(-Math.max(0f, DemoConfig.LOOK_UP_LIMIT), 90f));
    }

    /** "map" / "anchored" / "constant" -> the SDK enum. */
    public static com.carto.components.PanningSpeedMode panningSpeedMode(String name) {
        if ("map".equals(name)) {
            return com.carto.components.PanningSpeedMode.PANNING_SPEED_MODE_MAP;
        }
        if ("constant".equals(name)) {
            return com.carto.components.PanningSpeedMode.PANNING_SPEED_MODE_CONSTANT;
        }
        return com.carto.components.PanningSpeedMode.PANNING_SPEED_MODE_ANCHORED;
    }

    /** "off" / "look" / "fps" -> the SDK enum. */
    public static com.carto.components.FreeRoamMode freeRoamMode(String name) {
        if ("look".equals(name)) {
            return com.carto.components.FreeRoamMode.FREE_ROAM_MODE_LOOK;
        }
        if ("fps".equals(name)) {
            return com.carto.components.FreeRoamMode.FREE_ROAM_MODE_FIRST_PERSON;
        }
        return com.carto.components.FreeRoamMode.FREE_ROAM_MODE_OFF;
    }

    // =============================================================================================
    // STAR SKY: the map removed, the background cleared to nothing, only the sky left
    // =============================================================================================

    private Color savedClearColor;
    private Color savedSkyColor;
    private com.carto.graphics.Bitmap savedBackgroundBitmap;
    private boolean starSkySaved;

    /**
     * Switches the whole map off and leaves the sky.
     *
     * "Not drawn" here means NOT BUILT: the map layers leave the layer list (see isEnabled), the
     * terrain is disabled and the background is cleared to a fully transparent black, so the frame
     * costs an empty map plus the sky objects. The transparency is the point - with a translucent
     * surface, whatever is behind the view (a camera preview) shows through it.
     *
     * The map fades out before it is dropped and fades back in after it returns, so the switch is
     * not a pop.
     */
    public void applyStarSky(final boolean enabled) {
        if (enabled == DemoConfig.STAR_SKY && starSkySaved == enabled) {
            return;
        }
        long duration = (long) Math.max(0f, DemoConfig.STAR_SKY_FADE_MS);
        if (enabled) {
            saveMapAppearance();
            fadeMapLayers(1f, 0f, duration, new Runnable() {
                public void run() {
                    enterStarSky();
                }
            });
        } else {
            leaveStarSky();
            fadeMapLayers(0f, 1f, duration, null);
        }
    }

    private void saveMapAppearance() {
        if (starSkySaved) {
            return;
        }
        Options options = mapView.getOptions();
        savedClearColor = options.getClearColor();
        savedSkyColor = options.getSkyColor();
        savedBackgroundBitmap = options.getBackgroundBitmap();
        starSkySaved = true;
    }

    private void enterStarSky() {
        DemoConfig.STAR_SKY = true;
        Options options = mapView.getOptions();
        // Transparent, not black: the frame is then a hole that whatever is behind the surface
        // shows through, and it looks black on its own anyway.
        options.setClearColor(new Color((short) 0, (short) 0, (short) 0, (short) 0));
        options.setSkyColor(new Color((short) 0, (short) 0, (short) 0, (short) 0));
        options.setBackgroundBitmap(null);
        if (terrainOptions != null) {
            terrainOptions.setEnabled(false);
        }
        if (skyOptions != null) {
            skyOptions.setEnabled(false);
        }
        setSurfaceTranslucent(DemoConfig.STAR_SKY_TRANSLUCENT);
        setCameraPreviewEnabled(DemoConfig.STAR_SKY_CAMERA);
        rebuildLayers();
        Log.i(TAG, "star sky on: " + mapView.getLayers().count() + " layers, clear "
                + options.getClearColor().getARGB() + ", background " + options.getBackgroundBitmap());
        setMapLayerOpacity(1f); // the layers are out of the list now: leave them ready to come back
        applyLookRange();
        setOrientationFollowing(DemoConfig.STAR_SKY_ORIENTATION);
        mapView.requestRender();
    }

    private void leaveStarSky() {
        setOrientationFollowing(false);
        setCameraPreviewEnabled(false);
        DemoConfig.STAR_SKY = false;
        Options options = mapView.getOptions();
        if (starSkySaved) {
            options.setClearColor(savedClearColor);
            options.setSkyColor(savedSkyColor);
            options.setBackgroundBitmap(savedBackgroundBitmap);
            starSkySaved = false;
        }
        if (terrainOptions != null) {
            terrainOptions.setEnabled(DemoConfig.TERRAIN_ENABLED);
        }
        if (skyOptions != null) {
            skyOptions.setEnabled(DemoConfig.SKY_ENABLED);
        }
        if (DemoConfig.STAR_SKY_TRANSLUCENT) {
            setSurfaceTranslucent(false);
        }
        setMapLayerOpacity(0f);
        rebuildLayers();
        mapView.requestRender();
    }

    /** Turning the device turns the view, raising it looks up - the negative tilt in action. */
    public void setOrientationFollowing(boolean enabled) {
        DemoConfig.STAR_SKY_ORIENTATION = enabled;
        if (enabled) {
            if (orientation == null) {
                orientation = new DemoOrientation(context, mapView);
            }
            orientation.start();
        } else if (orientation != null) {
            orientation.stop();
        }
    }

    /**
     * The live camera behind the map: what the transparent clear colour is FOR. Only meaningful
     * with a translucent surface, and only in star sky mode - there is nothing to see through
     * otherwise.
     */
    public void setCameraPreviewEnabled(boolean enabled) {
        DemoConfig.STAR_SKY_CAMERA = enabled;
        if (enabled) {
            if (!(mapView.getParent() instanceof androidx.constraintlayout.widget.ConstraintLayout)) {
                Log.w(TAG, "the map is not in a ConstraintLayout: no place to put the preview");
                return;
            }
            if (cameraPreview == null) {
                cameraPreview = new DemoCameraPreview(context, (androidx.constraintlayout.widget.ConstraintLayout) mapView.getParent());
            }
            cameraPreview.start();
        } else if (cameraPreview != null) {
            cameraPreview.stop();
        }
    }

    /**
     * A translucent GL surface, which is what makes a transparent clear colour visible: the map is
     * then composited over whatever is behind it (with setZOrderMediaOverlay, a camera preview).
     * Without this the transparency is real but the surface is still opaque, so it just looks black.
     */
    private void setSurfaceTranslucent(final boolean translucent) {
        // Touches the view, and the demo builds on a worker thread.
        mapView.post(new Runnable() {
            public void run() {
                try {
                    mapView.setTranslucent(translucent);
                } catch (Exception e) {
                    Log.w(TAG, "could not change the surface format: " + e);
                }
            }
        });
    }

    /** Opacity of every layer that is NOT the sky. */
    private void setMapLayerOpacity(float opacity) {
        for (Map.Entry<Feature, Layer> entry : layers.entrySet()) {
            if (entry.getKey() != Feature.CELESTIAL && entry.getKey() != Feature.STARS) {
                entry.getValue().setOpacity(opacity);
            }
        }
        mapView.requestRender();
    }

    private void fadeMapLayers(final float from, final float to, long durationMs, final Runnable onEnd) {
        if (durationMs <= 0) {
            setMapLayerOpacity(to);
            if (onEnd != null) {
                onEnd.run();
            }
            return;
        }
        android.animation.ValueAnimator animator = android.animation.ValueAnimator.ofFloat(from, to);
        animator.setDuration(durationMs);
        animator.addUpdateListener(new android.animation.ValueAnimator.AnimatorUpdateListener() {
            public void onAnimationUpdate(android.animation.ValueAnimator a) {
                setMapLayerOpacity(((Float) a.getAnimatedValue()).floatValue());
            }
        });
        if (onEnd != null) {
            animator.addListener(new android.animation.AnimatorListenerAdapter() {
                public void onAnimationEnd(android.animation.Animator a) {
                    onEnd.run();
                }
            });
        }
        animator.start();
    }

    /**
     * '--es anim zoom|pan|rotate|zoomseq' drives a scripted camera move, so animation artifacts
     * (which still frames never show) can be captured with adb screenrecord without touch input.
     */
    private void startScriptedAnimation() {
        final String anim = DemoConfig.ANIM;
        if (anim == null || anim.isEmpty()) {
            return;
        }
        final float duration = DemoConfig.ANIM_DURATION_S;
        handler.postDelayed(new Runnable() {
            public void run() {
                Projection proj = mapView.getOptions().getBaseProjection();
                if ("zoom".equals(anim)) {
                    mapView.setZoom(DemoConfig.START_ZOOM + DemoConfig.ANIM_ZOOM_DELTA, duration);
                } else if ("pan".equals(anim)) {
                    mapView.setFocusPos(proj.fromWgs84(new MapPos(DemoConfig.START_LON + DemoConfig.ANIM_LON_DELTA, DemoConfig.START_LAT + DemoConfig.ANIM_LAT_DELTA)), duration);
                } else if ("rotate".equals(anim)) {
                    mapView.setMapRotation(DemoConfig.ANIM_ROTATION, duration);
                } else if ("zoomseq".equals(anim)) {
                    // zoom out, back in, out again - each step after the map settled, which is the
                    // repro shape for "stale content stays on screen" bugs.
                    final float zoomOut = DemoConfig.ANIM_ZOOM_OUT;
                    final float zoomIn = DemoConfig.START_ZOOM;
                    final float settle = DemoConfig.ANIM_SETTLE_MS;
                    handler.postDelayed(new Runnable() { public void run() { Log.i("zoomseq", "step1 out " + zoomOut); mapView.setZoom(zoomOut, 0); } }, 0);
                    handler.postDelayed(new Runnable() { public void run() { Log.i("zoomseq", "step2 in " + zoomIn); mapView.setZoom(zoomIn, 0); } }, (long) settle);
                    handler.postDelayed(new Runnable() { public void run() { Log.i("zoomseq", "step3 out " + zoomOut); mapView.setZoom(zoomOut, 0); } }, (long) (2 * settle));
                }
            }
        }, (long) DemoConfig.ANIM_DELAY_MS);
    }

    /** PeakFinder-style relief outline post-process effect. */
    public void setReliefOutlineEnabled(boolean enabled) {
        DemoConfig.RELIEF_OUTLINE = enabled;
        MapRenderer renderer = mapView.getMapRenderer();
        if (enabled) {
            reliefEffect = PostProcessEffect.createReliefOutlineEffect();
            applyReliefOutlineParameters();
            renderer.setPostProcessEffect(reliefEffect);
        } else {
            reliefEffect = null;
            renderer.setPostProcessEffect(null);
        }
        mapView.requestRender();
    }

    /** Pushes the outline knobs (and the light/dark palette) onto the attached effect. */
    public void applyReliefOutlineParameters() {
        if (reliefEffect == null) {
            return;
        }
        reliefEffect.setFloatParameter("uIntensity", 1.0f);
        reliefEffect.setFloatParameter("uOutlineWidth", DemoConfig.RELIEF_OUTLINE_WIDTH);
        reliefEffect.setFloatParameter("uHorizonBoost", DemoConfig.RELIEF_HORIZON_BOOST);
        reliefEffect.setFloatParameter("uDepthThreshold", DemoConfig.RELIEF_DEPTH_THRESHOLD);
        reliefEffect.setFloatParameter("uCreaseStrength", DemoConfig.RELIEF_CREASE_STRENGTH);
        reliefEffect.setFloatParameter("uHaze", DemoConfig.RELIEF_HAZE);
        reliefEffect.setColorParameter("uInkColor", color(DemoConfig.RELIEF_DARK ? 0xffe8ecf5 : 0xff14141a));
        reliefEffect.setColorParameter("uPaperColor", color(DemoConfig.RELIEF_DARK ? 0xff10131a : 0xffffffff));
        mapView.requestRender();
    }

    /**
     * Demonstrates a 'nuti::' user setting driving the style at runtime: flips the parameter every
     * NUTI_TOGGLE_INTERVAL_MS, which fades the hillshade slot in and out.
     */
    private void startNutiToggleLoop() {
        handler.postDelayed(new Runnable() {
            public void run() {
                if (baseDecoder == null || DemoConfig.STYLE_SOURCE != DemoConfig.StyleSource.NUTI) {
                    return; // the style was switched away: stop the loop
                }
                nutiParameterOn = !nutiParameterOn;
                baseDecoder.setStyleParameter(DemoStyles.NUTI_PARAMETER, Boolean.toString(nutiParameterOn));
                Log.d(TAG, "nuti " + DemoStyles.NUTI_PARAMETER + "=" + nutiParameterOn);
                handler.postDelayed(this, DemoConfig.NUTI_TOGGLE_INTERVAL_MS);
            }
        }, DemoConfig.NUTI_TOGGLE_INTERVAL_MS);
    }

    /** Elevation under a map position; blocks on tile loading, so call it off the UI thread. */
    public double getElevation(MapPos wgs84Pos) {
        return terrainOptions != null ? terrainOptions.getElevation(wgs84Pos) : 0;
    }

    private static Color color(int argb) {
        return new Color(argb);
    }
    /**
     * Terrain on/off as an EXPAND animation instead of a pop. Enabling flips the flag first and
     * ramps the exaggeration 0 -> target, disabling ramps it to 0 and only then flips the flag, so
     * the tile re-decode that a flag change forces happens while the map is already flat and is not
     * seen. Only the exaggeration moves per frame, and that no longer invalidates the tile cache.
     */
    public void animateTerrain(final boolean enabled) {
        final float target = DemoConfig.TERRAIN_EXAGGERATION;
        final long durationMs = DemoConfig.TERRAIN_ANIM_MS;
        if (durationMs <= 0) {
            terrainOptions.setEnabled(enabled);
            terrainOptions.setExaggeration(target);
            return;
        }
        if (enabled) {
            // Flat first, and hold the ramp until the terrain-decoded tiles are in. Flipping the
            // flag re-decodes every tile, and the old FLAT ones are tesselated without terrain
            // subdivision: displacing those chords a road straight between its endpoints, which
            // over a valley rides well above the ground - roads in the sky. At exaggeration 0
            // nothing of that is visible, so waiting costs nothing to look at.
            terrainOptions.setExaggeration(0f);
            terrainOptions.setEnabled(true);
            startRampWhenTilesLoaded();
            return;
        }
        final android.animation.ValueAnimator animator =
            android.animation.ValueAnimator.ofFloat(enabled ? 0f : target, enabled ? target : 0f);
        animator.setDuration(durationMs);
        animator.setInterpolator(new android.view.animation.DecelerateInterpolator());
        animator.addUpdateListener(new android.animation.ValueAnimator.AnimatorUpdateListener() {
            public void onAnimationUpdate(android.animation.ValueAnimator a) {
                terrainOptions.setExaggeration(((Float) a.getAnimatedValue()).floatValue());
            }
        });
        if (!enabled) {
            animator.addListener(new android.animation.AnimatorListenerAdapter() {
                public void onAnimationEnd(android.animation.Animator a) {
                    terrainOptions.setEnabled(false);
                    terrainOptions.setExaggeration(target);
                }
            });
        }
        animator.start();
    }

    /** Ramps the terrain in once the visible tiles have been re-decoded for it (see animateTerrain). */
    private void startRampWhenTilesLoaded() {
        final float target = DemoConfig.TERRAIN_EXAGGERATION;
        final long durationMs = DemoConfig.TERRAIN_ANIM_MS;
        final android.os.Handler handler = new android.os.Handler(android.os.Looper.getMainLooper());
        final Runnable ramp = new Runnable() {
            private boolean done = false;
            public void run() {
                if (done) {
                    return;
                }
                done = true;
                if (baseLayer != null) {
                    baseLayer.setTileLoadListener(null);
                }
                android.animation.ValueAnimator a = android.animation.ValueAnimator.ofFloat(0f, target);
                a.setDuration(durationMs);
                a.setInterpolator(new android.view.animation.DecelerateInterpolator());
                a.addUpdateListener(new android.animation.ValueAnimator.AnimatorUpdateListener() {
                    public void onAnimationUpdate(android.animation.ValueAnimator anim) {
                        terrainOptions.setExaggeration(((Float) anim.getAnimatedValue()).floatValue());
                    }
                });
                a.start();
            }
        };
        if (baseLayer != null) {
            baseLayer.setTileLoadListener(new com.carto.layers.TileLoadListener() {
                public void onVisibleTilesLoaded() {
                    handler.post(ramp);
                }
            });
        }
        // The listener can not fire when every visible tile is already decoded for the terrain
        // (toggling back and forth), so a timeout is what actually starts it in that case.
        handler.postDelayed(ramp, DemoConfig.TERRAIN_ANIM_TILE_TIMEOUT_MS);
    }

}
