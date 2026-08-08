package com.akylas.cartotest.demo;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;

import com.akylas.routing.LatLon;
import com.akylas.routing.RoutingRequest;
import com.akylas.routing.ValhallaOnlineRoutingService;
import com.akylas.routing.ValhallaRoutingService;
import com.carto.core.MapPos;
import com.carto.core.MapPosVector;
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

    /**
     * Drives a TWO-FINGER drag over the map, as a real gesture would.
     *
     * The only way to exercise it: adb can synthesize one pointer, not two, and the emulator's
     * touch devices are not writable from the shell. The MotionEvent goes through
     * MapView.onTouchEvent, which is the same entry point a finger uses, so what it tests is the
     * gesture path and not a shortcut around it.
     *
     * @param dx horizontal travel in pixels, @param dy vertical travel (negative = upwards)
     */
    public static void runTwoFingerDrag(final DemoMap demo, final float dx, final float dy) {
        final com.carto.ui.MapView mapView = demo.mapView;
        final Handler handler = new Handler(Looper.getMainLooper());
        final MapPos before = mapView.getFocusPos();
        final float x1 = 400, x2 = 700, y = 1600;
        final long downTime = android.os.SystemClock.uptimeMillis();
        final int steps = 12;
        // Real delays between the events, not a tight loop: the gesture is only recognised once
        // the click handler THREAD has seen the movement, so a burst of events with fake
        // timestamps is dropped as a two-finger tap.
        send(mapView, downTime, downTime, android.view.MotionEvent.ACTION_DOWN, 1, x1, y, x2, y);
        handler.postDelayed(new Runnable() {
            public void run() {
                send(mapView, downTime, android.os.SystemClock.uptimeMillis(),
                        android.view.MotionEvent.ACTION_POINTER_DOWN | (1 << android.view.MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                        2, x1, y, x2, y);
            }
        }, 30);
        for (int step = 1; step <= steps; step++) {
            final float fx = dx * step / steps, fy = dy * step / steps;
            handler.postDelayed(new Runnable() {
                public void run() {
                    send(mapView, downTime, android.os.SystemClock.uptimeMillis(),
                            android.view.MotionEvent.ACTION_MOVE, 2, x1 + fx, y + fy, x2 + fx, y + fy);
                }
            }, 60 + step * 25L);
        }
        handler.postDelayed(new Runnable() {
            public void run() {
                send(mapView, downTime, android.os.SystemClock.uptimeMillis(),
                        android.view.MotionEvent.ACTION_POINTER_UP | (1 << android.view.MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                        2, x1 + dx, y + dy, x2 + dx, y + dy);
                send(mapView, downTime, android.os.SystemClock.uptimeMillis(),
                        android.view.MotionEvent.ACTION_UP, 1, x1 + dx, y + dy, x2 + dx, y + dy);
                Log.i(TAG, "two-finger drag (" + dx + "," + dy + "): focus " + before + " -> " + mapView.getFocusPos());
            }
        }, 60 + (steps + 2) * 25L);
    }

    private static void send(com.carto.ui.MapView mapView, long downTime, long eventTime, int action,
                             int count, float x1, float y1, float x2, float y2) {
        android.view.MotionEvent.PointerProperties[] properties = new android.view.MotionEvent.PointerProperties[count];
        android.view.MotionEvent.PointerCoords[] coords = new android.view.MotionEvent.PointerCoords[count];
        for (int i = 0; i < count; i++) {
            properties[i] = new android.view.MotionEvent.PointerProperties();
            properties[i].id = i;
            properties[i].toolType = android.view.MotionEvent.TOOL_TYPE_FINGER;
            coords[i] = new android.view.MotionEvent.PointerCoords();
            coords[i].x = (i == 0 ? x1 : x2);
            coords[i].y = (i == 0 ? y1 : y2);
            coords[i].pressure = 1;
            coords[i].size = 1;
        }
        android.view.MotionEvent event = android.view.MotionEvent.obtain(downTime, eventTime, action, count,
                properties, coords, 0, 0, 1, 1, 0, 0, android.view.InputDevice.SOURCE_TOUCHSCREEN, 0);
        mapView.onTouchEvent(event);
        event.recycle();
    }


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
