#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

/**
 * Where the sun, the moon and the planets really are, for a date and a place on Earth. The
 * counterpart of scripts/android-dev's DemoAstro.java, ported term for term.
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

/** Azimuth (clockwise from north) and altitude (above the horizon), both in degrees. */
typedef struct {
    double azimuth;
    double altitude;
} DemoHorizon;

/** Right ascension in DEGREES and declination in degrees. */
typedef struct {
    double rightAscension;
    double declination;
} DemoEquatorial;

@interface DemoAstro : NSObject

/** Days since J2000.0 (2000-01-01 12:00 UTC) for a UTC calendar date and fractional hour. */
+ (double)daysSinceJ2000WithYear:(int)year month:(int)month day:(int)day hour:(double)hourUtc;
/** Greenwich mean sidereal time in degrees. */
+ (double)gmstDegrees:(double)n;
/** Equatorial to horizon: the hour angle is what makes the sky turn. */
+ (DemoHorizon)toHorizon:(DemoEquatorial)equatorial n:(double)n lat:(double)lat lon:(double)lon;

/** Azimuth and altitude of the sun. */
+ (DemoHorizon)sunHorizon:(double)n lat:(double)lat lon:(double)lon;
/** Azimuth and altitude of the moon. */
+ (DemoHorizon)moonHorizon:(double)n lat:(double)lat lon:(double)lon;
/**
 * The moon's phase: { illuminated fraction 0..1, +1 waxing / -1 waning }, returned as a point so
 * the two travel together the way the Java version's double[] does.
 */
+ (CGPoint)moonPhase:(double)n;

/** The planets this places, in the order {@link planetHorizon} indexes. */
+ (NSArray<NSString *> *)planetNames;
/** Azimuth and altitude of a planet, by index into {@link planetNames}. */
+ (DemoHorizon)planetHorizon:(int)planet n:(double)n lat:(double)lat lon:(double)lon;

/** Today's UTC date and fractional hour. */
+ (void)nowUtcYear:(int *)year month:(int *)month day:(int *)day hour:(double *)hour;
+ (double)normalizeDegrees:(double)degrees;

@end
