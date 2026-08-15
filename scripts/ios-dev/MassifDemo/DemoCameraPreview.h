#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

/**
 * A live camera preview BEHIND the map, which is what makes a transparent map worth having: the
 * star sky drawn over what the camera sees is an augmented-reality sky. The counterpart of
 * scripts/android-dev's DemoCameraPreview.java.
 *
 * Android needs a SurfaceView under the map's own surface for this; on iOS the map is an ordinary
 * UIView, so the preview is an AVCaptureVideoPreviewLayer on a view inserted at index 0 - below
 * everything else in the same hierarchy. What makes it visible is the same pair as on Android: a
 * transparent clear colour and a translucent map view.
 *
 * Nothing is captured or stored; the session has a preview output only.
 */
@interface DemoCameraPreview : NSObject

- (instancetype)initWithRootView:(UIView *)root;

@property (nonatomic, readonly) BOOL running;

/** Adds the preview under the map and starts it. Asks for the permission if it is missing. */
- (void)start;
/** Stops the preview and takes the view back out of the hierarchy. */
- (void)stop;

@end
