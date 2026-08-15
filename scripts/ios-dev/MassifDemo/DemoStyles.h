#import <Foundation/Foundation.h>
#import "MassifMaps.h"

/**
 * Style decoders and the generated CartoCSS, the counterpart of scripts/android-dev's
 * DemoStyles.java.
 *
 * The Android demo prefers a style read off the device (a folder, then a zip) and falls back to a
 * generated inline style. Here the inline style is the DEFAULT: it needs no assets on the
 * simulator, and being generated from DemoConfig is what makes it useful as a bench - the road
 * width, the label rules, the composite slots and the 3D buildings are all knobs.
 */
@interface DemoStyles : NSObject

/** Decoder for the base map, honouring the 'style' knob (inline | zip | project). */
+ (MSFMBVectorTileDecoder *)createDecoder;

/** Name of the boolean 'param::' parameter the demo flips; see DemoMap.s param toggle loop. */
+ (NSString *)boolParameter;

/** The generated base-map CartoCSS. */
+ (NSString *)inlineStyle;
/** Style for the stand-alone on-the-fly contour layer. */
+ (NSString *)contourStyle;
/** Style for the pre-baked contour vector tiles. */
+ (NSString *)contourTilesStyle;
/** Style for the GeoJSON route test layer. */
+ (NSString *)routeTestStyle;
/** Summit callout labels for the peaks layer. */
+ (NSString *)peaksStyle;
/** Shield test style: an icon that stays on the feature, a name the culler puts on a free side. */
+ (NSString *)poiTestStyle;
/** Style of the maneuver arrow layer; headPath is the head outline, empty for the built-in one. */
+ (NSString *)maneuverStyle:(NSString *)headPath;

// --- shaders ---
/** Slope-angle bands, as a lighting shader on the hillshade layer. */
+ (NSString *)slopesShader;
/** Hypsometric tint, decoding terrarium elevation from the raw DEM texel. */
+ (NSString *)hypsometricShader;
/** The peak-finder relief surface, drawn where no tile layer paints. */
+ (NSString *)reliefSurfaceShader;
/** The relief OUTLINE effect: silhouettes and creases from the packed terrain depth. */
+ (NSString *)reliefOutlineShader;

@end
