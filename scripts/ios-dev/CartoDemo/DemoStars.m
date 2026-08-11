#import "DemoStars.h"
#import "DemoAstro.h"
#import "DemoConfig.h"
#import "DemoStarCatalogue.h"
#import "DemoToast.h"
#import <UIKit/UIKit.h>
// Fork additions, not listed in the umbrella header.
#import "NTCelestialLayer.h"
#import "NTCelestialObject.h"
#import "NTCelestialSprite.h"
#import "NTCelestialArc.h"
#import "NTCelestialEventListener.h"

static NSString *const META_NAME = @"name";
static NSString *const META_INFO = @"info";

/** Reports what was tapped, exactly as the Android demo's Toast does. */
@interface DemoStarsListener : NTCelestialEventListener
@end

@implementation DemoStarsListener

- (BOOL)onCelestialObjectClicked:(NTClickInfo *)clickInfo celestialObject:(NTCelestialObject *)object {
    NSMutableString *message = [[[object getMetaDataElement:META_NAME] getString] mutableCopy];
    NSString *info = [[object getMetaDataElement:META_INFO] getString];
    if (info.length) {
        [message appendFormat:@"  %@", info];
    }
    if ([object isKindOfClass:[NTCelestialSprite class]]) {
        // Only a sprite HAS one direction - a curve is a set of them.
        [message appendFormat:@"   az %.0f° alt %.0f°", [object getAzimuth], [object getAltitude]];
    }
    [DemoToast show:message];
    return YES;
}

@end

@implementation DemoStars {
    NTCelestialLayer *_layer;
    NSMutableArray<NTCelestialSprite *> *_stars;
    NSMutableArray<NSValue *> *_starEquatorial;         // DemoEquatorial per star
    NSMutableArray<NTCelestialArc *> *_figures;
    NSMutableArray<NTCelestialSprite *> *_figureLabels;
    NSMutableArray<NSArray<NSValue *> *> *_figureEquatorial; // per figure, pairs of DemoEquatorial
    NSMutableArray<NTCelestialSprite *> *_planets;
    NTCelestialArc *_equator;
    DemoStarsListener *_listener;
    CGFloat _scale;
}

- (NTCelestialLayer *)createLayer:(NTMapView *)mapView {
    _layer = [[NTCelestialLayer alloc] init];
    // Drawn after any post-process effect (still depth-tested, so a path still goes behind the
    // ridges): the relief look is for the ground, not for the objects over it.
    [_layer setPostProcessed:NO];
    _stars = [NSMutableArray array];
    _starEquatorial = [NSMutableArray array];
    _figures = [NSMutableArray array];
    _figureLabels = [NSMutableArray array];
    _figureEquatorial = [NSMutableArray array];
    _planets = [NSMutableArray array];

    // The catalogue sizes are in points; the SDK's screen sizes are PIXELS, so they go through the
    // screen scale - the same thing the Android demo does with the display density. Without it a
    // 3x phone draws every star a third of the size it was meant to be.
    _scale = UIScreen.mainScreen.scale;

    NSMutableDictionary<NSString *, NSValue *> *byName = [NSMutableDictionary dictionary];
    for (NSString *entry in [DemoStarCatalogue stars]) {
        NSArray<NSString *> *parts = [entry componentsSeparatedByString:@"|"];
        if (parts.count != 4) {
            NSLog(@"CartoDemo: bad catalogue entry: %@", entry);
            continue;
        }
        NSString *name = parts[0];
        DemoEquatorial equatorial;
        equatorial.rightAscension = parts[1].doubleValue * 15.0;
        equatorial.declination = parts[2].doubleValue;
        double magnitude = parts[3].doubleValue;
        byName[name] = [NSValue valueWithBytes:&equatorial objCType:@encode(DemoEquatorial)];

        NTCelestialSprite *star = [[NTCelestialSprite alloc] init];
        // Brighter is bigger, the way a star chart draws them: a magnitude step is a factor of 2.5
        // in brightness, but on screen a linear size ramp reads better than the real one.
        [star setScreenSize:[self magnitudeToPixels:magnitude]];
        [star setColor:[[NTColor alloc] initWithR:255 g:255 b:250 a:255]];
        [star setSoftness:0.45f];
        [star setClickRadius:1.5f];
        [star setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:name]];
        [star setMetaDataElement:META_INFO
                         element:[[NTVariant alloc] initWithString:[NSString stringWithFormat:@"mag %@", parts[3]]]];
        [_layer add:star];
        [_stars addObject:star];
        [_starEquatorial addObject:byName[name]];
    }

    for (NSString *figureName in [DemoStarCatalogue figureNames]) {
        NSArray<NSString *> *names = [DemoStarCatalogue figures][figureName];
        NSMutableArray<NSValue *> *segments = [NSMutableArray array];
        for (NSUInteger i = 0; i + 1 < names.count; i += 2) {
            NSValue *from = byName[names[i]];
            NSValue *to = byName[names[i + 1]];
            if (!from || !to) {
                NSLog(@"CartoDemo: %@: no such star %@", figureName, from ? names[i + 1] : names[i]);
                continue;
            }
            [segments addObject:from];
            [segments addObject:to];
        }
        if (!segments.count) {
            continue;
        }

        NTCelestialArc *arc = [[NTCelestialArc alloc] init];
        [arc setColor:[[NTColor alloc] initWithR:120 g:170 b:255 a:110]];
        [arc setWidth:[DemoConfig floatFor:@"starsFigureWidth"] * _scale];
        [arc setBelowHorizonVisible:NO];
        [arc setClickRadius:[DemoConfig floatFor:@"starsFigureClickRadius"]];
        [arc setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:figureName]];
        [arc setMetaDataElement:META_INFO
                        element:[[NTVariant alloc] initWithString:
                                 [NSString stringWithFormat:@"%lu lines", (unsigned long)(segments.count / 2)]]];
        [_layer add:arc];
        [_figures addObject:arc];
        [_figureEquatorial addObject:segments];

        // The name, IN THE SKY, at the middle of the figure. It is a plain sprite with a bitmap the
        // demo paints - the SDK has no text of its own here, which is exactly what makes it
        // themeable from the app: change the paint, change the label.
        NTCelestialSprite *label = [[NTCelestialSprite alloc] init];
        NTBitmap *labelBitmap = [self textBitmap:figureName];
        [label setBitmap:labelBitmap];
        // The bitmap is square and the text fills its width, so drawing the quad at the bitmap's own
        // pixel size renders the text at the size it was painted - the same for every name. A fixed
        // quad size would shrink the long ones instead.
        [label setScreenSize:[labelBitmap getWidth] * [DemoConfig floatFor:@"starsLabelScale"]];
        [label setColor:[[NTColor alloc] initWithR:255 g:255 b:255
                                                 a:(int)roundf(255 * [DemoConfig floatFor:@"starsLabelOpacity"])]];
        [label setClickRadius:0]; // the figure itself is the clickable thing
        [label setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:figureName]];
        [label setMetaDataElement:META_INFO element:[[NTVariant alloc] initWithString:@""]];
        [_layer add:label];
        [_figureLabels addObject:label];
    }

    for (NSString *name in [DemoAstro planetNames]) {
        NTCelestialSprite *planet = [[NTCelestialSprite alloc] init];
        [planet setAngularSize:[DemoConfig floatFor:@"starsPlanetSize"]];
        [planet setColor:[self planetColor:name]];
        [planet setSoftness:0.4f];
        [planet setClickRadius:2.5f];
        [planet setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:name]];
        [planet setMetaDataElement:META_INFO element:[[NTVariant alloc] initWithString:@"planet"]];
        [_layer add:planet];
        [_planets addObject:planet];
    }

    // The celestial equator: the one line that makes the sky's rotation readable, and a circle about
    // the pole like the sun's daily path - the same object, radius 90 degrees.
    _equator = [[NTCelestialArc alloc] init];
    [_equator setColor:[[NTColor alloc] initWithR:90 g:200 b:190 a:90]];
    [_equator setWidth:1.5f];
    [_equator setBelowHorizonVisible:NO];
    [_equator setClickRadius:1.5f];
    [_equator setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:@"Celestial equator"]];
    [_equator setMetaDataElement:META_INFO element:[[NTVariant alloc] initWithString:@"declination 0"]];
    [_layer add:_equator];

    _listener = [[DemoStarsListener alloc] init];
    [_layer setCelestialEventListener:_listener];
    return _layer;
}

- (void)updateWithN:(double)n lat:(double)lat lon:(double)lon {
    if (!_layer) {
        return;
    }
    BOOL starsOn = [DemoConfig boolFor:@"starsStars"];
    for (NSUInteger i = 0; i < _stars.count; i++) {
        DemoEquatorial equatorial;
        [_starEquatorial[i] getValue:&equatorial];
        DemoHorizon horizon = [DemoAstro toHorizon:equatorial n:n lat:lat lon:lon];
        NTCelestialSprite *star = _stars[i];
        [star setDirection:horizon.azimuth altitude:horizon.altitude distance:0];
        // A star below the horizon is under the ground: not drawn, not clickable, not paid for.
        [star setVisible:starsOn && horizon.altitude > -2.0];
    }

    BOOL figuresOn = [DemoConfig boolFor:@"starsFigures"];
    BOOL labelsOn = [DemoConfig boolFor:@"starsLabels"];
    for (NSUInteger i = 0; i < _figures.count; i++) {
        NSArray<NSValue *> *segments = _figureEquatorial[i];
        NTDoubleVector *directions = [[NTDoubleVector alloc] init];
        for (NSValue *value in segments) {
            DemoEquatorial equatorial;
            [value getValue:&equatorial];
            DemoHorizon horizon = [DemoAstro toHorizon:equatorial n:n lat:lat lon:lon];
            [directions add:horizon.azimuth];
            [directions add:horizon.altitude];
        }
        [_figures[i] setSegments:directions];
        [_figures[i] setVisible:figuresOn];

        // The label goes at the MEAN DIRECTION of the figure, averaged as vectors: averaging
        // azimuths would put a figure straddling north somewhere near south.
        double x = 0, y = 0, z = 0;
        for (int k = 0; k + 1 < (int)[directions size]; k += 2) {
            double az = [directions get:k] * M_PI / 180.0;
            double alt = [directions get:k + 1] * M_PI / 180.0;
            x += cos(alt) * sin(az);
            y += cos(alt) * cos(az);
            z += sin(alt);
        }
        NTCelestialSprite *label = _figureLabels[i];
        double length = sqrt(x * x + y * y + z * z);
        if (length > 0) {
            double altitude = asin(z / length) * 180.0 / M_PI;
            double azimuth = [DemoAstro normalizeDegrees:atan2(x, y) * 180.0 / M_PI];
            [label setDirection:azimuth altitude:altitude distance:0];
            [label setVisible:figuresOn && labelsOn && altitude > 0];
        } else {
            [label setVisible:NO];
        }
    }

    BOOL planetsOn = [DemoConfig boolFor:@"starsPlanets"];
    for (NSUInteger i = 0; i < _planets.count; i++) {
        DemoHorizon horizon = [DemoAstro planetHorizon:(int)i n:n lat:lat lon:lon];
        [_planets[i] setDirection:horizon.azimuth altitude:horizon.altitude distance:0];
        [_planets[i] setVisible:planetsOn && horizon.altitude > -2.0];
    }

    // Declination 0 is 90 degrees from the pole, and the pole sits due north (south below the
    // equator) at an altitude equal to the latitude.
    [_equator setCircle:(lat >= 0 ? 0 : 180) axisAltitude:fabs(lat) radius:90];
    [_equator setVisible:[DemoConfig boolFor:@"starsEquator"]];
}

/**
 * The name painted into a square bitmap, which is what a celestial sprite draws. Square because
 * the sprite quad is: the text is centred in it and the transparent margin costs nothing but
 * texture.
 */
- (NTBitmap *)textBitmap:(NSString *)text {
    UIFont *font = [UIFont boldSystemFontOfSize:[DemoConfig floatFor:@"starsLabelTextSize"]];
    CGSize bounds = [text sizeWithAttributes:@{ NSFontAttributeName: font }];
    CGFloat side = MAX(16, MAX(bounds.width, bounds.height) + 8);

    UIGraphicsImageRendererFormat *format = [UIGraphicsImageRendererFormat defaultFormat];
    format.opaque = NO;
    format.scale = _scale;
    UIGraphicsImageRenderer *renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(side, side) format:format];
    UIImage *image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        CGPoint origin = CGPointMake((side - bounds.width) * 0.5, (side - bounds.height) * 0.5);
        // A dark outline, so a name stays readable over a bright sky as well as over black.
        [text drawAtPoint:origin withAttributes:@{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: [UIColor whiteColor],
            NSStrokeColorAttributeName: [UIColor colorWithWhite:0 alpha:0.75],
            // Negative: stroke AND fill, rather than stroke only.
            NSStrokeWidthAttributeName: @(-12),
        }];
    }];
    return [NTBitmapUtils createBitmapFromUIImage:image];
}

/** A star of this magnitude, in screen points. */
- (float)magnitudeToPixels:(double)magnitude {
    double size = [DemoConfig floatFor:@"starsSize"]
        - [DemoConfig floatFor:@"starsSizePerMagnitude"] * (magnitude + 1.5);
    return (float)(MAX([DemoConfig floatFor:@"starsFaintestSize"], size) * _scale);
}

- (NTColor *)planetColor:(NSString *)name {
    if ([name isEqualToString:@"Mars"]) {
        return [[NTColor alloc] initWithR:255 g:130 b:90 a:255];
    }
    if ([name isEqualToString:@"Venus"]) {
        return [[NTColor alloc] initWithR:255 g:250 b:220 a:255];
    }
    if ([name isEqualToString:@"Jupiter"]) {
        return [[NTColor alloc] initWithR:255 g:235 b:180 a:255];
    }
    if ([name isEqualToString:@"Saturn"]) {
        return [[NTColor alloc] initWithR:240 g:220 b:160 a:255];
    }
    return [[NTColor alloc] initWithR:220 g:220 b:230 a:255];
}

@end
