package com.akylas.cartotest.demo;

/**
 * EVERY default of the demo app, in one place.
 *
 * HOW TO USE THIS FILE
 *  - to change what the app shows on launch: edit the values below, nothing else;
 *  - to change something WITHOUT rebuilding: pass the matching intent extra (see
 *    {@link #applyIntentOverrides()} at the bottom for the key of every field);
 *  - to change something while the app runs: use the on-screen panel (gear button), which
 *    writes back into these very fields and then asks {@link DemoMap} to apply/rebuild.
 *
 * The fields are NOT final on purpose: the panel mutates them, so at any moment this class is
 * the single source of truth for "what is the demo currently configured to do".
 */
public final class DemoConfig {

    // =============================================================================================
    // WHAT THE BASE MAP IS
    // =============================================================================================

    /** How the base map layer is built. */
    public enum BaseMode {
        /** Plain VectorTileLayer: master vector source + style decoder, nothing woven in. */
        PLAIN,
        /** CompositeVectorTileLayer: hillshade / satellite / contour sources woven INTO the style. */
        COMPOSITE
    }

    /** Where the style (CartoCSS / compiled style project) of the base map comes from. */
    public enum StyleSource {
        /** DirAssetPackage over a PLAIN FOLDER on the device - edit the style, restart, done. */
        DIR,
        /** ZippedAssetPackage over osm.zip - the classic packaged style. */
        ZIP,
        /** A CartoCSS string built in DemoStyles - self-contained, no file needed. */
        INLINE,
        /** In-memory project bundle declaring a 'nuti::' parameter (see DemoStyles.nutiProject). */
        NUTI,
        /** AndroidAssetPackage over the style project bundled in the APK assets (assets/style).
         *  The smallest complete example of a style a composite layer can weave sources into. */
        ASSETS
    }

    public static BaseMode BASE_MODE = BaseMode.COMPOSITE;
    public static StyleSource STYLE_SOURCE = StyleSource.INLINE;

    // =============================================================================================
    // FILES ON THE DEVICE
    // Data root is <external-storage>/alpimaps_mbtiles (same convention as before).
    // =============================================================================================

    /** Data root, resolved as <sd-card>/DATA_DIR_NAME (shared with the other test apps). */
    public static String DATA_DIR_NAME = "alpimaps_mbtiles";
    /** Folder (relative to the data root) read by DirAssetPackage for StyleSource.DIR. */
    public static String STYLE_DIR_NAME = "osm";
    /** Zip (relative to the data root) read by ZippedAssetPackage for StyleSource.ZIP, and the
     *  automatic fallback when STYLE_DIR_NAME does not exist on the device. */
    public static String STYLE_ZIP_NAME = "osm.zip";
    /** Style project inside the APK assets, read by AndroidAssetPackage for StyleSource.ASSETS. */
    public static String STYLE_ASSETS_PATH = "style";
    /** Style zip used by the offline "routes" layer. */
    public static String ROUTES_STYLE_ZIP_NAME = "inner.zip";
    /** MBTiles used by the offline "routes" layer. */
    public static String ROUTES_MBTILES_NAME = "france/france_routes.mbtiles";
    /** Valhalla tiles used by the offline routing test action. */
    public static String ROUTING_VTILES_NAME = "france/france.vtiles";

    // =============================================================================================
    // WHICH LAYERS ARE ADDED (each one is independent and can be toggled live from the panel)
    // Draw order is the order of DemoMap.LAYER_ORDER, not the order of these fields.
    // =============================================================================================

    /** The base map (vector tiles + style). */
    public static boolean LAYER_BASE = true;
    /** Stand-alone HillshadeRasterTileLayer over the shared DEM (independent of the composite slot). */
    public static boolean LAYER_HILLSHADE = false;
    /** Stand-alone contour layer: ContourTileDataSource + its own CartoCSS. */
    public static boolean LAYER_CONTOUR = false;
    /** PRE-BAKED contour vector tiles fetched over HTTP, styled like the real style's '#contour'
     *  rules. The A/B reference for LAYER_CONTOUR / the '#contour' composite slot. */
    public static boolean LAYER_CONTOUR_TILES = false;
    /** Stand-alone raster layer (OSM raster tiles by default). */
    public static boolean LAYER_SATELLITE = false;
    /** CustomRasterTileLayer running a hypsometric-tint shader over the raw DEM tiles. */
    public static boolean LAYER_HYPSO = false;
    /** Markers on summits + a line across the valley: the terrain occlusion / drape test set. */
    public static boolean LAYER_ELEMENTS = true;
    /** Offline routes layer (needs ROUTES_MBTILES_NAME + ROUTES_STYLE_ZIP_NAME on the device). */
    public static boolean LAYER_ROUTES = false;

    // =============================================================================================
    // COMPOSITE SLOTS (BaseMode.COMPOSITE only)
    // These are sources woven into the master style at the position of their '#name' rule.
    // =============================================================================================

    public static boolean COMPOSITE_HILLSHADE = false;
    public static boolean COMPOSITE_SATELLITE = false;
    public static boolean COMPOSITE_CONTOUR = true;
    /** Single-pass segmented rendering (A/B switch of the composite renderer). */
    public static boolean COMPOSITE_SINGLE_PASS = true;
    /** Per-source zoom bias: +1 fetches the DEM one zoom deeper than the base map. */
    public static float COMPOSITE_HILLSHADE_ZOOM_BIAS = 0f;

    // =============================================================================================
    // TILE SOURCES
    // =============================================================================================

    /** Master vector tile source of the base map. */
    public static String VECTOR_URL = "https://tiles.akylas.fr/data/france/{z}/{x}/{y}.pbf";
    public static int VECTOR_MIN_ZOOM = 0;
    public static int VECTOR_MAX_ZOOM = 14;
    public static String VECTOR_CACHE_DB = "akylas_vect.db";
    public static String HTTP_USER_AGENT = "AlpiMaps/1.4 (contact: contact@akylas.fr)";

    /** Shared elevation source: 3D terrain, hillshade, contours and the hypsometric tint all use it. */
    public static String DEM_URL = "https://tiles.mapterhorn.com/{z}/{x}/{y}.webp";
    public static int DEM_MIN_ZOOM = 1;
    /** The REAL max zoom of the source (mapterhorn stops at 16). Setting it higher only produces
     *  404s: deeper camera zooms are served by overzooming the last available level. */
    public static int DEM_MAX_ZOOM = 16;
    /** "terrarium" or "mapbox" - decides which ElevationDecoder is used. */
    public static String DEM_ENCODING = "terrarium";
    public static String DEM_CACHE_DB = "mapterhorn.db";

    /** Pre-baked contour vector tiles (tippecanoe, layer 'contour', fields 'ele' + 'div').
     *  Zooms 11..14 only: the tileset has no data below 11 and 14 is overzoomed above. */
    public static String CONTOUR_TILES_URL = "https://tiles.akylas.fr/data/contours/{z}/{x}/{y}.pbf";
    public static int CONTOUR_TILES_MIN_ZOOM = 11;
    public static int CONTOUR_TILES_MAX_ZOOM = 14;
    public static String CONTOUR_TILES_CACHE_DB = "akylas_contours.db";

    /** Raster source used by the satellite layer / '#satellite' composite slot. */
    public static String RASTER_URL = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
    public static int RASTER_MIN_ZOOM = 0;
    public static int RASTER_MAX_ZOOM = 19;
    public static String RASTER_CACHE_DB = "openstreetmap.db";

    // =============================================================================================
    // CAMERA (start position; every value can be overridden with --es lon/lat/zoom/tilt/rotation)
    // =============================================================================================

    public static double START_LON = 5.718957;
    public static double START_LAT = 45.187362;
    public static float START_ZOOM = 16.22f;
    public static float START_TILT = 26f;
    public static float START_ROTATION = -15.12f;

    // =============================================================================================
    // 3D TERRAIN (com.carto.components.TerrainOptions)
    // =============================================================================================

    /** Tile decode threads (Options.setTileThreadPoolSize). The SDK default is 1; tangram-ng
     *  runs 2 (SceneOptions::numTileWorkers). Raise it to get tiles on screen sooner. */
    public static int TILE_THREAD_POOL_SIZE = 1;

    public static boolean TERRAIN_ENABLED = true;
    public static float TERRAIN_EXAGGERATION = 1.0f;
    /** Triangles per tile side. Slack against the draped content scales as (32/res)^2.
     *  64 is what tangram-ng uses (RasterStyle::build, hardcoded); 128 measured 8.5 fps against
     *  15.2 at 64 on the Crosscall (mesh 64, plain base, no labels/hillshade/contours). */
    public static int TERRAIN_MESH_RESOLUTION = 64;
    /** Painter-order depth model (per-tile-layer depth domain). Keep on unless debugging depth. */
    public static boolean TERRAIN_PAINTER_ORDER_DEPTH = true;
    /** Render fills through an offscreen drape pass instead of displacing their geometry.
     *  OFF: the drape is being dropped for the tangram arrangement - one shared ground pass for the
     *  whole layer stack, no bake, no per-layer depth pre-pass, no stencil masks (render-performance
     *  doc, section 10). '--es drape true' still brings the old path back for an A/B. */
    public static boolean TERRAIN_DRAPE_FILLS = false;
    public static boolean TERRAIN_DRAPE_LINES = false;
    public static int TERRAIN_DRAPE_RESOLUTION = 1024;
    /** Stitch neighbouring DEM tiles so ridges do not appear at tile borders. */
    public static boolean TERRAIN_TILE_EDGE_STITCHING = false;
    public static boolean TERRAIN_SEAMLESS_TILE_EDGES = true;
    public static boolean TERRAIN_ELEVATION_PREFETCH = true;
    /** Hide billboards behind relief; tolerance > 0 keeps summits partly behind a ridge visible. */
    public static boolean TERRAIN_BILLBOARD_OCCLUSION = true;
    public static float TERRAIN_OCCLUSION_TOLERANCE = 0.0f;
    /** 0 = off; caps terrain LOD tile detail at what flat rendering would show. */
    public static boolean TERRAIN_MAX_TILE_ZOOM_OFFSET_ENABLED = false;
    public static int TERRAIN_MAX_TILE_ZOOM_OFFSET = 0;
    /** Drapes Options.getBackgroundBitmap over the terrain surface where nothing is drawn. */
    public static boolean TERRAIN_BACKGROUND_BITMAP = false;

    // Fog / view distance: they belong together, the distance ENDS the ground and the fog is what
    // makes it fade out instead of being cut off.
    public static boolean FOG_ENABLED = false;
    public static int FOG_COLOR_ARGB = 0xffb8c6d8;
    public static float FOG_START_DISTANCE = 1500f;
    public static float FOG_DISTANCE = 0f;          // 0 = off
    public static float MAX_VISIBLE_DISTANCE = 0f;  // 0 = unlimited

    // =============================================================================================
    // SUN / LIGHT / SHADOWS (com.carto.components.LightOptions)
    // =============================================================================================

    public static boolean TERRAIN_LIGHTING = false;
    /** When >= 0 the sun is placed from the date+hour below instead of azimuth/altitude. */
    public static float SUN_HOUR_UTC = -1f;
    public static int SUN_YEAR = 2026, SUN_MONTH = 7, SUN_DAY = 26;
    public static float SUN_AZIMUTH = 355f;
    public static float SUN_ALTITUDE = 9f;
    public static float SUN_INTENSITY = 1.0f;
    public static float AMBIENT_INTENSITY = 1.0f;
    public static float SHADOW_STRENGTH = 0.3f;
    public static float SHADOW_SOFTNESS = 1.0f;
    public static int SHADOW_MAP_SIZE = 1024;
    public static int SHADOW_CASCADES = 3;
    public static float SHADOW_BIAS = 1.0f;
    public static float SHADOW_DISTANCE = 0f;   // 0 = whole view
    public static int SHADOW_CASTER_MARGIN = 1;

    // =============================================================================================
    // SKY (com.carto.components.SkyOptions) + the generated day-cycle shader
    // =============================================================================================

    public static boolean SKY_ENABLED = true;
    /** Day cycle: sun/moon/stars/clouds shader driven by SUN_HOUR_UTC, updated live by the panel. */
    public static boolean DAY_CYCLE = false;
    public static float DAY_CYCLE_HOUR = 12f;

    // =============================================================================================
    // HILLSHADE (stand-alone layer AND the composite '#hillshade' child layer where applicable)
    // =============================================================================================

    /** IGOR, COMBINED, BASIC ... see com.carto.layers.HillshadeMethod. */
    public static String HILLSHADE_METHOD = "IGOR";
    public static float HILLSHADE_CONTRAST = 0.5f;
    public static float HILLSHADE_HEIGHT_SCALE = 0.05f;
    public static float HILLSHADE_EXAGGERATION = 1.0f;
    public static float HILLSHADE_ILLUMINATION_DEGREES = 180f;
    public static boolean HILLSHADE_ILLUMINATION_FOLLOWS_MAP = false;
    public static int HILLSHADE_SHADOW_COLOR_ARGB = 0xB0000000;
    public static int HILLSHADE_HIGHLIGHT_COLOR_ARGB = 0xFF000000;
    public static int HILLSHADE_ACCENT_COLOR_ARGB = 0xFF000000;
    /** GPU contour lines drawn inside the hillshade pass (no labels, not styled from CartoCSS). */
    public static boolean HILLSHADE_CONTOUR_LINES = false;
    public static float HILLSHADE_CONTOUR_INTERVAL = 100f;
    public static float HILLSHADE_CONTOUR_WIDTH = 0.8f;
    public static int HILLSHADE_CONTOUR_COLOR_ARGB = 0xFFC56008;
    /** Replaces the lighting shader with the "slope colouring" one (steepness bands). */
    public static boolean HILLSHADE_SLOPES_SHADER = false;

    // =============================================================================================
    // CONTOURS (ContourTileDataSource - both the stand-alone layer and the composite slot)
    // =============================================================================================

    public static float CONTOUR_BASE_INTERVAL = 10f;
    /** DEM samples per tile side before tracing: lower = far fewer vertices to trace and drape. */
    public static int CONTOUR_RESOLUTION = 96;
    public static float CONTOUR_SIMPLIFY_TOLERANCE = 1.5f;
    /** Contours are generated only at or above this TILE zoom. */
    public static int CONTOUR_MIN_VISIBLE_ZOOM = 5;
    /** Fetch neighbour DEM tiles so lines meet across tile borders (up to 3 extra fetches/tile). */
    public static boolean CONTOUR_SEAMLESS_EDGES = true;
    public static int CONTOUR_MAX_OVERZOOM = 15;

    /** Font of the pre-baked contour tile labels. An inline CartoCSS string carries no font asset
     *  package, so this goes through the system-font fallback ("Arial" -> Roboto on Android). */
    public static String CONTOUR_TILES_FONT = "Arial";

    // =============================================================================================
    // INLINE STYLE KNOBS (StyleSource.INLINE / NUTI - see DemoStyles)
    // =============================================================================================

    public static String INLINE_BACKGROUND_COLOR = "#eef2f0";
    /** Extrude buildings: this is what gives the shadow pass real 3D casters. */
    public static boolean INLINE_BUILDINGS_3D = false;
    /** Move sun/shadow/fog INTO the style (Map block properties) instead of setting them in code. */
    public static boolean INLINE_STYLE_LIGHTING = false;
    /** Text rules of the inline style ('--es labels false' isolates the label pipeline's cost). */
    public static boolean INLINE_LABELS = true;
    /** Strip the inline style down to the Map background plus the composite slot blocks
     *  ('--es minimal true'). Nothing of the vector data is drawn, so what is left on screen is the
     *  terrain and whatever the slots put on it - which is how the hillshade's own cost is measured
     *  without the base map's geometry dominating the frame. The base layer stays, because it is
     *  what gives the drape its tile cover. */
    public static boolean INLINE_STYLE_MINIMAL = false;
    /** Opacity of the ground-shaped fills (#landcover, #landuse). 1 = opaque, today's behaviour.
     *  Tangram draws these translucent whenever something under them has to show: their
     *  'translucent-polygons' style is alpha 0.25 (res/scenes/hillshade.yaml), selected through
     *  global.earth_style, and it is how the hillshade and the contours read through the map
     *  instead of being painted over. An un-subdivided fill also floats above the ground by its
     *  chord error, and a translucent one hides far less of what it floats over.
     *  '--es landcoverOpacity 0.25' */
    public static float INLINE_LANDCOVER_OPACITY = 1.0f;
    public static int INLINE_SATELLITE_MIN_ZOOM = 11;
    public static String INLINE_HILLSHADE_SHADOW_COLOR = "#473B24";
    public static float INLINE_HILLSHADE_ILLUMINATION = 365f;
    /** Flip the 'show_relief' nuti parameter every N ms (StyleSource.NUTI). 0 = do not flip. */
    public static int NUTI_TOGGLE_INTERVAL_MS = 3000;

    // =============================================================================================
    // DEBUG / HARNESS
    // =============================================================================================

    /** false = no panel and no overlay text: clean screenshots for automated rendering checks. */
    public static boolean UI_ENABLED = true;
    /** PeakFinder-style relief outline post-process effect. */
    public static boolean RELIEF_OUTLINE = false;
    /** Delay before switching the effect on, in ms: attaching it before the GL surface exists
     *  leaves the offscreen colour buffer unwritten and the screen black. */
    public static float RELIEF_OUTLINE_DELAY_MS = 8000;
    /** Scripted camera move so animation artifacts can be captured with adb screenrecord:
     *  "" | zoom | pan | rotate | zoomseq. */
    public static String ANIM = "";
    public static float ANIM_DELAY_MS = 12000;
    public static float ANIM_DURATION_S = 8;
    public static float ANIM_ZOOM_DELTA = 3;
    public static float ANIM_LON_DELTA = 0.05f;
    /** North/south component of the scripted pan. Panning north into the mountains is the case
     *  that gets slow, and it exercises quite different work from panning over the valley. */
    public static float ANIM_LAT_DELTA = 0f;
    public static float ANIM_ROTATION = 180;
    public static float ANIM_ZOOM_OUT = 10.2f;
    public static float ANIM_SETTLE_MS = 8000;

    // =============================================================================================
    // INTENT OVERRIDES - the complete key list. Keys are historical, keep them stable: scripts and
    // AI debugging sessions use them.
    // =============================================================================================

    public static void applyIntentOverrides() {
        // what is shown
        BASE_MODE = DemoCfg.cfgEnum("base", BASE_MODE, BaseMode.class);              // --es base plain|composite
        STYLE_SOURCE = DemoCfg.cfgEnum("style", STYLE_SOURCE, StyleSource.class);    // --es style dir|zip|inline|nuti
        // legacy 'demo' names kept working: terrain = plain base + terrain, nuti = nuti style
        String demo = DemoCfg.cfg("demo");
        if ("terrain".equals(demo)) {
            BASE_MODE = BaseMode.PLAIN;
            STYLE_SOURCE = StyleSource.INLINE;
            LAYER_HILLSHADE = true;
            LAYER_CONTOUR = true;
        } else if ("nuti".equals(demo)) {
            BASE_MODE = BaseMode.COMPOSITE;
            STYLE_SOURCE = StyleSource.NUTI;
        } else if ("composite".equals(demo)) {
            BASE_MODE = BaseMode.COMPOSITE;
        }

        // layers
        LAYER_BASE = DemoCfg.cfgBool("map", LAYER_BASE);
        LAYER_HILLSHADE = DemoCfg.cfgBool("hillshade", LAYER_HILLSHADE);
        LAYER_CONTOUR = DemoCfg.cfgBool("contourLayer", LAYER_CONTOUR);
        LAYER_CONTOUR_TILES = DemoCfg.cfgBool("contourTiles", LAYER_CONTOUR_TILES);
        LAYER_SATELLITE = DemoCfg.cfgBool("satLayer", LAYER_SATELLITE);
        LAYER_HYPSO = DemoCfg.cfgBool("hypso", LAYER_HYPSO);
        LAYER_ELEMENTS = DemoCfg.cfgBool("elements", LAYER_ELEMENTS);
        LAYER_ROUTES = DemoCfg.cfgBool("routes", LAYER_ROUTES);

        // composite slots ('hs', 'sat', 'contour' are the historical keys)
        COMPOSITE_HILLSHADE = DemoCfg.cfgBool("hs", COMPOSITE_HILLSHADE);
        COMPOSITE_SATELLITE = DemoCfg.cfgBool("sat", COMPOSITE_SATELLITE);
        COMPOSITE_CONTOUR = DemoCfg.cfgBool("contour", COMPOSITE_CONTOUR);
        COMPOSITE_SINGLE_PASS = DemoCfg.cfgBool("singlePass", COMPOSITE_SINGLE_PASS);
        COMPOSITE_HILLSHADE_ZOOM_BIAS = DemoCfg.cfgFloat("hsBias", COMPOSITE_HILLSHADE_ZOOM_BIAS);

        // sources
        VECTOR_URL = DemoCfg.cfgStr("vectorUrl", VECTOR_URL);
        VECTOR_MAX_ZOOM = DemoCfg.cfgInt("vectorMaxZoom", VECTOR_MAX_ZOOM);
        DEM_URL = DemoCfg.cfgStr("demUrl", DEM_URL);
        DEM_MAX_ZOOM = DemoCfg.cfgInt("demMaxZoom", DEM_MAX_ZOOM);
        RASTER_URL = DemoCfg.cfgStr("rasterUrl", RASTER_URL);
        CONTOUR_TILES_URL = DemoCfg.cfgStr("contourTilesUrl", CONTOUR_TILES_URL);
        CONTOUR_TILES_MAX_ZOOM = DemoCfg.cfgInt("contourTilesMaxZoom", CONTOUR_TILES_MAX_ZOOM);
        STYLE_DIR_NAME = DemoCfg.cfgStr("styleDir", STYLE_DIR_NAME);
        STYLE_ZIP_NAME = DemoCfg.cfgStr("styleZip", STYLE_ZIP_NAME);
        STYLE_ASSETS_PATH = DemoCfg.cfgStr("styleAssets", STYLE_ASSETS_PATH);

        // camera
        START_LON = DemoCfg.cfgFloat("lon", (float) START_LON);
        START_LAT = DemoCfg.cfgFloat("lat", (float) START_LAT);
        START_ZOOM = DemoCfg.cfgFloat("zoom", START_ZOOM);
        START_TILT = DemoCfg.cfgFloat("tilt", START_TILT);
        START_ROTATION = DemoCfg.cfgFloat("rotation", START_ROTATION);

        // terrain
        TILE_THREAD_POOL_SIZE = DemoCfg.cfgInt("tilePool", TILE_THREAD_POOL_SIZE);
        TERRAIN_ENABLED = DemoCfg.cfgBool("terrain", TERRAIN_ENABLED);
        TERRAIN_EXAGGERATION = DemoCfg.cfgFloat("exaggeration", TERRAIN_EXAGGERATION);
        TERRAIN_MESH_RESOLUTION = DemoCfg.cfgInt("meshResolution", TERRAIN_MESH_RESOLUTION);
        TERRAIN_PAINTER_ORDER_DEPTH = DemoCfg.cfgBool("painterDepth", TERRAIN_PAINTER_ORDER_DEPTH);
        TERRAIN_DRAPE_FILLS = DemoCfg.cfgBool("drape", TERRAIN_DRAPE_FILLS);
        TERRAIN_DRAPE_LINES = DemoCfg.cfgBool("drapeLines", TERRAIN_DRAPE_LINES);
        TERRAIN_DRAPE_RESOLUTION = DemoCfg.cfgInt("drapeResolution", TERRAIN_DRAPE_RESOLUTION);
        TERRAIN_TILE_EDGE_STITCHING = DemoCfg.cfgBool("stitch", TERRAIN_TILE_EDGE_STITCHING);
        TERRAIN_SEAMLESS_TILE_EDGES = DemoCfg.cfgBool("seamlessEdges", TERRAIN_SEAMLESS_TILE_EDGES);
        TERRAIN_ELEVATION_PREFETCH = DemoCfg.cfgBool("prefetch", TERRAIN_ELEVATION_PREFETCH);
        TERRAIN_BILLBOARD_OCCLUSION = DemoCfg.cfgBool("occlusion", TERRAIN_BILLBOARD_OCCLUSION);
        TERRAIN_OCCLUSION_TOLERANCE = DemoCfg.cfgFloat("occlusionTolerance", TERRAIN_OCCLUSION_TOLERANCE);
        TERRAIN_BACKGROUND_BITMAP = DemoCfg.cfgBool("backgroundBitmap", TERRAIN_BACKGROUND_BITMAP);
        if (DemoCfg.cfg("maxTileZoomOffset") != null) {
            TERRAIN_MAX_TILE_ZOOM_OFFSET_ENABLED = true;
            TERRAIN_MAX_TILE_ZOOM_OFFSET = DemoCfg.cfgInt("maxTileZoomOffset", TERRAIN_MAX_TILE_ZOOM_OFFSET);
        }

        // fog / distance
        if (DemoCfg.cfg("fog") != null) {
            FOG_ENABLED = true;
            FOG_COLOR_ARGB = DemoCfg.cfgColorInt("fog", FOG_COLOR_ARGB);
        }
        FOG_START_DISTANCE = DemoCfg.cfgFloat("fogStart", FOG_START_DISTANCE);
        FOG_DISTANCE = DemoCfg.cfgFloat("fogDistance", FOG_DISTANCE);
        MAX_VISIBLE_DISTANCE = DemoCfg.cfgFloat("maxDistance", MAX_VISIBLE_DISTANCE);

        // sun / shadows
        TERRAIN_LIGHTING = DemoCfg.cfgBool("terrainLight", TERRAIN_LIGHTING);
        SUN_HOUR_UTC = DemoCfg.cfgFloat("sunHour", SUN_HOUR_UTC);
        SUN_YEAR = DemoCfg.cfgInt("sunYear", SUN_YEAR);
        SUN_MONTH = DemoCfg.cfgInt("sunMonth", SUN_MONTH);
        SUN_DAY = DemoCfg.cfgInt("sunDay", SUN_DAY);
        SUN_AZIMUTH = DemoCfg.cfgFloat("sunAzimuth", SUN_AZIMUTH);
        SUN_ALTITUDE = DemoCfg.cfgFloat("sunAltitude", SUN_ALTITUDE);
        SUN_INTENSITY = DemoCfg.cfgFloat("sunIntensity", SUN_INTENSITY);
        AMBIENT_INTENSITY = DemoCfg.cfgFloat("ambient", AMBIENT_INTENSITY);
        SHADOW_STRENGTH = DemoCfg.cfgFloat("shadow", SHADOW_STRENGTH);
        SHADOW_SOFTNESS = DemoCfg.cfgFloat("shadowSoftness", SHADOW_SOFTNESS);
        SHADOW_MAP_SIZE = DemoCfg.cfgInt("shadowMapSize", SHADOW_MAP_SIZE);
        SHADOW_CASCADES = DemoCfg.cfgInt("shadowCascades", SHADOW_CASCADES);
        SHADOW_BIAS = DemoCfg.cfgFloat("shadowBias", SHADOW_BIAS);
        SHADOW_DISTANCE = DemoCfg.cfgFloat("shadowDistance", SHADOW_DISTANCE);
        SHADOW_CASTER_MARGIN = DemoCfg.cfgInt("shadowMargin", SHADOW_CASTER_MARGIN);

        // sky
        SKY_ENABLED = DemoCfg.cfgBool("sky", SKY_ENABLED);
        DAY_CYCLE = DemoCfg.cfgBool("daycycle", DAY_CYCLE);
        DAY_CYCLE_HOUR = DemoCfg.cfgFloat("dayCycleHour", DAY_CYCLE_HOUR);

        // hillshade
        HILLSHADE_METHOD = DemoCfg.cfgStr("hsMethod", HILLSHADE_METHOD);
        HILLSHADE_CONTRAST = DemoCfg.cfgFloat("hsContrast", HILLSHADE_CONTRAST);
        HILLSHADE_HEIGHT_SCALE = DemoCfg.cfgFloat("hsHeightScale", HILLSHADE_HEIGHT_SCALE);
        HILLSHADE_EXAGGERATION = DemoCfg.cfgFloat("hsExaggeration", HILLSHADE_EXAGGERATION);
        HILLSHADE_ILLUMINATION_DEGREES = DemoCfg.cfgFloat("hsIllumination", HILLSHADE_ILLUMINATION_DEGREES);
        HILLSHADE_SHADOW_COLOR_ARGB = DemoCfg.cfgColorInt("hsShadowColor", HILLSHADE_SHADOW_COLOR_ARGB);
        HILLSHADE_CONTOUR_LINES = DemoCfg.cfgBool("hsContours", HILLSHADE_CONTOUR_LINES);
        HILLSHADE_CONTOUR_INTERVAL = DemoCfg.cfgFloat("hsContourInterval", HILLSHADE_CONTOUR_INTERVAL);
        HILLSHADE_SLOPES_SHADER = DemoCfg.cfgBool("slopes", HILLSHADE_SLOPES_SHADER);

        // contours
        CONTOUR_BASE_INTERVAL = DemoCfg.cfgFloat("contourInterval", CONTOUR_BASE_INTERVAL);
        CONTOUR_RESOLUTION = DemoCfg.cfgInt("contourResolution", CONTOUR_RESOLUTION);
        CONTOUR_SIMPLIFY_TOLERANCE = DemoCfg.cfgFloat("contourSimplify", CONTOUR_SIMPLIFY_TOLERANCE);
        CONTOUR_MIN_VISIBLE_ZOOM = DemoCfg.cfgInt("contourMinZoom", CONTOUR_MIN_VISIBLE_ZOOM);
        CONTOUR_SEAMLESS_EDGES = DemoCfg.cfgBool("contourSeamless", CONTOUR_SEAMLESS_EDGES);

        // inline style
        INLINE_BACKGROUND_COLOR = DemoCfg.cfgColor("bg", INLINE_BACKGROUND_COLOR);
        INLINE_BUILDINGS_3D = DemoCfg.cfgBool("bld3d", INLINE_BUILDINGS_3D);
        INLINE_STYLE_LIGHTING = DemoCfg.cfgBool("styleLight", INLINE_STYLE_LIGHTING);
        INLINE_LABELS = DemoCfg.cfgBool("labels", INLINE_LABELS);
        INLINE_STYLE_MINIMAL = DemoCfg.cfgBool("minimal", INLINE_STYLE_MINIMAL);
        INLINE_LANDCOVER_OPACITY = DemoCfg.cfgFloat("landcoverOpacity", INLINE_LANDCOVER_OPACITY);
        INLINE_SATELLITE_MIN_ZOOM = DemoCfg.cfgInt("satZoom", INLINE_SATELLITE_MIN_ZOOM);
        NUTI_TOGGLE_INTERVAL_MS = DemoCfg.cfgInt("nutiInterval", NUTI_TOGGLE_INTERVAL_MS);

        // harness
        UI_ENABLED = DemoCfg.cfgBool("ui", UI_ENABLED);
        RELIEF_OUTLINE = DemoCfg.cfgBool("peakfinder", RELIEF_OUTLINE);
        RELIEF_OUTLINE_DELAY_MS = DemoCfg.cfgFloat("peakfinderDelay", RELIEF_OUTLINE_DELAY_MS);
        ANIM = DemoCfg.cfgStr("anim", ANIM);
        ANIM_DELAY_MS = DemoCfg.cfgFloat("animDelay", ANIM_DELAY_MS);
        ANIM_DURATION_S = DemoCfg.cfgFloat("animDuration", ANIM_DURATION_S);
        ANIM_ZOOM_DELTA = DemoCfg.cfgFloat("animZoomDelta", ANIM_ZOOM_DELTA);
        ANIM_LON_DELTA = DemoCfg.cfgFloat("animLonDelta", ANIM_LON_DELTA);
        ANIM_LAT_DELTA = DemoCfg.cfgFloat("animLatDelta", ANIM_LAT_DELTA);
        ANIM_ROTATION = DemoCfg.cfgFloat("animRotation", ANIM_ROTATION);
        ANIM_ZOOM_OUT = DemoCfg.cfgFloat("animZoomOut", ANIM_ZOOM_OUT);
        ANIM_SETTLE_MS = DemoCfg.cfgFloat("animSettle", ANIM_SETTLE_MS);
    }

    private DemoConfig() {
    }
}
