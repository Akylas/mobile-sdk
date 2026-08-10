#import "DemoConfig.h"
#import "DemoCfg.h"

@implementation DemoConfig

static BOOL sBaseEnabled = YES;
static BOOL sSatelliteEnabled = NO;
static BOOL sHillshadeEnabled = NO;

static NSString *sRasterUrl = @"https://tile.openstreetmap.org/{z}/{x}/{y}.png";
static int sRasterMinZoom = 0;
static int sRasterMaxZoom = 19;
static NSString *sDemUrl = @"https://tiles.mapterhorn.com/{z}/{x}/{y}.webp";
static int sDemMinZoom = 1;
// The REAL max zoom of the source (mapterhorn stops at 16). Setting it higher only produces
// 404s: deeper camera zooms are served by overzooming the last available level.
static int sDemMaxZoom = 16;
static NSString *sDemEncoding = @"terrarium";
static NSString *sHttpUserAgent = @"CartoDemo-iOS/1.0";

static double sStartLon = 5.718957;
static double sStartLat = 45.187362;
static float sStartZoom = 16.22f;
static float sStartTilt = 26.0f;
static float sStartRotation = -15.12f;

static BOOL sTerrainEnabled = YES;
static float sTerrainExaggeration = 1.0f;
static int sTerrainMeshResolution = 64;

+ (BOOL)baseEnabled { return sBaseEnabled; }
+ (void)setBaseEnabled:(BOOL)v { sBaseEnabled = v; }
+ (BOOL)satelliteEnabled { return sSatelliteEnabled; }
+ (void)setSatelliteEnabled:(BOOL)v { sSatelliteEnabled = v; }
+ (BOOL)hillshadeEnabled { return sHillshadeEnabled; }
+ (void)setHillshadeEnabled:(BOOL)v { sHillshadeEnabled = v; }

+ (NSString *)rasterUrl { return sRasterUrl; }
+ (void)setRasterUrl:(NSString *)v { sRasterUrl = [v copy]; }
+ (int)rasterMinZoom { return sRasterMinZoom; }
+ (void)setRasterMinZoom:(int)v { sRasterMinZoom = v; }
+ (int)rasterMaxZoom { return sRasterMaxZoom; }
+ (void)setRasterMaxZoom:(int)v { sRasterMaxZoom = v; }
+ (NSString *)demUrl { return sDemUrl; }
+ (void)setDemUrl:(NSString *)v { sDemUrl = [v copy]; }
+ (int)demMinZoom { return sDemMinZoom; }
+ (void)setDemMinZoom:(int)v { sDemMinZoom = v; }
+ (int)demMaxZoom { return sDemMaxZoom; }
+ (void)setDemMaxZoom:(int)v { sDemMaxZoom = v; }
+ (NSString *)demEncoding { return sDemEncoding; }
+ (void)setDemEncoding:(NSString *)v { sDemEncoding = [v copy]; }
+ (NSString *)httpUserAgent { return sHttpUserAgent; }
+ (void)setHttpUserAgent:(NSString *)v { sHttpUserAgent = [v copy]; }

+ (double)startLon { return sStartLon; }
+ (void)setStartLon:(double)v { sStartLon = v; }
+ (double)startLat { return sStartLat; }
+ (void)setStartLat:(double)v { sStartLat = v; }
+ (float)startZoom { return sStartZoom; }
+ (void)setStartZoom:(float)v { sStartZoom = v; }
+ (float)startTilt { return sStartTilt; }
+ (void)setStartTilt:(float)v { sStartTilt = v; }
+ (float)startRotation { return sStartRotation; }
+ (void)setStartRotation:(float)v { sStartRotation = v; }

+ (BOOL)terrainEnabled { return sTerrainEnabled; }
+ (void)setTerrainEnabled:(BOOL)v { sTerrainEnabled = v; }
+ (float)terrainExaggeration { return sTerrainExaggeration; }
+ (void)setTerrainExaggeration:(float)v { sTerrainExaggeration = v; }
+ (int)terrainMeshResolution { return sTerrainMeshResolution; }
+ (void)setTerrainMeshResolution:(int)v { sTerrainMeshResolution = v; }

+ (void)applyLaunchArgumentOverrides {
    // Key names deliberately match DemoConfig.java's intent-extra keys.
    sBaseEnabled = [DemoCfg boolFor:@"base" defaultValue:sBaseEnabled];
    sSatelliteEnabled = [DemoCfg boolFor:@"satellite" defaultValue:sSatelliteEnabled];
    sHillshadeEnabled = [DemoCfg boolFor:@"hillshade" defaultValue:sHillshadeEnabled];

    sRasterUrl = [DemoCfg stringFor:@"rasterUrl" defaultValue:sRasterUrl];
    sDemUrl = [DemoCfg stringFor:@"demUrl" defaultValue:sDemUrl];
    sDemEncoding = [DemoCfg stringFor:@"demEncoding" defaultValue:sDemEncoding];

    sStartLon = [DemoCfg doubleFor:@"lon" defaultValue:sStartLon];
    sStartLat = [DemoCfg doubleFor:@"lat" defaultValue:sStartLat];
    sStartZoom = [DemoCfg floatFor:@"zoom" defaultValue:sStartZoom];
    sStartTilt = [DemoCfg floatFor:@"tilt" defaultValue:sStartTilt];
    sStartRotation = [DemoCfg floatFor:@"rotation" defaultValue:sStartRotation];

    sTerrainEnabled = [DemoCfg boolFor:@"terrain" defaultValue:sTerrainEnabled];
    sTerrainExaggeration = [DemoCfg floatFor:@"exaggeration" defaultValue:sTerrainExaggeration];
    sTerrainMeshResolution = [DemoCfg intFor:@"meshResolution" defaultValue:sTerrainMeshResolution];
}

@end
