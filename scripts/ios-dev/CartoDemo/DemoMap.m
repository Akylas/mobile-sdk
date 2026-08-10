#import "DemoMap.h"
#import "DemoConfig.h"

@implementation DemoMap

+ (void)setupMapView:(NTMapView *)mapView {
    [self applyTerrainConfig:mapView];
    [self applyLayers:mapView];
    [self applyCameraConfig:mapView];
}

+ (NTTileDataSource *)httpSourceWithURL:(NSString *)url minZoom:(int)minZoom maxZoom:(int)maxZoom {
    NTHTTPTileDataSource *source = [[NTHTTPTileDataSource alloc] initWithMinZoom:minZoom
                                                                         maxZoom:maxZoom
                                                                         baseURL:url];
    // The tile servers used here ask for an identifiable agent, same as the Android demo.
    NTStringMap *headers = [[NTStringMap alloc] init];
    [headers set:@"User-Agent" x:[DemoConfig httpUserAgent]];
    [source setHTTPHeaders:headers];
    return source;
}

+ (void)applyLayers:(NTMapView *)mapView {
    NTLayers *layers = [mapView getLayers];
    [layers clear];

    if ([DemoConfig baseEnabled] || [DemoConfig satelliteEnabled]) {
        NTTileDataSource *source = [self httpSourceWithURL:[DemoConfig rasterUrl]
                                                   minZoom:[DemoConfig rasterMinZoom]
                                                   maxZoom:[DemoConfig rasterMaxZoom]];
        [layers add:[[NTRasterTileLayer alloc] initWithDataSource:source]];
    }

    if ([DemoConfig hillshadeEnabled]) {
        NTTileDataSource *demSource = [self httpSourceWithURL:[DemoConfig demUrl]
                                                      minZoom:[DemoConfig demMinZoom]
                                                      maxZoom:[DemoConfig demMaxZoom]];
        [layers add:[[NTHillshadeRasterTileLayer alloc] initWithDataSource:demSource
                                                         elevationDecoder:[self elevationDecoder]]];
    }
}

+ (NTElevationDecoder *)elevationDecoder {
    if ([[DemoConfig demEncoding] isEqualToString:@"mapbox"]) {
        return [[NTMapBoxElevationDataDecoder alloc] init];
    }
    return [[NTTerrariumElevationDataDecoder alloc] init];
}

+ (void)applyTerrainConfig:(NTMapView *)mapView {
    // TerrainOptions takes its elevation source at construction, so a change of source is a new
    // options object rather than a setter - hence build, configure, then assign.
    NTTileDataSource *demSource = [self httpSourceWithURL:[DemoConfig demUrl]
                                                  minZoom:[DemoConfig demMinZoom]
                                                  maxZoom:[DemoConfig demMaxZoom]];
    NTTerrainOptions *terrain = [[NTTerrainOptions alloc] initWithDataSource:demSource
                                                           elevationDecoder:[self elevationDecoder]];
    [terrain setEnabled:[DemoConfig terrainEnabled]];
    [terrain setExaggeration:[DemoConfig terrainExaggeration]];
    [terrain setMeshResolution:[DemoConfig terrainMeshResolution]];
    [[mapView getOptions] setTerrainOptions:terrain];
}

+ (void)applyCameraConfig:(NTMapView *)mapView {
    // Convert through the map's OWN projection (EPSG3857 by default). Going through NTEPSG4326
    // instead looks right and is not: its fromWgs84 is the identity, so lon/lat would be fed to
    // the map as metres and land the camera in the ocean off 0,0.
    NTProjection *projection = [[mapView getOptions] getBaseProjection];
    NTMapPos *focus = [projection fromWgs84:[[NTMapPos alloc] initWithX:[DemoConfig startLon]
                                                                     y:[DemoConfig startLat]]];
    [mapView setFocusPos:focus durationSeconds:0];
    [mapView setZoom:[DemoConfig startZoom] durationSeconds:0];
    [mapView setTilt:[DemoConfig startTilt] durationSeconds:0];
    [mapView setRotation:[DemoConfig startRotation] durationSeconds:0];
}

@end
