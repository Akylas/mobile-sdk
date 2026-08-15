#import "DemoToast.h"
#import <UIKit/UIKit.h>

@implementation DemoToast

+ (void)show:(NSString *)message {
    NSLog(@"MassifDemo: %@", message);
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self show:message]; });
        return;
    }

    UIWindow *window = nil;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]) {
            for (UIWindow *candidate in ((UIWindowScene *)scene).windows) {
                if (candidate.isKeyWindow) {
                    window = candidate;
                }
            }
        }
    }
    if (!window) {
        return;
    }

    // A label has no padding of its own, so the plate is a view around it.
    UIView *plate = [[UIView alloc] init];
    plate.backgroundColor = [UIColor colorWithWhite:0 alpha:0.75];
    plate.layer.cornerRadius = 10;
    plate.alpha = 0;
    plate.translatesAutoresizingMaskIntoConstraints = NO;
    // Nothing under it should stop working while it is up.
    plate.userInteractionEnabled = NO;
    [window addSubview:plate];

    UILabel *label = [[UILabel alloc] init];
    label.text = message;
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    label.font = [UIFont systemFontOfSize:14];
    label.textColor = [UIColor whiteColor];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [plate addSubview:label];

    [NSLayoutConstraint activateConstraints:@[
        [plate.centerXAnchor constraintEqualToAnchor:window.centerXAnchor],
        [plate.bottomAnchor constraintEqualToAnchor:window.safeAreaLayoutGuide.bottomAnchor constant:-80],
        [plate.widthAnchor constraintLessThanOrEqualToAnchor:window.widthAnchor multiplier:0.85],
        [label.topAnchor constraintEqualToAnchor:plate.topAnchor constant:8],
        [label.bottomAnchor constraintEqualToAnchor:plate.bottomAnchor constant:-8],
        [label.leadingAnchor constraintEqualToAnchor:plate.leadingAnchor constant:14],
        [label.trailingAnchor constraintEqualToAnchor:plate.trailingAnchor constant:-14],
    ]];

    [UIView animateWithDuration:0.2 animations:^{
        plate.alpha = 1;
    } completion:^(BOOL finished) {
        [UIView animateWithDuration:0.3 delay:2.0 options:0 animations:^{
            plate.alpha = 0;
        } completion:^(BOOL done) {
            [plate removeFromSuperview];
        }];
    }];
}

@end
