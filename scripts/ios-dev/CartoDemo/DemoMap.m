#import "DemoMap.h"
#import "DemoConfig.h"
#import "DemoStyles.h"
#import "DemoSky.h"
#import "DemoCelestial.h"
#import "NTCustomRasterTileLayer.h"
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
    [DemoCelestial applyToMapView:mapView];
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
    if ([DemoConfig boolFor:@"hypso"]) {
        // Hypsometric tint: a CustomRasterTileLayer running a filter shader over the RAW DEM,
        // which is the general case the hillshade layer is a special case of.
        NTCustomRasterTileLayer *hypso =
            [[NTCustomRasterTileLayer alloc] initWithDataSource:[self demSource]];
        [hypso setShaderSource:[DemoStyles hypsometricShader]];
        [layers add:hypso];
    }
    if ([DemoConfig boolFor:@"elements"]) {
        [layers add:[self buildElementsLayer:mapView]];
    }
    if ([DemoConfig boolFor:@"peaks"]) {
        NTCartoCSSStyleSet *styleSet =
            [[NTCartoCSSStyleSet alloc] initWithCartoCSS:[DemoStyles peaksStyle]];
        [layers add:[[NTVectorTileLayer alloc]
                     initWithDataSource:[self httpSourceWithURL:[DemoConfig stringFor:@"vectorUrl"]
                                                        minZoom:0
                                                        maxZoom:[DemoConfig intFor:@"vectorMaxZoom"]
                                                          cache:@"akylas_vect.db"]
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

/**
 * A handful of vector elements around the camera - marker, line, polygon, text and a 3D polygon.
 * The Android demo's 'elements' layer; it is what the terrain-following and billboard-occlusion
 * work is judged on, since every element type sits on the terrain differently.
 */
+ (NTVectorLayer *)buildElementsLayer:(NTMapView *)mapView {
    NTProjection *projection = [[mapView getOptions] getBaseProjection];
    NTLocalVectorDataSource *source =
        [[NTLocalVectorDataSource alloc] initWithProjection:projection];

    NTMapPos *centre = [projection toWgs84:[mapView getFocusPos]];
    double lon = [centre getX], lat = [centre getY];
    NTMapPos *(^pos)(double, double) = ^NTMapPos *(double dx, double dy) {
        return [projection fromWgs84:[[NTMapPos alloc] initWithX:lon + dx y:lat + dy]];
    };

    NTMarkerStyleBuilder *markerStyle = [[NTMarkerStyleBuilder alloc] init];
    [markerStyle setSize:24];
    [markerStyle setColor:[[NTColor alloc] initWithR:220 g:60 b:60 a:255]];
    [source add:[[NTMarker alloc] initWithPos:pos(0, 0) style:[markerStyle buildStyle]]];

    NTTextStyleBuilder *textStyle = [[NTTextStyleBuilder alloc] init];
    [textStyle setFontSize:16];
    [textStyle setColor:[[NTColor alloc] initWithR:20 g:20 b:20 a:255]];
    [source add:[[NTText alloc] initWithPos:pos(0.0008, 0.0006)
                                      style:[textStyle buildStyle]
                                       text:@"element"]];

    NTLineStyleBuilder *lineStyle = [[NTLineStyleBuilder alloc] init];
    [lineStyle setWidth:6];
    [lineStyle setColor:[[NTColor alloc] initWithR:40 g:120 b:220 a:255]];
    NTMapPosVector *linePoints = [[NTMapPosVector alloc] init];
    for (int i = 0; i < 6; i++) {
        [linePoints add:pos((i - 3) * 0.0012, (i % 2 ? 1 : -1) * 0.0008)];
    }
    [source add:[[NTLine alloc] initWithPoses:linePoints style:[lineStyle buildStyle]]];

    NTPolygonStyleBuilder *polygonStyle = [[NTPolygonStyleBuilder alloc] init];
    [polygonStyle setColor:[[NTColor alloc] initWithR:60 g:180 b:120 a:140]];
    NTMapPosVector *ring = [[NTMapPosVector alloc] init];
    [ring add:pos(-0.002, -0.002)];
    [ring add:pos(0.002, -0.002)];
    [ring add:pos(0.002, 0.002)];
    [ring add:pos(-0.002, 0.002)];
    [source add:[[NTPolygon alloc] initWithPoses:ring style:[polygonStyle buildStyle]]];

    return [[NTVectorLayer alloc] initWithDataSource:source];
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
    if ([DemoConfig boolFor:@"slopes"]) {
        // Replaces the shading with slope-angle bands; same layer, different lighting shader.
        [layer setNormalMapLightingShader:[DemoStyles slopesShader]];
    }
    [layer setContourEnabled:[DemoConfig boolFor:@"hsContours"]];
    [layer setContourInterval:[DemoConfig floatFor:@"hsContourInterval"]];
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

    // Peak-finder relief: a shaded surface drawn where no tile layer paints, so it only shows with
    // the map layers off. Clearing the source string puts the ordinary surface back.
    [terrain setSurfaceShaderSource:[DemoConfig boolFor:@"reliefSurface"]
                                    ? [DemoStyles reliefSurfaceShader] : @""];
    if ([DemoConfig boolFor:@"reliefSurface"]) {
        BOOL dark = [DemoConfig boolFor:@"reliefDark"];
        [terrain setSurfaceColorParameter:@"uPaperColor"
                                    color:(dark ? [[NTColor alloc] initWithR:26 g:28 b:33 a:255]
                                                : [[NTColor alloc] initWithR:246 g:243 b:236 a:255])];
        [terrain setSurfaceColorParameter:@"uShadeColor"
                                    color:(dark ? [[NTColor alloc] initWithR:8 g:9 b:12 a:255]
                                                : [[NTColor alloc] initWithR:120 g:116 b:104 a:255])];
        [terrain setSurfaceParameter:@"uShadeStrength" value:[DemoConfig floatFor:@"reliefShade"]];
        [terrain setSurfaceParameter:@"uAmbient" value:[DemoConfig floatFor:@"reliefAmbient"]];
        [terrain setSurfaceParameter:@"uHaze" value:[DemoConfig floatFor:@"reliefHaze"]];
        [terrain setSurfaceParameter:@"uHazeDistance" value:[DemoConfig floatFor:@"reliefHazeDistance"]];
    }
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

/**
 * Peak finder: a panorama camera. Tilt is LOW here - in this SDK 90 is straight down, so a
 * horizon view is around 25, not 85 - and the view distance is opened up so distant summits are
 * still drawn.
 */
+ (void)applyPeakFinder:(NTMapView *)mapView {
    if (![DemoConfig boolFor:@"peakfinder"]) {
        return;
    }
    [mapView setTilt:[DemoConfig floatFor:@"peakFinderTilt"] durationSeconds:0];
    [mapView setZoom:[DemoConfig floatFor:@"peakFinderZoom"] durationSeconds:0];
}

/**
 * Free roam: let the camera look above the horizon, which the ordinary tilt range forbids.
 */
+ (void)applyFreeRoam:(NTMapView *)mapView {
    NTOptions *options = [mapView getOptions];
    if ([[DemoConfig stringFor:@"freeRoam"] isEqualToString:@"off"]) {
        [options setTiltRange:[[NTMapRange alloc] initWithMin:30 max:90]];
        [options setRestrictedPanning:YES];
        return;
    }
    // Negative tilt is what puts the horizon below the middle of the screen and the sky above it.
    [options setTiltRange:[[NTMapRange alloc] initWithMin:-[DemoConfig floatFor:@"lookUp"] max:90]];
    [options setRestrictedPanning:NO];
}

+ (void)applyCelestial:(NTMapView *)mapView {
    [DemoCelestial applyToMapView:mapView];
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
    [self applyFreeRoam:mapView];
    [self applyPeakFinder:mapView];
}

@end
