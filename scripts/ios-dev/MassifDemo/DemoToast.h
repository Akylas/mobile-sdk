#import <Foundation/Foundation.h>

/**
 * A brief message over the map, the counterpart of the Android demo's Toast - which is what every
 * click listener and one-shot test there reports through. UIKit has no such thing, so this is a
 * label that fades in over the key window and back out.
 */
@interface DemoToast : NSObject

/** Shows a message. Safe to call from any thread; the SDK's listeners run off the main one. */
+ (void)show:(NSString *)message;

@end
