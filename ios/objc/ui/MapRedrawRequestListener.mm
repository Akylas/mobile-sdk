#import "MapRedrawRequestListener.h"

@implementation MSFMapRedrawRequestListener

-(id)initWithView:(MSFGLKView*)view {
    self = [super init];
    _view = view;
    return self;
}

-(void)onRedrawRequested {
    dispatch_async(dispatch_get_main_queue(), ^{
        MSFGLKView* view = _view;
        if (view) {
            [view setNeedsDisplay];
        }
    });
}

@end
