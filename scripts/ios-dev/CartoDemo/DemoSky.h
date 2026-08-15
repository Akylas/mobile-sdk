#import <Foundation/Foundation.h>
#import "MassifMaps.h"

/**
 * Day-cycle demo: sun position, sky/horizon/ground colours, shadow strength and a GENERATED sky
 * shader that draws the sun disc, the moon, stars and procedural clouds - all from one hour value.
 * The counterpart of scripts/android-dev's DemoSky.java.
 *
 * Nothing here touches layers, only Options, so it can be switched on and off at runtime.
 *
 * The moon direction and the sun's arc are baked into the shader SOURCE instead of being passed as
 * uniforms: the sky shader contract has a fixed uniform set, and regenerating the source when the
 * hour changes is cheap enough for a demo.
 */
@interface DemoSky : NSObject

/**
 * Applies one hour of the day cycle. lat/lon should be the current map centre, since the sun
 * position is computed for it.
 */
+ (void)applyHour:(float)hourUtc light:(NTLightOptions *)light sky:(NTSkyOptions *)sky
              lat:(double)lat lon:(double)lon;

@end
