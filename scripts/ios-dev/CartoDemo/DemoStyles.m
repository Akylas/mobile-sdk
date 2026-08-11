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

+ (NSString *)slopesShader {
    // Bands rather than a gradient: the point is reading "is this skiable", not a smooth ramp.
    return [self join:@[
        @"uniform vec4 u_shadowColor;",
        @"uniform vec4 u_highlightColor;",
        @"uniform vec4 u_accentColor;",
        @"uniform vec3 u_lightDir;",
        @"vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {",
        @"    mediump float lighting = max(0.0, dot(normal, u_lightDir));",
        @"    mediump float slope = acos(dot(normal, surfaceNormal)) * 180.0 / 3.14159 * 1.2;",
        @"    if (slope >= 45.0) { return vec4(0.378, 0.272, 0.358, 0.5); }",
        @"    if (slope >= 40.0) { return vec4(0.5, 0.0, 0.0, 0.5); }",
        @"    if (slope >= 35.0) { return vec4(0.455, 0.231, 0.111, 0.5); }",
        @"    if (slope >= 30.0) { return vec4(0.470, 0.451, 0.153, 0.5); }",
        @"    return vec4(0.0, 0.0, 0.0, 0.0);",
        @"}"]];
}

+ (NSString *)hypsometricShader {
    // Reads the RAW DEM texel rather than the shaded colour, which is what shows that the
    // custom-raster base class can run any filter over any raster source.
    return [self join:@[
        @"vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {",
        @"  vec4 c = getRawColor();",
        @"  float h = (c.r * 255.0 * 256.0 + c.g * 255.0 + c.b * 255.0 / 256.0) - 32768.0;",
        @"  float t = clamp(h / 3000.0, 0.0, 1.0);",
        @"  vec3 col = mix(vec3(0.2, 0.4, 0.8), vec3(0.9, 0.9, 0.4), t);",
        @"  col = mix(col, vec3(0.5, 0.3, 0.1), clamp((h - 1500.0) / 1500.0, 0.0, 1.0));",
        @"  return vec4(col, 1.0);",
        @"}"]];
}

+ (NSString *)reliefSurfaceShader {
    return [self join:@[
        @"uniform vec4 uPaperColor;",
        @"uniform vec4 uShadeColor;",
        @"uniform float uShadeStrength;",
        @"uniform float uAmbient;",
        @"uniform float uHaze;",
        @"uniform float uHazeDistance;",
        @"vec4 surfaceColor() {",
        @"    vec3 n = normalize(v_normal);",
        @"    float lambert = max(dot(n, normalize(u_sunDir)), 0.0);",
        @"    float light = mix(uAmbient, 1.0, lambert);",
        @"    vec3 color = mix(uShadeColor.rgb, uPaperColor.rgb, clamp(1.0 - uShadeStrength * (1.0 - light), 0.0, 1.0));",
        @"    color = mix(color, uPaperColor.rgb, clamp(v_dist / max(uHazeDistance, 1.0), 0.0, 1.0) * uHaze);",
        @"    color = mix(color, u_fogColor.rgb, fogAmount(v_dist));",
        @"    return vec4(color, 1.0);",
        @"}"]];
}

+ (NSString *)peaksStyle {
    // 'nuticallout' lifts the label to a band near the top of the screen and joins it back to the
    // summit with a leader line; a label that would collide moves one row up instead of dropping.
    BOOL pinTop = [DemoConfig boolFor:@"peaksPinTop"];
    return [self join:@[
        @"Map { }",
        [NSString stringWithFormat:@"#mountain_peak['class'='peak'][zoom>=%d] {",
            [DemoConfig intFor:@"peaksMinZoom"]],
        @"  text-name: [name];",
        // The elevation as a second run of text: same label, same plate, smaller font.
        @"  text-secondary-name: [ele]+'m';",
        @"  text-secondary-scale: 0.8;",
        @"  text-secondary-dx: 4;",
        [NSString stringWithFormat:@"  text-size: %g;", [DemoConfig floatFor:@"peaksTextSize"]],
        @"  text-fill: #202020;",
        @"  text-halo-fill: #ffffff;",
        @"  text-halo-radius: 1.5;",
        @"  text-background-fill: #ffffff;",
        [NSString stringWithFormat:@"  text-background-opacity: %g;", [DemoConfig floatFor:@"peaksBgOpacity"]],
        @"  text-background-radius: 3;",
        @"  text-placement: nuticallout;",
        // The higher summit claims the row: otherwise the winner is whichever label the tile order
        // happened to offer first, and a 700 m hill hides a 2000 m one.
        @"  text-placement-priority: [ele];",
        [NSString stringWithFormat:@"  text-callout-align: %@;", pinTop ? @"top-right" : @"bottom-left"],
        @"}"]];
}

+ (NSString *)poiTestStyle {
    // A shield per label: the ICON stays on the feature and the NAME goes on whichever side the
    // culler finds free, falling back to the icon alone when none is.
    return [self join:@[
        @"Map { }",
        @"#poi {",
        @"  shield-name: [name];",
        @"  shield-size: 11;",
        @"  shield-fill: #333333;",
        @"  shield-halo-fill: #ffffff;",
        @"  shield-halo-radius: 1.5;",
        [NSString stringWithFormat:@"  shield-anchors: '%@';", [DemoConfig stringFor:@"poiAnchors"]],
        [NSString stringWithFormat:@"  shield-text-optional: %d;", [DemoConfig boolFor:@"poiTextOptional"] ? 1 : 0],
        [NSString stringWithFormat:@"  shield-dx: %g;", [DemoConfig floatFor:@"poiTextDx"]],
        [NSString stringWithFormat:@"  shield-wrap-width: %g;", [DemoConfig floatFor:@"poiWrapWidth"]],
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
