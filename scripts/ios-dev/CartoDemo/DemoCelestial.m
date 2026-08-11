#import "DemoCelestial.h"
#import "DemoAstro.h"
#import "DemoConfig.h"
#import "DemoToast.h"
#import <UIKit/UIKit.h>
// Fork additions, not listed in the umbrella header.
#import "NTCelestialLayer.h"
#import "NTCelestialObject.h"
#import "NTCelestialSprite.h"
#import "NTCelestialArc.h"
#import "NTCelestialEventListener.h"

static NSString *const META_NAME = @"name";
static const int MOON_BITMAP_SIZE = 128;
/** The sampling step of a daily path, in minutes. 10 is smooth at any field of view. */
static const int PATH_STEP_MINUTES = 10;

@interface DemoCelestialListener : NTCelestialEventListener
@end

@implementation DemoCelestialListener

- (BOOL)onCelestialObjectClicked:(NTClickInfo *)clickInfo celestialObject:(NTCelestialObject *)object {
    [DemoToast show:[NSString stringWithFormat:@"%@  az %.0f°  alt %.0f°",
                     [[object getMetaDataElement:META_NAME] getString],
                     [object getAzimuth], [object getAltitude]]];
    return YES;
}

@end

@implementation DemoCelestial {
    NTCelestialLayer *_layer;
    NTCelestialSprite *_sun;
    NTCelestialSprite *_moon;
    NTCelestialArc *_sunPath;
    NTCelestialArc *_moonPath;
    double _lastMoonPhase;
    DemoCelestialListener *_listener;
}

- (instancetype)init {
    if ((self = [super init])) {
        _lastMoonPhase = -1;
    }
    return self;
}

- (NTCelestialLayer *)createLayer:(NTMapView *)mapView {
    _layer = [[NTCelestialLayer alloc] init];
    // Drawn after any post-process effect (still depth-tested, so a path still goes behind the
    // ridges): the relief look is for the ground, not for the objects over it.
    [_layer setPostProcessed:NO];

    _sun = [[NTCelestialSprite alloc] init];
    [_sun setAngularSize:[DemoConfig floatFor:@"celestialSunSize"]];
    [_sun setColor:[[NTColor alloc] initWithR:255 g:244 b:214 a:255]];
    [_sun setSoftness:0.35f];
    [_sun setClickRadius:3];
    [_sun setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:@"Sun"]];

    _moon = [[NTCelestialSprite alloc] init];
    [_moon setAngularSize:[DemoConfig floatFor:@"celestialMoonSize"]];
    [_moon setColor:[[NTColor alloc] initWithR:245 g:245 b:235 a:255]];
    [_moon setSoftness:0.25f];
    [_moon setClickRadius:3];
    [_moon setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:@"Moon"]];

    _sunPath = [[NTCelestialArc alloc] init];
    [_sunPath setColor:[[NTColor alloc] initWithR:255 g:216 b:120 a:160]];
    [_sunPath setWidth:[DemoConfig floatFor:@"celestialArcWidth"]];
    [_sunPath setBelowHorizonVisible:NO];
    [_sunPath setClickRadius:2];
    [_sunPath setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:@"Sun path"]];

    _moonPath = [[NTCelestialArc alloc] init];
    [_moonPath setColor:[[NTColor alloc] initWithR:170 g:190 b:255 a:130]];
    [_moonPath setWidth:[DemoConfig floatFor:@"celestialArcWidth"]];
    [_moonPath setBelowHorizonVisible:NO];
    [_moonPath setClickRadius:2];
    [_moonPath setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:@"Moon path"]];

    [_layer add:_sun];
    [_layer add:_moon];
    [_layer add:_sunPath];
    [_layer add:_moonPath];

    _listener = [[DemoCelestialListener alloc] init];
    [_layer setCelestialEventListener:_listener];
    return _layer;
}

/**
 * Both bodies come from DemoAstro rather than from the light options, because the panel can drive
 * the sun's azimuth and altitude by hand: what is drawn here is always where the body really is on
 * that date, which is the only version of it whose daily arc means anything.
 */
- (void)update {
    if (!_layer) {
        return;
    }
    double hourUtc = [DemoConfig currentHourUtc];
    double lat = [DemoConfig doubleFor:@"lat"];
    double lon = [DemoConfig doubleFor:@"lon"];
    double n = [DemoAstro daysSinceJ2000WithYear:[DemoConfig intFor:@"sunYear"]
                                           month:[DemoConfig intFor:@"sunMonth"]
                                             day:[DemoConfig intFor:@"sunDay"]
                                            hour:hourUtc];

    DemoHorizon sun = [DemoAstro sunHorizon:n lat:lat lon:lon];
    [_sun setDirection:sun.azimuth altitude:sun.altitude distance:0];

    DemoHorizon moon = [DemoAstro moonHorizon:n lat:lat lon:lon];
    [_moon setDirection:moon.azimuth altitude:moon.altitude distance:0];
    [self updateMoonPhase:n];

    // The path across the day, sampled from the same ephemeris every PATH_STEP_MINUTES from
    // midnight to midnight. A circle about the celestial pole would be a good enough sun path
    // (declination barely moves in a day), but the moon's does move - a quarter of the sky in a day
    // - so both are sampled and the two arcs are then the same kind of object.
    [_sunPath setDirections:[self dailyPath:YES lat:lat lon:lon]];
    [_moonPath setDirections:[self dailyPath:NO lat:lat lon:lon]];

    [_sun setVisible:[DemoConfig boolFor:@"celestialSun"]];
    [_moon setVisible:[DemoConfig boolFor:@"celestialMoon"]];
    [_sunPath setVisible:[DemoConfig boolFor:@"celestialArc"]];
    [_moonPath setVisible:[DemoConfig boolFor:@"celestialMoonArc"]];
    NSLog(@"CartoDemo: sun az %.0f alt %.0f, moon az %.0f alt %.0f, hour %.2f UTC %d-%d-%d",
          sun.azimuth, sun.altitude, moon.azimuth, moon.altitude, hourUtc,
          [DemoConfig intFor:@"sunYear"], [DemoConfig intFor:@"sunMonth"], [DemoConfig intFor:@"sunDay"]);
}

/** The body's track over the configured date, as alternating azimuth/altitude degrees. */
- (NTDoubleVector *)dailyPath:(BOOL)isSun lat:(double)lat lon:(double)lon {
    NTDoubleVector *directions = [[NTDoubleVector alloc] init];
    for (int minute = 0; minute <= 24 * 60; minute += PATH_STEP_MINUTES) {
        double n = [DemoAstro daysSinceJ2000WithYear:[DemoConfig intFor:@"sunYear"]
                                               month:[DemoConfig intFor:@"sunMonth"]
                                                 day:[DemoConfig intFor:@"sunDay"]
                                                hour:minute / 60.0];
        DemoHorizon horizon = isSun ? [DemoAstro sunHorizon:n lat:lat lon:lon]
                                    : [DemoAstro moonHorizon:n lat:lat lon:lon];
        [directions add:horizon.azimuth];
        [directions add:horizon.altitude];
    }
    return directions;
}

/**
 * Draws the moon with the phase it really has: a disc with a bite taken out of it by a second
 * ellipse, which is what a terminator is - the projection of the circle dividing the lit and unlit
 * halves. Painting it into the sprite's bitmap keeps this out of the SDK entirely.
 */
- (void)updateMoonPhase:(double)n {
    CGPoint phase = [DemoAstro moonPhase:n];
    if (![DemoConfig boolFor:@"celestialMoonPhase"]) {
        if (_lastMoonPhase >= 0) {
            [_moon setBitmap:nil];
            _lastMoonPhase = -1;
        }
        return;
    }
    double illuminated = phase.x;
    double signedPhase = phase.y * illuminated;
    if (fabs(signedPhase - _lastMoonPhase) < 0.01) {
        return; // a hundredth of a phase is invisible; do not rebuild the texture for it
    }
    _lastMoonPhase = signedPhase;

    CGFloat side = MOON_BITMAP_SIZE;
    CGFloat radius = side * 0.5;
    // The terminator: an ellipse whose half-width goes from the full radius at new moon, through
    // zero at the quarter, to the full radius again at full moon - the same circle seen edge on.
    CGFloat terminator = (CGFloat)fabs(1.0 - 2.0 * illuminated) * (radius - 1);
    BOOL waxing = phase.y > 0;

    UIGraphicsImageRendererFormat *format = [UIGraphicsImageRendererFormat defaultFormat];
    format.opaque = NO;
    format.scale = 1;
    UIGraphicsImageRenderer *renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(side, side) format:format];
    UIImage *image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *rendererContext) {
        CGContextRef context = rendererContext.CGContext;
        UIColor *lit = [UIColor colorWithRed:245 / 255.0 green:245 / 255.0 blue:235 / 255.0 alpha:1];
        [lit setFill];
        CGContextFillEllipseInRect(context, CGRectMake(1, 1, side - 2, side - 2));

        // Waxing: lit on the side towards the sun, the western limb - kept on the right, so the
        // dark half is the left one.
        CGContextSetBlendMode(context, kCGBlendModeClear);
        CGContextFillRect(context, waxing ? CGRectMake(0, 0, radius, side) : CGRectMake(radius, 0, radius, side));

        CGRect terminatorOval = CGRectMake(radius - terminator, 1, terminator * 2, side - 2);
        if (illuminated < 0.5) {
            // Crescent: the terminator bulges INTO the lit half, so the ellipse erases as well.
            CGContextFillEllipseInRect(context, terminatorOval);
        } else {
            // Gibbous: it bulges into the dark half, so the ellipse paints the moon back in.
            CGContextSetBlendMode(context, kCGBlendModeNormal);
            [lit setFill];
            CGContextFillEllipseInRect(context, terminatorOval);
        }
    }];
    [_moon setBitmap:[NTBitmapUtils createBitmapFromUIImage:image]];
}

- (NTCelestialSprite *)addAircraft:(double)lon lat:(double)lat altitude:(double)altitudeMeters {
    NTCelestialSprite *aircraft = [[NTCelestialSprite alloc] init];
    [aircraft setScreenSize:24];
    [aircraft setColor:[[NTColor alloc] initWithR:255 g:255 b:255 a:255]];
    [aircraft setPosition:[[NTMapPos alloc] initWithX:lon y:lat] altitude:altitudeMeters];
    [aircraft setMetaDataElement:META_NAME element:[[NTVariant alloc] initWithString:@"Aircraft"]];
    [_layer add:aircraft];
    return aircraft;
}

@end
