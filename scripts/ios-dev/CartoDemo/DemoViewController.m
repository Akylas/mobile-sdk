#import "DemoViewController.h"
#import "DemoConfig.h"
#import "DemoMap.h"

@interface DemoViewController ()
@property (nonatomic, strong) NTMapView *mapView;
@end

@implementation DemoViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    // Android reads its knobs in the fragment; here the launch arguments are already folded into
    // NSUserDefaults by the time the view loads, so this is the equivalent hook.
    [DemoConfig applyLaunchArgumentOverrides];

    self.mapView = [[NTMapView alloc] initWithFrame:self.view.bounds];
    self.mapView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.mapView];

    [DemoMap setupMapView:self.mapView];
}

@end
