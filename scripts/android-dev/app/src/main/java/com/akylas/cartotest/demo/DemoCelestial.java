package com.akylas.cartotest.demo;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;
import android.widget.Toast;

import com.carto.celestial.CelestialArc;
import com.carto.celestial.CelestialObject;
import com.carto.celestial.CelestialSprite;
import com.carto.core.DoubleVector;
import com.carto.core.MapPos;
import com.carto.core.Variant;
import com.carto.graphics.Color;
import com.carto.layers.CelestialEventListener;
import com.carto.layers.CelestialLayer;
import com.carto.ui.ClickInfo;
import com.carto.ui.MapView;
import com.carto.utils.BitmapUtils;

/**
 * The sun, the moon and their paths across the day, built on the generic celestial API.
 *
 * NOTHING here is a sun or a moon as far as the SDK is concerned: they are two sprites and two
 * arcs, placed by direction. The astronomy lives in {@link DemoAstro}, which is the point - the
 * same API carries an aircraft ({@link #addAircraft}) or a star catalogue ({@link DemoStars}) with
 * no SDK change at all.
 *
 * The positions are the REAL ones for the demo's date, hour and location, and the arcs are sampled
 * from the same ephemeris over the whole day, so the disc always sits exactly on its own arc. That
 * is a free correctness check on both: they are computed independently of each other.
 */
public final class DemoCelestial {

    private static final String META_NAME = "name";
    private static final int MOON_BITMAP_SIZE = 128;
    /** The sampling step of a daily path, in minutes. 10 is smooth at any field of view. */
    private static final int PATH_STEP_MINUTES = 10;

    private CelestialLayer layer;
    private CelestialSprite sun;
    private CelestialSprite moon;
    private CelestialArc sunPath;
    private CelestialArc moonPath;
    private double lastMoonPhase = -1;

    /** Builds the layer and its objects. Added FIRST, so the map and the terrain draw over them. */
    public CelestialLayer createLayer(final MapView mapView) {
        layer = new CelestialLayer();
        // Drawn after any post-process effect (still depth-tested, so a path still goes behind
        // the ridges): the relief look is for the ground, not for the objects over it.
        layer.setPostProcessed(false);

        sun = new CelestialSprite();
        sun.setAngularSize(DemoConfig.CELESTIAL_SUN_SIZE);
        sun.setColor(new Color((short) 255, (short) 244, (short) 214, (short) 255));
        sun.setSoftness(0.35f);
        sun.setClickRadius(3f);
        sun.setMetaDataElement(META_NAME, new Variant("Sun"));

        moon = new CelestialSprite();
        moon.setAngularSize(DemoConfig.CELESTIAL_MOON_SIZE);
        moon.setColor(new Color((short) 245, (short) 245, (short) 235, (short) 255));
        moon.setSoftness(0.25f);
        moon.setClickRadius(3f);
        moon.setMetaDataElement(META_NAME, new Variant("Moon"));

        sunPath = new CelestialArc();
        sunPath.setColor(new Color((short) 255, (short) 216, (short) 120, (short) 160));
        sunPath.setWidth(DemoConfig.CELESTIAL_ARC_WIDTH);
        sunPath.setBelowHorizonVisible(false);
        sunPath.setClickRadius(2f);
        sunPath.setMetaDataElement(META_NAME, new Variant("Sun path"));

        moonPath = new CelestialArc();
        moonPath.setColor(new Color((short) 170, (short) 190, (short) 255, (short) 130));
        moonPath.setWidth(DemoConfig.CELESTIAL_ARC_WIDTH);
        moonPath.setBelowHorizonVisible(false);
        moonPath.setClickRadius(2f);
        moonPath.setMetaDataElement(META_NAME, new Variant("Moon path"));

        layer.add(sun);
        layer.add(moon);
        layer.add(sunPath);
        layer.add(moonPath);

        layer.setCelestialEventListener(new CelestialEventListener() {
            @Override
            public boolean onCelestialObjectClicked(ClickInfo clickInfo, CelestialObject object) {
                final String message = object.getMetaDataElement(META_NAME).getString()
                        + "  az " + Math.round(object.getAzimuth()) + "°  alt " + Math.round(object.getAltitude()) + "°";
                // The listener is called on the touch thread; a Toast needs the UI thread.
                mapView.post(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(mapView.getContext(), message, Toast.LENGTH_SHORT).show();
                    }
                });
                return true;
            }
        });
        return layer;
    }

    /**
     * Places the objects for the demo's current date, time and location.
     *
     * Both bodies come from {@link DemoAstro} rather than from the light options, because the panel
     * can drive the sun's azimuth and altitude by hand: what is drawn here is always where the body
     * really is on that date, which is the only version of it whose daily arc means anything.
     */
    public void update() {
        if (layer == null) {
            return;
        }
        double hourUtc = DemoConfig.currentHourUtc();
        double lat = DemoConfig.START_LAT;
        double lon = DemoConfig.START_LON;
        double n = DemoAstro.daysSinceJ2000(DemoConfig.SUN_YEAR, DemoConfig.SUN_MONTH, DemoConfig.SUN_DAY, hourUtc);

        double[] sunDir = DemoAstro.sunHorizon(n, lat, lon);
        sun.setDirection((float) sunDir[0], (float) sunDir[1], 0);

        double[] moonDir = DemoAstro.moonHorizon(n, lat, lon);
        moon.setDirection((float) moonDir[0], (float) moonDir[1], 0);
        updateMoonPhase(n);

        // The path across the day, sampled from the same ephemeris every PATH_STEP_MINUTES from
        // midnight to midnight. A circle about the celestial pole would be a good enough sun path
        // (declination barely moves in a day), but the moon's does move - a quarter of the sky in a
        // day - so both are sampled and the two arcs are then the same kind of object.
        sunPath.setDirections(dailyPath(true, lat, lon));
        moonPath.setDirections(dailyPath(false, lat, lon));

        sun.setVisible(DemoConfig.CELESTIAL_SUN);
        moon.setVisible(DemoConfig.CELESTIAL_MOON);
        sunPath.setVisible(DemoConfig.CELESTIAL_ARC);
        moonPath.setVisible(DemoConfig.CELESTIAL_MOON_ARC);
        android.util.Log.i("DemoCelestial", "sun az " + Math.round(sunDir[0]) + " alt " + Math.round(sunDir[1])
                + ", moon az " + Math.round(moonDir[0]) + " alt " + Math.round(moonDir[1])
                + ", hour " + hourUtc + " UTC " + DemoConfig.SUN_YEAR + "-" + DemoConfig.SUN_MONTH + "-" + DemoConfig.SUN_DAY);
    }

    /** The body's track over the configured date, as alternating azimuth/altitude degrees. */
    private static DoubleVector dailyPath(boolean isSun, double lat, double lon) {
        DoubleVector directions = new DoubleVector();
        for (int minute = 0; minute <= 24 * 60; minute += PATH_STEP_MINUTES) {
            double n = DemoAstro.daysSinceJ2000(DemoConfig.SUN_YEAR, DemoConfig.SUN_MONTH, DemoConfig.SUN_DAY, minute / 60.0);
            double[] horizon = isSun ? DemoAstro.sunHorizon(n, lat, lon) : DemoAstro.moonHorizon(n, lat, lon);
            directions.add(horizon[0]);
            directions.add(horizon[1]);
        }
        return directions;
    }

    /**
     * Draws the moon with the phase it really has: a disc with a bite taken out of it by a second
     * ellipse, which is what a terminator is - the projection of the circle dividing the lit and
     * unlit halves. Painting it into the sprite's bitmap keeps this out of the SDK entirely.
     */
    private void updateMoonPhase(double n) {
        double[] phase = DemoAstro.moonPhase(n);
        if (!DemoConfig.CELESTIAL_MOON_PHASE) {
            if (lastMoonPhase >= 0) {
                moon.setBitmap(null);
                lastMoonPhase = -1;
            }
            return;
        }
        double illuminated = phase[0];
        double signedPhase = phase[1] * illuminated;
        if (Math.abs(signedPhase - lastMoonPhase) < 0.01) {
            return; // a hundredth of a phase is invisible; do not rebuild the texture for it
        }
        lastMoonPhase = signedPhase;

        android.graphics.Bitmap bitmap = android.graphics.Bitmap.createBitmap(MOON_BITMAP_SIZE, MOON_BITMAP_SIZE, android.graphics.Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        float radius = MOON_BITMAP_SIZE * 0.5f;
        paint.setColor(0xFFF5F5EB);
        canvas.drawCircle(radius, radius, radius - 1, paint);

        // The terminator: an ellipse whose half-width goes from the full radius at new moon, through
        // zero at the quarter, to the full radius again at full moon - the same circle seen edge on.
        float terminator = (float) Math.abs(1.0 - 2.0 * illuminated) * (radius - 1);
        Paint eraser = new Paint(Paint.ANTI_ALIAS_FLAG);
        eraser.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        boolean waxing = phase[1] > 0;
        // Waxing: lit on the side towards the sun, the western limb - drawn on the right, so the
        // dark half is the left one (angles run clockwise from 3 o'clock in Canvas).
        RectF disc = new RectF(1, 1, MOON_BITMAP_SIZE - 1, MOON_BITMAP_SIZE - 1);
        canvas.drawArc(disc, waxing ? 90 : -90, 180, true, eraser);
        RectF terminatorOval = new RectF(radius - terminator, 1, radius + terminator, MOON_BITMAP_SIZE - 1);
        if (illuminated < 0.5) {
            // Crescent: the terminator bulges INTO the lit half, so the ellipse erases as well.
            canvas.drawOval(terminatorOval, eraser);
        } else {
            // Gibbous: it bulges into the dark half, so the ellipse paints the moon back in.
            canvas.drawOval(terminatorOval, paint);
        }
        moon.setBitmap(BitmapUtils.createBitmapFromAndroidBitmap(bitmap));
    }

    /**
     * An aircraft or a satellite: the same API, anchored above a real place instead of by
     * direction, so it parallaxes with the map like anything else that has a location.
     */
    public CelestialSprite addAircraft(double lon, double lat, double altitudeMeters) {
        CelestialSprite aircraft = new CelestialSprite();
        aircraft.setScreenSize(24f);
        aircraft.setColor(new Color((short) 255, (short) 255, (short) 255, (short) 255));
        aircraft.setPosition(new MapPos(lon, lat), altitudeMeters);
        aircraft.setMetaDataElement(META_NAME, new Variant("Aircraft"));
        layer.add(aircraft);
        return aircraft;
    }
}
