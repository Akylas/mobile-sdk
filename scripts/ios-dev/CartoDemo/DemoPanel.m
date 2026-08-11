#import "DemoPanel.h"
#import "DemoConfig.h"
#import "DemoMap.h"
#import "DemoTests.h"
#import "DemoOrientation.h"
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
    DemoApplyCelestial,
    DemoApplyOrientation,
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

/** Matched against the label AND the config key, so searching "zoom" finds both. */
- (BOOL)matches:(NSString *)query {
    if (!query.length) {
        return YES;
    }
    return [self.label rangeOfString:query options:NSCaseInsensitiveSearch].location != NSNotFound
        || (self.key && [self.key rangeOfString:query options:NSCaseInsensitiveSearch].location != NSNotFound);
}

@end

@interface DemoSection : NSObject
@property (nonatomic, copy) NSString *title;
@property (nonatomic, copy) NSArray<DemoEntry *> *entries;
@property (nonatomic) BOOL collapsed;
@end

@implementation DemoSection
@end

@interface DemoPanel () <UITableViewDataSource, UITableViewDelegate, UISearchBarDelegate>
@property (nonatomic, weak) NTMapView *mapView;
@property (nonatomic, strong) NSArray<DemoSection *> *allSections;
/** Sections after the search filter, with only their surviving entries. */
@property (nonatomic, strong) NSArray<DemoSection *> *visibleSections;
@property (nonatomic, copy) NSString *query;
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) UISearchBar *searchBar;
@end

@implementation DemoPanel

- (instancetype)initWithMapView:(NTMapView *)mapView {
    if ((self = [super init])) {
        _mapView = mapView;
        _query = @"";
        [self buildEntries];
        // Everything but the layer list starts closed: the panel is long, and this is the section
        // you reach for first.
        for (NSUInteger i = 1; i < _allSections.count; i++) {
            _allSections[i].collapsed = YES;
        }
        [self applyFilter];
    }
    return self;
}

- (DemoSection *)section:(NSString *)title entries:(NSArray<DemoEntry *> *)entries {
    DemoSection *section = [DemoSection new];
    section.title = title;
    section.entries = entries;
    return section;
}

- (void)buildEntries {
    self.allSections = @[
        [self section:@"Layers" entries:@[
            [DemoEntry toggle:@"map" label:@"Base map" apply:DemoApplyLayers],
            [DemoEntry toggle:@"satellite" label:@"Satellite layer" apply:DemoApplyLayers],
            [DemoEntry toggle:@"hillshade" label:@"Hillshade layer" apply:DemoApplyLayers],
            [DemoEntry toggle:@"contourLayer" label:@"Contour layer (own layer)" apply:DemoApplyLayers],
            [DemoEntry toggle:@"contourTiles" label:@"Contour tiles" apply:DemoApplyLayers],
            [DemoEntry toggle:@"hypso" label:@"Hypsometric tint" apply:DemoApplyLayers],
            [DemoEntry toggle:@"elements" label:@"Vector elements" apply:DemoApplyLayers],
            [DemoEntry toggle:@"peaks" label:@"Peak callouts" apply:DemoApplyLayers],
        ]],
        [self section:@"Base map" entries:@[
            [DemoEntry choice:@"base" label:@"Mode" choices:@[@"composite", @"plain"] apply:DemoApplyLayers],
            [DemoEntry choice:@"style" label:@"Style source" choices:@[@"inline", @"zip"] apply:DemoApplyLayers],
            [DemoEntry toggle:@"hs" label:@"Composite hillshade" apply:DemoApplyLayers],
            [DemoEntry toggle:@"sat" label:@"Composite satellite" apply:DemoApplyLayers],
            [DemoEntry toggle:@"contour" label:@"Composite contours" apply:DemoApplyLayers],
            [DemoEntry toggle:@"singlePass" label:@"Single-pass rendering" apply:DemoApplyLayers],
            [DemoEntry toggle:@"labels" label:@"Labels" apply:DemoApplyLayers],
            [DemoEntry toggle:@"bld3d" label:@"3D buildings" apply:DemoApplyLayers],
            [DemoEntry toggle:@"minimal" label:@"Minimal style" apply:DemoApplyLayers],
            [DemoEntry slider:@"landcoverOpacity" label:@"Landcover opacity" min:0 max:1 apply:DemoApplyLayers],
            [DemoEntry slider:@"satZoom" label:@"Satellite min zoom" min:0 max:19 apply:DemoApplyLayers],
        ]],
        [self section:@"3D terrain" entries:@[
            [DemoEntry toggle:@"terrain" label:@"Enabled" apply:DemoApplyTerrain],
            [DemoEntry slider:@"exaggeration" label:@"Exaggeration" min:0 max:3 apply:DemoApplyTerrain],
            [DemoEntry slider:@"meshResolution" label:@"Mesh resolution" min:8 max:256 apply:DemoApplyTerrain],
            [DemoEntry toggle:@"stitch" label:@"Tile edge stitching" apply:DemoApplyTerrain],
            [DemoEntry toggle:@"seamlessEdges" label:@"Seamless tile edges" apply:DemoApplyTerrain],
            [DemoEntry toggle:@"prefetch" label:@"Elevation prefetch" apply:DemoApplyTerrain],
            [DemoEntry toggle:@"painterDepth" label:@"Painter-order depth" apply:DemoApplyTerrain],
        ]],
        [self section:@"Sun & sky" entries:@[
            [DemoEntry toggle:@"sky" label:@"Sky" apply:DemoApplyOptions],
            [DemoEntry toggle:@"daycycle" label:@"Day cycle" apply:DemoApplyLight],
            [DemoEntry slider:@"dayCycleHour" label:@"Hour" min:0 max:24 apply:DemoApplyLight],
            [DemoEntry toggle:@"terrainLight" label:@"Terrain lighting" apply:DemoApplyLight],
            [DemoEntry slider:@"sunAzimuth" label:@"Sun azimuth" min:0 max:360 apply:DemoApplyLight],
            [DemoEntry slider:@"sunAltitude" label:@"Sun altitude" min:-10 max:90 apply:DemoApplyLight],
            [DemoEntry slider:@"sunIntensity" label:@"Sun intensity" min:0 max:2 apply:DemoApplyLight],
            [DemoEntry slider:@"ambient" label:@"Ambient" min:0 max:2 apply:DemoApplyLight],
            [DemoEntry slider:@"shadow" label:@"Shadow strength" min:0 max:1 apply:DemoApplyLight],
            [DemoEntry slider:@"shadowSoftness" label:@"Shadow softness" min:0 max:4 apply:DemoApplyLight],
        ]],
        [self section:@"Fog & view distance" entries:@[
            [DemoEntry toggle:@"fog" label:@"Fog" apply:DemoApplyTerrain],
            [DemoEntry slider:@"fogStart" label:@"Fog start (m)" min:0 max:20000 apply:DemoApplyTerrain],
            [DemoEntry slider:@"fogDistance" label:@"Fog distance (m)" min:0 max:80000 apply:DemoApplyTerrain],
            [DemoEntry slider:@"viewDistance" label:@"View distance factor" min:0 max:4 apply:DemoApplyTerrain],
        ]],
        [self section:@"Hillshade" entries:@[
            [DemoEntry slider:@"hsContrast" label:@"Contrast" min:0 max:1 apply:DemoApplyLayers],
            [DemoEntry slider:@"hsHeightScale" label:@"Height scale" min:0 max:0.5 apply:DemoApplyLayers],
            [DemoEntry slider:@"hsIllumination" label:@"Illumination" min:0 max:360 apply:DemoApplyLayers],
            [DemoEntry slider:@"hsBias" label:@"Composite zoom bias" min:-2 max:2 apply:DemoApplyLayers],
            [DemoEntry toggle:@"hsContours" label:@"Shader contour lines" apply:DemoApplyLayers],
            [DemoEntry toggle:@"slopes" label:@"Slope-angle bands" apply:DemoApplyLayers],
            [DemoEntry slider:@"hsContourInterval" label:@"Shader interval (m)" min:10 max:500 apply:DemoApplyLayers],
        ]],
        [self section:@"Contours" entries:@[
            [DemoEntry slider:@"contourInterval" label:@"Base interval" min:5 max:200 apply:DemoApplyLayers],
            [DemoEntry slider:@"contourMinZoom" label:@"Min zoom" min:1 max:16 apply:DemoApplyLayers],
            [DemoEntry toggle:@"contourStubs" label:@"Label stubs" apply:DemoApplyLayers],
            [DemoEntry slider:@"contourStubInterval" label:@"Stub interval" min:0 max:500 apply:DemoApplyLayers],
        ]],
        [self section:@"Peak finder & relief" entries:@[
            [DemoEntry toggle:@"peakfinder" label:@"Peak finder camera" apply:DemoApplyCamera],
            [DemoEntry slider:@"peakFinderTilt" label:@"Tilt (low = panorama)" min:0 max:90 apply:DemoApplyCamera],
            [DemoEntry toggle:@"reliefSurface" label:@"Relief surface" apply:DemoApplyTerrain],
            [DemoEntry toggle:@"reliefDark" label:@"Dark palette" apply:DemoApplyTerrain],
            [DemoEntry slider:@"reliefShade" label:@"Shade strength" min:0 max:1 apply:DemoApplyTerrain],
            [DemoEntry slider:@"reliefAmbient" label:@"Ambient" min:0 max:1 apply:DemoApplyTerrain],
            [DemoEntry slider:@"reliefHaze" label:@"Haze" min:0 max:1 apply:DemoApplyTerrain],
        ]],
        [self section:@"Celestial & stars" entries:@[
            [DemoEntry toggle:@"celestial" label:@"Sun & moon" apply:DemoApplyCelestial],
            [DemoEntry toggle:@"celestialArc" label:@"Sun arc" apply:DemoApplyCelestial],
            [DemoEntry toggle:@"celestialMoonArc" label:@"Moon arc" apply:DemoApplyCelestial],
            [DemoEntry slider:@"celestialSunSize" label:@"Sun size" min:0.5 max:10 apply:DemoApplyCelestial],
            [DemoEntry toggle:@"stars" label:@"Stars" apply:DemoApplyCelestial],
            [DemoEntry slider:@"starsSize" label:@"Brightest star size" min:1 max:12 apply:DemoApplyCelestial],
            [DemoEntry toggle:@"starsLabels" label:@"Star labels" apply:DemoApplyCelestial],
            [DemoEntry toggle:@"starsEquator" label:@"Celestial equator" apply:DemoApplyCelestial],
        ]],
        [self section:@"Free roam" entries:@[
            [DemoEntry choice:@"freeRoam" label:@"Mode" choices:@[@"off", @"on"] apply:DemoApplyCamera],
            [DemoEntry slider:@"lookUp" label:@"Look-up limit" min:0 max:90 apply:DemoApplyCamera],
            [DemoEntry toggle:@"orientation" label:@"Follow device heading" apply:DemoApplyOrientation],
        ]],
        [self section:@"Camera" entries:@[
            [DemoEntry slider:@"zoom" label:@"Zoom" min:1 max:20 apply:DemoApplyCamera],
            [DemoEntry slider:@"tilt" label:@"Tilt" min:0 max:90 apply:DemoApplyCamera],
            [DemoEntry slider:@"rotation" label:@"Rotation" min:-180 max:180 apply:DemoApplyCamera],
        ]],
        [self section:@"Route & maneuvers" entries:@[
            [DemoEntry slider:@"routeWidth" label:@"Route width" min:1 max:30 apply:DemoApplyLayers],
            [DemoEntry slider:@"routeCaseWidth" label:@"Casing width" min:1 max:40 apply:DemoApplyLayers],
            [DemoEntry choice:@"routeJoin" label:@"Join" choices:@[@"round", @"miter", @"bevel"] apply:DemoApplyLayers],
            [DemoEntry choice:@"routeCap" label:@"Cap" choices:@[@"round", @"square", @"butt"] apply:DemoApplyLayers],
            [DemoEntry slider:@"routeOpacity" label:@"Opacity" min:0 max:1 apply:DemoApplyLayers],
        ]],
        [self section:@"Actions" entries:@[
            [DemoEntry action:@"route" label:@"Route test (GeoJSON)"],
            [DemoEntry action:@"maneuvers" label:@"Maneuver arrows"],
            [DemoEntry action:@"geojsonBench" label:@"GeoJSON benchmark"],
            [DemoEntry action:@"search" label:@"Search test"],
            [DemoEntry action:@"clear" label:@"Clear test layers"],
        ]],
    ];
}

- (void)applyFilter {
    if (!self.query.length) {
        self.visibleSections = self.allSections;
        return;
    }
    // A search flattens the accordion: matching rows show whatever their section's state, since a
    // hit inside a closed section would otherwise look like no hit at all.
    NSMutableArray<DemoSection *> *filtered = [NSMutableArray array];
    for (DemoSection *section in self.allSections) {
        NSMutableArray<DemoEntry *> *matches = [NSMutableArray array];
        for (DemoEntry *entry in section.entries) {
            if ([entry matches:self.query]) {
                [matches addObject:entry];
            }
        }
        if (matches.count) {
            DemoSection *copy = [self section:section.title entries:matches];
            [filtered addObject:copy];
        }
    }
    self.visibleSections = filtered;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor systemBackgroundColor];

    self.searchBar = [[UISearchBar alloc] init];
    self.searchBar.placeholder = @"Search settings";
    self.searchBar.delegate = self;
    self.searchBar.searchBarStyle = UISearchBarStyleMinimal;
    self.searchBar.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.searchBar];

    self.tableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleInsetGrouped];
    self.tableView.translatesAutoresizingMaskIntoConstraints = NO;
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    self.tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
    [self.view addSubview:self.tableView];

    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [self.searchBar.topAnchor constraintEqualToAnchor:safe.topAnchor constant:8],
        [self.searchBar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.searchBar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.tableView.topAnchor constraintEqualToAnchor:self.searchBar.bottomAnchor],
        [self.tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.tableView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.tableView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    ]];
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
        case DemoApplyCelestial: [DemoMap applyCelestial:mapView]; break;
        case DemoApplyOrientation:
            [DemoOrientation setFollowing:[DemoConfig boolFor:@"orientation"] mapView:mapView];
            break;
    }
}

// ---- search ----

- (void)searchBar:(UISearchBar *)searchBar textDidChange:(NSString *)text {
    self.query = text;
    [self applyFilter];
    [self.tableView reloadData];
}

- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar {
    [searchBar resignFirstResponder];
}

// ---- table ----

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return self.visibleSections.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    DemoSection *demoSection = self.visibleSections[section];
    return demoSection.collapsed ? 0 : demoSection.entries.count;
}

- (UIView *)tableView:(UITableView *)tableView viewForHeaderInSection:(NSInteger)section {
    DemoSection *demoSection = self.visibleSections[section];

    UITableViewHeaderFooterView *header = [[UITableViewHeaderFooterView alloc] initWithReuseIdentifier:nil];
    UIListContentConfiguration *content = [UIListContentConfiguration groupedHeaderConfiguration];
    content.text = [NSString stringWithFormat:@"%@  %@",
                    demoSection.collapsed ? @"▸" : @"▾", demoSection.title];
    header.contentConfiguration = content;

    header.tag = section;
    [header addGestureRecognizer:
        [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(headerTapped:)]];
    return header;
}

- (void)headerTapped:(UITapGestureRecognizer *)recognizer {
    NSInteger index = recognizer.view.tag;
    DemoSection *demoSection = self.visibleSections[index];
    demoSection.collapsed = !demoSection.collapsed;
    [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    DemoEntry *entry = self.visibleSections[indexPath.section].entries[indexPath.row];
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
            UISlider *slider = [[UISlider alloc] initWithFrame:CGRectMake(0, 0, 150, 30)];
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
    DemoEntry *entry = self.visibleSections[indexPath.section].entries[indexPath.row];
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
