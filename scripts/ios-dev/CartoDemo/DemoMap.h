#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

/**
 * Builds and updates the map, the iOS counterpart of scripts/android-dev's DemoMap.java: the
 * layer registry, the shared tile sources, the terrain options and the camera all live here, and
 * the panel (once ported) writes DemoConfig and calls back into an apply* method.
 */
@interface DemoMap : NSObject

+ (void)setupMapView:(NTMapView *)mapView;

/** Rebuild the layer stack from DemoConfig. */
+ (void)applyLayers:(NTMapView *)mapView;
/** Push DemoConfig's terrain settings onto Options. */
+ (void)applyTerrainConfig:(NTMapView *)mapView;
/** Move the camera to DemoConfig's start position. */
+ (void)applyCameraConfig:(NTMapView *)mapView;

@end
