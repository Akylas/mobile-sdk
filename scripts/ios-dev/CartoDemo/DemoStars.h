#import <Foundation/Foundation.h>
#import "MassifMaps.h"

@class NTCelestialLayer;

/**
 * The star demo: the real sky over the demo's position, right now. The counterpart of
 * scripts/android-dev's DemoStars.java.
 *
 * Every star is a CelestialSprite and every constellation figure is one CelestialArc in segment
 * mode, so the whole catalogue is ONE draw call for the stars plus one per figure, and both are
 * clickable. The planets are sprites too - the API does not know what any of them are.
 *
 * The astronomy is in DemoAstro and the data in DemoStarCatalogue; this file is the mapping onto
 * the SDK.
 */
@interface DemoStars : NSObject

/** Builds the layer: stars, constellation figures, planets. Positions come from -update. */
- (NTCelestialLayer *)createLayer:(NTMapView *)mapView;
/** Places everything for a date (days since J2000) and a position. */
- (void)updateWithN:(double)n lat:(double)lat lon:(double)lon;

@end
