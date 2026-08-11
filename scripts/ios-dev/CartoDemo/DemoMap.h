#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

@class DemoCelestial;
@class DemoStars;
// Fork additions, not listed in the umbrella header.
@class NTContourTileDataSource;
@class NTGeoJSONVectorTileDataSource;

/**
 * The whole demo map, as ONE composable configuration instead of a set of separate examples. The
 * counterpart of scripts/android-dev's DemoMap.java, with the same model and the same method names.
 *
 * MODEL
 *  - DemoConfig says what should exist;
 *  - this class owns the map objects and makes reality match the config: -build once, then
 *    -rebuildLayers / apply* whenever something changes;
 *  - the panel (DemoPanel) only writes DemoConfig values and calls back into here.
 *
 * LAYERS
 *  Each DemoFeature is an independent layer that can be switched on or off at any time. They are
 *  (re)built lazily and cached, and always re-added in the same order, so toggling one never
 *  changes the draw order of the others.
 *
 * SOURCES
 *  Tile sources are created ONCE and shared: the DEM feeds the 3D terrain, the hillshade, the
 *  contours and the hypsometric tint at the same time, so a tile is downloaded and decoded once.
 */
typedef NS_ENUM(NSInteger, DemoFeature) {
    DemoFeatureCelestial,
    DemoFeatureStars,
    DemoFeatureBase,
    DemoFeatureSatellite,
    DemoFeatureHillshade,
    DemoFeatureHypso,
    DemoFeatureContour,
    DemoFeatureContourTiles,
    DemoFeatureRouteTest,
    DemoFeatureManeuvers,
    DemoFeatureElements,
    DemoFeaturePeaks,
};

@interface DemoMap : NSObject

- (instancetype)initWithMapView:(NTMapView *)mapView;

@property (nonatomic, readonly, weak) NTMapView *mapView;
// --- map objects the panel needs to reach ---
@property (nonatomic, readonly) NTTerrainOptions *terrainOptions;
@property (nonatomic, readonly) NTLightOptions *lightOptions;
@property (nonatomic, readonly) NTSkyOptions *skyOptions;
/** Sun, moon and their daily paths - demo content built on the generic celestial API. */
@property (nonatomic, readonly) DemoCelestial *celestial;
/** The bright-star catalogue, the constellation figures and the planets - same API. */
@property (nonatomic, readonly) DemoStars *stars;
/** Result of the last composite slot check: which slots the style really has. */
@property (nonatomic, readonly, copy) NSString *compositeStatus;

/** Applies the whole DemoConfig to a fresh map. */
- (void)build;

// --- layers ---
- (BOOL)isEnabled:(DemoFeature)feature;
- (void)setEnabled:(DemoFeature)feature enabled:(BOOL)enabled;
/** Drops the cached instance of a layer so the next rebuild constructs it from the config. */
- (void)invalidate:(DemoFeature)feature;
- (void)rebuildLayers;
/** Needed after a style-source or base-mode change. */
- (void)rebuildBaseLayer;
/** The peak labels are style-driven, so every callout knob needs a new decoder. */
- (void)rebuildPeaksLayer;
/** Adds/removes the composite slots to match the config. */
- (void)syncCompositeSources;
/** Puts every sky object where it really is for the configured date, hour and position. */
- (void)updateSky;

// --- options ---
- (void)applyOptions;
- (void)applyTerrainOptions;
- (void)applyLightOptions;
- (void)applySkyOptions;
- (void)applyHillshadeConfig;
- (void)applyContourConfig;
- (void)applyDebugConfig;
- (void)applyDayCycle:(float)hourUtc;
- (void)applyCamera;
/** Redraw request, the counterpart of Android's mapView.requestRender(). */
- (void)requestRender;
/** Elevation under a WGS84 position; blocks on tile loading, so call it off the main thread. */
- (double)getElevation:(NTMapPos *)wgs84Pos;
/** Free roam, panning mode and how far above the horizon the view may look. */
- (void)applyLookRange;

// --- the relief / peak-finder look ---
- (void)applyReliefSurface;
- (void)setReliefOutlineEnabled:(BOOL)enabled;
- (void)applyReliefOutlineParameters;
- (void)setReliefDark:(BOOL)dark;
/** Enters the peak-finder view from wherever the map is, as ONE camera move. */
- (void)flyToPeakFinder;
- (void)setPeakFinderMode:(BOOL)enabled;
/** Lifts the viewpoint by the configured elevation, so you can see over the ridge in front. */
- (void)applyViewpointElevation;
/** AR: the relief view over the camera preview. */
- (void)setArMode:(BOOL)enabled;

// --- star sky ---
- (void)applyStarSky:(BOOL)enabled;
- (void)setOrientationFollowing:(BOOL)enabled;
- (void)setCameraPreviewEnabled:(BOOL)enabled;

// --- maneuver arrows ---
/** The arrow source, kept across layer rebuilds so the arrows survive a toggle. */
- (NTGeoJSONVectorTileDataSource *)maneuverSource;
/** Shows one arrow under an id, or removes it when the arrow is nil or empty. */
- (void)setManeuverArrow:(int)arrowId arrow:(NTFeatureCollection *)arrow;
- (void)clearManeuverArrows;
/** Moves to the next head outline and rebuilds the layer; returns what it landed on. */
- (NSString *)cycleManeuverHead;

// --- shared tile sources ---
- (NTTileDataSource *)vectorSource;
- (NTTileDataSource *)demSource;
- (NTTileDataSource *)rasterSource;
- (NTTileDataSource *)contourTilesSource;
- (NTContourTileDataSource *)contourSource;
- (NTElevationDecoder *)elevationDecoder;

/** The relief palette, as CartoCSS colours: the surface, the ink, the plate and the sky. */
+ (NSString *)reliefInk;
+ (NSString *)reliefPaper;
+ (NSString *)reliefShade;
+ (NSString *)reliefSky;
/** "#rrggbb" / "#aarrggbb" as an SDK colour. */
+ (NTColor *)colorFromHex:(NSString *)hex;

@end
