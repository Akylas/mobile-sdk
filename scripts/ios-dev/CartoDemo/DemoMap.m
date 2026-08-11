#import "DemoMap.h"
#import "DemoConfig.h"
#import "DemoStyles.h"
#import "DemoSky.h"
// Not in the umbrella header (fork additions), so imported directly.
#import "NTCompositeVectorTileLayer.h"
#import "NTContourTileDataSource.h"

@implementation DemoMap

+ (void)setupMapView:(NTMapView *)mapView {
    [self applyOptions:mapView];
    [self applyTerrainConfig:mapView];
    [self applySkyAndLightConfig:mapView];
    [self applyLayers:mapView];
    [self applyCameraConfig:mapView];
}

// =================================================================================================
// SHARED SOURCES
// The DEM is shared by 3D terrain, the hillshade, the contours and the hypsometric tint, so it is
// built once - the Android demo does the same, and a second instance would double the downloads.
// =================================================================================================

+ (NSString *)cachePathFor:(NSString *)name {
    NSString *dir = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES).firstObject;
    return [dir stringByAppendingPathComponent:name];
}

+ (NTTileDataSource *)httpSourceWithURL:(NSString *)url
                                minZoom:(int)minZoom
                                maxZoom:(int)maxZoom
                                  cache:(NSString *)cacheName {
    NTHTTPTileDataSource *source = [[NTHTTPTileDataSource alloc] initWithMinZoom:minZoom
                                                                         maxZoom:maxZoom
                                                                         baseURL:url];
    NTStringMap *headers = [[NTStringMap alloc] init];
    [headers set:@"User-Agent" x:[DemoConfig stringFor:@"userAgent"]];
    [source setHTTPHeaders:headers];
    if (cacheName) {
        return [[NTPersistentCacheTileDataSource alloc] initWithDataSource:source
                                                             databasePath:[self cachePathFor:cacheName]];
    }
    return source;
}

+ (NTElevationDecoder *)elevationDecoder {
    if ([[DemoConfig stringFor:@"demEncoding"] isEqualToString:@"mapbox"]) {
        return [[NTMapBoxElevationDataDecoder alloc] init];
    }
    return [[NTTerrariumElevationDataDecoder alloc] init];
}

+ (NTTileDataSource *)demSource {
    static NTTileDataSource *shared = nil;
    static NSString *builtFor = nil;
    NSString *key = [NSString stringWithFormat:@"%@|%d", [DemoConfig stringFor:@"demUrl"],
                     [DemoConfig intFor:@"demMaxZoom"]];
    if (!shared || ![builtFor isEqualToString:key]) {
        shared = [self httpSourceWithURL:[DemoConfig stringFor:@"demUrl"]
                                 minZoom:1
                                 maxZoom:[DemoConfig intFor:@"demMaxZoom"]
                                   cache:@"mapterhorn.db"];
        builtFor = key;
    }
    return shared;
}

+ (NTTileDataSource *)contourSource {
    return [[NTContourTileDataSource alloc] initWithDataSource:[self demSource]
                                             elevationDecoder:[self elevationDecoder]];
}

// =================================================================================================
// LAYERS
// =================================================================================================

+ (void)applyLayers:(NTMapView *)mapView {
    NTLayers *layers = [mapView getLayers];
    [layers clear];

    if ([DemoConfig boolFor:@"map"]) {
        [layers add:[self buildBaseLayer]];
    }
    if ([DemoConfig boolFor:@"satellite"]) {
        [layers add:[[NTRasterTileLayer alloc] initWithDataSource:
                     [self httpSourceWithURL:[DemoConfig stringFor:@"rasterUrl"]
                                     minZoom:0 maxZoom:19 cache:@"openstreetmap.db"]]];
    }
    if ([DemoConfig boolFor:@"hillshade"]) {
        [layers add:[self buildHillshadeLayer]];
    }
    if ([DemoConfig boolFor:@"contourLayer"]) {
        // Contours traced from the DEM on the fly, as their OWN layer. The composite slot below
        // is the other way to get them, and the usual one.
        NTContourTileDataSource *source =
            [[NTContourTileDataSource alloc] initWithDataSource:[self demSource]
                                              elevationDecoder:[self elevationDecoder]];
        NTCartoCSSStyleSet *styleSet =
            [[NTCartoCSSStyleSet alloc] initWithCartoCSS:[DemoStyles contourStyle]];
        [layers add:[[NTVectorTileLayer alloc] initWithDataSource:source
                                                          decoder:[[NTMBVectorTileDecoder alloc]
                                                                   initWithCartoCSSStyleSet:styleSet]]];
    }
    if ([DemoConfig boolFor:@"contourTiles"]) {
        NTCartoCSSStyleSet *styleSet =
            [[NTCartoCSSStyleSet alloc] initWithCartoCSS:[DemoStyles contourTilesStyle]];
        [layers add:[[NTVectorTileLayer alloc]
                     initWithDataSource:[self httpSourceWithURL:[DemoConfig stringFor:@"contourTilesUrl"]
                                                        minZoom:11
                                                        maxZoom:[DemoConfig intFor:@"contourTilesMaxZoom"]
                                                          cache:@"akylas_contours.db"]
                                decoder:[[NTMBVectorTileDecoder alloc]
                                         initWithCartoCSSStyleSet:styleSet]]];
    }
}

+ (NTTileLayer *)buildBaseLayer {
    NTTileDataSource *vector = [self httpSourceWithURL:[DemoConfig stringFor:@"vectorUrl"]
                                               minZoom:0
                                               maxZoom:[DemoConfig intFor:@"vectorMaxZoom"]
                                                 cache:@"akylas_vect.db"];
    NTMBVectorTileDecoder *decoder = [DemoStyles createDecoder];

    if (![[DemoConfig stringFor:@"base"] isEqualToString:@"composite"]) {
        return [[NTVectorTileLayer alloc] initWithDataSource:vector decoder:decoder];
    }

    // COMPOSITE: extra sources are woven into the master style at the position of their '#name'
    // rule, so their draw order comes from the style rather than from the layer list.
    NTCompositeVectorTileLayer *layer =
        [[NTCompositeVectorTileLayer alloc] initWithDataSource:vector decoder:decoder];
    [layer setSinglePassRenderingEnabled:[DemoConfig boolFor:@"singlePass"]];
    if ([DemoConfig boolFor:@"hs"]) {
        [layer addExternalDataSource:@"hillshade"
                          dataSource:[self demSource]
                                type:NT_COMPOSITE_SOURCE_TYPE_HILLSHADE
                    elevationDecoder:[self elevationDecoder]];
        [layer setExternalDataSourceZoomLevelBias:@"hillshade" bias:[DemoConfig floatFor:@"hsBias"]];
    }
    if ([DemoConfig boolFor:@"contour"]) {
        // Merged into the master source rather than added beside it, so the '#contour' rules of
        // the base style draw it - addExternalDataSource would put it in its own pass and the
        // style's contour block would still get no data.
        [layer addVectorDataSource:@"contour" dataSource:[self contourSource]];
    }
    if ([DemoConfig boolFor:@"sat"]) {
        [layer addExternalDataSource:@"satellite"
                          dataSource:[self httpSourceWithURL:[DemoConfig stringFor:@"rasterUrl"]
                                                     minZoom:0 maxZoom:19 cache:@"openstreetmap.db"]
                                type:NT_COMPOSITE_SOURCE_TYPE_RASTER];
    }
    return layer;
}

+ (NTHillshadeRasterTileLayer *)buildHillshadeLayer {
    NTHillshadeRasterTileLayer *layer =
        [[NTHillshadeRasterTileLayer alloc] initWithDataSource:[self demSource]
                                              elevationDecoder:[self elevationDecoder]];
    [layer setContrast:[DemoConfig floatFor:@"hsContrast"]];
    [layer setHeightScale:[DemoConfig floatFor:@"hsHeightScale"]];
    float degrees = [DemoConfig floatFor:@"hsIllumination"];
    double radians = degrees * M_PI / 180.0;
    [layer setIlluminationDirection:[[NTMapVec alloc] initWithX:sin(radians) y:cos(radians) z:0]];
    [layer setIlluminationMapRotationEnabled:NO];
    return layer;
}

// =================================================================================================
// OPTIONS / TERRAIN / SKY + LIGHT
// =================================================================================================

+ (void)applyOptions:(NTMapView *)mapView {
    NTOptions *options = [mapView getOptions];
    [options setTileThreadPoolSize:[DemoConfig intFor:@"tilePool"]];
    NTSkyOptions *sky = [options getSkyOptions];
    [sky setEnabled:[DemoConfig boolFor:@"sky"]];
    [options setSkyOptions:sky];
}

+ (void)applyTerrainConfig:(NTMapView *)mapView {
    // TerrainOptions takes its elevation source at construction, so a change of source is a new
    // options object rather than a setter - hence build, configure, then assign.
    NTTerrainOptions *terrain =
        [[NTTerrainOptions alloc] initWithDataSource:[self demSource]
                                    elevationDecoder:[self elevationDecoder]];
    [terrain setEnabled:[DemoConfig boolFor:@"terrain"]];
    [terrain setExaggeration:[DemoConfig floatFor:@"exaggeration"]];
    [terrain setMeshResolution:[DemoConfig intFor:@"meshResolution"]];
    [terrain setTileEdgeStitchingEnabled:[DemoConfig boolFor:@"stitch"]];
    [terrain setSeamlessTileEdgesEnabled:[DemoConfig boolFor:@"seamlessEdges"]];
    [terrain setElevationPrefetchEnabled:[DemoConfig boolFor:@"prefetch"]];
    [terrain setPainterOrderDepthEnabled:[DemoConfig boolFor:@"painterDepth"]];
    [terrain setViewDistanceFactor:[DemoConfig floatFor:@"viewDistance"]];
    [[mapView getOptions] setTerrainOptions:terrain];
}

+ (void)applySkyAndLightConfig:(NTMapView *)mapView {
    if ([DemoConfig boolFor:@"daycycle"]) {
        // The day cycle owns the sun and the sky colours together, so it replaces the manual
        // sliders rather than layering on top of them.
        [DemoSky applyDayCycle:mapView hour:[DemoConfig floatFor:@"dayCycleHour"]];
        return;
    }
    NTOptions *options = [mapView getOptions];

    NTLightOptions *light = [[NTLightOptions alloc] init];
    [light setSunAzimuth:[DemoConfig floatFor:@"sunAzimuth"]];
    [light setSunAltitude:[DemoConfig floatFor:@"sunAltitude"]];
    [light setSunIntensity:[DemoConfig floatFor:@"sunIntensity"]];
    [light setAmbientIntensity:[DemoConfig floatFor:@"ambient"]];
    [light setShadowStrength:[DemoConfig floatFor:@"shadow"]];
    [light setShadowSoftness:[DemoConfig floatFor:@"shadowSoftness"]];
    [options setLightOptions:light];

    NTSkyOptions *sky = [options getSkyOptions];
    [sky setEnabled:[DemoConfig boolFor:@"sky"]];
    [options setSkyOptions:sky];
}

+ (void)applyCameraConfig:(NTMapView *)mapView {
    // Convert through the map's OWN projection (EPSG3857 by default). Going through NTEPSG4326
    // instead looks right and is not: its fromWgs84 is the identity, so lon/lat would be fed to
    // the map as metres and land the camera in the ocean off 0,0.
    NTProjection *projection = [[mapView getOptions] getBaseProjection];
    NTMapPos *focus = [projection fromWgs84:[[NTMapPos alloc] initWithX:[DemoConfig doubleFor:@"lon"]
                                                                     y:[DemoConfig doubleFor:@"lat"]]];
    [mapView setFocusPos:focus durationSeconds:0];
    [mapView setZoom:[DemoConfig floatFor:@"zoom"] durationSeconds:0];
    [mapView setTilt:[DemoConfig floatFor:@"tilt"] durationSeconds:0];
    [mapView setRotation:[DemoConfig floatFor:@"rotation"] durationSeconds:0];
}

@end
