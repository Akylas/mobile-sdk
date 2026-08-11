#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

/**
 * Day cycle: sun position from an hour of the day, and the sky/light colours that follow it. The
 * counterpart of scripts/android-dev's DemoSky.java.
 *
 * The azimuth/altitude pair here is a simple solar model, not an ephemeris - it is enough to drive
 * the lighting and to watch the sky and the terrain shading move together, which is what the bench
 * is for.
 */
@interface DemoSky : NSObject

/** Sun azimuth in degrees for an hour of the day at the map's latitude. */
+ (float)sunAzimuthForHour:(float)hour latitude:(double)latitude;
/** Sun altitude in degrees for the same. Negative below the horizon. */
+ (float)sunAltitudeForHour:(float)hour latitude:(double)latitude;

/** Push the day-cycle sun and the matching sky colours onto the map. */
+ (void)applyDayCycle:(NTMapView *)mapView hour:(float)hour;

@end
