#import "DemoOrientation.h"
#import <CoreMotion/CoreMotion.h>

@implementation DemoOrientation

static CMMotionManager *sMotionManager = nil;
static BOOL sFollowing = NO;

+ (BOOL)isFollowing {
    return sFollowing;
}

+ (void)setFollowing:(BOOL)following mapView:(NTMapView *)mapView {
    sFollowing = following;

    if (!following) {
        [sMotionManager stopDeviceMotionUpdates];
        return;
    }
    if (!sMotionManager) {
        sMotionManager = [[CMMotionManager alloc] init];
    }
    if (!sMotionManager.isDeviceMotionAvailable) {
        NSLog(@"CartoDemo: no device motion here (expected on the simulator)");
        sFollowing = NO;
        return;
    }

    sMotionManager.deviceMotionUpdateInterval = 1.0 / 30.0;
    __weak NTMapView *weakMapView = mapView;
    // True north rather than magnetic, so the heading matches the map's north.
    [sMotionManager startDeviceMotionUpdatesUsingReferenceFrame:CMAttitudeReferenceFrameXTrueNorthZVertical
                                                        toQueue:[NSOperationQueue mainQueue]
                                                    withHandler:^(CMDeviceMotion *motion, NSError *error) {
        NTMapView *strongMapView = weakMapView;
        if (!motion || !strongMapView || !sFollowing) {
            return;
        }
        // Yaw is counter-clockwise from the reference frame; the map's rotation runs the other way.
        float heading = (float)(-motion.attitude.yaw * 180.0 / M_PI);
        [strongMapView setRotation:heading durationSeconds:0.1f];
    }];
}

@end
