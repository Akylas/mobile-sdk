#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

@class NTCelestialLayer;
@class NTCelestialSprite;

/**
 * The sun, the moon and their paths across the day, built on the generic celestial API. The
 * counterpart of scripts/android-dev's DemoCelestial.java.
 *
 * NOTHING here is a sun or a moon as far as the SDK is concerned: they are two sprites and two
 * arcs, placed by direction. The astronomy lives in DemoAstro, which is the point - the same API
 * carries an aircraft (-addAircraft) or a star catalogue (DemoStars) with no SDK change at all.
 *
 * The positions are the REAL ones for the demo's date, hour and location, and the arcs are sampled
 * from the same ephemeris over the whole day, so the disc always sits exactly on its own arc. That
 * is a free correctness check on both: they are computed independently of each other.
 */
@interface DemoCelestial : NSObject

/** Builds the layer and its objects. Added FIRST, so the map and the terrain draw over them. */
- (NTCelestialLayer *)createLayer:(NTMapView *)mapView;
/** Places the objects for the demo's current date, time and location. */
- (void)update;
/** An aircraft or a satellite: the same API, anchored above a real place instead of by direction. */
- (NTCelestialSprite *)addAircraft:(double)lon lat:(double)lat altitude:(double)altitudeMeters;

@end
