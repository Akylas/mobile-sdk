#import "DemoPanel.h"
#import "DemoConfig.h"
#import "DemoMap.h"
#import "DemoTests.h"
#import <objc/runtime.h>

// The entry a control belongs to, carried on the control itself so the target/action handlers stay
// one-liners instead of resolving an index path back to a row.
static const void *kDemoEntryKey = &kDemoEntryKey;
static void setEntry(UIControl *control, id entry) {
    objc_setAssociatedObject(control, kDemoEntryKey, entry, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
static id getEntry(UIControl *control) {
    return objc_getAssociatedObject(control, kDemoEntryKey);
}

typedef NS_ENUM(NSInteger, DemoEntryKind) {
    DemoEntryToggle,
    DemoEntrySlider,
    DemoEntryChoice,
    DemoEntryAction,
};

/** What rebuilding a knob costs: the cheapest apply that still shows the change. */
typedef NS_ENUM(NSInteger, DemoApply) {
    DemoApplyLayers,
    DemoApplyTerrain,
    DemoApplyLight,
    DemoApplyCamera,
    DemoApplyOptions,
};

@interface DemoEntry : NSObject
@property (nonatomic, copy) NSString *key;
@property (nonatomic, copy) NSString *label;
@property (nonatomic) DemoEntryKind kind;
@property (nonatomic) DemoApply apply;
@property (nonatomic) float minimum;
@property (nonatomic) float maximum;
@property (nonatomic, copy) NSArray<NSString *> *choices;
@property (nonatomic, copy) NSString *action;
@end

@implementation DemoEntry

+ (instancetype)toggle:(NSString *)key label:(NSString *)label apply:(DemoApply)apply {
    DemoEntry *e = [DemoEntry new];
    e.key = key; e.label = label; e.kind = DemoEntryToggle; e.apply = apply;
    return e;
}

+ (instancetype)slider:(NSString *)key label:(NSString *)label
                   min:(float)minimum max:(float)maximum apply:(DemoApply)apply {
    DemoEntry *e = [DemoEntry new];
    e.key = key; e.label = label; e.kind = DemoEntrySlider; e.apply = apply;
    e.minimum = minimum; e.maximum = maximum;
    return e;
}

+ (instancetype)choice:(NSString *)key label:(NSString *)label
               choices:(NSArray<NSString *> *)choices apply:(DemoApply)apply {
    DemoEntry *e = [DemoEntry new];
    e.key = key; e.label = label; e.kind = DemoEntryChoice; e.apply = apply;
    e.choices = choices;
    return e;
}

+ (instancetype)action:(NSString *)action label:(NSString *)label {
    DemoEntry *e = [DemoEntry new];
    e.label = label; e.kind = DemoEntryAction; e.action = action;
    return e;
}

@end

@interface DemoPanel () <UITableViewDataSource, UITableViewDelegate>
@property (nonatomic, weak) NTMapView *mapView;
@property (nonatomic, strong) NSArray<NSString *> *sectionTitles;
@property (nonatomic, strong) NSArray<NSArray<DemoEntry *> *> *sections;
@property (nonatomic, strong) UITableView *tableView;
@end

@implementation DemoPanel

- (instancetype)initWithMapView:(NTMapView *)mapView {
    if ((self = [super init])) {
        _mapView = mapView;
        [self buildEntries];
    }
    return self;
}

- (void)buildEntries {
    self.sectionTitles = @[@"Layers", @"Base map", @"3D terrain", @"Sun & sky", @"Hillshade",
                           @"Contours", @"Camera", @"Actions"];
    self.sections = @[
        @[
            [DemoEntry toggle:@"map" label:@"Base map" apply:DemoApplyLayers],
            [DemoEntry toggle:@"satellite" label:@"Satellite layer" apply:DemoApplyLayers],
            [DemoEntry toggle:@"hillshade" label:@"Hillshade layer" apply:DemoApplyLayers],
            [DemoEntry toggle:@"contour" label:@"Contours (on the fly)" apply:DemoApplyLayers],
            [DemoEntry toggle:@"contourTiles" label:@"Contour tiles" apply:DemoApplyLayers],
        ],
        @[
            [DemoEntry choice:@"base" label:@"Mode" choices:@[@"composite", @"plain"] apply:DemoApplyLayers],
            [DemoEntry choice:@"style" label:@"Style source" choices:@[@"inline", @"zip"] apply:DemoApplyLayers],
            [DemoEntry toggle:@"hs" label:@"Composite hillshade" apply:DemoApplyLayers],
            [DemoEntry toggle:@"sat" label:@"Composite satellite" apply:DemoApplyLayers],
            [DemoEntry toggle:@"singlePass" label:@"Single-pass rendering" apply:DemoApplyLayers],
            [DemoEntry toggle:@"labels" label:@"Labels" apply:DemoApplyLayers],
            [DemoEntry toggle:@"bld3d" label:@"3D buildings" apply:DemoApplyLayers],
            [DemoEntry toggle:@"minimal" label:@"Minimal style" apply:DemoApplyLayers],
            [DemoEntry slider:@"landcoverOpacity" label:@"Landcover opacity" min:0 max:1 apply:DemoApplyLayers],
        ],
        @[
            [DemoEntry toggle:@"terrain" label:@"Enabled" apply:DemoApplyTerrain],
            [DemoEntry slider:@"exaggeration" label:@"Exaggeration" min:0 max:3 apply:DemoApplyTerrain],
            [DemoEntry slider:@"meshResolution" label:@"Mesh resolution" min:8 max:256 apply:DemoApplyTerrain],
            [DemoEntry toggle:@"stitch" label:@"Tile edge stitching" apply:DemoApplyTerrain],
            [DemoEntry toggle:@"seamlessEdges" label:@"Seamless tile edges" apply:DemoApplyTerrain],
            [DemoEntry toggle:@"prefetch" label:@"Elevation prefetch" apply:DemoApplyTerrain],
            [DemoEntry toggle:@"painterDepth" label:@"Painter-order depth" apply:DemoApplyTerrain],
        ],
        @[
            [DemoEntry toggle:@"sky" label:@"Sky" apply:DemoApplyOptions],
            [DemoEntry slider:@"sunAzimuth" label:@"Sun azimuth" min:0 max:360 apply:DemoApplyLight],
            [DemoEntry slider:@"sunAltitude" label:@"Sun altitude" min:-10 max:90 apply:DemoApplyLight],
            [DemoEntry slider:@"sunIntensity" label:@"Sun intensity" min:0 max:2 apply:DemoApplyLight],
            [DemoEntry slider:@"ambient" label:@"Ambient" min:0 max:2 apply:DemoApplyLight],
            [DemoEntry slider:@"shadow" label:@"Shadow strength" min:0 max:1 apply:DemoApplyLight],
        ],
        @[
            [DemoEntry slider:@"hsContrast" label:@"Contrast" min:0 max:1 apply:DemoApplyLayers],
            [DemoEntry slider:@"hsHeightScale" label:@"Height scale" min:0 max:0.5 apply:DemoApplyLayers],
            [DemoEntry slider:@"hsIllumination" label:@"Illumination" min:0 max:360 apply:DemoApplyLayers],
            [DemoEntry toggle:@"hsContours" label:@"Shader contour lines" apply:DemoApplyLayers],
        ],
        @[
            [DemoEntry slider:@"contourInterval" label:@"Base interval" min:5 max:200 apply:DemoApplyLayers],
            [DemoEntry slider:@"contourMinZoom" label:@"Min zoom" min:1 max:16 apply:DemoApplyLayers],
            [DemoEntry toggle:@"contourStubs" label:@"Label stubs" apply:DemoApplyLayers],
        ],
        @[
            [DemoEntry slider:@"zoom" label:@"Zoom" min:1 max:20 apply:DemoApplyCamera],
            [DemoEntry slider:@"tilt" label:@"Tilt" min:0 max:90 apply:DemoApplyCamera],
            [DemoEntry slider:@"rotation" label:@"Rotation" min:-180 max:180 apply:DemoApplyCamera],
        ],
        @[
            [DemoEntry action:@"route" label:@"Route test (GeoJSON)"],
            [DemoEntry action:@"search" label:@"Search test"],
            [DemoEntry action:@"clear" label:@"Clear test layers"],
        ],
    ];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor systemBackgroundColor];
    self.title = @"Demo settings";
    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                      target:self
                                                      action:@selector(dismissPanel)];

    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds
                                                  style:UITableViewStyleInsetGrouped];
    self.tableView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    [self.view addSubview:self.tableView];
}

- (void)dismissPanel {
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)applyEntry:(DemoEntry *)entry {
    NTMapView *mapView = self.mapView;
    if (!mapView) {
        return;
    }
    switch (entry.apply) {
        case DemoApplyLayers:  [DemoMap applyLayers:mapView]; break;
        case DemoApplyTerrain: [DemoMap applyTerrainConfig:mapView]; break;
        case DemoApplyLight:   [DemoMap applySkyAndLightConfig:mapView]; break;
        case DemoApplyCamera:  [DemoMap applyCameraConfig:mapView]; break;
        case DemoApplyOptions: [DemoMap applyOptions:mapView]; break;
    }
}

// ---- table ----

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return self.sections.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return self.sections[section].count;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    return self.sectionTitles[section];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    DemoEntry *entry = self.sections[indexPath.section][indexPath.row];
    UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
                                                   reuseIdentifier:nil];
    cell.textLabel.text = entry.label;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;

    switch (entry.kind) {
        case DemoEntryToggle: {
            UISwitch *toggle = [[UISwitch alloc] init];
            toggle.on = [DemoConfig boolFor:entry.key];
            setEntry(toggle, entry);
            [toggle addTarget:self action:@selector(toggleChanged:)
             forControlEvents:UIControlEventValueChanged];
            cell.accessoryView = toggle;
            break;
        }
        case DemoEntrySlider: {
            UISlider *slider = [[UISlider alloc] initWithFrame:CGRectMake(0, 0, 170, 30)];
            slider.minimumValue = entry.minimum;
            slider.maximumValue = entry.maximum;
            slider.value = [DemoConfig floatFor:entry.key];
            setEntry(slider, entry);
            // Continuous would rebuild the layer stack on every pixel of drag.
            slider.continuous = NO;
            [slider addTarget:self action:@selector(sliderChanged:)
             forControlEvents:UIControlEventValueChanged];
            cell.accessoryView = slider;
            cell.detailTextLabel.text = [NSString stringWithFormat:@"%g", slider.value];
            break;
        }
        case DemoEntryChoice: {
            cell.detailTextLabel.text = [DemoConfig stringFor:entry.key];
            cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
            cell.selectionStyle = UITableViewCellSelectionStyleDefault;
            break;
        }
        case DemoEntryAction: {
            cell.textLabel.textColor = [UIColor systemBlueColor];
            cell.selectionStyle = UITableViewCellSelectionStyleDefault;
            break;
        }
    }
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    DemoEntry *entry = self.sections[indexPath.section][indexPath.row];
    [tableView deselectRowAtIndexPath:indexPath animated:YES];

    if (entry.kind == DemoEntryChoice) {
        // Cycle rather than push a picker: two or three values per knob, and one tap is faster.
        NSString *current = [DemoConfig stringFor:entry.key];
        NSUInteger index = [entry.choices indexOfObject:current];
        index = (index == NSNotFound) ? 0 : (index + 1) % entry.choices.count;
        [DemoConfig setValue:entry.choices[index] forKey:entry.key];
        [tableView reloadRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationNone];
        [self applyEntry:entry];
    } else if (entry.kind == DemoEntryAction) {
        [DemoTests run:entry.action mapView:self.mapView];
    }
}

- (void)toggleChanged:(UISwitch *)sender {
    DemoEntry *entry = getEntry(sender);
    [DemoConfig setValue:@(sender.isOn) forKey:entry.key];
    [self applyEntry:entry];
}

- (void)sliderChanged:(UISlider *)sender {
    DemoEntry *entry = getEntry(sender);
    [DemoConfig setValue:@(sender.value) forKey:entry.key];
    [self.tableView reloadData];
    [self applyEntry:entry];
}

@end
