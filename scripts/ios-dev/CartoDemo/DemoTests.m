#import "DemoTests.h"
#import "DemoConfig.h"
#import "DemoStyles.h"
#import "DemoMap.h"
#import "NTGeoJSONVectorTileDataSource.h"
#import "NTManeuverArrowBuilder.h"
#import "NTVectorTileSearchService.h"

@implementation DemoTests

/** Layers these actions add, so 'clear' can drop them without touching the base map. */
static NSMutableArray<NTLayer *> *sTestLayers = nil;

+ (void)run:(NSString *)action mapView:(NTMapView *)mapView {
    if (!mapView) {
        return;
    }
    if (!sTestLayers) {
        sTestLayers = [NSMutableArray array];
    }
    if ([action isEqualToString:@"route"]) {
        [self addRouteTest:mapView];
    } else if ([action isEqualToString:@"maneuvers"]) {
        [self addManeuverArrows:mapView];
    } else if ([action isEqualToString:@"geojsonBench"]) {
        [self runGeoJSONBench:mapView];
    } else if ([action isEqualToString:@"search"]) {
        [self runSearchTest:mapView];
    } else if ([action isEqualToString:@"clear"]) {
        [self clear:mapView];
    }
}

+ (void)clear:(NTMapView *)mapView {
    NTLayers *layers = [mapView getLayers];
    for (NTLayer *layer in sTestLayers) {
        [layers remove:layer];
    }
    [sTestLayers removeAllObjects];
}

+ (NTMapPos *)centreOf:(NTMapView *)mapView {
    NTProjection *projection = [[mapView getOptions] getBaseProjection];
    return [projection toWgs84:[mapView getFocusPos]];
}

/** A zig-zag through the current view: sharp corners are what exercise the joins and caps. */
+ (NSArray<NSString *> *)zigZagAround:(NTMapPos *)centre {
    double lon = [centre getX], lat = [centre getY];
    NSMutableArray<NSString *> *points = [NSMutableArray array];
    for (int i = 0; i < 12; i++) {
        double dx = (i - 6) * 0.004;
        double dy = ((i % 2) ? 0.0025 : -0.0025) + (i - 6) * 0.0012;
        [points addObject:[NSString stringWithFormat:@"[%f,%f]", lon + dx, lat + dy]];
    }
    return points;
}

+ (NTVectorTileLayer *)layerForGeoJSON:(NSString *)geoJSON style:(NSString *)style maxZoom:(int)maxZoom {
    NTGeoJSONVectorTileDataSource *source =
        [[NTGeoJSONVectorTileDataSource alloc] initWithMinZoom:0 maxZoom:maxZoom];
    [source createLayer:@"route"];
    [source setLayerGeoJSONString:0 geoJSON:geoJSON];

    NTCartoCSSStyleSet *styleSet = [[NTCartoCSSStyleSet alloc] initWithCartoCSS:style];
    return [[NTVectorTileLayer alloc] initWithDataSource:source
                                                 decoder:[[NTMBVectorTileDecoder alloc]
                                                          initWithCartoCSSStyleSet:styleSet]];
}

/**
 * A GeoJSON line through the current view, tiled by the SDK's own geojson-vt pyramid and drawn
 * with the route style. This is the path the line-join and casing work is judged on.
 */
+ (void)addRouteTest:(NTMapView *)mapView {
    @try {
        NSString *geoJSON = [NSString stringWithFormat:
            @"{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\",\"properties\":{},"
            @"\"geometry\":{\"type\":\"LineString\",\"coordinates\":[%@]}}]}",
            [[self zigZagAround:[self centreOf:mapView]] componentsJoinedByString:@","]];

        NTVectorTileLayer *layer = [self layerForGeoJSON:geoJSON
                                                   style:[DemoStyles routeTestStyle]
                                                 maxZoom:14];
        [[mapView getLayers] add:layer];
        [sTestLayers addObject:layer];
    } @catch (NSException *exception) {
        NSLog(@"CartoDemo: route test failed: %@", exception.reason);
    }
}

/**
 * Maneuver arrows cut out of a route by ManeuverArrowBuilder: the same geometry the navigation
 * code produces, drawn as its own layer so the head and the casing can be looked at closely.
 */
+ (void)addManeuverArrows:(NTMapView *)mapView {
    @try {
        NTProjection *projection = [[mapView getOptions] getBaseProjection];
        NTMapPos *centre = [self centreOf:mapView];

        // The route the arrows are cut from, in map coordinates.
        NTMapPosVector *points = [[NTMapPosVector alloc] init];
        double lon = [centre getX], lat = [centre getY];
        for (int i = 0; i < 12; i++) {
            double dx = (i - 6) * 0.004;
            double dy = ((i % 2) ? 0.0025 : -0.0025) + (i - 6) * 0.0012;
            [points add:[projection fromWgs84:[[NTMapPos alloc] initWithX:lon + dx y:lat + dy]]];
        }

        NTManeuverArrowBuilder *builder = [[NTManeuverArrowBuilder alloc] init];
        [builder setLengthBefore:[DemoConfig floatFor:@"maneuverBefore"]];
        [builder setLengthAfter:[DemoConfig floatFor:@"maneuverAfter"]];

        NTLocalVectorDataSource *source = [[NTLocalVectorDataSource alloc] initWithProjection:projection];
        NTLineStyleBuilder *lineStyle = [[NTLineStyleBuilder alloc] init];
        [lineStyle setWidth:[DemoConfig floatFor:@"maneuverWidth"]];
        [lineStyle setColor:[[NTColor alloc] initWithColor:(int)[DemoConfig colorFor:@"maneuverCaseColor"]]];

        // One arrow per interior vertex - every turn of the zig-zag is a maneuver.
        for (int i = 1; i + 1 < (int)[points size]; i++) {
            NTFeatureCollection *arrow = [builder buildArrowAtIndex:projection
                                                             points:points
                                                      maneuverIndex:i];
            for (int f = 0; f < [arrow getFeatureCount]; f++) {
                NTGeometry *geometry = [[arrow getFeature:f] getGeometry];
                if ([geometry isKindOfClass:[NTLineGeometry class]]) {
                    [source add:[[NTLine alloc] initWithGeometry:(NTLineGeometry *)geometry
                                                           style:[lineStyle buildStyle]]];
                }
            }
        }

        NTVectorLayer *layer = [[NTVectorLayer alloc] initWithDataSource:source];
        [[mapView getLayers] add:layer];
        [sTestLayers addObject:layer];
    } @catch (NSException *exception) {
        NSLog(@"CartoDemo: maneuver arrows failed: %@", exception.reason);
    }
}

/**
 * Many long lines through one GeoJSON source, timed. The geojson-vt pyramid work was measured on
 * exactly this shape, so it is worth having on both benches.
 */
+ (void)runGeoJSONBench:(NTMapView *)mapView {
    @try {
        NTMapPos *centre = [self centreOf:mapView];
        double lon = [centre getX], lat = [centre getY];

        NSMutableArray<NSString *> *features = [NSMutableArray array];
        for (int line = 0; line < 200; line++) {
            NSMutableArray<NSString *> *points = [NSMutableArray array];
            for (int i = 0; i < 60; i++) {
                double dx = (i - 30) * 0.002 + line * 0.0004;
                double dy = sin(i * 0.4 + line) * 0.002 + (line - 100) * 0.0004;
                [points addObject:[NSString stringWithFormat:@"[%f,%f]", lon + dx, lat + dy]];
            }
            [features addObject:[NSString stringWithFormat:
                @"{\"type\":\"Feature\",\"properties\":{},\"geometry\":"
                @"{\"type\":\"LineString\",\"coordinates\":[%@]}}",
                [points componentsJoinedByString:@","]]];
        }
        NSString *geoJSON = [NSString stringWithFormat:
            @"{\"type\":\"FeatureCollection\",\"features\":[%@]}",
            [features componentsJoinedByString:@","]];

        NSDate *start = [NSDate date];
        NTVectorTileLayer *layer = [self layerForGeoJSON:geoJSON
                                                   style:[DemoStyles routeTestStyle]
                                                 maxZoom:14];
        NSTimeInterval buildMs = -[start timeIntervalSinceNow] * 1000.0;

        [[mapView getLayers] add:layer];
        [sTestLayers addObject:layer];
        NSLog(@"CartoDemo: GeoJSON bench - 200 lines x 60 points, source built in %.1f ms", buildMs);
    } @catch (NSException *exception) {
        NSLog(@"CartoDemo: GeoJSON bench failed: %@", exception.reason);
    }
}

/**
 * VectorTileSearchService over the base map's own source: what is actually under the camera,
 * rather than a canned query.
 */
+ (void)runSearchTest:(NTMapView *)mapView {
    @try {
        NTLayers *layers = [mapView getLayers];
        NTVectorTileLayer *baseLayer = nil;
        for (int i = 0; i < [layers count]; i++) {
            NTLayer *layer = [layers get:i];
            if ([layer isKindOfClass:[NTVectorTileLayer class]]) {
                baseLayer = (NTVectorTileLayer *)layer;
                break;
            }
        }
        if (!baseLayer) {
            NSLog(@"CartoDemo: search test needs a vector tile layer - turn the base map on");
            return;
        }

        NTProjection *projection = [[mapView getOptions] getBaseProjection];
        NTVectorTileSearchService *service =
            [[NTVectorTileSearchService alloc] initWithDataSource:[baseLayer getDataSource]
                                                     tileDecoder:[baseLayer getTileDecoder]];
        [service setMaxResults:20];
        [service setMinZoom:14];
        [service setMaxZoom:14];

        NTSearchRequest *request = [[NTSearchRequest alloc] init];
        [request setProjection:projection];
        [request setGeometry:[[NTPointGeometry alloc] initWithPos:[mapView getFocusPos]]];
        [request setSearchRadius:250];

        NSDate *start = [NSDate date];
        NTVectorTileFeatureCollection *results = [service findFeatures:request];
        NSTimeInterval ms = -[start timeIntervalSinceNow] * 1000.0;

        int count = results ? [results getFeatureCount] : 0;
        NSLog(@"CartoDemo: search found %d features within 250 m in %.1f ms", count, ms);
        for (int i = 0; i < MIN(count, 5); i++) {
            NTVectorTileFeature *feature = [results getFeature:i];
            NSLog(@"CartoDemo:   layer '%@' id %lld", [feature getLayerName], [feature getId]);
        }
    } @catch (NSException *exception) {
        NSLog(@"CartoDemo: search test failed: %@", exception.reason);
    }
}

@end
