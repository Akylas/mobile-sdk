#import "DemoOrientation.h"
#import "DemoConfig.h"
#import <CoreMotion/CoreMotion.h>

/** Smoothing of the fused reading: 1 = raw, smaller = calmer and laggier. */
static const float SMOOTHING = 0.2f;
/** Below this much movement, in degrees, the camera is left alone. */
static const float DEAD_ZONE_DEGREES = 0.3f;

@implementation DemoOrientation {
    __weak MSFMapView *_mapView;
    CMMotionManager *_motionManager;
    BOOL _running;
    BOOL _primed;
    float _heading;
    float _elevation;
}

- (instancetype)initWithMapView:(MSFMapView *)mapView {
    if ((self = [super init])) {
        _mapView = mapView;
    }
    return self;
}

- (BOOL)running {
    return _running;
}

- (void)start {
    if (_running) {
        return;
    }
    if (!_motionManager) {
        _motionManager = [[CMMotionManager alloc] init];
    }
    if (!_motionManager.isDeviceMotionAvailable) {
        NSLog(@"MassifDemo: no device motion here (expected on the simulator)");
        return;
    }
    _running = YES;
    _primed = NO;
    _motionManager.deviceMotionUpdateInterval = 1.0 / 60.0;
    __weak DemoOrientation *weakSelf = self;
    // True north rather than magnetic, so the heading matches the map's north.
    [_motionManager startDeviceMotionUpdatesUsingReferenceFrame:CMAttitudeReferenceFrameXTrueNorthZVertical
                                                        toQueue:[NSOperationQueue mainQueue]
                                                    withHandler:^(CMDeviceMotion *motion, NSError *error) {
        [weakSelf handleMotion:motion];
    }];
}

- (void)stop {
    if (!_running) {
        return;
    }
    [_motionManager stopDeviceMotionUpdates];
    _running = NO;
}

- (void)handleMotion:(CMDeviceMotion *)motion {
    MSFMapView *mapView = _mapView;
    if (!motion || !mapView || !_running) {
        return;
    }

    // The phone is held up like a window on the sky, so the axis pointing OUT of the back of the
    // device is what aims the camera - the same thing Android's AXIS_X/AXIS_Z remap produces. The
    // attitude matrix takes a device vector into the reference frame, so the back of the device,
    // (0, 0, -1), is minus its third column.
    CMRotationMatrix m = motion.attitude.rotationMatrix;
    double x = -m.m13, y = -m.m23, z = -m.m33;
    // Reference frame: X is true north, Z is up, so Y is west - hence the negated Y for east.
    float newHeading = (float)(atan2(-y, x) * 180.0 / M_PI);
    // 0 level, negative looking up: a negative map tilt is exactly the same thing.
    float newElevation = (float)(-asin(fmax(-1.0, fmin(1.0, z))) * 180.0 / M_PI);

    if (!_primed) {
        _heading = newHeading;
        _elevation = newElevation;
        _primed = YES;
    } else {
        // Headings wrap: smooth the SHORTEST way round, or crossing north swings the view through
        // a full turn.
        float delta = fmodf(newHeading - _heading + 540.0f, 360.0f) - 180.0f;
        _heading += SMOOTHING * delta;
        _elevation += SMOOTHING * (newElevation - _elevation);
    }

    float tilt = fmaxf(-[DemoConfig floatFor:@"lookUp"], fminf(90.0f, _elevation));
    float rotation = -_heading;
    if (fabsf(tilt - [mapView getTilt]) < DEAD_ZONE_DEGREES
            && fabsf(fmodf(rotation - [mapView getRotation] + 540.0f, 360.0f) - 180.0f) < DEAD_ZONE_DEGREES) {
        return;
    }
    [mapView setTilt:tilt durationSeconds:0];
    [mapView setRotation:rotation durationSeconds:0];
}

@end
