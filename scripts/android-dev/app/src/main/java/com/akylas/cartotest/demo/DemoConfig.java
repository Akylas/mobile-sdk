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
    /** Synthetic mountain-road route (GeoJSON tiles + CartoCSS): the line join / cap / opacity bench. */
    public static boolean LAYER_ROUTE_TEST = false;

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
    /** Screen size a tile may cover before the next zoom level is used, as a factor on tangram's
     *  rule (a 2x2 block of nominal tiles). 1 = their rule; larger keeps tiles coarser at a tilt
     *  (fewer tiles, fewer far labels); 0 refines everything to the camera zoom.
     *  '--es lodFactor 2'. */
    public static float TILE_LOD_FACTOR = 0.5f;
    /** Metres beyond which the inline style's street labels are not placed (0 = no limit). Only
     *  the inline style uses it; it is the 'text-max-distance' CartoCSS property.
     *  '--es labelMaxDistance 2000'. */
    public static float LABEL_MAX_DISTANCE = 2000f;

    public static boolean TERRAIN_ENABLED = true;
    public static float TERRAIN_EXAGGERATION = 1.0f;
    /** Terrain toggle 'expand' animation, ms (0 = pop, the old behaviour). */
    public static long TERRAIN_ANIM_MS = 700;
    /** How long the expand animation waits for terrain-decoded tiles before ramping anyway, ms. */
    public static long TERRAIN_ANIM_TILE_TIMEOUT_MS = 2500;
    /** Triangles per tile side. Slack against the draped content scales as (32/res)^2.
     *  64 is what tangram-ng uses (RasterStyle::build, hardcoded); 128 measured 8.5 fps against
     *  15.2 at 64 on the Crosscall (mesh 64, plain base, no labels/hillshade/contours). */
    public static int TERRAIN_MESH_RESOLUTION = 64;
    /** Metres the camera is held above the ground. The SDK default is 200, which stops you well
     *  short of the surface; 30 lets you get close enough to judge mesh and hillshade detail.
     *  '--es clearance N' (0 disables the clamp entirely - you can then fly through the ground). */
    public static float TERRAIN_CAMERA_CLEARANCE = 60.0f;
    /** Painter-order depth model (per-tile-layer depth domain). Keep on unless debugging depth. */
    public static boolean TERRAIN_PAINTER_ORDER_DEPTH = true;
    /** Render fills through an offscreen drape pass instead of displacing their geometry.
     *  ON, and it is both the correct and the fast choice - this is tangram's arrangement, where the
     *  ground draw samples a texture (`base_color = sampleRaster(0)`, res/scenes/hillshade.yaml)
     *  instead of stacking vector fills over the terrain. A displaced fill chords over the ground
     *  between its vertices and z-fights it (pale slivers on slopes), and it cannot be given room
     *  without the forward pull that leaks content through ridges. Measured on the Crosscall, north
     *  pan with contours and hillshade: 12.9 fps against 10.5 with fills as geometry (bake +1.7 ms,
     *  geometry submission -3.8 ms). '--es drape false' goes back for an A/B. */
    public static boolean TERRAIN_DRAPE_FILLS = true;
    public static boolean TERRAIN_DRAPE_LINES = false;
    public static int TERRAIN_DRAPE_RESOLUTION = 0;
    /** Stitch neighbouring DEM tiles so ridges do not appear at tile borders. */
    public static boolean TERRAIN_TILE_EDGE_STITCHING = true;
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
    /** How far the map is drawn AND where the far plane sits, as a factor on tangram's own rule
     *  (far = 2 * cameraHeight / cos(pitch + fovy/2), capped at 127 tile widths). 1 is their rule
     *  verbatim; 0 falls back to the visible ground, which reaches the horizon.
     *  '--es viewDistance 0.5' halves it. */
    /** Degrees above the fog horizon the sky haze fades out over ('--es fogBlend 12'). */
    public static float SKY_FOG_BLEND = 12f;
    /** Elevation angle the sky haze is still full at: -1 = from the terrain skyline (capped at half
     *  the blend), 0 = from the mathematical horizon, >0 = pinned. '--es fogHorizon 0'. */
    public static float SKY_FOG_HORIZON = -1f;
    public static float VIEW_DISTANCE_FACTOR = 1f;
    /** Zoom levels below the camera a tile may coarsen to in terrain mode. The tile surface is the
     *  depth occluder and the DEM level follows the tile zoom, so unbounded coarsening means leaky
     *  ridges and blocky hillshade. '--es coarsening 2'. */
    public static int TERRAIN_MAX_TILE_ZOOM_COARSENING = 3;

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
    public static int SHADOW_CASTER_MARGIN = 3;

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
    /** DEM samples per tile side before tracing: lower = far fewer vertices to trace and drape.
     *  0 = the DEM's own resolution, which is what matching 3D terrain needs - the ground is
     *  displaced by every texel of the same tile, so a line traced on a coarser grid cuts through
     *  the spurs and gullies between its samples. */
    public static int CONTOUR_RESOLUTION = 128;
    public static float CONTOUR_SIMPLIFY_TOLERANCE = 1.5f;
    /** Interval multiplier per tile zoom, as "maxZoom:multiplier" rungs ("-1" = every zoom above the
     *  others). The rungs must NEST (each a multiple of the finer ones) or lines stop at a tile
     *  border between zooms; cost tracks how fine they are. "" = leave the SDK defaults. */
    public static String CONTOUR_INTERVAL_LADDER = "";
    /** Tracing grid resolution per tile zoom, same syntax. Tracing is ~quadratic in it, so this is
     *  the cheapest knob for zoomed-out frames. "" = CONTOUR_RESOLUTION at every zoom. */
    public static String CONTOUR_RESOLUTION_LADDER = "";
    /** Tile substitution for the contour layer: "all" | "visible" | "none". A missing tile is stood
     *  in for by a cached one of another zoom - for contours that means a 48-sample z9 grid stretched
     *  over the view, which reads as long straight chords until the real tile lands. */
    public static String CONTOUR_TILE_SUBSTITUTION = "all";
    /** How many zoom levels the contour layer may walk UP for a stand-in tile while the right one
     *  loads. Every level is a different interval and grid, so a deep walk shows the same area as a
     *  ladder of coarser and coarser lines. 1 = only the immediate parent. */
    public static int CONTOUR_MAX_OVERZOOM_STANDIN = 6;
    /** Decoded-tile cache of the BASE map layer, in MB. Same story as the contour one: tiles that
     *  leave the visible set are moved into this cache, and whatever does not fit is evicted - so
     *  when it is too small a zoom step throws away the very tiles that should have stood in. */
    public static int BASE_TILE_CACHE_MB = 10;   // SDK default; raise to A/B the eviction theory
    /** Decoded-tile cache of the contour layer, in MB. The SDK default is 10, which a traced
     *  contour tile fills fast - and a tile evicted from it is a tile that DISAPPEARS when it should
     *  have stood in for the one still loading. */
    public static int CONTOUR_TILE_CACHE_MB = 10; // SDK default; raise to A/B the eviction theory
    /** Contours are generated only at or above this TILE zoom. */
    public static int CONTOUR_MIN_VISIBLE_ZOOM = 5;
    /** Fetch neighbour DEM tiles so lines meet across tile borders (up to 3 extra fetches/tile). */
    public static boolean CONTOUR_SEAMLESS_EDGES = true;
    public static int CONTOUR_MAX_OVERZOOM = 15;
    /** Emit only short label stubs (tangram's contour label generator) instead of traced contour
     *  geometry. Pair it with HILLSHADE_CONTOUR_LINES, which draws the lines in the shader for
     *  free, and with CONTOUR_LABEL_INTERVAL matching HILLSHADE_CONTOUR_INTERVAL. */
    public static boolean CONTOUR_LABEL_STUBS = false;
    /** Contour interval of the label stubs in meters; 0 follows the zoom ladder of the traced
     *  geometry. Must match the interval the shader draws or the labels sit between the lines. */
    public static float CONTOUR_LABEL_INTERVAL = 0f;
    /** Generate the label stubs from the TERRAIN's elevation grid (tangram's model) instead of
     *  loading and decoding a DEM tile of the contour source's own. Stubs only. */
    public static boolean CONTOUR_STUBS_FROM_TERRAIN = true;

    /** Font of the pre-baked contour tile labels. An inline CartoCSS string carries no font asset
     *  package, so this goes through the system-font fallback ("Arial" -> Roboto on Android). */
    public static String CONTOUR_TILES_FONT = "Arial";

    // =============================================================================================
    // INLINE STYLE KNOBS (StyleSource.INLINE / NUTI - see DemoStyles)
    // =============================================================================================

    public static String INLINE_BACKGROUND_COLOR = "#eef2f0";
    /** Extrude buildings: this is what gives the shadow pass real 3D casters. */
    public static boolean INLINE_BUILDINGS_3D = false;
    /** Line widths of the inline style, as CartoCSS expressions - so they can be made
     *  zoom-dependent for testing how a line behaves as you zoom and tilt. The defaults widen
     *  with zoom the way a real style does; pass a plain number to pin a width instead. */
    public static String INLINE_ROAD_WIDTH = "linear([view::zoom], (12, 0.6), (18, 4.0))";
    public static String INLINE_MOTORWAY_WIDTH = "linear([view::zoom], (12, 1.5), (18, 9.0))";
    public static String INLINE_CONTOUR_WIDTH = "linear([view::zoom], (12, 0.4), (18, 1))";
    /** Extrusion lighting declared BY THE STYLE (needs --es styleLight true): intensity 0 keeps
     *  the legacy view-direction shading, above 0 is the soft normalised Lambert the terrain uses. */
    public static float INLINE_BUILDING_LIGHT = 1f;
    public static float INLINE_BUILDING_AMBIENT = 0.35f;
    /** Extrusion height in meters. Same vertex count at any value: the knob that separates the
     *  extrusion pass's fill cost from its vertex cost. */
    public static float INLINE_BUILDING_HEIGHT = 14f;
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
    /** LAYER-level comp-op on the '#landcover' block of the inline style ("multiply", "screen",
     *  "darken", ...). Empty = none. A layer comp-op is what routes a layer through the renderer's
     *  overlay buffer, which is also where the stencil tile masks are re-stamped - so this is the
     *  knob that exercises that path. */
    public static String INLINE_COMP_OP = "";

    public static float INLINE_LANDCOVER_OPACITY = 1.0f;
    public static int INLINE_SATELLITE_MIN_ZOOM = 11;
    public static String INLINE_HILLSHADE_SHADOW_COLOR = "#473B24";
    public static float INLINE_HILLSHADE_ILLUMINATION = 365f;
    /** Flip the 'show_relief' nuti parameter every N ms (StyleSource.NUTI). 0 = do not flip. */
    public static int NUTI_TOGGLE_INTERVAL_MS = 3000;

    // =============================================================================================
    // ROUTE TEST LAYER (LAYER_ROUTE_TEST)
    // A navigation-style route over GeoJSON vector tiles, so it goes through the SAME line
    // tesselator and shaders as the base map's roads (a Line vector element does NOT - it has its
    // own tesselator in LineDrawData). Zoom out: that is where a miter turns into a needle.
    // =============================================================================================

    /** Route geometry: <data dir>/<name> if present, else the APK asset of the same name. */
    public static String ROUTE_TEST_GEOJSON_NAME = "route-test.geojson";
    /** Casing drawn under the route (Google-Maps look). 0 = no casing. */
    public static float ROUTE_TEST_CASE_WIDTH = 16f;
    public static float ROUTE_TEST_WIDTH = 10f;
    public static String ROUTE_TEST_COLOR = "#4285F4";      // Google-navigation blue
    public static String ROUTE_TEST_CASE_COLOR = "#FFFFFF"; // white casing: the outline of the route
    /** miter | bevel | round. NOTE: the vt tesselator draws 'round' as a miter today. */
    public static String ROUTE_TEST_JOIN = "round";
    /** butt | square | round */
    public static String ROUTE_TEST_CAP = "round";
    /** CartoCSS line-miterlimit: miter length / line width above which the join falls back to a bevel. */
    public static float ROUTE_TEST_MITER_LIMIT = 4f;
    /** Simplify tolerance of the route source, in tile subpixels. Vertices closer together than the
     *  line is wide make every join fold over itself - the artifact reads as darker blobs on a
     *  translucent line, and it is why a route needs LESS geometry as it zooms out, not the same. */
    public static float ROUTE_TEST_SIMPLIFY = 0f;
    /** < 1 exposes the join over-blending: every overlapping triangle blends its alpha again. */
    public static float ROUTE_TEST_OPACITY = 1f;
    /** How the opacity is applied, which picks the renderer path that removes the join doubling:
     *  geom  = line-opacity, baked into the geometry colour -> the vt single-blend stencil pass;
     *  layer = layer opacity + comp-op -> the layer is drawn opaque into the overlay FBO and
     *          composited once (no seams, but a full-screen pass, and that buffer has no depth,
     *          so in 3D terrain the route stops being occluded by ridges). */
    public static String ROUTE_TEST_OPACITY_MODE = "geom";

    // =============================================================================================
    // GEOJSON TILE-BUILD BENCH (DemoTests.runGeoJSONBench)
    // Times GeoJSONVectorTileDataSource with no renderer in the way. '--es geojsonBench many|long|
    // both|<name>' runs it at startup; the panel has a button for each dataset.
    // =============================================================================================

    /** Empty = off. many | long | both | an asset (or data-directory) file name. */
    public static String GEOJSON_BENCH = "";
    /** MANY OBJECTS: 5000 short routes, ~165k points - the per-tile feature scan. */
    public static String GEOJSON_BENCH_MANY_NAME = "bench-many-routes.geojson";
    /** LONG LINES: 8 routes of 100-250 km, ~300k points - re-clipping a long line per tile. */
    public static String GEOJSON_BENCH_LONG_NAME = "bench-long-routes.geojson";
    public static int GEOJSON_BENCH_MIN_ZOOM = 8;
    public static int GEOJSON_BENCH_MAX_ZOOM = 17;
    /** Tiles per side around the data centre, at every zoom. 4 -> 16 tiles x 10 zooms = 160 tiles. */
    public static int GEOJSON_BENCH_TILES_PER_SIDE = 4;
    /** Adds a bench dataset as a REAL layer (route style) instead of timing tile builds, so the
     *  RENDER cost can be panned through. Empty = off. many | long | both | a file name. */
    public static String GEOJSON_BENCH_LAYER = "";

    // =============================================================================================
    // DEBUG / HARNESS
    // =============================================================================================

    /** false = no panel and no overlay text: clean screenshots for automated rendering checks. */
    /** Outline every tile each layer draws, on the ground: colour per zoom, brightness alternating
     *  with the tile parity, half opacity for a tile standing in with another tile's data. */
    public static boolean DEBUG_TILE_BORDERS = false;

    /** Drive the GL thread continuously instead of MapView's RENDERMODE_WHEN_DIRTY. */
    public static boolean CONTINUOUS_RENDER = false;

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
        LAYER_ROUTE_TEST = DemoCfg.cfgBool("routeTest", LAYER_ROUTE_TEST);

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
        TILE_LOD_FACTOR = DemoCfg.cfgFloat("lodFactor", TILE_LOD_FACTOR);
        LABEL_MAX_DISTANCE = DemoCfg.cfgFloat("labelMaxDistance", LABEL_MAX_DISTANCE);
        TERRAIN_ENABLED = DemoCfg.cfgBool("terrain", TERRAIN_ENABLED);
        TERRAIN_CAMERA_CLEARANCE = DemoCfg.cfgFloat("clearance", TERRAIN_CAMERA_CLEARANCE);
        TERRAIN_EXAGGERATION = DemoCfg.cfgFloat("exaggeration", TERRAIN_EXAGGERATION);
        TERRAIN_ANIM_MS = (long) DemoCfg.cfgFloat("terrainAnimMs", TERRAIN_ANIM_MS);
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
        SKY_FOG_BLEND = DemoCfg.cfgFloat("fogBlend", SKY_FOG_BLEND);
        SKY_FOG_HORIZON = DemoCfg.cfgFloat("fogHorizon", SKY_FOG_HORIZON);
        VIEW_DISTANCE_FACTOR = DemoCfg.cfgFloat("viewDistance", VIEW_DISTANCE_FACTOR);
        TERRAIN_MAX_TILE_ZOOM_COARSENING = DemoCfg.cfgInt("coarsening", TERRAIN_MAX_TILE_ZOOM_COARSENING);

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
        CONTOUR_INTERVAL_LADDER = DemoCfg.cfgStr("contourLadder", CONTOUR_INTERVAL_LADDER);
        CONTOUR_TILE_SUBSTITUTION = DemoCfg.cfgStr("contourSubst", CONTOUR_TILE_SUBSTITUTION);
        CONTOUR_MAX_OVERZOOM_STANDIN = DemoCfg.cfgInt("contourStandIn", CONTOUR_MAX_OVERZOOM_STANDIN);
        CONTOUR_TILE_CACHE_MB = DemoCfg.cfgInt("contourCacheMB", CONTOUR_TILE_CACHE_MB);
        BASE_TILE_CACHE_MB = DemoCfg.cfgInt("baseCacheMB", BASE_TILE_CACHE_MB);
        CONTOUR_RESOLUTION_LADDER = DemoCfg.cfgStr("contourResLadder", CONTOUR_RESOLUTION_LADDER);
        CONTOUR_SEAMLESS_EDGES = DemoCfg.cfgBool("contourSeamless", CONTOUR_SEAMLESS_EDGES);
        CONTOUR_LABEL_STUBS = DemoCfg.cfgBool("contourStubs", CONTOUR_LABEL_STUBS);
        CONTOUR_LABEL_INTERVAL = DemoCfg.cfgFloat("contourStubInterval", CONTOUR_LABEL_INTERVAL);
        CONTOUR_STUBS_FROM_TERRAIN = DemoCfg.cfgBool("stubsFromTerrain", CONTOUR_STUBS_FROM_TERRAIN);

        // route test layer
        ROUTE_TEST_GEOJSON_NAME = DemoCfg.cfgStr("routeGeojson", ROUTE_TEST_GEOJSON_NAME);
        ROUTE_TEST_WIDTH = DemoCfg.cfgFloat("routeWidth", ROUTE_TEST_WIDTH);
        ROUTE_TEST_CASE_WIDTH = DemoCfg.cfgFloat("routeCaseWidth", ROUTE_TEST_CASE_WIDTH);
        ROUTE_TEST_COLOR = DemoCfg.cfgColor("routeColor", ROUTE_TEST_COLOR);
        ROUTE_TEST_CASE_COLOR = DemoCfg.cfgColor("routeCaseColor", ROUTE_TEST_CASE_COLOR);
        ROUTE_TEST_JOIN = DemoCfg.cfgStr("routeJoin", ROUTE_TEST_JOIN);
        ROUTE_TEST_CAP = DemoCfg.cfgStr("routeCap", ROUTE_TEST_CAP);
        ROUTE_TEST_MITER_LIMIT = DemoCfg.cfgFloat("routeMiterLimit", ROUTE_TEST_MITER_LIMIT);
        ROUTE_TEST_OPACITY = DemoCfg.cfgFloat("routeOpacity", ROUTE_TEST_OPACITY);
        ROUTE_TEST_SIMPLIFY = DemoCfg.cfgFloat("routeSimplify", ROUTE_TEST_SIMPLIFY);
        ROUTE_TEST_OPACITY_MODE = DemoCfg.cfgStr("routeOpacityMode", ROUTE_TEST_OPACITY_MODE);
        GEOJSON_BENCH = DemoCfg.cfgStr("geojsonBench", GEOJSON_BENCH);
        GEOJSON_BENCH_MIN_ZOOM = DemoCfg.cfgInt("geojsonBenchMinZoom", GEOJSON_BENCH_MIN_ZOOM);
        GEOJSON_BENCH_MAX_ZOOM = DemoCfg.cfgInt("geojsonBenchMaxZoom", GEOJSON_BENCH_MAX_ZOOM);
        GEOJSON_BENCH_TILES_PER_SIDE = DemoCfg.cfgInt("geojsonBenchTiles", GEOJSON_BENCH_TILES_PER_SIDE);
        GEOJSON_BENCH_LAYER = DemoCfg.cfgStr("geojsonLayer", GEOJSON_BENCH_LAYER);

        // inline style
        INLINE_BACKGROUND_COLOR = DemoCfg.cfgColor("bg", INLINE_BACKGROUND_COLOR);
        INLINE_BUILDINGS_3D = DemoCfg.cfgBool("bld3d", INLINE_BUILDINGS_3D);
        INLINE_BUILDING_HEIGHT = DemoCfg.cfgFloat("bldHeight", INLINE_BUILDING_HEIGHT);
        INLINE_ROAD_WIDTH = DemoCfg.cfgStr("roadWidth", INLINE_ROAD_WIDTH);
        INLINE_MOTORWAY_WIDTH = DemoCfg.cfgStr("motorwayWidth", INLINE_MOTORWAY_WIDTH);
        INLINE_CONTOUR_WIDTH = DemoCfg.cfgStr("contourWidth", INLINE_CONTOUR_WIDTH);
        INLINE_BUILDING_LIGHT = DemoCfg.cfgFloat("bldLight", INLINE_BUILDING_LIGHT);
        INLINE_BUILDING_AMBIENT = DemoCfg.cfgFloat("bldAmbient", INLINE_BUILDING_AMBIENT);
        INLINE_STYLE_LIGHTING = DemoCfg.cfgBool("styleLight", INLINE_STYLE_LIGHTING);
        INLINE_LABELS = DemoCfg.cfgBool("labels", INLINE_LABELS);
        INLINE_STYLE_MINIMAL = DemoCfg.cfgBool("minimal", INLINE_STYLE_MINIMAL);
        INLINE_LANDCOVER_OPACITY = DemoCfg.cfgFloat("landcoverOpacity", INLINE_LANDCOVER_OPACITY);
        INLINE_COMP_OP = DemoCfg.cfgStr("compOp", INLINE_COMP_OP);
        INLINE_SATELLITE_MIN_ZOOM = DemoCfg.cfgInt("satZoom", INLINE_SATELLITE_MIN_ZOOM);
        NUTI_TOGGLE_INTERVAL_MS = DemoCfg.cfgInt("nutiInterval", NUTI_TOGGLE_INTERVAL_MS);

        // harness
        DEBUG_TILE_BORDERS = DemoCfg.cfgBool("tileBorders", DEBUG_TILE_BORDERS);
        CONTINUOUS_RENDER = DemoCfg.cfgBool("continuousRender", CONTINUOUS_RENDER);
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
