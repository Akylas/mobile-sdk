#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

/**
 * Sky-anchored objects: the sun and the moon as sprites, their day arcs as circles, and a star
 * field. The counterpart of scripts/android-dev's DemoCelestial.java and DemoStars.java, on the
 * SDK's CelestialLayer.
 *
 * Everything here is positioned by azimuth/altitude rather than by map position, which is the
 * point of the layer: the objects sit on the sky dome and stay put as the map moves under them.
 */
@interface DemoCelestial : NSObject

/** Rebuild the celestial layer from DemoConfig and add it to the map. Removes any previous one. */
+ (void)applyToMapView:(NTMapView *)mapView;
/** Drop the layer. */
+ (void)removeFromMapView:(NTMapView *)mapView;

@end
