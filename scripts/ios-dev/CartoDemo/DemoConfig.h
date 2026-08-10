#import <Foundation/Foundation.h>

/**
 * Every default in one place, mirroring scripts/android-dev's DemoConfig.java field for field so
 * the two demos can be compared knob by knob. Change a default here, not in DemoMap.
 *
 * applyLaunchArgumentOverrides maps launch arguments onto these, the same way DemoConfig.java's
 * applyIntentOverrides maps intent extras.
 */
@interface DemoConfig : NSObject

// Layers
@property (class, nonatomic) BOOL baseEnabled;
@property (class, nonatomic) BOOL satelliteEnabled;
@property (class, nonatomic) BOOL hillshadeEnabled;

// Tile sources - kept identical to DemoConfig.java
@property (class, nonatomic, copy) NSString *rasterUrl;
@property (class, nonatomic) int rasterMinZoom;
@property (class, nonatomic) int rasterMaxZoom;
@property (class, nonatomic, copy) NSString *demUrl;
@property (class, nonatomic) int demMinZoom;
@property (class, nonatomic) int demMaxZoom;
/** "terrarium" or "mapbox" - decides which ElevationDecoder is used. */
@property (class, nonatomic, copy) NSString *demEncoding;
@property (class, nonatomic, copy) NSString *httpUserAgent;

// Camera
@property (class, nonatomic) double startLon;
@property (class, nonatomic) double startLat;
@property (class, nonatomic) float startZoom;
@property (class, nonatomic) float startTilt;
@property (class, nonatomic) float startRotation;

// 3D terrain
@property (class, nonatomic) BOOL terrainEnabled;
@property (class, nonatomic) float terrainExaggeration;
@property (class, nonatomic) int terrainMeshResolution;

+ (void)applyLaunchArgumentOverrides;

@end
