#import "DemoAstro.h"

@implementation DemoAstro

static double radians(double degrees) { return degrees * M_PI / 180.0; }
static double degrees(double rad) { return rad * 180.0 / M_PI; }
static double clampUnit(double value) { return fmax(-1.0, fmin(1.0, value)); }

+ (double)normalizeDegrees:(double)value {
    double result = fmod(value, 360.0);
    return result < 0 ? result + 360.0 : result;
}

+ (double)daysSinceJ2000WithYear:(int)year month:(int)month day:(int)day hour:(double)hourUtc {
    // Standard Julian day for a Gregorian date, valid well beyond any date this demo cares about.
    long a = (14 - month) / 12;
    long y = year + 4800 - a;
    long m = month + 12 * a - 3;
    long jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    return (jdn - 2451545.0) - 0.5 + hourUtc / 24.0;
}

+ (double)gmstDegrees:(double)n {
    double gmstHours = fmod(18.697374558 + 24.06570982441908 * n, 24.0);
    if (gmstHours < 0) {
        gmstHours += 24.0;
    }
    return gmstHours * 15.0;
}

/** Mean obliquity of the ecliptic in degrees. */
+ (double)obliquity:(double)n {
    return 23.4392911 - 3.563E-7 * n;
}

+ (DemoHorizon)toHorizon:(DemoEquatorial)equatorial n:(double)n lat:(double)lat lon:(double)lon {
    double hourAngle = radians([self gmstDegrees:n] + lon - equatorial.rightAscension);
    double decl = radians(equatorial.declination);
    double latRad = radians(lat);
    double sinAlt = sin(latRad) * sin(decl) + cos(latRad) * cos(decl) * cos(hourAngle);
    double altitude = asin(clampUnit(sinAlt));
    double azimuth = atan2(sin(hourAngle), cos(hourAngle) * sin(latRad) - tan(decl) * cos(latRad));
    DemoHorizon result;
    result.azimuth = [self normalizeDegrees:degrees(azimuth) + 180.0];
    result.altitude = degrees(altitude);
    return result;
}

/** Ecliptic longitude/latitude (degrees) to right ascension (degrees) and declination. */
+ (DemoEquatorial)eclipticToEquatorial:(double)eclipticLong latitude:(double)eclipticLat n:(double)n {
    double lambda = radians(eclipticLong);
    double beta = radians(eclipticLat);
    double eps = radians([self obliquity:n]);
    double rightAsc = atan2(sin(lambda) * cos(eps) - tan(beta) * sin(eps), cos(lambda));
    double decl = asin(clampUnit(sin(beta) * cos(eps) + cos(beta) * sin(eps) * sin(lambda)));
    DemoEquatorial result;
    result.rightAscension = [self normalizeDegrees:degrees(rightAsc)];
    result.declination = degrees(decl);
    return result;
}

// =================================================================================================
// SUN
// =================================================================================================

/** Geocentric ecliptic longitude of the sun in degrees. */
+ (double)sunEclipticLongitude:(double)n {
    double meanLong = 280.460 + 0.9856474 * n;
    double meanAnom = radians(357.528 + 0.9856003 * n);
    return [self normalizeDegrees:meanLong + 1.915 * sin(meanAnom) + 0.020 * sin(2 * meanAnom)];
}

+ (DemoHorizon)sunHorizon:(double)n lat:(double)lat lon:(double)lon {
    DemoEquatorial equatorial = [self eclipticToEquatorial:[self sunEclipticLongitude:n] latitude:0.0 n:n];
    return [self toHorizon:equatorial n:n lat:lat lon:lon];
}

// =================================================================================================
// MOON
// =================================================================================================

/** Geocentric ecliptic longitude and latitude of the moon, in degrees. */
+ (CGPoint)moonEcliptic:(double)n {
    double meanLong = 218.316 + 13.176396 * n;
    double meanAnom = radians(134.963 + 13.064993 * n);
    double argLat = radians(93.272 + 13.229350 * n);
    return CGPointMake([self normalizeDegrees:meanLong + 6.289 * sin(meanAnom)], 5.128 * sin(argLat));
}

+ (DemoHorizon)moonHorizon:(double)n lat:(double)lat lon:(double)lon {
    CGPoint ecliptic = [self moonEcliptic:n];
    DemoEquatorial equatorial = [self eclipticToEquatorial:ecliptic.x latitude:ecliptic.y n:n];
    return [self toHorizon:equatorial n:n lat:lat lon:lon];
}

+ (CGPoint)moonPhase:(double)n {
    // The elongation between the moon and the sun IS the phase - 0 at new moon, 180 at full - and
    // its sign says whether the moon is waxing (lit on the side towards the sun, i.e. the western
    // limb in the northern hemisphere) or waning.
    double elongation = [self normalizeDegrees:[self moonEcliptic:n].x - [self sunEclipticLongitude:n]];
    double illuminated = (1.0 - cos(radians(elongation))) * 0.5;
    return CGPointMake(illuminated, elongation < 180.0 ? 1.0 : -1.0);
}

// =================================================================================================
// PLANETS
// =================================================================================================

+ (NSArray<NSString *> *)planetNames {
    return @[@"Mercury", @"Venus", @"Mars", @"Jupiter", @"Saturn"];
}

/**
 * JPL's approximate Keplerian elements, valid 1800-2050 (the "Approximate Positions of the Major
 * Planets" table): semi-major axis (au), eccentricity, inclination, mean longitude, longitude of
 * perihelion and longitude of the ascending node, each with its rate per Julian century. Earth is
 * last and is not drawn - it is what the geocentric view is taken from.
 */
static const double PLANET_ELEMENTS[6][12] = {
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

static const int EARTH = 5;

/** Heliocentric ecliptic rectangular coordinates (au) of one body of PLANET_ELEMENTS. */
+ (void)heliocentric:(int)planet n:(double)n out:(double *)out {
    const double *e = PLANET_ELEMENTS[planet];
    double t = n / 36525.0;
    double a = e[0] + e[6] * t;
    double ecc = e[1] + e[7] * t;
    double inc = radians(e[2] + e[8] * t);
    double meanLong = e[3] + e[9] * t;
    double longPeri = e[4] + e[10] * t;
    double longNodeDeg = e[5] + e[11] * t;
    double longNode = radians(longNodeDeg);
    double argPeri = radians(longPeri - longNodeDeg);

    // Mean anomaly, folded into -180..180 as the Kepler iteration below expects.
    double meanAnom = radians([self normalizeDegrees:meanLong - longPeri + 180.0] - 180.0);
    double eccAnom = meanAnom;
    for (int i = 0; i < 12; i++) {
        double delta = (eccAnom - ecc * sin(eccAnom) - meanAnom) / (1.0 - ecc * cos(eccAnom));
        eccAnom -= delta;
        if (fabs(delta) < 1.0E-10) {
            break;
        }
    }

    // In the orbital plane, then rotated by the argument of perihelion, the inclination and the
    // longitude of the ascending node into the J2000 ecliptic frame.
    double xOrbit = a * (cos(eccAnom) - ecc);
    double yOrbit = a * sqrt(1.0 - ecc * ecc) * sin(eccAnom);
    double cosPeri = cos(argPeri), sinPeri = sin(argPeri);
    double cosNode = cos(longNode), sinNode = sin(longNode);
    double cosInc = cos(inc), sinInc = sin(inc);
    double xPlane = xOrbit * cosPeri - yOrbit * sinPeri;
    double yPlane = xOrbit * sinPeri + yOrbit * cosPeri;
    out[0] = xPlane * cosNode - yPlane * cosInc * sinNode;
    out[1] = xPlane * sinNode + yPlane * cosInc * cosNode;
    out[2] = yPlane * sinInc;
}

+ (DemoHorizon)planetHorizon:(int)planet n:(double)n lat:(double)lat lon:(double)lon {
    // Heliocentric position from the elements, minus the Earth's, gives the geocentric ecliptic
    // direction; the rest is the same conversion everything else here goes through. Light travel
    // time and planetary aberration are ignored (tens of arcseconds).
    double body[3], earth[3];
    [self heliocentric:planet n:n out:body];
    [self heliocentric:EARTH n:n out:earth];
    double x = body[0] - earth[0];
    double y = body[1] - earth[1];
    double z = body[2] - earth[2];
    DemoEquatorial equatorial = [self eclipticToEquatorial:degrees(atan2(y, x))
                                                  latitude:degrees(atan2(z, sqrt(x * x + y * y)))
                                                         n:n];
    return [self toHorizon:equatorial n:n lat:lat lon:lon];
}

// =================================================================================================
// HELPERS
// =================================================================================================

+ (void)nowUtcYear:(int *)year month:(int *)month day:(int *)day hour:(double *)hour {
    NSCalendar *calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    calendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
    NSDateComponents *parts = [calendar components:(NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay |
                                                    NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond)
                                          fromDate:[NSDate date]];
    *year = (int)parts.year;
    *month = (int)parts.month;
    *day = (int)parts.day;
    *hour = parts.hour + parts.minute / 60.0 + parts.second / 3600.0;
}

@end
