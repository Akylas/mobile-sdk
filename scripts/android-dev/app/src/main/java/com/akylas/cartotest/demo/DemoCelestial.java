package com.akylas.cartotest.demo;

import android.widget.Toast;

import com.carto.celestial.CelestialArc;
import com.carto.celestial.CelestialObject;
import com.carto.celestial.CelestialSprite;
import com.carto.graphics.Color;
import com.carto.layers.CelestialEventListener;
import com.carto.layers.CelestialLayer;
import com.carto.ui.ClickInfo;
import com.carto.ui.MapView;

/**
 * The sun, the moon and the sun's path across the day, built on the generic celestial API.
 *
 * NOTHING here is a sun or a moon as far as the SDK is concerned: they are two sprites and one
 * arc, placed by direction. The astronomy lives in this file, which is the point - the same API
 * carries an aircraft ({@link #addAircraft}) or a star catalogue with no SDK change at all.
 */
public final class DemoCelestial {

    private static final String META_NAME = "name";

    private CelestialLayer layer;
    private CelestialSprite sun;
    private CelestialSprite moon;
    private CelestialArc sunPath;

    /** Builds the layer and its objects. Added FIRST, so the map and the terrain draw over them. */
    public CelestialLayer createLayer(final MapView mapView) {
        layer = new CelestialLayer();

        sun = new CelestialSprite();
        sun.setAngularSize(DemoConfig.CELESTIAL_SUN_SIZE);
        sun.setColor(new Color((short) 255, (short) 244, (short) 214, (short) 255));
        sun.setSoftness(0.35f);
        sun.setClickRadius(3f);
        sun.setMetaDataElement(META_NAME, new com.carto.core.Variant("Sun"));

        moon = new CelestialSprite();
        moon.setAngularSize(DemoConfig.CELESTIAL_MOON_SIZE);
        moon.setColor(new Color((short) 235, (short) 235, (short) 225, (short) 255));
        moon.setSoftness(0.25f);
        moon.setClickRadius(3f);
        moon.setMetaDataElement(META_NAME, new com.carto.core.Variant("Moon"));

        sunPath = new CelestialArc();
        sunPath.setColor(new Color((short) 255, (short) 216, (short) 120, (short) 160));
        sunPath.setWidth(DemoConfig.CELESTIAL_ARC_WIDTH);
        sunPath.setBelowHorizonVisible(false);
        sunPath.setMetaDataElement(META_NAME, new com.carto.core.Variant("Sun path"));

        layer.add(sun);
        layer.add(moon);
        layer.add(sunPath);

        layer.setCelestialEventListener(new CelestialEventListener() {
            @Override
            public boolean onCelestialObjectClicked(ClickInfo clickInfo, CelestialObject object) {
                final String message = object.getMetaDataElement(META_NAME).getString()
                        + "  az " + Math.round(object.getAzimuth()) + "\u00b0  alt " + Math.round(object.getAltitude()) + "\u00b0";
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
     * Places the objects for the demo's current date, time and location. The sun's direction is
     * taken from the SDK's own solar position (LightOptions), so the sprite sits exactly where the
     * light comes from; the moon and the arc are computed here.
     */
    public void update(DemoMap demoMap) {
        if (layer == null || demoMap == null || demoMap.lightOptions == null) {
            return;
        }
        double hourUtc = DemoConfig.SUN_HOUR_UTC >= 0 ? DemoConfig.SUN_HOUR_UTC : DemoConfig.DAY_CYCLE_HOUR;
        double lat = DemoConfig.START_LAT;
        double lon = DemoConfig.START_LON;

        float sunAzimuth = demoMap.lightOptions.getSunAzimuth();
        float sunAltitude = demoMap.lightOptions.getSunAltitude();
        sun.setDirection(sunAzimuth, sunAltitude, 0);

        double[] moonDir = moonAzimuthAltitude(DemoConfig.SUN_YEAR, DemoConfig.SUN_MONTH, DemoConfig.SUN_DAY, hourUtc, lat, lon);
        moon.setDirection((float) moonDir[0], (float) moonDir[1], 0);

        // The sun's path across the day is the circle of constant declination about the celestial
        // pole: the axis points north at an altitude equal to the latitude, and the radius is 90
        // degrees minus the declination. Declination comes back out of the sun's own direction.
        double altRad = Math.toRadians(sunAltitude);
        double azRad = Math.toRadians(sunAzimuth);
        double latRad = Math.toRadians(lat);
        double sinDecl = Math.sin(altRad) * Math.sin(latRad) + Math.cos(altRad) * Math.cos(latRad) * Math.cos(azRad);
        double declination = Math.toDegrees(Math.asin(Math.max(-1, Math.min(1, sinDecl))));
        sunPath.setCircle(lat >= 0 ? 0f : 180f, (float) Math.abs(lat), (float) (90.0 - declination));

        sun.setVisible(DemoConfig.CELESTIAL_SUN);
        moon.setVisible(DemoConfig.CELESTIAL_MOON);
        sunPath.setVisible(DemoConfig.CELESTIAL_ARC);
    }

    /**
     * An aircraft or a satellite: the same API, anchored above a real place instead of by
     * direction, so it parallaxes with the map like anything else that has a location.
     */
    public CelestialSprite addAircraft(double lon, double lat, double altitudeMeters) {
        CelestialSprite aircraft = new CelestialSprite();
        aircraft.setScreenSize(24f);
        aircraft.setColor(new Color((short) 255, (short) 255, (short) 255, (short) 255));
        aircraft.setPosition(new com.carto.core.MapPos(lon, lat), altitudeMeters);
        aircraft.setMetaDataElement(META_NAME, new com.carto.core.Variant("Aircraft"));
        layer.add(aircraft);
        return aircraft;
    }

    /**
     * Moon position, low-precision (about 0.3 degrees, which is the moon's own diameter): the
     * standard abbreviated lunar series, then the same equatorial-to-horizon conversion the sun
     * uses. Good enough to point at the moon in the sky; not an ephemeris.
     */
    private static double[] moonAzimuthAltitude(int year, int month, int day, double hourUtc, double lat, double lon) {
        int a = (14 - month) / 12;
        int y = year + 4800 - a;
        int m = month + 12 * a - 3;
        double jdn = day + (153 * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045;
        double jd = jdn + (hourUtc - 12.0) / 24.0;
        double n = jd - 2451545.0;

        double meanLong = Math.toRadians(218.316 + 13.176396 * n);
        double meanAnom = Math.toRadians(134.963 + 13.064993 * n);
        double argLat = Math.toRadians(93.272 + 13.229350 * n);

        double eclipticLong = meanLong + Math.toRadians(6.289) * Math.sin(meanAnom);
        double eclipticLat = Math.toRadians(5.128) * Math.sin(argLat);
        double obliquity = Math.toRadians(23.439 - 0.0000004 * n);

        double rightAsc = Math.atan2(Math.sin(eclipticLong) * Math.cos(obliquity) - Math.tan(eclipticLat) * Math.sin(obliquity),
                Math.cos(eclipticLong));
        double decl = Math.asin(Math.sin(eclipticLat) * Math.cos(obliquity)
                + Math.cos(eclipticLat) * Math.sin(obliquity) * Math.sin(eclipticLong));

        double gmst = (18.697374558 + 24.06570982441908 * n) % 24.0;
        if (gmst < 0) {
            gmst += 24.0;
        }
        double lmst = Math.toRadians(gmst * 15.0) + Math.toRadians(lon);
        double hourAngle = lmst - rightAsc;

        double latRad = Math.toRadians(lat);
        double altitude = Math.asin(Math.sin(latRad) * Math.sin(decl) + Math.cos(latRad) * Math.cos(decl) * Math.cos(hourAngle));
        double azimuth = Math.atan2(Math.sin(hourAngle), Math.cos(hourAngle) * Math.sin(latRad) - Math.tan(decl) * Math.cos(latRad));
        return new double[] { Math.toDegrees(azimuth) + 180.0, Math.toDegrees(altitude) };
    }
}
