#import "DemoViewController.h"
#import "DemoConfig.h"
#import "DemoMap.h"
#import "DemoPanel.h"

@interface DemoViewController ()
@property (nonatomic, strong) NTMapView *mapView;
@property (nonatomic, strong) DemoMap *demo;
@end

@implementation DemoViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    // Android reads its knobs in the fragment; here the launch arguments are already folded into
    // NSUserDefaults by the time the view loads, so this is the equivalent hook.
    [DemoConfig applyLaunchArgumentOverrides];

    // Native logs go to stdout, so they only show with 'simctl launch --console-pty'.
    [NTLog setShowInfo:YES];
    [NTLog setShowDebug:YES];
    [NTLog setShowWarn:YES];
    [NTLog setShowError:YES];

    self.mapView = [[NTMapView alloc] initWithFrame:self.view.bounds];
    self.mapView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.mapView];

    // Base map options that are not part of the demo configuration itself - the same block
    // SecondFragment.java sets on Android, and the reason double-tap zoom did nothing here:
    // Options.zoomGestures is OFF by default in the SDK, so both the double-tap step and the
    // double-tap-and-drag zoom are disabled until an app asks for them.
    // The base projection is EPSG4326 on both demos as well, so a camera copied from one command
    // line to the other lands in the same place - zoom levels are per projection.
    NTOptions *options = [self.mapView getOptions];
    [options setBaseProjection:[[NTEPSG4326 alloc] init]];
    [options setZoomGestures:YES];
    [options setRestrictedPanning:YES];
    [options setSeamlessPanning:YES];
    [options setRotatable:YES];
    [options setTiltRange:[[NTMapRange alloc] initWithMin:30 max:90]];
    [options setPanningMode:NT_PANNING_MODE_STICKY];

    self.demo = [[DemoMap alloc] initWithMapView:self.mapView];
    [self.demo build];

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

/**
 * The panel comes up as a BOTTOM SHEET rather than a full-screen modal: the point of a knob is
 * watching the map change as you drag it, and a sheet at a medium detent keeps the map on screen.
 * Undimmed for the same reason, and .large is available for the long sections.
 */
- (void)showPanel {
    DemoPanel *panel = [[DemoPanel alloc] initWithDemo:self.demo];
    UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:panel];

    UISheetPresentationController *sheet = nav.sheetPresentationController;
    if (sheet) {
        sheet.detents = @[[UISheetPresentationControllerDetent mediumDetent],
                          [UISheetPresentationControllerDetent largeDetent]];
        sheet.prefersGrabberVisible = YES;
        sheet.prefersScrollingExpandsWhenScrolledToEdge = NO;
        // Keep the map live behind the sheet at the medium detent.
        sheet.largestUndimmedDetentIdentifier = UISheetPresentationControllerDetentIdentifierMedium;
        sheet.preferredCornerRadius = 16;
    }
    [self presentViewController:nav animated:YES completion:nil];
}

@end
