package com.akylas.cartotest.demo;

import android.util.Log;

import com.carto.core.BinaryData;
import com.carto.styles.CartoCSSStyleSet;
import com.carto.styles.CompiledStyleSet;
import com.carto.utils.AndroidAssetPackage;
import com.carto.utils.AssetPackage;
import com.carto.utils.DirAssetPackage;
import com.carto.utils.ZippedAssetPackage;
import com.carto.vectortiles.MBVectorTileDecoder;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;

/**
 * Everything style-related: builds the tile decoder for each {@link DemoConfig.StyleSource}.
 *
 * The four sources answer four different questions:
 *  - DIR    : "does my style folder render?" - DirAssetPackage reads the style straight from a
 *             FOLDER on the device, so editing a .mss and restarting the app is the whole loop;
 *  - ZIP    : the same style, packaged (what production ships);
 *  - INLINE : a self-contained CartoCSS string - no file on the device, always works, and is the
 *             style that documents the composite slot syntax ('#hillshade', '#satellite', ...);
 *  - NUTI   : an in-memory style PROJECT (project.json + style.mss zipped). Only a project can
 *             declare 'nuti::' parameters, which is how a user setting drives the style at runtime;
 *  - ASSETS : the style PROJECT bundled in the APK (app/src/main/assets/style), read with
 *             AndroidAssetPackage. APK assets are not files, so DirAssetPackage cannot read them.
 *             This is also the reference example of a style that composite slots work with.
 */
public final class DemoStyles {

    private static final String TAG = "DemoStyles";

    /** Set by {@link #create} to whatever was really loaded, for the panel/log to show. */
    public static String lastLoadedDescription = "";

    /**
     * Builds a decoder for the given style source, falling back down the list (dir -> zip ->
     * inline) rather than crashing: on a device without the data files the demo must still start.
     */
    public static MBVectorTileDecoder create(DemoConfig.StyleSource source, String dataPath) {
        switch (source) {
            case DIR: {
                AssetPackage pack = openDir(dataPath);
                if (pack == null) {
                    pack = openZip(dataPath);
                }
                if (pack != null) {
                    return new MBVectorTileDecoder(new CompiledStyleSet(pack));
                }
                break;
            }
            case ZIP: {
                AssetPackage pack = openZip(dataPath);
                if (pack != null) {
                    return new MBVectorTileDecoder(new CompiledStyleSet(pack));
                }
                break;
            }
            case ASSETS: {
                AssetPackage pack = openAppAssets();
                if (pack != null) {
                    return new MBVectorTileDecoder(new CompiledStyleSet(pack, "osm"));
                }
                break;
            }
            case NUTI: {
                MBVectorTileDecoder decoder = createNutiDecoder();
                if (decoder != null) {
                    return decoder;
                }
                break;
            }
            case INLINE:
            default:
                break;
        }
        lastLoadedDescription = "inline CartoCSS";
        return new MBVectorTileDecoder(new CartoCSSStyleSet(inlineStyle()));
    }

    /**
     * The live-editable style: a plain folder on the device, read through the SDK's
     * DirAssetPackage. Push a modified style with:
     *   adb push my-style/. /sdcard/alpimaps_mbtiles/osm/
     * and restart the app - no repackaging, no rebuild.
     */
    private static AssetPackage openDir(String dataPath) {
        String dirPath = dataPath + "/" + DemoConfig.STYLE_DIR_NAME;
        try {
            DirAssetPackage pack = new DirAssetPackage(dirPath);
            lastLoadedDescription = "dir " + dirPath + " (" + pack.getAssetNames().size() + " assets)";
            Log.i(TAG, "style: " + lastLoadedDescription);
            return pack;
        } catch (Exception e) {
            // Missing folder is the normal case on a device where only osm.zip was pushed.
            Log.w(TAG, "style dir not usable (" + dirPath + "): " + e.getMessage());
            return null;
        }
    }

    /**
     * The style bundled in the APK. Note the asset manager must already be connected, which a
     * MapView does when it is constructed - so this cannot be called before the map view exists.
     */
    private static AssetPackage openAppAssets() {
        try {
            AndroidAssetPackage pack = new AndroidAssetPackage(DemoConfig.STYLE_ASSETS_PATH);
            lastLoadedDescription = "app assets " + DemoConfig.STYLE_ASSETS_PATH
                    + " (" + pack.getAssetNames().size() + " assets)";
            Log.i(TAG, "style: " + lastLoadedDescription);
            return pack;
        } catch (Exception e) {
            Log.w(TAG, "app asset style not usable (" + DemoConfig.STYLE_ASSETS_PATH + "): " + e.getMessage());
            return null;
        }
    }

    private static AssetPackage openZip(String dataPath) {
        String zipPath = dataPath + "/" + DemoConfig.STYLE_ZIP_NAME;
        try {
            lastLoadedDescription = "zip " + zipPath;
            Log.i(TAG, "style: " + lastLoadedDescription);
            return new ZippedAssetPackage(readFile(zipPath));
        } catch (Exception e) {
            Log.w(TAG, "style zip not usable (" + zipPath + "): " + e.getMessage());
            return null;
        }
    }

    /** Style zip of the offline routes layer. */
    public static MBVectorTileDecoder createZipDecoder(String zipPath) {
        try {
            return new MBVectorTileDecoder(new CompiledStyleSet(new ZippedAssetPackage(readFile(zipPath))));
        } catch (Exception e) {
            Log.w(TAG, "could not read " + zipPath + ": " + e.getMessage());
            return null;
        }
    }

    public static BinaryData readFile(String path) throws java.io.IOException {
        File file = new File(path);
        FileInputStream stream = new FileInputStream(file);
        try {
            DataInputStream dataInputStream = new DataInputStream(stream);
            byte[] bytes = new byte[(int) file.length()];
            dataInputStream.readFully(bytes);
            return new BinaryData(bytes);
        } finally {
            stream.close();
        }
    }

    // =============================================================================================
    // INLINE STYLE
    // OpenMapTiles schema (openfreemap / akylas tiles). Text is omitted on purpose: labels need a
    // font asset package, which a raw CartoCSS string cannot provide.
    //
    // COMPOSITE SLOTS: in a CompositeVectorTileLayer the position of a source in the draw order is
    // the position of the FIRST rule referencing its '#name'; the '#name { ... }' block is where
    // the per-source settings live (they accept zoom-dependent expressions).
    // =============================================================================================

    /** 'polygon-opacity' for the ground-shaped fills, or nothing at all while they are opaque -
     *  so the default style string is byte-identical to what it was before the knob existed. */
    private static String landcoverOpacity() {
        if (DemoConfig.INLINE_LANDCOVER_OPACITY >= 1.0f) {
            return "";
        }
        return " polygon-opacity: " + DemoConfig.INLINE_LANDCOVER_OPACITY + ";";
    }

    public static String inlineStyle() {
        StringBuilder map = new StringBuilder("Map { background-color: ").append(DemoConfig.INLINE_BACKGROUND_COLOR).append(";");
        if (DemoConfig.INLINE_STYLE_LIGHTING) {
            // The same sun/shadow/fog values the code sets on LightOptions/TerrainOptions, but
            // expressed IN the style - and zoom-dependent, which only the style can do.
            map.append(" terrain-lighting: 1;")
               .append(" sun-azimuth: 250;")
               .append(" sun-altitude: linear([view::zoom], (11, 55), (15, 12));")
               .append(" sun-intensity: 1;")
               .append(" ambient-intensity: 0.4;")
               .append(" shadow-strength: 0.8;")
               .append(" shadow-softness: 1;")
               .append(" fog-color: #b8c6d8;")
               .append(" fog-start-distance: 1500;")
               .append(" fog-distance: linear([view::zoom], (11, 60000), (15, 12000));")
               .append(" terrain-max-visible-distance: 40000;");
        }
        map.append(" }");

        if (DemoConfig.INLINE_STYLE_MINIMAL) {
            // Background plus the composite slots only: no vector geometry, so a frame costs the
            // terrain and the slots and nothing else. The slot blocks have to stay - a source's
            // position in the draw order IS the position of the first rule naming it.
            return String.join("\n",
                map.toString(),
                "#hillshade[zoom>=4][zoom<=19] {",
                "  hillshade-illumination-direction: " + (int) DemoConfig.INLINE_HILLSHADE_ILLUMINATION + ";",
                "  hillshade-shadow-color: " + DemoConfig.INLINE_HILLSHADE_SHADOW_COLOR + ";",
                "}",
                "#satellite[zoom>=" + DemoConfig.INLINE_SATELLITE_MIN_ZOOM + "] { raster-opacity: 1; raster-comp-op: src-over; }");
        }

        return String.join("\n",
            map.toString(),
            "#water { polygon-fill: #9cc3e0; }",
            // Ground-shaped fills carry the landcover opacity: opaque by default, translucent when
            // the hillshade and the contours underneath have to read through them (tangram's
            // 'translucent-polygons', alpha 0.25).
            "#landuse { polygon-fill: #dddddd;" + landcoverOpacity() + " }",
            "#landcover { polygon-fill: #dbe8cc;" + landcoverOpacity() + " }",
            // --- composite slots, in draw order ---
            "#satellite[zoom>=" + DemoConfig.INLINE_SATELLITE_MIN_ZOOM + "] { raster-opacity: 1; raster-comp-op: src-over; }",
            "#hillshade[zoom>=4][zoom<=19] {",
            "  hillshade-illumination-direction: " + (int) DemoConfig.INLINE_HILLSHADE_ILLUMINATION + ";",
            "  hillshade-shadow-color: " + DemoConfig.INLINE_HILLSHADE_SHADOW_COLOR + ";",
            "}",
            "#transportation { line-color: #ffffff; line-width: 1.2;}",
            DemoConfig.INLINE_LABELS
                ? String.join("\n",
                    "#transportation_name {",
                        "text-name: [name];",
                        "text-fill: #000000;",
                        " text-spacing: 10;",
                        "text-placement: line;",
                        "text-size: 10;",
                        " }")
                : "",
            "#transportation['class'='motorway'] { line-color: #e27d60; line-width: 3; }",
            DemoConfig.INLINE_BUILDINGS_3D
                ? "#building[zoom>=14] { building-fill: #d9cfc4; building-height: 14; }"
                : "#building[zoom>=14] { polygon-fill: #d9cfc4; }",
            "#contour[zoom>=" + DemoConfig.CONTOUR_MIN_VISIBLE_ZOOM + "] {",
                "  line-color: #C56008;",
                "  line-width: 0.8;",
                "  line-opacity: 0.4;",
                "  [div>=50]  { line-opacity: 0.7; line-width: 1.0; }",
                "  [div>=100] { line-opacity: 0.9; line-width: 1.4; }",
                "  [div>=500] { line-width: 2.0; }",
                DemoConfig.INLINE_LABELS
                ? String.join("\n",
                    "[div=1000][zoom>=12],",
                    "[div=500][zoom>=12],",
                    "[div=200][zoom>=14],",
                    "[div=250][zoom>=13][zoom<14],",
                    "[div=100][zoom>=14],",
                    "[div=50][zoom>=15] {",
                    "text-name: [ele]+' m';",
                    "text-fill: #000000;",
                    " text-spacing: 10;",
                    "text-placement: line;",
                    // [zoom] here is the CONTOUR TILE zoom, which never drops below the DEM zoom - it
                    // does not gate on the camera. [view::zoom] is evaluated per frame, and size 0
                    // hides the label.
                    "text-size: linear([view::zoom], (11.99, 0), (12, 14));",
                    "}")
                : "",
            "  contour-base-interval: " + (int) DemoConfig.CONTOUR_BASE_INTERVAL + ";",
            "}");
    }

    /**
     * Style of the STAND-ALONE contour layer (DemoConfig.LAYER_CONTOUR).
     * ContourTileDataSource exposes 'ele' (metres) and 'div' (importance = largest nice divisor),
     * so the whole look is CartoCSS. NOTE: in CartoCSS 'zoom' is the TILE zoom, not the camera
     * zoom - contour tiles are generated at the DEM source zoom.
     */
    public static String contourStyle() {
        return String.join("\n",
            "#contour {",
            "  line-color: #C56008;",
            "  line-width: 0.8;",
            "  line-opacity: 0.4;",
            "  [div>=50]  { line-opacity: 0.7; line-width: 1.0; }",
            "  [div>=100] { line-opacity: 0.9; line-width: 1.4; }",
            "  [div>=500] { line-width: 2.0; }",
            // Labels need a font asset package (text-face-name -> a bundled font), so they are
            // only available with a DIR/ZIP style, not with this raw CartoCSS string.
            "}");
    }

    /**
     * Style of the PRE-BAKED contour tile layer (DemoConfig.LAYER_CONTOUR_TILES).
     *
     * This is the '#contour' block of assets/style/shared/terrain.less, verbatim except that the
     * style variables are inlined with their osm/style.less + shared/style.less values and the
     * ['nuti::contours'>0] guard is dropped (a raw CartoCSS string can not declare nuti
     * parameters). Same rules, same 'ele'/'div' attributes as the generated contours, so the two
     * layers can be compared one against the other.
     *
     * NOTE: 'zoom' is the TILE zoom. The tileset stops at z14, so the [zoom>=15] label rule of the
     * original never fires here (kept as-is on purpose - it does not in the real style either
     * until the source goes deeper).
     */
    public static String contourTilesStyle() {
        String font = DemoConfig.CONTOUR_TILES_FONT;
        return String.join("\n",
            "#contour {",
            "  [div=10][zoom>=14],",
            "  [div=20][zoom>=14] {",
            "    line-color: #226600;",
            "    line-opacity: 0.2;",                                              // @contour_opacity * 0.5
            "    line-width: linear([view::zoom], (16, 0.6), (22, 1.6));",
            "  }",
            "",
            "  [div=100][zoom>=12],",
            "  [div=200][zoom>=12],",
            "  [div=50][zoom>=13] {",
            "    line-color: #226600;",
            "    line-opacity: step([view::zoom], (12, 0.2), (14, 0.4));",         // @contour_opacity_semi
            "    line-width: linear([view::zoom], (16, 0.6), (22, 1.6));",
            "  }",
            "",
            "  [div=1000][zoom>=12],",
            "  [div=500][zoom>=12],",
            "  [div=250][zoom>=13][zoom<14] {",
            "    line-color: #226600;",
            "    line-opacity: 0.4;",                                              // [nuti::contoursOpacity]
            "    line-width: linear([view::zoom], (16, 0.6), (22, 1.6));",
            "  }",
            "",
            "  [div=1000][zoom>=12],",
            "  [div=500][zoom>=12],",
            "  [div=200][zoom>=14],",
            "  [div=250][zoom>=13][zoom<14],",
            "  [div=100][zoom>=14],",
            "  [div=50][zoom>=15] {",
            "    text-face-name: '" + font + "';",
            "    text-name: [ele]+' m';",
            "    text-fill: #226600;",
            "    text-spacing: 10;",
            "    text-placement: line;",
            "    text-halo-radius: 1;",
            "    text-halo-fill: #f2f5f888;",
            "    text-size: linear([view::zoom], (12, 7), (16, 8), (20, 9));",
            "  }",
            "}");
    }

    // =============================================================================================
    // NUTI PARAMETER STYLE
    // 'nuti::' parameters are user settings the style reacts to at runtime
    // (decoder.setStyleParameter). They can only be DECLARED in a style project, so the project is
    // built in memory here: project.json + style.mss, zipped, wrapped in a CompiledStyleSet.
    // =============================================================================================

    /** Name of the boolean parameter the demo flips; see DemoMap.startNutiToggleLoop. */
    public static final String NUTI_PARAMETER = "show_relief";

    private static MBVectorTileDecoder createNutiDecoder() {
        // 'layers' is TOP -> BOTTOM (reversed into draw order) and must list every composite slot.
        String projectJson = String.join("\n",
            "{",
            "  \"styles\": [\"style.mss\"],",
            "  \"layers\": [\"contour\", \"building\", \"transportation\", \"satellite\", \"hillshade\", \"landcover\", \"water\"],",
            "  \"nutiparameters\": { \"" + NUTI_PARAMETER + "\": { \"default\": true } }",
            "}");
        String mss = String.join("\n",
            "Map { background-color: " + DemoConfig.INLINE_BACKGROUND_COLOR + "; }",
            "#water { polygon-fill: #9cc3e0; }",
            "#landcover { polygon-fill: #dbe8cc;" + landcoverOpacity() + " }",
            // the hillshade slot exists only while the user setting is on
            "#hillshade['nuti::" + NUTI_PARAMETER + "'=true][zoom>=4] {",
            "  hillshade-opacity: linear([view::zoom], (4, 0.5), (12, 0.9));",
            "  hillshade-exaggeration: linear([view::zoom], (4, 0.6), (12, 1.4));",
            "  hillshade-illumination-direction: 315;",
            "  hillshade-shadow-color: #103040;",
            "}",
            "#satellite[zoom>=" + DemoConfig.INLINE_SATELLITE_MIN_ZOOM + "] { raster-opacity: 0.45; }",
            "#transportation { line-color: #ffffff; line-width: 1.2; }",
            "#transportation['class'='motorway'] { line-color: #e27d60; line-width: 3; }",
            DemoConfig.INLINE_BUILDINGS_3D
                ? "#building[zoom>=14] { building-fill: #d9cfc4; building-height: 14; }"
                : "#building[zoom>=14] { polygon-fill: #d9cfc4; }",
            "#contour[zoom>=12] { line-color: #9a5a12; line-width: 0.8; line-opacity: 0.7; }");

        try {
            java.io.ByteArrayOutputStream bos = new java.io.ByteArrayOutputStream();
            java.util.zip.ZipOutputStream zos = new java.util.zip.ZipOutputStream(bos);
            String[][] entries = new String[][] { { "project.json", projectJson }, { "style.mss", mss } };
            for (String[] entry : entries) {
                zos.putNextEntry(new java.util.zip.ZipEntry(entry[0]));
                zos.write(entry[1].getBytes("UTF-8"));
                zos.closeEntry();
            }
            zos.close();
            lastLoadedDescription = "in-memory nuti project";
            return new MBVectorTileDecoder(new CompiledStyleSet(new ZippedAssetPackage(new BinaryData(bos.toByteArray()))));
        } catch (Exception e) {
            Log.e(TAG, "could not build the nuti project style", e);
            return null;
        }
    }

    // =============================================================================================
    // SHADERS used by the hillshade / custom raster layers
    // =============================================================================================

    /**
     * Slope colouring: replaces the hillshade lighting with steepness bands (ski-touring style).
     * A custom normal-map lighting shader returns a PREMULTIPLIED colour and must be transparent
     * where it draws nothing, otherwise it greys out the map below.
     */
    public static String slopesShader() {
        return String.join("\n",
            "uniform vec4 u_shadowColor;",
            "uniform vec4 u_highlightColor;",
            "uniform vec4 u_accentColor;",
            "uniform vec3 u_lightDir;",
            "vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {",
            "    mediump float lighting = max(0.0, dot(normal, u_lightDir));",
            "    mediump float slope = acos(dot(normal, surfaceNormal)) * 180.0 / 3.14159 * 1.2;",
            "    if (slope >= 45.0) { return vec4(0.378, 0.272, 0.358, 0.5); }",
            "    if (slope >= 40.0) { return vec4(0.5, 0.0, 0.0, 0.5); }",
            "    if (slope >= 35.0) { return vec4(0.455, 0.231, 0.111, 0.5); }",
            "    if (slope >= 30.0) { return vec4(0.470, 0.451, 0.153, 0.5); }",
            "    return vec4(0.0, 0.0, 0.0, 0.0);",
            "}");
    }

    /**
     * Hypsometric tint for the CustomRasterTileLayer: decodes terrarium elevation from the RAW DEM
     * texel (getRawColor()) and colours it by height. Shows that the custom-raster base class can
     * run any filter shader over any raster source, not just hillshading.
     */
    public static String hypsometricShader() {
        return String.join("\n",
            "vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {",
            "  vec4 c = getRawColor();",
            "  float h = (c.r * 255.0 * 256.0 + c.g * 255.0 + c.b * 255.0 / 256.0) - 32768.0;",
            "  float t = clamp(h / 3000.0, 0.0, 1.0);",
            "  vec3 col = mix(vec3(0.2, 0.4, 0.8), vec3(0.9, 0.9, 0.4), t);",
            "  col = mix(col, vec3(0.5, 0.3, 0.1), clamp((h - 1500.0) / 1500.0, 0.0, 1.0));",
            "  return vec4(col, 1.0);",
            "}");
    }

    private DemoStyles() {
    }
}
