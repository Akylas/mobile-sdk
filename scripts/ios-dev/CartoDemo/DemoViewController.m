#import "DemoViewController.h"
#import "DemoConfig.h"
#import "DemoMap.h"
#import "DemoPanel.h"

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

    if ([DemoConfig boolFor:@"ui"]) {
        [self addSettingsButton];
    }
}

/** Bottom-left gear, the same corner the Android demo puts it in. */
- (void)addSettingsButton {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setImage:[UIImage systemImageNamed:@"gearshape.fill"] forState:UIControlStateNormal];
    button.tintColor = [UIColor labelColor];
    button.backgroundColor = [[UIColor systemBackgroundColor] colorWithAlphaComponent:0.85];
    button.layer.cornerRadius = 22;
    button.translatesAutoresizingMaskIntoConstraints = NO;
    [button addTarget:self action:@selector(showPanel) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:button];

    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [button.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:16],
        [button.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-16],
        [button.widthAnchor constraintEqualToConstant:44],
        [button.heightAnchor constraintEqualToConstant:44],
    ]];
}

- (void)showPanel {
    DemoPanel *panel = [[DemoPanel alloc] initWithMapView:self.mapView];
    UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:panel];
    [self presentViewController:nav animated:YES completion:nil];
}

@end
