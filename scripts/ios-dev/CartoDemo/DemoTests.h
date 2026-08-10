#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

/**
 * One-shot actions, the counterpart of scripts/android-dev's DemoTests.java: things you trigger
 * from the panel rather than configure. Each adds its own layer so 'clear' can remove them without
 * touching the base map.
 */
@interface DemoTests : NSObject

+ (void)run:(NSString *)action mapView:(NTMapView *)mapView;

@end
