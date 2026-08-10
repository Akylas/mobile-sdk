#import "DemoTests.h"
#import "DemoConfig.h"
#import "DemoStyles.h"
#import "NTGeoJSONVectorTileDataSource.h"

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

/**
 * A GeoJSON line through the current view, tiled by the SDK's own geojson-vt pyramid and drawn
 * with the route style. This is the path the line-join and casing work is judged on.
 */
+ (void)addRouteTest:(NTMapView *)mapView {
    NTProjection *projection = [[mapView getOptions] getBaseProjection];
    NTMapPos *centre = [projection toWgs84:[mapView getFocusPos]];

    double lon = [centre getX], lat = [centre getY];
    NSMutableArray<NSString *> *points = [NSMutableArray array];
    // A zig-zag rather than a straight line: sharp corners are what exercise the joins.
    for (int i = 0; i < 12; i++) {
        double dx = (i - 6) * 0.004;
        double dy = ((i % 2) ? 0.0025 : -0.0025) + (i - 6) * 0.0012;
        [points addObject:[NSString stringWithFormat:@"[%f,%f]", lon + dx, lat + dy]];
    }
    NSString *geoJSON = [NSString stringWithFormat:
        @"{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\",\"properties\":{},"
        @"\"geometry\":{\"type\":\"LineString\",\"coordinates\":[%@]}}]}",
        [points componentsJoinedByString:@","]];

    @try {
        NTGeoJSONVectorTileDataSource *source =
            [[NTGeoJSONVectorTileDataSource alloc] initWithMinZoom:0 maxZoom:14];
        [source createLayer:@"route"];
        [source setLayerGeoJSONString:0 geoJSON:geoJSON];

        NTCartoCSSStyleSet *styleSet =
            [[NTCartoCSSStyleSet alloc] initWithCartoCSS:[DemoStyles routeTestStyle]];
        NTVectorTileLayer *layer =
            [[NTVectorTileLayer alloc] initWithDataSource:source
                                                  decoder:[[NTMBVectorTileDecoder alloc]
                                                           initWithCartoCSSStyleSet:styleSet]];
        [[mapView getLayers] add:layer];
        [sTestLayers addObject:layer];
    } @catch (NSException *exception) {
        NSLog(@"CartoDemo: route test failed: %@", exception.reason);
    }
}

/**
 * Reverse geocode of the current camera position against the vector tiles on screen, which is the
 * cheapest way to prove the search stack is wired at all.
 */
+ (void)runSearchTest:(NTMapView *)mapView {
    @try {
        NTProjection *projection = [[mapView getOptions] getBaseProjection];
        NTMapPos *centre = [projection toWgs84:[mapView getFocusPos]];
        NSLog(@"CartoDemo: search test at %f, %f", [centre getX], [centre getY]);
        // The Android demo runs VectorTileSearchService over the base layer here; the iOS port of
        // that is still to do, so this only reports the query point rather than pretending.
    } @catch (NSException *exception) {
        NSLog(@"CartoDemo: search test failed: %@", exception.reason);
    }
}

@end
