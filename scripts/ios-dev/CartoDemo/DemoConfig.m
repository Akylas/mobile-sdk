#import "DemoConfig.h"

@implementation DemoConfig

static NSMutableDictionary *sValues = nil;

+ (void)initialize {
    if (self != [DemoConfig class]) {
        return;
    }
    // Defaults, grouped and named exactly as in DemoConfig.java. A key that is not here cannot be
    // overridden at launch either, so adding a knob is one line.
    sValues = [@{
        // --- base map ---
        @"base":                @"composite",   // plain | composite
        @"style":               @"inline",      // dir | zip | inline | nuti
        @"map":                 @YES,
        @"singlePass":          @YES,

        // --- layers ---
        @"satellite":           @NO,
        @"hillshade":           @NO,
        @"hypso":               @NO,
        @"contour":             @NO,
        @"contourTiles":        @NO,
        @"elements":            @NO,
        @"routes":              @NO,

        // --- composite slots ---
        @"hs":                  @NO,
        @"sat":                 @NO,
        @"hsBias":              @0.0f,

        // --- tile sources ---
        @"vectorUrl":           @"https://tiles.akylas.fr/data/france/{z}/{x}/{y}.pbf",
        @"vectorMaxZoom":       @14,
        @"demUrl":              @"https://tiles.mapterhorn.com/{z}/{x}/{y}.webp",
        @"demMaxZoom":          @16,
        @"demEncoding":         @"terrarium",
        @"contourTilesUrl":     @"https://tiles.akylas.fr/data/contours/{z}/{x}/{y}.pbf",
        @"contourTilesMaxZoom": @14,
        @"rasterUrl":           @"https://tile.openstreetmap.org/{z}/{x}/{y}.png",
        @"userAgent":           @"CartoDemo-iOS/1.0",

        // --- camera ---
        @"lon":                 @5.718957,
        @"lat":                 @45.187362,
        @"zoom":                @16.22f,
        @"tilt":                @26.0f,
        @"rotation":            @(-15.12f),

        // --- 3D terrain ---
        @"terrain":             @YES,
        @"exaggeration":        @1.0f,
        @"meshResolution":      @64,
        @"clearance":           @60.0f,
        @"painterDepth":        @YES,
        @"drape":               @YES,
        @"drapeLines":          @NO,
        @"drapeResolution":     @0,
        @"stitch":              @YES,
        @"seamlessEdges":       @YES,
        @"prefetch":            @YES,
        @"occlusion":           @YES,
        @"occlusionTolerance":  @0.0f,
        @"maxTileZoomOffset":   @0,
        @"coarsening":          @8,
        @"backgroundBitmap":    @NO,
        @"tilePool":            @1,
        @"lodFactor":           @0.5f,
        @"labelMaxDistance":    @2000.0f,

        // --- fog / view distance / sky ---
        @"fog":                 @NO,
        @"fogStart":            @1500.0f,
        @"fogDistance":         @0.0f,
        @"fogBlend":            @12.0f,
        @"fogHorizon":          @(-1.0f),
        @"viewDistance":        @1.0f,
        @"viewDistanceMeters":  @170000.0f,
        @"sky":                 @YES,

        // --- lighting / shadows ---
        @"terrainLight":        @NO,
        @"sunHour":             @(-1.0f),
        @"sunAzimuth":          @355.0f,
        @"sunAltitude":         @9.0f,
        @"sunIntensity":        @1.0f,
        @"ambient":             @1.0f,
        @"shadow":              @0.3f,
        @"shadowSoftness":      @1.0f,
        @"shadowMapSize":       @1024,
        @"shadowCascades":      @3,
        @"shadowBias":          @1.0f,
        @"shadowDistance":      @0.0f,
        @"shadowMargin":        @3,
        @"daycycle":            @NO,
        @"dayCycleHour":        @12.0f,

        // --- hillshade ---
        @"hsMethod":            @"IGOR",
        @"hsContrast":          @0.5f,
        @"hsHeightScale":       @0.05f,
        @"hsExaggeration":      @1.0f,
        @"hsIllumination":      @180.0f,
        @"hsContours":          @NO,
        @"hsContourInterval":   @100.0f,
        @"slopes":              @NO,

        // --- on-the-fly contours ---
        @"contourInterval":     @10.0f,
        @"contourResolution":   @128,
        @"contourSimplify":     @1.5f,
        @"contourMinZoom":      @5,
        @"contourSeamless":     @YES,
        @"contourStubs":        @NO,
        @"contourStubInterval": @0.0f,
        @"stubsFromTerrain":    @YES,

        // --- inline style ---
        @"bg":                  @"#eef2f0",
        @"bld3d":               @NO,
        @"bldLight":            @1.0f,
        @"bldAmbient":          @0.35f,
        @"bldHeight":           @14.0f,
        @"styleLight":          @NO,
        @"labels":              @YES,
        @"minimal":             @NO,
        @"compOp":              @"",
        @"roadWidth":           @"linear([view::zoom], (12, 0.6), (18, 4.0))",
        @"motorwayWidth":       @"linear([view::zoom], (12, 1.5), (18, 9.0))",
        @"contourWidth":        @"linear([view::zoom], (12, 0.4), (18, 1))",
        @"landcoverOpacity":    @1.0f,
        @"satZoom":             @11,

        // --- POI labels ---
        @"poiAnchors":          @"right,left,top,bottom",
        @"poiTextOptional":     @YES,
        @"poiTextDx":           @2.0f,
        @"poiFontIcon":         @YES,
        @"poiBitmapIcon":       @NO,
        @"poiTextAlign":        @"auto",
        @"poiTextBg":           @NO,
        @"poiIconBg":           @NO,
        @"poiBgRadius":         @3.0f,
        @"poiBgPadding":        @3.0f,
        @"poiWrapWidth":        @90.0f,

        // --- route test / maneuvers ---
        @"routeTest":           @NO,
        @"routeWidth":          @10.0f,
        @"routeCaseWidth":      @16.0f,
        @"routeColor":          @"#4285F4",
        @"routeCaseColor":      @"#FFFFFF",
        @"routeJoin":           @"round",
        @"routeCap":            @"round",
        @"routeMiterLimit":     @4.0f,
        @"routeOpacity":        @1.0f,
        @"maneuvers":           @NO,
        @"maneuverBefore":      @30.0f,
        @"maneuverAfter":       @30.0f,
        @"maneuverWidth":       @8.0f,
        @"maneuverCaseWidth":   @13.0f,
        @"maneuverColor":       @"#FFFFFF",
        @"maneuverCaseColor":   @"#1A73E8",

        // --- app ---
        @"ui":                  @YES,
    } mutableCopy];
}

+ (id)valueFor:(NSString *)key {
    return sValues[key];
}

+ (BOOL)boolFor:(NSString *)key {
    id value = sValues[key];
    if ([value isKindOfClass:[NSString class]]) {
        return [value caseInsensitiveCompare:@"true"] == NSOrderedSame || [value intValue] != 0;
    }
    return [value boolValue];
}

+ (float)floatFor:(NSString *)key { return [sValues[key] floatValue]; }
+ (double)doubleFor:(NSString *)key { return [sValues[key] doubleValue]; }
+ (int)intFor:(NSString *)key { return [sValues[key] intValue]; }

+ (NSString *)stringFor:(NSString *)key {
    id value = sValues[key];
    return [value isKindOfClass:[NSString class]] ? value : [value stringValue];
}

+ (unsigned int)colorFor:(NSString *)key {
    NSString *text = [[self stringFor:key] stringByReplacingOccurrencesOfString:@"#" withString:@""];
    unsigned int value = 0;
    [[NSScanner scannerWithString:text] scanHexInt:&value];
    // "#rrggbb" carries no alpha; the SDK wants ARGB, so assume opaque.
    return text.length <= 6 ? (0xff000000 | value) : value;
}

+ (void)setValue:(id)value forKey:(NSString *)key {
    sValues[key] = value;
}

+ (NSArray<NSString *> *)allKeys {
    return [sValues.allKeys sortedArrayUsingSelector:@selector(compare:)];
}

+ (void)applyLaunchArgumentOverrides {
    // UIKit has already folded '-key value' launch arguments into NSUserDefaults; anything that
    // names a known key wins over the default. Values stay strings, which the accessors coerce.
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    for (NSString *key in [sValues.allKeys copy]) {
        NSString *override = [defaults stringForKey:key];
        if (override) {
            sValues[key] = override;
        }
    }
}

@end
