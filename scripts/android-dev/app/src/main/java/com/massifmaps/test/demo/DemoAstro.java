package com.massifmaps.test.demo;

import java.util.Calendar;
import java.util.TimeZone;

/**
 * Where the sun, the moon and the planets really are, for a date and a place on Earth.
 *
 * This is DEMO content, not SDK code: the celestial API takes an azimuth and an altitude and knows
 * nothing about astronomy. Everything here is the standard low-precision series (the Astronomical
 * Almanac's sun, the abbreviated lunar series, and JPL's approximate Keplerian elements for the
 * planets), which is worth a few arcminutes - far better than the half degree a body covers, and
 * the point is that what the demo draws is where you can go outside and look.
 *
 * Conventions, everywhere in this file: angles in DEGREES, time in UTC, azimuth clockwise from
 * north and altitude above the horizon - the same frame the celestial API and LightOptions use.
 */
public final class DemoAstro {

    private DemoAstro() {
    }

    /** Days since J2000.0 (2000-01-01 12:00 UTC) for a UTC calendar date and fractional hour. */
    public static double daysSinceJ2000(int year, int month, int day, double hourUtc) {
        // Standard Julian day for a Gregorian date, valid well beyond any date this demo cares about.
        long a = (14 - month) / 12;
        long y = year + 4800 - a;
        long m = month + 12 * a - 3;
        long jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
        return (jdn - 2451545.0) - 0.5 + hourUtc / 24.0;
    }

    /** Greenwich mean sidereal time in degrees. */
    public static double gmstDegrees(double n) {
        double gmstHours = (18.697374558 + 24.06570982441908 * n) % 24.0;
        if (gmstHours < 0) {
            gmstHours += 24.0;
        }
        return gmstHours * 15.0;
    }

    /** Mean obliquity of the ecliptic in degrees. */
    public static double obliquity(double n) {
        return 23.4392911 - 3.563E-7 * n;
    }

    /**
     * Equatorial to horizon. The hour angle is the local sidereal time minus the right ascension,
     * which is the whole of what makes the sky turn.
     *
     * @param rightAscension right ascension in DEGREES
     * @param declination declination in degrees
     * @return { azimuth, altitude } in degrees
     */
    public static double[] toHorizon(double rightAscension, double declination, double n, double lat, double lon) {
        double hourAngle = Math.toRadians(gmstDegrees(n) + lon - rightAscension);
        double decl = Math.toRadians(declination);
        double latRad = Math.toRadians(lat);
        double sinAlt = Math.sin(latRad) * Math.sin(decl) + Math.cos(latRad) * Math.cos(decl) * Math.cos(hourAngle);
        double altitude = Math.asin(clamp(sinAlt));
        double azimuth = Math.atan2(Math.sin(hourAngle),
                Math.cos(hourAngle) * Math.sin(latRad) - Math.tan(decl) * Math.cos(latRad));
        return new double[] { normalizeDegrees(Math.toDegrees(azimuth) + 180.0), Math.toDegrees(altitude) };
    }

    /** Ecliptic longitude/latitude (degrees) to right ascension (degrees) and declination. */
    public static double[] eclipticToEquatorial(double eclipticLong, double eclipticLat, double n) {
        double lambda = Math.toRadians(eclipticLong);
        double beta = Math.toRadians(eclipticLat);
        double eps = Math.toRadians(obliquity(n));
        double rightAsc = Math.atan2(Math.sin(lambda) * Math.cos(eps) - Math.tan(beta) * Math.sin(eps), Math.cos(lambda));
        double decl = Math.asin(clamp(Math.sin(beta) * Math.cos(eps) + Math.cos(beta) * Math.sin(eps) * Math.sin(lambda)));
        return new double[] { normalizeDegrees(Math.toDegrees(rightAsc)), Math.toDegrees(decl) };
    }

    // =================================================================================================
    // SUN
    // =================================================================================================

    /** Geocentric ecliptic longitude of the sun in degrees. */
    public static double sunEclipticLongitude(double n) {
        double meanLong = 280.460 + 0.9856474 * n;
        double meanAnom = Math.toRadians(357.528 + 0.9856003 * n);
        return normalizeDegrees(meanLong + 1.915 * Math.sin(meanAnom) + 0.020 * Math.sin(2 * meanAnom));
    }

    /** Right ascension (degrees) and declination of the sun. */
    public static double[] sunEquatorial(double n) {
        return eclipticToEquatorial(sunEclipticLongitude(n), 0.0, n);
    }

    /** Azimuth and altitude of the sun. */
    public static double[] sunHorizon(double n, double lat, double lon) {
        double[] eq = sunEquatorial(n);
        return toHorizon(eq[0], eq[1], n, lat, lon);
    }

    // =================================================================================================
    // MOON
    // =================================================================================================

    /** Geocentric ecliptic longitude and latitude of the moon, in degrees. */
    public static double[] moonEcliptic(double n) {
        double meanLong = 218.316 + 13.176396 * n;
        double meanAnom = Math.toRadians(134.963 + 13.064993 * n);
        double argLat = Math.toRadians(93.272 + 13.229350 * n);
        double eclipticLong = meanLong + 6.289 * Math.sin(meanAnom);
        double eclipticLat = 5.128 * Math.sin(argLat);
        return new double[] { normalizeDegrees(eclipticLong), eclipticLat };
    }

    /** Right ascension (degrees) and declination of the moon. */
    public static double[] moonEquatorial(double n) {
        double[] ecliptic = moonEcliptic(n);
        return eclipticToEquatorial(ecliptic[0], ecliptic[1], n);
    }

    /** Azimuth and altitude of the moon. */
    public static double[] moonHorizon(double n, double lat, double lon) {
        double[] eq = moonEquatorial(n);
        return toHorizon(eq[0], eq[1], n, lat, lon);
    }

    /**
     * The moon's phase: the illuminated fraction, and which limb is lit.
     *
     * The elongation between the moon and the sun IS the phase - 0 at new moon, 180 at full - and
     * its sign says whether the moon is waxing (lit on the side towards the sun, i.e. the western
     * limb in the northern hemisphere) or waning.
     *
     * @return { illuminated fraction 0..1, +1 waxing / -1 waning }
     */
    public static double[] moonPhase(double n) {
        double elongation = normalizeDegrees(moonEcliptic(n)[0] - sunEclipticLongitude(n));
        double illuminated = (1.0 - Math.cos(Math.toRadians(elongation))) * 0.5;
        return new double[] { illuminated, elongation < 180.0 ? 1.0 : -1.0 };
    }

    // =================================================================================================
    // PLANETS
    // =================================================================================================

    /** The planets the demo places, in the order of {@link #PLANET_ELEMENTS}. */
    public static final String[] PLANET_NAMES = { "Mercury", "Venus", "Mars", "Jupiter", "Saturn" };

    /**
     * JPL's approximate Keplerian elements, valid 1800-2050 (the "Approximate Positions of the
     * Major Planets" table): semi-major axis (au), eccentricity, inclination, mean longitude,
     * longitude of perihelion and longitude of the ascending node, each with its rate per Julian
     * century. Earth is last and is not drawn - it is what the geocentric view is taken from.
     */
    private static final double[][] PLANET_ELEMENTS = {
        // a, e, I, L, longPeri, longNode  then the six rates per century
        { 0.38709927, 0.20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593,
          0.00000037, 0.00001906, -0.00594749, 149472.67411175, 0.16047689, -0.12534081 },
        { 0.72333566, 0.00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255,
          0.00000390, -0.00004107, -0.00078890, 58517.81538729, 0.00268329, -0.27769418 },
        { 1.52371034, 0.09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891,
          0.00001847, 0.00007882, -0.00813131, 19140.30268499, 0.44441088, -0.29257343 },
        { 5.20288700, 0.04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909,
          -0.00011607, -0.00013253, -0.00183714, 3034.74612775, 0.21252668, 0.20469106 },
        { 9.53667594, 0.05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448,
          -0.00125060, -0.00050991, 0.00193609, 1222.49362201, -0.41897216, -0.28867794 },
        { 1.00000261, 0.01671123, -0.00001531, 100.46457166, 102.93768193, 0.0,
          0.00000562, -0.00004392, -0.01294668, 35999.37244981, 0.32327364, 0.0 },
    };

    private static final int EARTH = PLANET_ELEMENTS.length - 1;

    /**
     * Azimuth and altitude of a planet, by index into {@link #PLANET_NAMES}.
     *
     * Heliocentric position from the elements, minus the Earth's, gives the geocentric ecliptic
     * direction; the rest is the same conversion everything else here goes through. Light travel
     * time and planetary aberration are ignored (tens of arcseconds).
     */
    public static double[] planetHorizon(int planet, double n, double lat, double lon) {
        double[] body = heliocentric(planet, n);
        double[] earth = heliocentric(EARTH, n);
        double x = body[0] - earth[0];
        double y = body[1] - earth[1];
        double z = body[2] - earth[2];
        double eclipticLong = Math.toDegrees(Math.atan2(y, x));
        double eclipticLat = Math.toDegrees(Math.atan2(z, Math.sqrt(x * x + y * y)));
        double[] eq = eclipticToEquatorial(eclipticLong, eclipticLat, n);
        return toHorizon(eq[0], eq[1], n, lat, lon);
    }

    /** Heliocentric ecliptic rectangular coordinates (au) of one body of {@link #PLANET_ELEMENTS}. */
    private static double[] heliocentric(int planet, double n) {
        double[] e = PLANET_ELEMENTS[planet];
        double t = n / 36525.0;
        double a = e[0] + e[6] * t;
        double ecc = e[1] + e[7] * t;
        double inc = Math.toRadians(e[2] + e[8] * t);
        double meanLong = e[3] + e[9] * t;
        double longPeri = e[4] + e[10] * t;
        double longNodeDeg = e[5] + e[11] * t;
        double longNode = Math.toRadians(longNodeDeg);
        double argPeri = Math.toRadians(longPeri - longNodeDeg);

        // Mean anomaly, folded into -180..180 as the Kepler iteration below expects.
        double meanAnom = Math.toRadians(normalizeDegrees(meanLong - longPeri + 180.0) - 180.0);
        double eccAnom = meanAnom;
        for (int i = 0; i < 12; i++) {
            double delta = (eccAnom - ecc * Math.sin(eccAnom) - meanAnom) / (1.0 - ecc * Math.cos(eccAnom));
            eccAnom -= delta;
            if (Math.abs(delta) < 1.0E-10) {
                break;
            }
        }

        // In the orbital plane, then rotated by the argument of perihelion, the inclination and the
        // longitude of the ascending node into the J2000 ecliptic frame.
        double xOrbit = a * (Math.cos(eccAnom) - ecc);
        double yOrbit = a * Math.sqrt(1.0 - ecc * ecc) * Math.sin(eccAnom);
        double cosPeri = Math.cos(argPeri), sinPeri = Math.sin(argPeri);
        double cosNode = Math.cos(longNode), sinNode = Math.sin(longNode);
        double cosInc = Math.cos(inc), sinInc = Math.sin(inc);
        double xPlane = xOrbit * cosPeri - yOrbit * sinPeri;
        double yPlane = xOrbit * sinPeri + yOrbit * cosPeri;
        return new double[] {
            xPlane * cosNode - yPlane * cosInc * sinNode,
            xPlane * sinNode + yPlane * cosInc * cosNode,
            yPlane * sinInc,
        };
    }

    // =================================================================================================
    // HELPERS
    // =================================================================================================

    /** Today's UTC date and fractional hour: { year, month, day, hour }. */
    public static double[] nowUtc() {
        Calendar calendar = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        double hour = calendar.get(Calendar.HOUR_OF_DAY)
                + calendar.get(Calendar.MINUTE) / 60.0
                + calendar.get(Calendar.SECOND) / 3600.0;
        return new double[] { calendar.get(Calendar.YEAR), calendar.get(Calendar.MONTH) + 1, calendar.get(Calendar.DAY_OF_MONTH), hour };
    }

    public static double normalizeDegrees(double degrees) {
        double result = degrees % 360.0;
        return result < 0 ? result + 360.0 : result;
    }

    private static double clamp(double value) {
        return Math.max(-1.0, Math.min(1.0, value));
    }
}
