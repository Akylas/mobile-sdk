package com.akylas.cartotest.demo;

import android.content.Context;
import android.graphics.Typeface;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

/**
 * The on-screen debug panel: every {@link DemoConfig} knob, live.
 *
 * The panel NEVER touches SDK objects directly. It writes a DemoConfig field and then calls the
 * matching apply/rebuild method on {@link DemoMap}, which is the only place that knows how a
 * config value maps onto the SDK. Adding a knob is therefore: add the field in DemoConfig, apply
 * it in DemoMap.apply*(), add one line here.
 *
 * Panel is hidden behind the small gear button so the map stays unobstructed. '--es ui false'
 * skips it entirely (clean screenshots for automated rendering checks).
 */
public final class DemoPanel {

    private interface BoolSetting { void set(boolean value); }
    private interface FloatSetting { void set(float value); }
    private interface Action { void run(); }

    /** Live "z=.. tilt=.." readout, updated by the fragment's map listener. */
    public static TextView statusText;
    /** Shows which style was actually loaded (dir / zip / inline / nuti). */
    private static TextView styleText;
    /** Shows, per composite slot, whether the style actually declares it. */
    private static TextView slotText;

    /** Re-runs the slot check and updates both status lines. */
    private static void refreshStatus(DemoMap demo) {
        demo.checkCompositeSlots();
        if (slotText != null) {
            slotText.setText(demo.compositeStatus);
        }
        if (styleText != null) {
            styleText.setText("style: " + DemoStyles.lastLoadedDescription);
        }
    }

    public static void build(final Context context, View root, final DemoMap demo) {
        if (!DemoConfig.UI_ENABLED) {
            return;
        }
        LinearLayout panel = new LinearLayout(context);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setBackgroundColor(0xA0FFFFFF);
        panel.setPadding(10, 10, 10, 10);

        statusText = new TextView(context);
        statusText.setText("zoom -");
        panel.addView(statusText);
        styleText = label(context, panel, "style: " + DemoStyles.lastLoadedDescription);

        buildLayerSection(context, panel, demo);
        buildCompositeSection(context, panel, demo);
        buildTerrainSection(context, panel, demo);
        buildHillshadeSection(context, panel, demo);
        buildContourSection(context, panel, demo);
        buildRouteTestSection(context, panel, demo);
        buildSunSection(context, panel, demo);
        buildSkyFogSection(context, panel, demo);
        buildDebugSection(context, panel, demo);
        buildActionsSection(context, panel, demo);

        // The panel is taller than the screen: scroll it, and keep it behind a small toggle.
        final ScrollView scroll = new ScrollView(context);
        scroll.addView(panel);
        scroll.setVisibility(View.GONE);

        ConstraintLayout parent = (ConstraintLayout) root;
        ConstraintLayout.LayoutParams lp = new ConstraintLayout.LayoutParams(820, 1500);
        lp.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.bottomMargin = 320;
        lp.leftMargin = 10;
        parent.addView(scroll, lp);

        final Button toggle = new Button(context);
        toggle.setText("⚙");
        toggle.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                scroll.setVisibility(scroll.getVisibility() == View.GONE ? View.VISIBLE : View.GONE);
            }
        });
        ConstraintLayout.LayoutParams tlp = new ConstraintLayout.LayoutParams(150, ViewGroup.LayoutParams.WRAP_CONTENT);
        tlp.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID;
        tlp.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
        tlp.bottomMargin = 100; // clear of the system navigation bar
        tlp.leftMargin = 10;
        parent.addView(toggle, tlp);
    }

    // =============================================================================================
    // SECTIONS
    // =============================================================================================

    /** Add / remove whole layers, and pick what the base map is built from. */
    private static void buildLayerSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "LAYERS");
        for (final DemoMap.Feature feature : DemoMap.Feature.values()) {
            check(context, panel, feature.name().toLowerCase(), demo.isEnabled(feature), new BoolSetting() {
                public void set(boolean value) { demo.setEnabled(feature, value); }
            });
        }

        header(context, panel, "BASE MAP");
        // Switching either of these rebuilds the base layer with a new decoder / layer class.
        choice(context, panel, "mode", enumNames(DemoConfig.BaseMode.values()), DemoConfig.BASE_MODE.ordinal(), new IntSetting() {
            public void set(int index) {
                DemoConfig.BASE_MODE = DemoConfig.BaseMode.values()[index];
                demo.rebuildBaseLayer();
                refreshStatus(demo);
            }
        });
        choice(context, panel, "style", enumNames(DemoConfig.StyleSource.values()), DemoConfig.STYLE_SOURCE.ordinal(), new IntSetting() {
            public void set(int index) {
                DemoConfig.STYLE_SOURCE = DemoConfig.StyleSource.values()[index];
                demo.rebuildBaseLayer();
                refreshStatus(demo);
            }
        });
    }

    /** Sources woven INTO the base style (CompositeVectorTileLayer only). */
    private static void buildCompositeSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "COMPOSITE SLOTS");
        // A slot only exists if the STYLE declares a layer with that name; otherwise the source is
        // registered but never drawn. This line says which of the two it is, per slot.
        slotText = label(context, panel, demo.compositeStatus);
        check(context, panel, "#hillshade", DemoConfig.COMPOSITE_HILLSHADE, new BoolSetting() {
            public void set(boolean value) { DemoConfig.COMPOSITE_HILLSHADE = value; demo.syncCompositeSources(); refreshStatus(demo); }
        });
        check(context, panel, "#satellite", DemoConfig.COMPOSITE_SATELLITE, new BoolSetting() {
            public void set(boolean value) { DemoConfig.COMPOSITE_SATELLITE = value; demo.syncCompositeSources(); refreshStatus(demo); }
        });
        check(context, panel, "#contour", DemoConfig.COMPOSITE_CONTOUR, new BoolSetting() {
            public void set(boolean value) { DemoConfig.COMPOSITE_CONTOUR = value; demo.syncCompositeSources(); refreshStatus(demo); }
        });
        check(context, panel, "single-pass rendering", DemoConfig.COMPOSITE_SINGLE_PASS, new BoolSetting() {
            public void set(boolean value) {
                DemoConfig.COMPOSITE_SINGLE_PASS = value;
                if (demo.compositeLayer != null) {
                    demo.compositeLayer.setSinglePassRenderingEnabled(value);
                }
            }
        });
        // +1 = fetch the DEM one zoom level deeper than the base map.
        slider(context, panel, "#hillshade zoom bias", -2, 2, DemoConfig.COMPOSITE_HILLSHADE_ZOOM_BIAS, true, new FloatSetting() {
            public void set(float value) {
                DemoConfig.COMPOSITE_HILLSHADE_ZOOM_BIAS = Math.round(value);
                demo.syncCompositeSources();
            }
        });
    }

    private static void buildTerrainSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "TERRAIN");
        check(context, panel, "3D terrain", DemoConfig.TERRAIN_ENABLED, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_ENABLED = value; demo.animateTerrain(value); }
        });
        check(context, panel, "billboard occlusion", DemoConfig.TERRAIN_BILLBOARD_OCCLUSION, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_BILLBOARD_OCCLUSION = value; demo.terrainOptions.setBillboardOcclusionEnabled(value); }
        });
        slider(context, panel, "occlusion tolerance", 0f, 0.5f, DemoConfig.TERRAIN_OCCLUSION_TOLERANCE, false, new FloatSetting() {
            public void set(float value) { DemoConfig.TERRAIN_OCCLUSION_TOLERANCE = value; demo.terrainOptions.setBillboardOcclusionTolerance(value); }
        });
        // Exaggeration and the resolutions re-tesselate / drop every cached tile texture, so they
        // are applied on release only - applying them per pixel of drag is a guaranteed stall.
        slider(context, panel, "exaggeration", 0f, 3f, DemoConfig.TERRAIN_EXAGGERATION, true, new FloatSetting() {
            public void set(float value) { DemoConfig.TERRAIN_EXAGGERATION = value; demo.terrainOptions.setExaggeration(value); }
        });
        slider(context, panel, "mesh resolution", 16, 192, DemoConfig.TERRAIN_MESH_RESOLUTION, true, new FloatSetting() {
            public void set(float value) {
                DemoConfig.TERRAIN_MESH_RESOLUTION = Math.max(16, ((int) value / 16) * 16);
                demo.terrainOptions.setMeshResolution(DemoConfig.TERRAIN_MESH_RESOLUTION);
            }
        });
        check(context, panel, "drape fills (RTT)", DemoConfig.TERRAIN_DRAPE_FILLS, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_DRAPE_FILLS = value; demo.terrainOptions.setDrapeFillsEnabled(value); }
        });
        check(context, panel, "drape lines", DemoConfig.TERRAIN_DRAPE_LINES, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_DRAPE_LINES = value; demo.terrainOptions.setDrapeLinesEnabled(value); }
        });
        slider(context, panel, "drape resolution", 256, 2048, DemoConfig.TERRAIN_DRAPE_RESOLUTION, true, new FloatSetting() {
            public void set(float value) {
                DemoConfig.TERRAIN_DRAPE_RESOLUTION = Math.max(256, ((int) value / 256) * 256);
                demo.terrainOptions.setDrapeResolution(DemoConfig.TERRAIN_DRAPE_RESOLUTION);
            }
        });
        check(context, panel, "tile edge stitching", DemoConfig.TERRAIN_TILE_EDGE_STITCHING, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_TILE_EDGE_STITCHING = value; demo.terrainOptions.setTileEdgeStitchingEnabled(value); }
        });
        check(context, panel, "seamless tile edges", DemoConfig.TERRAIN_SEAMLESS_TILE_EDGES, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_SEAMLESS_TILE_EDGES = value; demo.terrainOptions.setSeamlessTileEdgesEnabled(value); }
        });
        check(context, panel, "elevation prefetch", DemoConfig.TERRAIN_ELEVATION_PREFETCH, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_ELEVATION_PREFETCH = value; demo.terrainOptions.setElevationPrefetchEnabled(value); }
        });
        check(context, panel, "background bitmap", DemoConfig.TERRAIN_BACKGROUND_BITMAP, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_BACKGROUND_BITMAP = value; demo.terrainOptions.setBackgroundBitmapEnabled(value); }
        });
    }

    /** Stand-alone hillshade layer (LAYERS > hillshade). */
    private static void buildHillshadeSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "HILLSHADE LAYER");
        final String[] methods = { "STANDARD", "COMBINED", "IGOR", "MULTIDIRECTIONAL", "BASIC" };
        int current = 2;
        for (int i = 0; i < methods.length; i++) {
            if (methods[i].equals(DemoConfig.HILLSHADE_METHOD)) {
                current = i;
            }
        }
        choice(context, panel, "method", methods, current, new IntSetting() {
            public void set(int index) { DemoConfig.HILLSHADE_METHOD = methods[index]; demo.applyHillshadeConfig(); }
        });
        slider(context, panel, "contrast", 0f, 1f, DemoConfig.HILLSHADE_CONTRAST, false, new FloatSetting() {
            public void set(float value) { DemoConfig.HILLSHADE_CONTRAST = value; demo.applyHillshadeConfig(); }
        });
        slider(context, panel, "height scale", 0f, 1f, DemoConfig.HILLSHADE_HEIGHT_SCALE, false, new FloatSetting() {
            public void set(float value) { DemoConfig.HILLSHADE_HEIGHT_SCALE = value; demo.applyHillshadeConfig(); }
        });
        slider(context, panel, "exaggeration", 0f, 3f, DemoConfig.HILLSHADE_EXAGGERATION, false, new FloatSetting() {
            public void set(float value) { DemoConfig.HILLSHADE_EXAGGERATION = value; demo.applyHillshadeConfig(); }
        });
        slider(context, panel, "illumination (deg)", 0, 360, DemoConfig.HILLSHADE_ILLUMINATION_DEGREES, false, new FloatSetting() {
            public void set(float value) { DemoConfig.HILLSHADE_ILLUMINATION_DEGREES = value; demo.applyHillshadeConfig(); }
        });
        check(context, panel, "illumination follows map", DemoConfig.HILLSHADE_ILLUMINATION_FOLLOWS_MAP, new BoolSetting() {
            public void set(boolean value) { DemoConfig.HILLSHADE_ILLUMINATION_FOLLOWS_MAP = value; demo.applyHillshadeConfig(); }
        });
        check(context, panel, "slope colouring shader", DemoConfig.HILLSHADE_SLOPES_SHADER, new BoolSetting() {
            public void set(boolean value) { DemoConfig.HILLSHADE_SLOPES_SHADER = value; demo.applyHillshadeConfig(); }
        });
        check(context, panel, "shader contour lines", DemoConfig.HILLSHADE_CONTOUR_LINES, new BoolSetting() {
            public void set(boolean value) { DemoConfig.HILLSHADE_CONTOUR_LINES = value; demo.applyHillshadeConfig(); }
        });
        slider(context, panel, "shader contour interval (m)", 10, 500, DemoConfig.HILLSHADE_CONTOUR_INTERVAL, true, new FloatSetting() {
            public void set(float value) { DemoConfig.HILLSHADE_CONTOUR_INTERVAL = value; demo.applyHillshadeConfig(); }
        });
        slider(context, panel, "shader contour width", 0.2f, 3f, DemoConfig.HILLSHADE_CONTOUR_WIDTH, true, new FloatSetting() {
            public void set(float value) { DemoConfig.HILLSHADE_CONTOUR_WIDTH = value; demo.applyHillshadeConfig(); }
        });
    }

    /** Geometry contours (ContourTileDataSource): shared by the layer and the composite slot. */
    private static void buildContourSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "CONTOUR SOURCE");
        // All of these re-generate tiles, so they apply on release and then drop the cached tiles.
        slider(context, panel, "base interval (m)", 1, 100, DemoConfig.CONTOUR_BASE_INTERVAL, true, new FloatSetting() {
            public void set(float value) { DemoConfig.CONTOUR_BASE_INTERVAL = value; reloadContours(demo); }
        });
        slider(context, panel, "resolution (samples)", 32, 256, DemoConfig.CONTOUR_RESOLUTION, true, new FloatSetting() {
            public void set(float value) { DemoConfig.CONTOUR_RESOLUTION = (int) value; reloadContours(demo); }
        });
        slider(context, panel, "simplify tolerance (px)", 0, 5, DemoConfig.CONTOUR_SIMPLIFY_TOLERANCE, true, new FloatSetting() {
            public void set(float value) { DemoConfig.CONTOUR_SIMPLIFY_TOLERANCE = value; reloadContours(demo); }
        });
        slider(context, panel, "min visible tile zoom", 0, 16, DemoConfig.CONTOUR_MIN_VISIBLE_ZOOM, true, new FloatSetting() {
            public void set(float value) { DemoConfig.CONTOUR_MIN_VISIBLE_ZOOM = (int) value; reloadContours(demo); }
        });
        check(context, panel, "seamless edges", DemoConfig.CONTOUR_SEAMLESS_EDGES, new BoolSetting() {
            public void set(boolean value) { DemoConfig.CONTOUR_SEAMLESS_EDGES = value; reloadContours(demo); }
        });
    }

    private static void reloadContours(DemoMap demo) {
        demo.applyContourConfig();
        // Generated tiles are cached by the layers, so drop them to see the new parameters.
        demo.contourSource().notifyTilesChanged(true);
    }

    /** Join / cap / opacity of the route test layer - the line tesselation bench. */
    private static void buildRouteTestSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "ROUTE TEST");
        final String[] joins = { "miter", "bevel", "round" };
        choice(context, panel, "join", joins, indexOf(joins, DemoConfig.ROUTE_TEST_JOIN), new IntSetting() {
            public void set(int index) { DemoConfig.ROUTE_TEST_JOIN = joins[index]; reloadRouteTest(demo); }
        });
        final String[] caps = { "butt", "square", "round" };
        choice(context, panel, "cap", caps, indexOf(caps, DemoConfig.ROUTE_TEST_CAP), new IntSetting() {
            public void set(int index) { DemoConfig.ROUTE_TEST_CAP = caps[index]; reloadRouteTest(demo); }
        });
        slider(context, panel, "width", 1, 30, DemoConfig.ROUTE_TEST_WIDTH, true, new FloatSetting() {
            public void set(float value) { DemoConfig.ROUTE_TEST_WIDTH = value; reloadRouteTest(demo); }
        });
        slider(context, panel, "casing width", 0, 40, DemoConfig.ROUTE_TEST_CASE_WIDTH, true, new FloatSetting() {
            public void set(float value) { DemoConfig.ROUTE_TEST_CASE_WIDTH = value; reloadRouteTest(demo); }
        });
        slider(context, panel, "miter limit", 1, 12, DemoConfig.ROUTE_TEST_MITER_LIMIT, true, new FloatSetting() {
            public void set(float value) { DemoConfig.ROUTE_TEST_MITER_LIMIT = value; reloadRouteTest(demo); }
        });
        slider(context, panel, "opacity", 0.1f, 1, DemoConfig.ROUTE_TEST_OPACITY, true, new FloatSetting() {
            public void set(float value) { DemoConfig.ROUTE_TEST_OPACITY = value; reloadRouteTest(demo); }
        });
    }

    /** The style is baked into the decoder, so the layer is rebuilt from scratch. */
    private static void reloadRouteTest(DemoMap demo) {
        demo.invalidate(DemoMap.Feature.ROUTE_TEST);
        demo.rebuildLayers();
    }

    private static int indexOf(String[] options, String value) {
        for (int i = 0; i < options.length; i++) {
            if (options[i].equalsIgnoreCase(value)) {
                return i;
            }
        }
        return 0;
    }

    private static void buildSunSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "SUN");
        check(context, panel, "terrain lighting", DemoConfig.TERRAIN_LIGHTING, new BoolSetting() {
            public void set(boolean value) { DemoConfig.TERRAIN_LIGHTING = value; demo.lightOptions.setTerrainLightingEnabled(value); }
        });
        check(context, panel, "day cycle (sun/moon/sky)", DemoConfig.DAY_CYCLE, new BoolSetting() {
            public void set(boolean value) { DemoConfig.DAY_CYCLE = value; demo.applyDayCycle(DemoConfig.DAY_CYCLE_HOUR); }
        });
        // With the day cycle on, the hour drives everything; otherwise it only moves the sun.
        slider(context, panel, "hour (UTC)", 0, 24, DemoConfig.DAY_CYCLE_HOUR, false, new FloatSetting() {
            public void set(float value) {
                DemoConfig.DAY_CYCLE_HOUR = value;
                if (DemoConfig.DAY_CYCLE) {
                    demo.applyDayCycle(value);
                } else {
                    DemoConfig.SUN_HOUR_UTC = value;
                    demo.applyLightOptions();
                }
            }
        });
        slider(context, panel, "azimuth", 0, 360, DemoConfig.SUN_AZIMUTH, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SUN_AZIMUTH = value; DemoConfig.SUN_HOUR_UTC = -1; demo.lightOptions.setSunAzimuth(value); }
        });
        slider(context, panel, "altitude", -10, 90, DemoConfig.SUN_ALTITUDE, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SUN_ALTITUDE = value; DemoConfig.SUN_HOUR_UTC = -1; demo.lightOptions.setSunAltitude(value); }
        });
        slider(context, panel, "sun intensity", 0, 2, DemoConfig.SUN_INTENSITY, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SUN_INTENSITY = value; demo.lightOptions.setSunIntensity(value); }
        });
        slider(context, panel, "ambient", 0, 1, DemoConfig.AMBIENT_INTENSITY, false, new FloatSetting() {
            public void set(float value) { DemoConfig.AMBIENT_INTENSITY = value; demo.lightOptions.setAmbientIntensity(value); }
        });

        header(context, panel, "SHADOWS");
        slider(context, panel, "strength", 0, 1, DemoConfig.SHADOW_STRENGTH, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SHADOW_STRENGTH = value; demo.lightOptions.setShadowStrength(value); }
        });
        slider(context, panel, "softness (texels)", 0, 4, DemoConfig.SHADOW_SOFTNESS, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SHADOW_SOFTNESS = value; demo.lightOptions.setShadowSoftness(value); }
        });
        // Reallocates the shadow map atlas, so apply on release only.
        slider(context, panel, "map size", 512, 4096, DemoConfig.SHADOW_MAP_SIZE, true, new FloatSetting() {
            public void set(float value) {
                DemoConfig.SHADOW_MAP_SIZE = Math.max(512, ((int) value / 512) * 512);
                demo.lightOptions.setShadowMapSize(DemoConfig.SHADOW_MAP_SIZE);
            }
        });
        slider(context, panel, "cascades", 1, 4, DemoConfig.SHADOW_CASCADES, true, new FloatSetting() {
            public void set(float value) { DemoConfig.SHADOW_CASCADES = Math.round(value); demo.lightOptions.setShadowCascades(DemoConfig.SHADOW_CASCADES); }
        });
        slider(context, panel, "distance (m, 0=all)", 0, 20000, DemoConfig.SHADOW_DISTANCE, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SHADOW_DISTANCE = value < 200 ? 0 : value; demo.lightOptions.setShadowDistance(DemoConfig.SHADOW_DISTANCE); }
        });
        slider(context, panel, "caster margin (tiles)", 0, 6, DemoConfig.SHADOW_CASTER_MARGIN, true, new FloatSetting() {
            public void set(float value) { DemoConfig.SHADOW_CASTER_MARGIN = Math.round(value); demo.lightOptions.setShadowCasterMargin(DemoConfig.SHADOW_CASTER_MARGIN); }
        });
        slider(context, panel, "depth bias (m)", 0f, 5f, DemoConfig.SHADOW_BIAS, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SHADOW_BIAS = value; demo.lightOptions.setShadowBias(value); }
        });
    }

    private static void buildSkyFogSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "SKY");
        check(context, panel, "sky", DemoConfig.SKY_ENABLED, new BoolSetting() {
            public void set(boolean value) { DemoConfig.SKY_ENABLED = value; demo.skyOptions.setEnabled(value); }
        });

        // Buildings come from the STYLE, so the switch rebuilds the base layer. Only the inline
        // style is generated here, so it is the one this can turn on and off; a dir/zip/nuti style
        // draws whatever it was authored with.
        check(context, panel, "3D buildings (inline style)", DemoConfig.INLINE_BUILDINGS_3D, new BoolSetting() {
            public void set(boolean value) { DemoConfig.INLINE_BUILDINGS_3D = value; demo.rebuildBaseLayer(); }
        });

        header(context, panel, "FOG / DISTANCE");
        check(context, panel, "fog", DemoConfig.FOG_ENABLED, new BoolSetting() {
            public void set(boolean value) {
                DemoConfig.FOG_ENABLED = value;
                if (value && DemoConfig.FOG_DISTANCE <= 0) {
                    DemoConfig.FOG_DISTANCE = 30000;
                }
                demo.applyTerrainOptions();
            }
        });
        slider(context, panel, "fog start (m)", 0, 40000, DemoConfig.FOG_START_DISTANCE, false, new FloatSetting() {
            public void set(float value) { DemoConfig.FOG_START_DISTANCE = value; demo.terrainOptions.setFogStartDistance(value); }
        });
        slider(context, panel, "fog distance (m, 0=off)", 0, 120000, DemoConfig.FOG_DISTANCE, false, new FloatSetting() {
            public void set(float value) { DemoConfig.FOG_DISTANCE = value < 500 ? 0 : value; demo.terrainOptions.setFogDistance(DemoConfig.FOG_DISTANCE); }
        });
        // How much of the SKY the same haze takes: the blend is the fade width, the horizon is the
        // angle it is still full at (below 0 on the slider = follow the terrain skyline).
        slider(context, panel, "sky fog blend (deg)", 0, 45, DemoConfig.SKY_FOG_BLEND, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SKY_FOG_BLEND = value; demo.skyOptions.setFogBlend(value); }
        });
        slider(context, panel, "sky fog horizon (deg, <0=auto)", -1, 30, DemoConfig.SKY_FOG_HORIZON, false, new FloatSetting() {
            public void set(float value) { DemoConfig.SKY_FOG_HORIZON = value; demo.skyOptions.setFogHorizon(value); }
        });
        // Changes the visible tile set, so apply on release only.
        slider(context, panel, "tile LOD (x tangram, 0=finest)", 0, 4, DemoConfig.TILE_LOD_FACTOR, true, new FloatSetting() {
            public void set(float value) { DemoConfig.TILE_LOD_FACTOR = value; demo.mapView.getOptions().setTileLODFactor(value); }
        });
        slider(context, panel, "tile coarsening (levels)", 0, 6, DemoConfig.TERRAIN_MAX_TILE_ZOOM_COARSENING, true, new FloatSetting() {
            public void set(float value) { DemoConfig.TERRAIN_MAX_TILE_ZOOM_COARSENING = (int) value; demo.terrainOptions.setMaxTileZoomCoarsening((int) value); }
        });
        slider(context, panel, "view distance (x tangram, 0=all)", 0, 4, DemoConfig.VIEW_DISTANCE_FACTOR, true, new FloatSetting() {
            public void set(float value) { DemoConfig.VIEW_DISTANCE_FACTOR = value < 0.05f ? 0 : value; demo.terrainOptions.setViewDistanceFactor(DemoConfig.VIEW_DISTANCE_FACTOR); }
        });
    }

    /** One-shot actions: post-process effects and the routing / search / geometry test cases. */
    private static void buildDebugSection(Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "DEBUG");
        check(context, panel, "tile borders", DemoConfig.DEBUG_TILE_BORDERS, new BoolSetting() {
            public void set(boolean value) { DemoConfig.DEBUG_TILE_BORDERS = value; demo.applyDebugConfig(); }
        });
    }

    private static void buildActionsSection(final Context context, LinearLayout panel, final DemoMap demo) {
        header(context, panel, "ACTIONS");
        check(context, panel, "relief outline effect", DemoConfig.RELIEF_OUTLINE, new BoolSetting() {
            public void set(boolean value) { demo.setReliefOutlineEnabled(value); }
        });
        button(context, panel, "offline routing test", new Action() {
            public void run() { DemoTests.runOfflineRouting(demo); }
        });
        button(context, panel, "online routing test", new Action() {
            public void run() { DemoTests.runOnlineRouting(demo); }
        });
        button(context, panel, "vector tile search test", new Action() {
            public void run() { DemoTests.runVectorTileSearch(demo); }
        });
        button(context, panel, "geojson line test", new Action() {
            public void run() { DemoTests.addGeoJSONLine(demo); }
        });
    }

    // =============================================================================================
    // WIDGET BUILDERS
    // =============================================================================================

    private interface IntSetting { void set(int index); }

    private static void header(Context context, LinearLayout panel, String label) {
        TextView text = new TextView(context);
        text.setText(label);
        text.setTextColor(0xFF204060);
        text.setPadding(0, 18, 0, 2);
        text.setTypeface(null, Typeface.BOLD);
        panel.addView(text);
    }

    private static TextView label(Context context, LinearLayout panel, String value) {
        TextView text = new TextView(context);
        text.setText(value);
        text.setTextSize(10);
        panel.addView(text);
        return text;
    }

    private static CheckBox check(Context context, LinearLayout panel, String label, boolean initial, final BoolSetting setting) {
        CheckBox box = new CheckBox(context);
        box.setText(label);
        box.setChecked(initial);
        box.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                setting.set(isChecked);
            }
        });
        panel.addView(box);
        return box;
    }

    /**
     * Continuous while dragging unless applyOnRelease: some settings (mesh/drape resolution,
     * contour generation) throw away every cached tile when they change, so applying them per
     * pixel of drag is a guaranteed stall.
     */
    private static void slider(Context context, LinearLayout panel, final String label,
                               float min, float max, float initial,
                               final boolean applyOnRelease, final FloatSetting setting) {
        final float lo = min, span = max - min;
        final TextView text = new TextView(context);
        text.setText(String.format("%s %.2f%s", label, initial, applyOnRelease ? " (release)" : ""));
        panel.addView(text);
        SeekBar seek = new SeekBar(context);
        seek.setMax(1000);
        seek.setProgress((int) ((initial - lo) / span * 1000));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            private float valueOf(SeekBar bar) { return lo + span * bar.getProgress() / 1000.0f; }
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                text.setText(String.format("%s %.2f%s", label, valueOf(seekBar), applyOnRelease ? " (release)" : ""));
                if (!applyOnRelease) {
                    setting.set(valueOf(seekBar));
                }
            }
            public void onStartTrackingTouch(SeekBar seekBar) { }
            public void onStopTrackingTouch(SeekBar seekBar) {
                setting.set(valueOf(seekBar));
            }
        });
        panel.addView(seek, new LinearLayout.LayoutParams(760, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /** A row of small buttons acting as a radio group (no spinner: less code, easier to read). */
    private static void choice(Context context, LinearLayout panel, String label,
                               final String[] options, int initial, final IntSetting setting) {
        TextView text = new TextView(context);
        text.setText(label);
        panel.addView(text);

        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(LinearLayout.HORIZONTAL);
        final Button[] buttons = new Button[options.length];
        for (int i = 0; i < options.length; i++) {
            final int index = i;
            Button button = new Button(context);
            button.setText(options[i]);
            button.setTextSize(9);
            button.setPadding(2, 2, 2, 2);
            button.setAllCaps(false);
            button.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    setting.set(index);
                    highlight(buttons, index);
                }
            });
            buttons[i] = button;
            row.addView(button, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        }
        highlight(buttons, initial);
        panel.addView(row);
    }

    private static void highlight(Button[] buttons, int selected) {
        for (int i = 0; i < buttons.length; i++) {
            buttons[i].setTypeface(null, i == selected ? Typeface.BOLD : Typeface.NORMAL);
            buttons[i].setAlpha(i == selected ? 1f : 0.55f);
        }
    }

    private static void button(Context context, LinearLayout panel, String label, final Action action) {
        Button button = new Button(context);
        button.setText(label);
        button.setAllCaps(false);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { action.run(); }
        });
        panel.addView(button);
    }

    private static String[] enumNames(Enum<?>[] values) {
        String[] names = new String[values.length];
        for (int i = 0; i < values.length; i++) {
            names[i] = values[i].name().toLowerCase();
        }
        return names;
    }

    private DemoPanel() {
    }
}
