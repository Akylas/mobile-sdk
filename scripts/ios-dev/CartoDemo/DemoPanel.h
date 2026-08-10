#import <UIKit/UIKit.h>
#import "CartoMobileSDK.h"

/**
 * On-screen settings panel, the counterpart of scripts/android-dev's DemoPanel.java: a control
 * writes DemoConfig and then calls the matching DemoMap apply* method, so nothing here knows how
 * the map is built.
 *
 * Driven by a table of entries rather than a hand-built layout, so exposing another knob is one
 * row - the Java panel is 1200 lines mostly because every control is written out.
 */
@interface DemoPanel : UIViewController

- (instancetype)initWithMapView:(NTMapView *)mapView;

@end
