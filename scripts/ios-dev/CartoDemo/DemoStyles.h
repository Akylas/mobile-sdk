#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

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

/** Decoder for the base map, honouring the 'style' knob (inline | zip | assets). */
+ (NTMBVectorTileDecoder *)createDecoder;

/** The generated base-map CartoCSS. */
+ (NSString *)inlineStyle;
/** Style for the stand-alone on-the-fly contour layer. */
+ (NSString *)contourStyle;
/** Style for the pre-baked contour vector tiles. */
+ (NSString *)contourTilesStyle;
/** Style for the GeoJSON route test layer. */
+ (NSString *)routeTestStyle;

@end
