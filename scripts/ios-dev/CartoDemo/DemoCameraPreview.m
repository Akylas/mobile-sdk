#import "DemoCameraPreview.h"
#import <AVFoundation/AVFoundation.h>

/** A view backed by the capture preview layer, so rotating the device re-frames it for free. */
@interface DemoCameraPreviewView : UIView
@end

@implementation DemoCameraPreviewView

+ (Class)layerClass {
    return [AVCaptureVideoPreviewLayer class];
}

@end

@implementation DemoCameraPreview {
    __weak UIView *_root;
    DemoCameraPreviewView *_previewView;
    AVCaptureSession *_session;
    AVCaptureVideoPreviewLayer *_previewLayer;
}

- (instancetype)initWithRootView:(UIView *)root {
    if ((self = [super init])) {
        _root = root;
    }
    return self;
}

- (BOOL)running {
    return _previewView != nil;
}

- (void)start {
    // The demo can reach this off the main thread (a panel callback runs on it, a map listener does
    // not), and a view hierarchy may only be touched on the main one.
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self start]; });
        return;
    }
    if (_previewView) {
        return;
    }

    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (status == AVAuthorizationStatusNotDetermined) {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
            if (granted) {
                dispatch_async(dispatch_get_main_queue(), ^{ [self start]; });
            } else {
                NSLog(@"CartoDemo: camera permission refused - the AR sky has nothing behind it");
            }
        }];
        return;
    }
    if (status != AVAuthorizationStatusAuthorized) {
        NSLog(@"CartoDemo: no camera permission - grant it in Settings and switch the mode again");
        return;
    }

    UIView *root = _root;
    if (!root) {
        return;
    }

    AVCaptureDevice *camera = [AVCaptureDevice defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera
                                                                mediaType:AVMediaTypeVideo
                                                                 position:AVCaptureDevicePositionBack];
    if (!camera) {
        NSLog(@"CartoDemo: no back camera here (expected on the simulator)");
        return;
    }
    NSError *error = nil;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:camera error:&error];
    if (!input) {
        NSLog(@"CartoDemo: could not open the camera: %@", error.localizedDescription);
        return;
    }

    _session = [[AVCaptureSession alloc] init];
    _session.sessionPreset = AVCaptureSessionPresetHigh;
    if (![_session canAddInput:input]) {
        NSLog(@"CartoDemo: the camera input was refused by the session");
        _session = nil;
        return;
    }
    [_session addInput:input];

    _previewView = [[DemoCameraPreviewView alloc] initWithFrame:root.bounds];
    _previewView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _previewView.backgroundColor = [UIColor blackColor];
    // Index 0: below every other view, the map included.
    [root insertSubview:_previewView atIndex:0];

    _previewLayer = (AVCaptureVideoPreviewLayer *)_previewView.layer;
    _previewLayer.session = _session;
    _previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;

    // Starting the session blocks for a moment, and the caller is the main thread.
    AVCaptureSession *session = _session;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        [session startRunning];
    });
}

- (void)stop {
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self stop]; });
        return;
    }
    [_session stopRunning];
    _session = nil;
    _previewLayer = nil;
    [_previewView removeFromSuperview];
    _previewView = nil;
}

@end
