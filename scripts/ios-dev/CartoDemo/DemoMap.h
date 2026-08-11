#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

/**
 * Builds and updates the map, the counterpart of scripts/android-dev's DemoMap.java: the layer
 * registry, the shared tile sources, terrain, sky/light and the camera. The panel writes
 * DemoConfig and then calls one of the apply* methods, exactly as DemoPanel.java does.
 */
@interface DemoMap : NSObject

+ (void)setupMapView:(NTMapView *)mapView;

/** Rebuild the layer stack from DemoConfig. */
+ (void)applyLayers:(NTMapView *)mapView;
/** Push DemoConfig's Options-level settings (tile pool, sky). */
+ (void)applyOptions:(NTMapView *)mapView;
/** Rebuild TerrainOptions from DemoConfig. */
+ (void)applyTerrainConfig:(NTMapView *)mapView;
/** Push sun/ambient/shadow onto LightOptions. */
+ (void)applySkyAndLightConfig:(NTMapView *)mapView;
/** Move the camera to DemoConfig's position, then apply the free-roam and peak-finder modes. */
+ (void)applyCameraConfig:(NTMapView *)mapView;
/** Rebuild the sky-anchored objects (sun, moon, arcs, stars). */
+ (void)applyCelestial:(NTMapView *)mapView;

@end
