package com.akylas.cartotest.demo;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;

import com.akylas.routing.LatLon;
import com.akylas.routing.RoutingRequest;
import com.akylas.routing.ValhallaOnlineRoutingService;
import com.akylas.routing.ValhallaRoutingService;
import com.carto.core.MapBounds;
import com.carto.core.MapPos;
import com.carto.core.MapPosVector;
import com.carto.core.MapTile;
import com.carto.datasources.components.TileData;
import com.carto.datasources.GeoJSONVectorTileDataSource;
import com.carto.datasources.LocalVectorDataSource;
import com.carto.geometry.LineGeometry;
import com.carto.geometry.GeoJSONGeometryReader;
import com.carto.geometry.PointGeometry;
import com.carto.graphics.Color;
import com.carto.layers.VectorLayer;
import com.carto.layers.VectorTileLayer;
import com.carto.projections.Projection;
import com.carto.search.SearchRequest;
import com.carto.search.VectorTileSearchService;
import com.carto.styles.CartoCSSStyleSet;
import com.carto.styles.LineStyleBuilder;
import com.carto.vectorelements.Line;
import com.carto.vectortiles.MBVectorTileDecoder;

import org.json.JSONArray;
import org.json.JSONObject;

/**
 * The one-shot test cases the demo can RUN (as opposed to the layers it can SHOW): routing,
 * vector-tile search, GeoJSON geometry. Each is a panel button; none of them is part of the map
 * configuration, which is why they live outside {@link DemoMap}.
 *
 * All of them draw their result into one shared vector layer, created on first use and always
 * added on top.
 */
public final class DemoTests {

    private static final String TAG = "DemoTests";

    private static LocalVectorDataSource resultSource;

    /** Lazily creates (and adds) the layer every test draws its result into. */
    private static LocalVectorDataSource results(DemoMap demo) {
        if (resultSource == null) {
            Projection proj = demo.mapView.getOptions().getBaseProjection();
            resultSource = new LocalVectorDataSource(proj);
            demo.mapView.getLayers().add(new VectorLayer(resultSource));
        }
        return resultSource;
    }

    // =============================================================================================
    // ROUTING
    // =============================================================================================

    /** Offline Valhalla routing from the .vtiles package on the device. */
    public static void runOfflineRouting(final DemoMap demo) {
        final String tiles = demo.dataPath + "/" + DemoConfig.ROUTING_VTILES_NAME;
        new Thread(new Runnable() {
            public void run() {
                try {
                    ValhallaRoutingService service = new ValhallaRoutingService();
                    service.setProfile("pedestrian");
                    service.addMBTilesPath(tiles);

                    java.util.List<LatLon> points = new java.util.ArrayList<LatLon>();
                    points.add(new LatLon(45.1845, 5.7168));
                    points.add(new LatLon(45.24433, 5.74027));
                    RoutingRequest request = new RoutingRequest(points);
                    request.setParameter("costing_options",
                            "{\"bicycle\":{\"non_network_penalty\":0,\"use_ferry\":0,\"shortest\":true,\"use_roads\":0.0,\"use_tracks\":0.5,\"bicycle_type\":\"Hybrid\"}}");
                    request.setParameter("language", "\"fr-FR\"");

                    drawRoute(demo, service.calculateRoute(request), service);
                } catch (Exception e) {
                    report(demo, "offline routing failed: " + e.getMessage());
                    Log.e(TAG, "offline routing failed", e);
                }
            }
        }).start();
    }

    /** Online Valhalla routing (no data files needed) through a plain java.net POST. */
    public static void runOnlineRouting(final DemoMap demo) {
        new Thread(new Runnable() {
            public void run() {
                try {
                    ValhallaOnlineRoutingService service = new ValhallaOnlineRoutingService(
                            "https://valhalla.openstreetmap.de",
                            (url, body, headers) -> {
                                try {
                                    java.net.URL netUrl = new java.net.URL(url);
                                    java.net.HttpURLConnection conn = (java.net.HttpURLConnection) netUrl.openConnection();
                                    conn.setRequestMethod("POST");
                                    conn.setDoOutput(true);
                                    conn.setRequestProperty("Content-Type", "application/json");
                                    conn.getOutputStream().write(body.getBytes("UTF-8"));
                                    int code = conn.getResponseCode();
                                    java.io.InputStream is = (code < 400) ? conn.getInputStream() : conn.getErrorStream();
                                    java.util.Scanner s = new java.util.Scanner(is).useDelimiter("\\A");
                                    return s.hasNext() ? s.next() : "";
                                } catch (Exception e) {
                                    throw new RuntimeException(e);
                                }
                            });
                    service.setProfile("pedestrian");

                    java.util.List<LatLon> points = new java.util.ArrayList<LatLon>();
                    points.add(new LatLon(45.1877, 5.7249));   // Place Grenette
                    points.add(new LatLon(45.1916, 5.7148));   // Gare de Grenoble
                    // The online service returns raw Valhalla JSON and has no shape decoder, so
                    // this test only reports the response (the offline one draws the geometry).
                    String rawJson = service.calculateRoute(new RoutingRequest(points));
                    report(demo, "online routing OK (" + rawJson.length() + " bytes)");
                    largeLog(TAG, rawJson);
                } catch (Exception e) {
                    report(demo, "online routing failed: " + e.getMessage());
                    Log.e(TAG, "online routing failed", e);
                }
            }
        }).start();
    }

    /** Decodes the Valhalla shape of every leg and draws it. */
    private static void drawRoute(DemoMap demo, String rawJson, ValhallaRoutingService service) throws Exception {
        GeoJSONGeometryReader reader = new GeoJSONGeometryReader();
        JSONArray legs = new JSONObject(rawJson).getJSONObject("trip").getJSONArray("legs");
        LineStyleBuilder style = new LineStyleBuilder();
        style.setWidth(4);
        style.setColor(new Color((short) 255, (short) 0, (short) 0, (short) 255));
        int points = 0;
        for (int i = 0; i < legs.length(); i++) {
            String coordinates = service.parseShape(legs.getJSONObject(i).getString("shape"));
            LineGeometry geometry = (LineGeometry) reader.readGeometry("{\"type\":\"LineString\", \"coordinates\":" + coordinates + "}");
            MapPosVector poses = geometry.getPoses();
            points += (int) poses.size();
            results(demo).add(new Line(poses, style.buildStyle()));
        }
        report(demo, "route drawn (" + points + " points)");
    }

    // =============================================================================================
    // SEARCH
    // =============================================================================================

    /**
     * Searches the base layer's own tiles around the map centre. Needs a VECTOR base layer:
     * the search service reads the same source and decoder the layer renders from.
     */
    public static void runVectorTileSearch(final DemoMap demo) {
        final VectorTileLayer layer = demo.baseLayer;
        if (layer == null) {
            report(demo, "search needs the vector base layer (LAYERS > base)");
            return;
        }
        final MapPos centre = demo.mapView.getFocusPos();
        final Projection projection = demo.mapView.getOptions().getBaseProjection();
        new Thread(new Runnable() {
            public void run() {
                try {
                    long start = System.nanoTime();
                    VectorTileSearchService service = new VectorTileSearchService(layer.getDataSource(), layer.getTileDecoder());
                    service.setMinZoom(14);
                    service.setMaxZoom(14);
                    service.setSortByDistance(true);
                    service.setPreventDuplicates(true);

                    SearchRequest request = new SearchRequest();
                    request.setSearchRadius(2000);
                    request.setGeometry(new PointGeometry(centre));
                    request.setProjection(projection);
                    request.setFilterExpression("regexp_ilike(class,'.*peak.*') OR regexp_ilike(class,'.*motorway.*')");

                    long count = service.findFeatures(request).getFeatureCount();
                    report(demo, "search: " + count + " features in " + ((System.nanoTime() - start) / 1000000) + " ms");
                } catch (Exception e) {
                    report(demo, "search failed: " + e.getMessage());
                    Log.e(TAG, "search failed", e);
                }
            }
        }).start();
    }

    // =============================================================================================
    // GEOMETRY
    // =============================================================================================

    /**
     * GeoJSONVectorTileDataSource: features added at runtime and styled with CartoCSS (as opposed
     * to the vector elements of the ELEMENTS layer, which carry their own style objects).
     */
    public static void addGeoJSONLine(DemoMap demo) {
        try {
            MBVectorTileDecoder decoder = new MBVectorTileDecoder(new CartoCSSStyleSet(
                    "#items { line-color: #374C70; line-cap: round; line-join: round; line-width: 12; }"));
            GeoJSONVectorTileDataSource source = new GeoJSONVectorTileDataSource(0, 24);
            source.createLayer("items");
            source.addGeoJSONStringFeature(1, "{\"type\":\"Feature\",\"id\":1,\"properties\":{\"name\":\"test\"},"
                    + "\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
                    + "[[5.7249,45.1982],[5.7225,45.1975],[5.7220,45.1949],[5.7201,45.1935],[5.7255,45.1915]]}}");
            demo.mapView.getLayers().add(new VectorTileLayer(source, decoder));
            report(demo, "geojson line added");
        } catch (Exception e) {
            report(demo, "geojson test failed: " + e.getMessage());
            Log.e(TAG, "geojson test failed", e);
        }
    }

    // =============================================================================================
    // GEOJSON TILE-BUILD BENCH
    // =============================================================================================

    /**
     * Times {@link GeoJSONVectorTileDataSource} with no renderer in the way: import the asset, then
     * build a fixed set of tiles and add up the wall time. The tile set is derived from the data
     * extent, so the same asset always walks the same tiles and two builds are comparable.
     *
     * Two shapes, because they stress different things:
     *   bench-many-routes.geojson  5000 short routes / 165k points - MANY OBJECTS (per-tile scan)
     *   bench-long-routes.geojson  8 routes of 100-250 km / 300k points - LONG LINES (re-clipping)
     *
     *   adb shell am start -n com.akylas.cartotest/.MainActivity --es ui false --es geojsonBench many
     *   adb shell am start -n com.akylas.cartotest/.MainActivity --es ui false --es geojsonBench long
     *
     * A file of the same name under the data directory wins over the asset, so another dataset can
     * be tried with a push and no rebuild.
     */
    public static void runGeoJSONBench(final DemoMap demo, final String which) {
        new Thread(new Runnable() {
            public void run() {
                if ("both".equalsIgnoreCase(which)) {
                    benchOne(demo, DemoConfig.GEOJSON_BENCH_MANY_NAME);
                    benchOne(demo, DemoConfig.GEOJSON_BENCH_LONG_NAME);
                } else if ("long".equalsIgnoreCase(which)) {
                    benchOne(demo, DemoConfig.GEOJSON_BENCH_LONG_NAME);
                } else if ("many".equalsIgnoreCase(which) || "true".equalsIgnoreCase(which)) {
                    benchOne(demo, DemoConfig.GEOJSON_BENCH_MANY_NAME);
                } else {
                    benchOne(demo, which); // an explicit asset / data-directory file name
                }
            }
        }).start();
    }

    /**
     * The same two datasets, but as a REAL layer (route style, same tesselator and shaders as the
     * route test) so the RENDER cost can be compared instead of the tile-build cost. Pan across an
     * empty area and across the data at the same zoom to separate "cost of having the layer" from
     * "cost of drawing the features".
     *
     *   --es geojsonLayer many|long|both     (add at startup, then pan)
     *   data around lon 5.55..6.05 / lat 44.95..45.31; lon 6.6 is empty at the same zoom.
     */
    public static void addGeoJSONBenchLayer(final DemoMap demo, final String which) {
        new Thread(new Runnable() {
            public void run() {
                if ("both".equalsIgnoreCase(which)) {
                    addBenchLayerOne(demo, DemoConfig.GEOJSON_BENCH_MANY_NAME);
                    addBenchLayerOne(demo, DemoConfig.GEOJSON_BENCH_LONG_NAME);
                } else if ("long".equalsIgnoreCase(which)) {
                    addBenchLayerOne(demo, DemoConfig.GEOJSON_BENCH_LONG_NAME);
                } else if ("many".equalsIgnoreCase(which) || "true".equalsIgnoreCase(which)) {
                    addBenchLayerOne(demo, DemoConfig.GEOJSON_BENCH_MANY_NAME);
                } else {
                    addBenchLayerOne(demo, which);
                }
            }
        }).start();
    }

    private static void addBenchLayerOne(final DemoMap demo, String name) {
        String geoJSON = demo.readDataOrAsset(name);
        if (geoJSON == null) {
            report(demo, "bench layer: " + name + " not found");
            return;
        }
        try {
            GeoJSONVectorTileDataSource source = new GeoJSONVectorTileDataSource(0, 24);
            source.setSimplifyTolerance(DemoConfig.GEOJSON_BENCH_SIMPLIFY);
            int layerIndex = source.createLayer("route");
            long start = System.nanoTime();
            source.setLayerGeoJSONString(layerIndex, geoJSON);
            long importMs = (System.nanoTime() - start) / 1000000L;

            final MBVectorTileDecoder decoder = new MBVectorTileDecoder(new CartoCSSStyleSet(DemoStyles.routeTestStyle()));
            final VectorTileLayer layer = new VectorTileLayer(source, decoder);
            new Handler(Looper.getMainLooper()).post(new Runnable() {
                public void run() {
                    demo.mapView.getLayers().add(layer);
                }
            });
            report(demo, "bench layer " + name + " added (import " + importMs + " ms)");
        } catch (Exception e) {
            report(demo, "bench layer failed: " + e.getMessage());
            Log.e(TAG, "bench layer failed", e);
        }
    }

    private static void benchOne(DemoMap demo, String name) {
        String geoJSON = demo.readDataOrAsset(name);
        if (geoJSON == null) {
            report(demo, "bench: " + name + " not found");
            return;
        }

        try {
            GeoJSONVectorTileDataSource source = new GeoJSONVectorTileDataSource(0, 24);
            source.setSimplifyTolerance(DemoConfig.GEOJSON_BENCH_SIMPLIFY);
            int layerIndex = source.createLayer("route");

            long importStart = System.nanoTime();
            source.setLayerGeoJSONString(layerIndex, geoJSON);
            long importMs = (System.nanoTime() - importStart) / 1000000L;

            // getDataExtent comes back in EPSG3857 metres whatever the base projection is, so undo
            // the web mercator here rather than going through Projection.
            MapBounds extent = source.getDataExtent();
            double centerXm = (extent.getMin().getX() + extent.getMax().getX()) * 0.5;
            double centerYm = (extent.getMin().getY() + extent.getMax().getY()) * 0.5;
            double lon = centerXm / 6378137.0 * 180.0 / Math.PI;
            double lat = Math.toDegrees(Math.atan(Math.sinh(centerYm / 6378137.0)));
            Log.i(TAG, "bench " + name + " centre " + lon + "," + lat);

            // Deepest zooms first: that is the order a user zooming in produces, and the order that
            // makes a per-tile feature scan hurt most (many tiles, few features in each).
            StringBuilder perZoom = new StringBuilder();
            long totalNs = 0;
            long totalBytes = 0;
            int tiles = 0;
            for (int zoom = DemoConfig.GEOJSON_BENCH_MAX_ZOOM; zoom >= DemoConfig.GEOJSON_BENCH_MIN_ZOOM; zoom--) {
                int span = 1 << zoom;
                int centerX = (int) Math.floor((lon + 180.0) / 360.0 * span);
                double latRad = Math.toRadians(lat);
                int centerY = (int) Math.floor((1.0 - Math.log(Math.tan(latRad) + 1.0 / Math.cos(latRad)) / Math.PI) / 2.0 * span);

                long zoomNs = 0;
                int side = DemoConfig.GEOJSON_BENCH_TILES_PER_SIDE;
                for (int dy = 0; dy < side; dy++) {
                    for (int dx = 0; dx < side; dx++) {
                        int x = centerX + dx - side / 2;
                        int y = centerY + dy - side / 2;
                        if (x < 0 || y < 0 || x >= span || y >= span) {
                            continue;
                        }
                        // The builder measures tile y from the NORTH edge, so this is plain XYZ.
                        MapTile tile = new MapTile(x, y, zoom, 0);
                        long start = System.nanoTime();
                        TileData data = source.loadTile(tile);
                        zoomNs += System.nanoTime() - start;
                        tiles++;
                        if (data != null && data.getData() != null) {
                            totalBytes += data.getData().size();
                        }
                    }
                }
                totalNs += zoomNs;
                perZoom.append(" z").append(zoom).append("=").append(zoomNs / 1000000L);
            }

            String summary = "bench " + name + ": import " + importMs + " ms, " + tiles + " tiles in "
                    + (totalNs / 1000000L) + " ms (" + (totalNs / 1000L / Math.max(1, tiles)) + " us/tile), "
                    + totalBytes + " bytes";
            Log.i(TAG, summary);
            Log.i(TAG, "bench " + name + " per zoom (ms):" + perZoom);
            report(demo, summary);
        } catch (Exception e) {
            report(demo, "bench failed: " + e.getMessage());
            Log.e(TAG, "bench failed", e);
        }
    }

    // =============================================================================================
    // HELPERS
    // =============================================================================================

    /** Toast + log, from any thread. */
    private static void report(final DemoMap demo, final String message) {
        Log.i(TAG, message);
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            public void run() {
                Toast.makeText(demo.mapView.getContext(), message, Toast.LENGTH_SHORT).show();
            }
        });
        demo.mapView.requestRender();
    }

    /** Logcat truncates at ~4k, so long JSON has to be split. */
    public static void largeLog(String tag, String content) {
        if (content.length() > 4000) {
            Log.d(tag, content.substring(0, 4000));
            largeLog(tag, content.substring(4000));
        } else {
            Log.d(tag, content);
        }
    }

    private DemoTests() {
    }
}
