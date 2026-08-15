#import <Foundation/Foundation.h>
#import "MassifMaps.h"

/**
 * Points the camera where the device points: turning the phone turns the view, raising it looks up.
 * The counterpart of scripts/android-dev's DemoOrientation.java - Android reads the rotation vector
 * sensor, here it is CoreMotion's device motion, which is fused the same way.
 *
 * This is the star-sky demo's reason for a negative tilt. The map's rotation is the opposite of the
 * heading (rotating the map right turns the view left), and the elevation above the horizon IS the
 * negative tilt the SDK now supports - at tilt -90 the view looks straight at the zenith.
 *
 * Only useful on a device: the simulator reports no device motion.
 */
@interface DemoOrientation : NSObject

- (instancetype)initWithMapView:(MSFMapView *)mapView;

@property (nonatomic, readonly) BOOL running;

- (void)start;
- (void)stop;

@end
