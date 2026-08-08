package com.akylas.cartotest.demo;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.util.Log;
import android.widget.Toast;

import com.carto.celestial.CelestialArc;
import com.carto.celestial.CelestialObject;
import com.carto.celestial.CelestialSprite;
import com.carto.core.DoubleVector;
import com.carto.core.Variant;
import com.carto.graphics.Color;
import com.carto.layers.CelestialEventListener;
import com.carto.layers.CelestialLayer;
import com.carto.ui.ClickInfo;
import com.carto.ui.MapView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The star demo: the real sky over the demo's position, right now.
 *
 * Every star is a {@link CelestialSprite} and every constellation figure is one {@link CelestialArc}
 * in segment mode, so the whole catalogue is ONE draw call for the stars plus one per figure, and
 * both are clickable. The planets are sprites too - the API does not know what any of them are.
 *
 * The astronomy is in {@link DemoAstro} and the data in {@link DemoStarCatalogue}; this file is the
 * mapping onto the SDK.
 */
public final class DemoStars {

    private static final String TAG = "DemoStars";
    private static final String META_NAME = "name";
    private static final String META_INFO = "info";

    private CelestialLayer layer;
    private final List<CelestialSprite> stars = new ArrayList<CelestialSprite>();
    private final List<double[]> starEquatorial = new ArrayList<double[]>();   // { ra degrees, dec }
    private final List<CelestialArc> figures = new ArrayList<CelestialArc>();
    private final List<CelestialSprite> figureLabels = new ArrayList<CelestialSprite>();
    private final List<double[][]> figureEquatorial = new ArrayList<double[][]>(); // per segment { ra, dec, ra, dec }
    private final List<CelestialSprite> planets = new ArrayList<CelestialSprite>();
    private CelestialArc equator;

    /** Builds the layer: stars, constellation figures, planets. Positions come from {@link #update}. */
    public CelestialLayer createLayer(final MapView mapView) {
        layer = new CelestialLayer();
        layer.setPostProcessed(false); // see DemoCelestial: the sky objects keep their own look
        // The catalogue sizes are in pixels at 160 dpi, the unit the rest of the demo uses.
        final float density = mapView.getContext().getResources().getDisplayMetrics().density;

        Map<String, double[]> byName = new HashMap<String, double[]>();
        for (String entry : DemoStarCatalogue.STARS) {
            String[] parts = entry.split("\\|");
            if (parts.length != 4) {
                Log.w(TAG, "bad catalogue entry: " + entry);
                continue;
            }
            String name = parts[0];
            double raDegrees = Double.parseDouble(parts[1]) * 15.0;
            double declination = Double.parseDouble(parts[2]);
            double magnitude = Double.parseDouble(parts[3]);
            byName.put(name, new double[] { raDegrees, declination });

            CelestialSprite star = new CelestialSprite();
            // Brighter is bigger, the way a star chart draws them: a magnitude step is a factor of
            // 2.5 in brightness, but on screen a linear size ramp reads better than the real one.
            star.setScreenSize((float) magnitudeToPixels(magnitude) * density);
            star.setColor(new Color((short) 255, (short) 255, (short) 250, (short) 255));
            star.setSoftness(0.45f);
            star.setClickRadius(1.5f);
            star.setMetaDataElement(META_NAME, new Variant(name));
            star.setMetaDataElement(META_INFO, new Variant("mag " + magnitude));
            layer.add(star);
            stars.add(star);
            starEquatorial.add(new double[] { raDegrees, declination });
        }

        for (Map.Entry<String, String[][]> figure : DemoStarCatalogue.figures().entrySet()) {
            List<double[]> segments = new ArrayList<double[]>();
            for (String[] segment : figure.getValue()) {
                double[] from = byName.get(segment[0]);
                double[] to = byName.get(segment[1]);
                if (from == null || to == null) {
                    Log.w(TAG, figure.getKey() + ": no such star " + (from == null ? segment[0] : segment[1]));
                    continue;
                }
                segments.add(new double[] { from[0], from[1], to[0], to[1] });
            }
            if (segments.isEmpty()) {
                continue;
            }
            CelestialArc arc = new CelestialArc();
            arc.setColor(new Color((short) 120, (short) 170, (short) 255, (short) 110));
            arc.setWidth(DemoConfig.STARS_FIGURE_WIDTH * density);
            arc.setBelowHorizonVisible(false);
            arc.setClickRadius(DemoConfig.STARS_FIGURE_CLICK_RADIUS);
            arc.setMetaDataElement(META_NAME, new Variant(figure.getKey()));
            arc.setMetaDataElement(META_INFO, new Variant(segments.size() + " lines"));
            layer.add(arc);
            figures.add(arc);
            figureEquatorial.add(segments.toArray(new double[segments.size()][]));

            // The name, IN THE SKY, at the middle of the figure. It is a plain sprite with a
            // bitmap the demo paints - the SDK has no text of its own here, which is exactly what
            // makes it themeable from the app: change the paint, change the label.
            CelestialSprite label = new CelestialSprite();
            com.carto.graphics.Bitmap labelBitmap = textBitmap(figure.getKey(), density);
            label.setBitmap(labelBitmap);
            // The bitmap is square and the text fills its width, so drawing the quad at the
            // bitmap's own pixel size renders the text at the size it was painted - the same for
            // every name. A fixed quad size would shrink the long ones instead.
            label.setScreenSize(labelBitmap.getWidth() * DemoConfig.STARS_LABEL_SCALE);
            label.setColor(new Color((short) 255, (short) 255, (short) 255,
                    (short) Math.round(255 * DemoConfig.STARS_LABEL_OPACITY)));
            label.setClickRadius(0f); // the figure itself is the clickable thing
            label.setMetaDataElement(META_NAME, new Variant(figure.getKey()));
            label.setMetaDataElement(META_INFO, new Variant(""));
            layer.add(label);
            figureLabels.add(label);
        }

        for (String name : DemoAstro.PLANET_NAMES) {
            CelestialSprite planet = new CelestialSprite();
            planet.setAngularSize(DemoConfig.STARS_PLANET_SIZE);
            planet.setColor(planetColor(name));
            planet.setSoftness(0.4f);
            planet.setClickRadius(2.5f);
            planet.setMetaDataElement(META_NAME, new Variant(name));
            planet.setMetaDataElement(META_INFO, new Variant("planet"));
            layer.add(planet);
            planets.add(planet);
        }

        // The celestial equator: the one line that makes the sky's rotation readable, and a circle
        // about the pole like the sun's daily path - the same object, radius 90 degrees.
        equator = new CelestialArc();
        equator.setColor(new Color((short) 90, (short) 200, (short) 190, (short) 90));
        equator.setWidth(1.5f);
        equator.setBelowHorizonVisible(false);
        equator.setClickRadius(1.5f);
        equator.setMetaDataElement(META_NAME, new Variant("Celestial equator"));
        equator.setMetaDataElement(META_INFO, new Variant("declination 0"));
        layer.add(equator);

        layer.setCelestialEventListener(new CelestialEventListener() {
            @Override
            public boolean onCelestialObjectClicked(ClickInfo clickInfo, CelestialObject object) {
                StringBuilder message = new StringBuilder(object.getMetaDataElement(META_NAME).getString());
                String info = object.getMetaDataElement(META_INFO).getString();
                if (info != null && info.length() > 0) {
                    message.append("  ").append(info);
                }
                if (object instanceof CelestialSprite) {
                    // Only a sprite HAS one direction - a curve is a set of them.
                    message.append("   az ").append(Math.round(object.getAzimuth()))
                           .append("° alt ").append(Math.round(object.getAltitude())).append("°");
                }
                final String text = message.toString();
                Log.i(TAG, "clicked " + text);
                mapView.post(new Runnable() {
                    public void run() {
                        Toast.makeText(mapView.getContext(), text, Toast.LENGTH_SHORT).show();
                    }
                });
                return true;
            }
        });
        return layer;
    }

    /**
     * Places everything for a date and a position: the whole catalogue turned into the horizon
     * frame. This is what the sky really looks like from there at that moment.
     */
    public void update(double n, double lat, double lon) {
        if (layer == null) {
            return;
        }
        for (int i = 0; i < stars.size(); i++) {
            double[] eq = starEquatorial.get(i);
            double[] horizon = DemoAstro.toHorizon(eq[0], eq[1], n, lat, lon);
            CelestialSprite star = stars.get(i);
            star.setDirection((float) horizon[0], (float) horizon[1], 0);
            // A star below the horizon is under the ground: not drawn, not clickable, not paid for.
            star.setVisible(DemoConfig.STARS_STARS && horizon[1] > -2.0);
        }

        for (int i = 0; i < figures.size(); i++) {
            double[][] segments = figureEquatorial.get(i);
            DoubleVector directions = new DoubleVector();
            for (double[] segment : segments) {
                double[] from = DemoAstro.toHorizon(segment[0], segment[1], n, lat, lon);
                double[] to = DemoAstro.toHorizon(segment[2], segment[3], n, lat, lon);
                directions.add(from[0]);
                directions.add(from[1]);
                directions.add(to[0]);
                directions.add(to[1]);
            }
            figures.get(i).setSegments(directions);
            figures.get(i).setVisible(DemoConfig.STARS_FIGURES);

            // The label goes at the MEAN DIRECTION of the figure, averaged as vectors: averaging
            // azimuths would put a figure straddling north somewhere near south.
            double x = 0, y = 0, z = 0;
            for (int k = 0; k + 1 < directions.size(); k += 2) {
                double az = Math.toRadians(directions.get(k)), alt = Math.toRadians(directions.get(k + 1));
                x += Math.cos(alt) * Math.sin(az);
                y += Math.cos(alt) * Math.cos(az);
                z += Math.sin(alt);
            }
            CelestialSprite label = figureLabels.get(i);
            double length = Math.sqrt(x * x + y * y + z * z);
            if (length > 0) {
                double altitude = Math.toDegrees(Math.asin(z / length));
                double azimuth = DemoAstro.normalizeDegrees(Math.toDegrees(Math.atan2(x, y)));
                label.setDirection((float) azimuth, (float) altitude, 0);
                label.setVisible(DemoConfig.STARS_FIGURES && DemoConfig.STARS_LABELS && altitude > 0);
            } else {
                label.setVisible(false);
            }
        }

        for (int i = 0; i < planets.size(); i++) {
            double[] horizon = DemoAstro.planetHorizon(i, n, lat, lon);
            CelestialSprite planet = planets.get(i);
            planet.setDirection((float) horizon[0], (float) horizon[1], 0);
            planet.setVisible(DemoConfig.STARS_PLANETS && horizon[1] > -2.0);
        }

        // Declination 0 is 90 degrees from the pole, and the pole sits due north (south below the
        // equator) at an altitude equal to the latitude.
        equator.setCircle(lat >= 0 ? 0f : 180f, (float) Math.abs(lat), 90f);
        equator.setVisible(DemoConfig.STARS_EQUATOR);
    }

    /**
     * The name painted into a square bitmap, which is what a celestial sprite draws. Square
     * because the sprite quad is: the text is centred in it and the transparent margin costs
     * nothing but texture.
     */
    private static com.carto.graphics.Bitmap textBitmap(String text, float density) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
        paint.setTextSize(DemoConfig.STARS_LABEL_TEXT_SIZE * density);
        paint.setColor(0xFFFFFFFF);
        Rect bounds = new Rect();
        paint.getTextBounds(text, 0, text.length(), bounds);
        int side = Math.max(16, Math.max(bounds.width(), bounds.height()) + Math.round(8 * density));
        android.graphics.Bitmap bitmap = android.graphics.Bitmap.createBitmap(side, side, android.graphics.Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        // A dark outline, so a name stays readable over a bright sky as well as over black.
        Paint outline = new Paint(paint);
        outline.setStyle(Paint.Style.STROKE);
        outline.setStrokeWidth(3 * density);
        outline.setColor(0xC0000000);
        float x = side * 0.5f - bounds.exactCenterX();
        float y = side * 0.5f - bounds.exactCenterY();
        canvas.drawText(text, x, y, outline);
        canvas.drawText(text, x, y, paint);
        return com.carto.utils.BitmapUtils.createBitmapFromAndroidBitmap(bitmap);
    }

    /** A star of this magnitude, in screen pixels. */
    private static double magnitudeToPixels(double magnitude) {
        double size = DemoConfig.STARS_BRIGHTEST_SIZE - DemoConfig.STARS_SIZE_PER_MAGNITUDE * (magnitude + 1.5);
        return Math.max(DemoConfig.STARS_FAINTEST_SIZE, size);
    }

    private static Color planetColor(String name) {
        if ("Mars".equals(name)) {
            return new Color((short) 255, (short) 130, (short) 90, (short) 255);
        }
        if ("Venus".equals(name)) {
            return new Color((short) 255, (short) 250, (short) 220, (short) 255);
        }
        if ("Jupiter".equals(name)) {
            return new Color((short) 255, (short) 235, (short) 180, (short) 255);
        }
        if ("Saturn".equals(name)) {
            return new Color((short) 240, (short) 220, (short) 160, (short) 255);
        }
        return new Color((short) 220, (short) 220, (short) 230, (short) 255);
    }
}
