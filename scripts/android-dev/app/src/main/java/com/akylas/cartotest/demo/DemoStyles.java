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
            case POI: {
                // The CartoCSS is written here, the FONTS come from the APK asset package: a shield
                // icon shaped from osm.ttf needs a font, and a bare CartoCSS string carries none.
                AssetPackage pack = openAppAssets();
                if (pack != null) {
                    lastLoadedDescription = "shield test style + app asset fonts";
                    return new MBVectorTileDecoder(new CartoCSSStyleSet(poiTestStyle(), pack));
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

    /** A LAYER-level 'comp-op', which is the one that makes the renderer composite the layer
     *  through its overlay buffer (and re-stamp the stencil tile masks into it) instead of
     *  drawing it straight into the frame. Symbolizer-level properties such as
     *  'polygon-comp-op' do NOT take that path, so this is what exercises it. */
    private static String compOp() {
        if (DemoConfig.INLINE_COMP_OP == null || DemoConfig.INLINE_COMP_OP.isEmpty()) {
            return "";
        }
        return " comp-op: " + DemoConfig.INLINE_COMP_OP + ";";
    }

    /** An ARGB int as the '#rrggbb' CartoCSS literal (the alpha goes in a *-opacity property). */
    private static String hex(int argb) {
        return String.format("#%06X", argb & 0xFFFFFF);
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
               // Extrusion lighting, declared by the STYLE: keeps the soft normalised-Lambert
               // walls whatever terrain lighting does, instead of the harder legacy shading.
               .append(" building-light-intensity: " + DemoConfig.INLINE_BUILDING_LIGHT + ";")
               .append(" building-ambient: " + DemoConfig.INLINE_BUILDING_AMBIENT + ";")
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
            "#landcover { polygon-fill: #dbe8cc;" + landcoverOpacity() + compOp() + " }",
            // --- composite slots, in draw order ---
            "#satellite[zoom>=" + DemoConfig.INLINE_SATELLITE_MIN_ZOOM + "] { raster-opacity: 1; raster-comp-op: src-over; }",
            "#hillshade[zoom>=4][zoom<=19] {",
            "  hillshade-illumination-direction: " + (int) DemoConfig.INLINE_HILLSHADE_ILLUMINATION + ";",
            "  hillshade-shadow-color: " + DemoConfig.INLINE_HILLSHADE_SHADOW_COLOR + ";",
            // The composite slot takes its contour settings from the STYLE, not from the
            // HillshadeRasterTileLayer setters (those only reach the stand-alone layer) - so this
            // is what turns the shader-drawn contour lines on in the composite base.
            DemoConfig.HILLSHADE_CONTOUR_LINES
                ? String.join("\n",
                    "  hillshade-contour-interval: " + (int) DemoConfig.HILLSHADE_CONTOUR_INTERVAL + ";",
                    "  hillshade-contour-width: " + DemoConfig.HILLSHADE_CONTOUR_WIDTH + ";",
                    "  hillshade-contour-color: " + DemoStyles.hex(DemoConfig.HILLSHADE_CONTOUR_COLOR_ARGB) + ";")
                : "",
            "}",
            "#transportation { line-color: #ffffff; line-width: " + DemoConfig.INLINE_ROAD_WIDTH + "; }",
            DemoConfig.INLINE_LABELS
                ? String.join("\n",
                    "#transportation_name {",
                        "text-name: [name];",
                        "text-fill: #000000;",
                        " text-spacing: 10;",
                        "text-placement: line;",
                        "text-size: 10;",
                        DemoConfig.LABEL_MAX_DISTANCE > 0
                            ? "text-max-distance: " + DemoConfig.LABEL_MAX_DISTANCE + ";"
                            : "",
                        " }")
                : "",
            "#transportation['class'='motorway'] { line-color: #e27d60; line-width: " + DemoConfig.INLINE_MOTORWAY_WIDTH + "; }",
            DemoConfig.INLINE_BUILDINGS_3D
                ? "#building[zoom>=14] { building-fill: #d9cfc4; building-height: " + DemoConfig.INLINE_BUILDING_HEIGHT + "; }"
                : "#building[zoom>=14] { polygon-fill: #d9cfc4; }",
            "#contour[zoom>=" + DemoConfig.CONTOUR_MIN_VISIBLE_ZOOM + "] {",
                // Lines only for the traced geometry: a label stub is a ~20 point fragment of a
                // contour, long enough to lay text along and nothing more, so drawing it as a line
                // paints dashes over the map. Both modes carry 'stub', so the filter is safe in
                // either. In stub mode the LINES come from the hillshade shader instead.
                "  [stub=0] {",
                "    line-color: #C56008;",
                contourWidthByDiv(),
                "  }",
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
            // The composite slot reads the source's generation parameters from the style too.
            DemoConfig.CONTOUR_LABEL_STUBS
                ? String.join("\n",
                    "  contour-label-stubs: 1;",
                    "  contour-label-interval: " + (int) DemoConfig.CONTOUR_LABEL_INTERVAL + ";")
                : "",
            "}");
    }

    // =============================================================================================
    // SHIELD TEST STYLE (StyleSource.POI)
    // One shield rule per label: an ICON that stays on the feature and a NAME the culler puts on
    // whichever side is free ('shield-anchors'), falling back to the icon alone when none is
    // ('shield-text-optional'). The icon is a GLYPH of assets/style/fonts/osm.ttf - the same font
    // the real style uses - so it costs one atlas cell and no bitmap.
    //
    // Deliberately dense: every '#poi' and every '#place' carries one, which is what makes the
    // side selection visible (and what a perf comparison needs).
    // =============================================================================================

    /** A PUA glyph of assets/style/fonts/osm.ttf, as the real style's 'nuti::osm-*' values have them. */
    private static final String ICON_DOT = "\ue934";
    private static final String ICON_PEAK = "\uea04";
    private static final String ICON_RESTAURANT = "\ue919";
    private static final String ICON_HOTEL = "\ue9d6";
    private static final String ICON_CAFE = "\ue990";

    /** The shield properties shared by every rule of the test style. */
    private static String shieldCommon(String icon, String fill, float size) {
        StringBuilder mss = new StringBuilder();
        mss.append("  shield-face-name: 'DIN Pro Medium';\n");
        mss.append("  shield-size: ").append(size).append(";\n");
        mss.append("  shield-fill: ").append(fill).append(";\n");
        mss.append("  shield-halo-fill: #ffffff;\n");
        mss.append("  shield-halo-radius: 1.5;\n");
        mss.append("  shield-text-dx: ").append(DemoConfig.POI_TEXT_DX).append(";\n");
        mss.append("  shield-wrap-width: ").append(DemoConfig.POI_WRAP_WIDTH).append(";\n");
        mss.append("  shield-wrap-character: ' ';\n");
        if (DemoConfig.POI_BITMAP_ICON) {
            mss.append("  shield-file: url(shields/place.svg);\n");
        }
        if (DemoConfig.POI_FONT_ICON) {
            mss.append("  shield-icon-name: '").append(icon).append("';\n");
            mss.append("  shield-icon-face-name: 'osm';\n");
            mss.append("  shield-icon-size: ").append(size + 4f).append(";\n");
            mss.append("  shield-icon-fill: ").append(fill).append(";\n");
        }
        if (DemoConfig.POI_TEXT_ALIGN != null && !DemoConfig.POI_TEXT_ALIGN.trim().isEmpty()) {
            mss.append("  shield-text-horizontal-alignment: '").append(DemoConfig.POI_TEXT_ALIGN.trim()).append("';\n");
        }
        if (DemoConfig.POI_TEXT_BG) {
            mss.append("  shield-background-fill: #ffffff;\n");
            mss.append("  shield-background-opacity: 0.85;\n");
            mss.append("  shield-background-radius: ").append(DemoConfig.POI_BG_RADIUS).append(";\n");
            mss.append("  shield-background-padding-x: ").append(DemoConfig.POI_BG_PADDING).append(";\n");
            mss.append("  shield-background-padding-y: ").append(DemoConfig.POI_BG_PADDING * 0.6f).append(";\n");
            if (DemoConfig.POI_BG_BORDER > 0) {
                mss.append("  shield-background-border-fill: ").append(fill).append(";\n");
                mss.append("  shield-background-border-width: ").append(DemoConfig.POI_BG_BORDER).append(";\n");
            }
        }
        if (DemoConfig.POI_ICON_BG) {
            mss.append("  shield-icon-background-fill: #ffffff;\n");
            mss.append("  shield-icon-background-opacity: 0.9;\n");
            mss.append("  shield-icon-background-radius: 20;\n");   // a pill around the icon
            mss.append("  shield-icon-background-padding-x: ").append(DemoConfig.POI_BG_PADDING).append(";\n");
            mss.append("  shield-icon-background-padding-y: ").append(DemoConfig.POI_BG_PADDING).append(";\n");
            if (DemoConfig.POI_BG_BORDER > 0) {
                mss.append("  shield-icon-background-border-fill: ").append(fill).append(";\n");
                mss.append("  shield-icon-background-border-width: ").append(DemoConfig.POI_BG_BORDER).append(";\n");
            }
        }
        if (DemoConfig.POI_ANCHORS != null && !DemoConfig.POI_ANCHORS.trim().isEmpty()) {
            mss.append("  shield-anchors: '").append(DemoConfig.POI_ANCHORS.trim()).append("';\n");
            mss.append("  shield-text-optional: ").append(DemoConfig.POI_TEXT_OPTIONAL ? "true" : "false").append(";\n");
        }
        return mss.toString();
    }

    public static String poiTestStyle() {
        String css = String.join("\n",
            "Map { background-color: #f4f1ec; }",
            "#water { polygon-fill: #9cc3e0; }",
            "#landcover { polygon-fill: #dbe8cc; }",
            "#landuse { polygon-fill: #e7e3dc; }",
            "#transportation { line-color: #ffffff; line-width: linear([view::zoom], (12, 0.6), (18, 4.0)); }",
            "#transportation['class'='motorway'] { line-color: #e8b48a; line-width: linear([view::zoom], (12, 1.5), (18, 9.0)); }",
            "#building[zoom>=15] { polygon-fill: #ded8d0; }",

            // Cities and towns: the low-zoom test - a screen full of them, all competing.
            "#place[class=city][zoom>=4],",
            "#place[class=town][zoom>=8],",
            "#place[class=village][zoom>=11] {",
            "  shield-name: [name];",
            shieldCommon(ICON_DOT, "#333333", 12f),
            "  shield-placement-priority: 10;",
            "}",

            // Every POI, at the zooms where a real style shows them. One rule per class rather than
            // nested filter blocks: a nested block builds a symbolizer of its own, and what it
            // inherits from the block around it is a CartoCSS question this test has no reason to
            // ask. Several icons so the atlas holds more than one glyph and the screen mixes label
            // widths, which is what makes the side selection visible.
            "#poi[zoom>=14][class=restaurant],",
            "#poi[zoom>=14][class=fast_food] {",
            "  shield-name: [name];",
            shieldCommon(ICON_RESTAURANT, "#b5651d", 11f),
            "}",
            "#poi[zoom>=14][class=lodging] {",
            "  shield-name: [name];",
            shieldCommon(ICON_HOTEL, "#2a6f97", 11f),
            "}",
            "#poi[zoom>=14][class=cafe] {",
            "  shield-name: [name];",
            shieldCommon(ICON_CAFE, "#7d5a3c", 11f),
            "}",
            "#poi[zoom>=14][class!=restaurant][class!=fast_food][class!=lodging][class!=cafe] {",
            "  shield-name: [name];",
            shieldCommon(ICON_CAFE, "#4a4a4a", 11f),
            "}",

            // Peaks: the 3D test - these sit on the terrain, so their icons ride the relief.
            "#mountain_peak[zoom>=11] {",
            "  shield-name: [name];",
            shieldCommon(ICON_PEAK, "#5a4632", 11f),
            "}");
        return css;
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
            contourWidthByDiv(),
            // Labels need a font asset package (text-face-name -> a bundled font), so they are
            // only available with a DIR/ZIP style, not with this raw CartoCSS string.
            "}");
    }

    /**
     * Which contours are VISIBLE, per camera zoom. The tile carries every line its zoom can place
     * (see ContourTileDataSource::getIntervalForZoom), and 'div' - the largest nice divisor of the
     * elevation - ranks them; the style fades a rank in when the camera is close enough for it.
     *
     * This has to be a WIDTH ramp, not a filter: a CartoCSS filter is evaluated per tile at decode
     * time, so it cannot see the camera. [view::zoom] is evaluated per frame, and a width of 0
     * draws nothing (the quad is degenerate).
     */
    private static String contourWidthByDiv() {
        return String.join("\n",
            "  line-opacity: 0.75;",
            "  line-width: 0;",
            "  [div>=10]  { line-width: linear([view::zoom], (14, 0), (14.5, 0.5));  line-opacity: linear([view::zoom], (14, 0), (14.5, 1)); }",
            "  [div>=50]  { line-width: linear([view::zoom], (13, 0), (13.5, 0.7)); line-opacity: linear([view::zoom], (13, 0), (13.5, 1));}",
            "  [div>=100] { line-width: linear([view::zoom], (11.5, 0), (12, 1)); line-opacity: 0.9; }",
            "  [div>=500] { line-width: 1.3; line-opacity: 0.9; }");
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
            "#transportation['class'='motorway'] { line-color: #e27d60; line-width: " + DemoConfig.INLINE_MOTORWAY_WIDTH + "; }",
            DemoConfig.INLINE_BUILDINGS_3D
                ? "#building[zoom>=14] { building-fill: #d9cfc4; building-height: " + DemoConfig.INLINE_BUILDING_HEIGHT + "; }"
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

    /**
     * Style of the ROUTE TEST layer (DemoConfig.LAYER_ROUTE_TEST): a navigation route drawn the way
     * a turn-by-turn app draws it - a dark casing attachment first (CartoCSS renders attachments in
     * declaration order, so it lands UNDER) and the coloured fill over it.
     *
     * Both attachments carry the same join/cap/miterlimit, so one screenshot says what a setting
     * does to the whole route, casing included. line-opacity below 1 is the join over-blending
     * test: overlapping triangles of ONE line blend twice where they overlap.
     */
    public static String routeTestStyle() {
        // "layer" opacity is a layer-level property, so the renderer draws the whole layer opaque
        // into the overlay buffer and composites it once - overlaps can not blend twice, at the
        // cost of a full-screen pass per layer. "geom" bakes it into the colour, which is the path
        // the single-blend stencil pass covers.
        boolean layerOpacity = "layer".equalsIgnoreCase(DemoConfig.ROUTE_TEST_OPACITY_MODE);
        String common = " line-join: " + DemoConfig.ROUTE_TEST_JOIN
                + "; line-cap: " + DemoConfig.ROUTE_TEST_CAP
                + "; line-miterlimit: " + DemoConfig.ROUTE_TEST_MITER_LIMIT
                + (layerOpacity
                    ? "; opacity: " + DemoConfig.ROUTE_TEST_OPACITY + "; comp-op: src-over;"
                    : "; line-opacity: " + DemoConfig.ROUTE_TEST_OPACITY + ";");
        StringBuilder mss = new StringBuilder();
        if (DemoConfig.ROUTE_TEST_CASE_WIDTH > 0) {
            mss.append("#route::case { line-color: ").append(DemoConfig.ROUTE_TEST_CASE_COLOR)
               .append("; line-width: ").append(DemoConfig.ROUTE_TEST_CASE_WIDTH).append(";")
               .append(common).append(" }\n");
        }
        mss.append("#route { line-color: ").append(DemoConfig.ROUTE_TEST_COLOR)
           .append("; line-width: ").append(DemoConfig.ROUTE_TEST_WIDTH).append(";")
           .append(common).append(" }");
        return mss.toString();
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
