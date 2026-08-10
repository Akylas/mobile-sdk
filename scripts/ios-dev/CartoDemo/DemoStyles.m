#import "DemoStyles.h"
#import "DemoConfig.h"

@implementation DemoStyles

+ (NSString *)landcoverOpacity {
    float opacity = [DemoConfig floatFor:@"landcoverOpacity"];
    // Opaque by default; translucent when the hillshade and contours underneath have to read
    // through (tangram's 'translucent-polygons').
    return opacity >= 1.0f ? @"" : [NSString stringWithFormat:@" polygon-opacity: %g;", opacity];
}

+ (NSString *)compOp {
    NSString *op = [DemoConfig stringFor:@"compOp"];
    return op.length ? [NSString stringWithFormat:@" polygon-comp-op: %@;", op] : @"";
}

+ (NSString *)join:(NSArray<NSString *> *)lines {
    return [lines componentsJoinedByString:@"\n"];
}

+ (NSString *)inlineStyle {
    NSMutableString *map = [NSMutableString stringWithFormat:@"Map { background-color: %@;",
                            [DemoConfig stringFor:@"bg"]];
    if ([DemoConfig boolFor:@"styleLight"]) {
        // The same sun/shadow/fog values the code sets on LightOptions/TerrainOptions, but
        // expressed IN the style - and zoom-dependent, which only the style can do.
        [map appendString:@" terrain-lighting: 1;"];
        [map appendString:@" sun-azimuth: 250;"];
        [map appendString:@" sun-altitude: linear([view::zoom], (11, 55), (15, 12));"];
        [map appendString:@" sun-intensity: 1;"];
        [map appendString:@" ambient-intensity: 0.4;"];
        [map appendFormat:@" building-light-intensity: %g;", [DemoConfig floatFor:@"bldLight"]];
        [map appendFormat:@" building-ambient: %g;", [DemoConfig floatFor:@"bldAmbient"]];
        [map appendString:@" shadow-strength: 0.8;"];
        [map appendString:@" shadow-softness: 1;"];
        [map appendString:@" fog-color: #b8c6d8;"];
        [map appendString:@" fog-start-distance: 1500;"];
        [map appendString:@" fog-distance: linear([view::zoom], (11, 60000), (15, 12000));"];
        [map appendString:@" terrain-max-visible-distance: 40000;"];
    }
    [map appendString:@" }"];

    NSString *satelliteSlot = [NSString stringWithFormat:
        @"#satellite[zoom>=%d] { raster-opacity: 1; raster-comp-op: src-over; }",
        [DemoConfig intFor:@"satZoom"]];
    NSString *hillshadeSlot = [self join:@[
        @"#hillshade[zoom>=4][zoom<=19] {",
        [NSString stringWithFormat:@"  hillshade-illumination-direction: %d;",
            (int)[DemoConfig floatFor:@"hsIllumination"]],
        @"  hillshade-shadow-color: #473B24;",
        [DemoConfig boolFor:@"hsContours"] ? [self join:@[
            [NSString stringWithFormat:@"  hillshade-contour-interval: %d;",
                (int)[DemoConfig floatFor:@"hsContourInterval"]],
            @"  hillshade-contour-width: 0.8;",
            @"  hillshade-contour-color: #FFC56008;"]] : @"",
        @"}"]];

    if ([DemoConfig boolFor:@"minimal"]) {
        // Background plus the composite slots only: no vector geometry, so a frame costs the
        // terrain and the slots and nothing else. The slot blocks have to stay - a source's
        // position in the draw order IS the position of the first rule naming it.
        return [self join:@[map, hillshadeSlot, satelliteSlot]];
    }

    BOOL labels = [DemoConfig boolFor:@"labels"];
    float labelMaxDistance = [DemoConfig floatFor:@"labelMaxDistance"];

    return [self join:@[
        map,
        @"#water { polygon-fill: #9cc3e0; }",
        [NSString stringWithFormat:@"#landuse { polygon-fill: #dddddd;%@ }", [self landcoverOpacity]],
        [NSString stringWithFormat:@"#landcover { polygon-fill: #dbe8cc;%@%@ }",
            [self landcoverOpacity], [self compOp]],
        // --- composite slots, in draw order ---
        satelliteSlot,
        hillshadeSlot,
        [self join:@[
            [NSString stringWithFormat:@"#contour[zoom>=%d] {", [DemoConfig intFor:@"contourMinZoom"]],
            // Lines only for the traced geometry: a label stub is a short fragment of a contour,
            // long enough to lay text along and nothing more, so drawing it as a line paints
            // dashes over the map.
            @"  [stub=0] {",
            @"    line-color: #C56008;",
            [NSString stringWithFormat:@"    line-width: %@;", [DemoConfig stringFor:@"contourWidth"]],
            @"  }",
            [NSString stringWithFormat:@"  contour-base-interval: %d;",
                (int)[DemoConfig floatFor:@"contourInterval"]],
            [DemoConfig boolFor:@"contourStubs"] ? [self join:@[
                @"  contour-label-stubs: 1;",
                [NSString stringWithFormat:@"  contour-label-interval: %d;",
                    (int)[DemoConfig floatFor:@"contourStubInterval"]]]] : @"",
            @"}"]],
        [NSString stringWithFormat:@"#transportation { line-color: #ffffff; line-width: %@; }",
            [DemoConfig stringFor:@"roadWidth"]],
        labels ? [self join:@[
            @"#transportation_name {",
            @"  text-name: [name];",
            @"  text-fill: #000000;",
            @"  text-spacing: 10;",
            @"  text-placement: line;",
            @"  text-size: 10;",
            labelMaxDistance > 0
                ? [NSString stringWithFormat:@"  text-max-distance: %g;", labelMaxDistance] : @"",
            @"}"]] : @"",
        [NSString stringWithFormat:
            @"#transportation['class'='motorway'] { line-color: #e27d60; line-width: %@; }",
            [DemoConfig stringFor:@"motorwayWidth"]],
        [DemoConfig boolFor:@"bld3d"]
            ? [NSString stringWithFormat:
                @"#building[zoom>=14] { building-fill: #d9cfc4; building-height: %g; }",
                [DemoConfig floatFor:@"bldHeight"]]
            : @"#building[zoom>=14] { polygon-fill: #d9cfc4; }",
    ]];
}

+ (NSString *)contourStyle {
    return [self join:@[
        @"Map { }",
        [NSString stringWithFormat:@"#contour[zoom>=%d] {", [DemoConfig intFor:@"contourMinZoom"]],
        @"  [stub=0] { line-color: #C56008; line-width: 0.8; }",
        [NSString stringWithFormat:@"  contour-base-interval: %d;",
            (int)[DemoConfig floatFor:@"contourInterval"]],
        @"}"]];
}

+ (NSString *)contourTilesStyle {
    return [self join:@[
        @"Map { }",
        @"#contour {",
        [NSString stringWithFormat:@"  line-color: #C56008; line-width: %@;",
            [DemoConfig stringFor:@"contourWidth"]],
        @"}"]];
}

+ (NSString *)routeTestStyle {
    // Casing under the line, both from the same source, which is what makes the join and cap
    // knobs visible.
    return [self join:@[
        @"Map { }",
        @"#route::case {",
        [NSString stringWithFormat:@"  line-color: %@;", [DemoConfig stringFor:@"routeCaseColor"]],
        [NSString stringWithFormat:@"  line-width: %g;", [DemoConfig floatFor:@"routeCaseWidth"]],
        [NSString stringWithFormat:@"  line-join: %@;", [DemoConfig stringFor:@"routeJoin"]],
        [NSString stringWithFormat:@"  line-cap: %@;", [DemoConfig stringFor:@"routeCap"]],
        @"}",
        @"#route::line {",
        [NSString stringWithFormat:@"  line-color: %@;", [DemoConfig stringFor:@"routeColor"]],
        [NSString stringWithFormat:@"  line-width: %g;", [DemoConfig floatFor:@"routeWidth"]],
        [NSString stringWithFormat:@"  line-join: %@;", [DemoConfig stringFor:@"routeJoin"]],
        [NSString stringWithFormat:@"  line-cap: %@;", [DemoConfig stringFor:@"routeCap"]],
        [NSString stringWithFormat:@"  line-opacity: %g;", [DemoConfig floatFor:@"routeOpacity"]],
        @"}"]];
}

+ (NTMBVectorTileDecoder *)createDecoder {
    NSString *source = [DemoConfig stringFor:@"style"];

    if ([source isEqualToString:@"zip"] || [source isEqualToString:@"assets"]) {
        // A style zip bundled with the app, mirroring the Android demo's osm.zip path.
        NSString *path = [[NSBundle mainBundle] pathForResource:@"osm" ofType:@"zip"];
        if (path) {
            NSData *bytes = [NSData dataWithContentsOfFile:path];
            NTBinaryData *data = [[NTBinaryData alloc] initWithDataPtr:(unsigned char *)bytes.bytes
                                                                  size:(unsigned int)bytes.length];
            NTZippedAssetPackage *package = [[NTZippedAssetPackage alloc] initWithZipData:data];
            return [[NTMBVectorTileDecoder alloc] initWithCompiledStyleSet:
                    [[NTCompiledStyleSet alloc] initWithAssetPackage:package]];
        }
        NSLog(@"CartoDemo: no osm.zip in the bundle, falling back to the inline style");
    }

    NTCartoCSSStyleSet *styleSet = [[NTCartoCSSStyleSet alloc] initWithCartoCSS:[self inlineStyle]];
    return [[NTMBVectorTileDecoder alloc] initWithCartoCSSStyleSet:styleSet];
}

@end
