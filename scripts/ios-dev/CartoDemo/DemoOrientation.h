#import <Foundation/Foundation.h>
#import "CartoMobileSDK.h"

/**
 * Turns the map with the device, the counterpart of scripts/android-dev's DemoOrientation.java.
 * Android reads the rotation vector sensor; here it is CoreMotion's device motion, whose yaw is
 * already fused from gyro and magnetometer.
 *
 * Only useful on a device - the simulator reports no motion - but it is wired the same way so the
 * free-roam and peak-finder modes have something to drive them.
 */
@interface DemoOrientation : NSObject

+ (void)setFollowing:(BOOL)following mapView:(NTMapView *)mapView;
+ (BOOL)isFollowing;

@end
