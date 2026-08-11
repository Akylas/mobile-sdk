#import "DemoCelestial.h"
#import "DemoConfig.h"
#import "DemoSky.h"
// Fork additions, not listed in the umbrella header.
#import "NTCelestialLayer.h"
#import "NTCelestialObject.h"
#import "NTCelestialSprite.h"
#import "NTCelestialArc.h"

@implementation DemoCelestial

static NTCelestialLayer *sLayer = nil;

+ (void)removeFromMapView:(NTMapView *)mapView {
    if (sLayer) {
        [[mapView getLayers] remove:sLayer];
        sLayer = nil;
    }
}

+ (NTColor *)colorWithRed:(int)red green:(int)green blue:(int)blue alpha:(int)alpha {
    return [[NTColor alloc] initWithR:red g:green b:blue a:alpha];
}

+ (void)applyToMapView:(NTMapView *)mapView {
    [self removeFromMapView:mapView];
    if (![DemoConfig boolFor:@"celestial"] && ![DemoConfig boolFor:@"stars"]) {
        return;
    }

    NTProjection *projection = [[mapView getOptions] getBaseProjection];
    NTMapPos *centre = [projection toWgs84:[mapView getFocusPos]];
    double latitude = [centre getY];
    float hour = [DemoConfig boolFor:@"daycycle"] ? [DemoConfig floatFor:@"dayCycleHour"] : 12.0f;

    sLayer = [[NTCelestialLayer alloc] init];

    if ([DemoConfig boolFor:@"celestial"]) {
        [self addSunAndMoon:sLayer hour:hour latitude:latitude];
    }
    if ([DemoConfig boolFor:@"stars"]) {
        [self addStars:sLayer latitude:latitude];
    }
    [[mapView getLayers] add:sLayer];
}

+ (void)addSunAndMoon:(NTCelestialLayer *)layer hour:(float)hour latitude:(double)latitude {
    if ([DemoConfig boolFor:@"celestialSun"]) {
        NTCelestialSprite *sun = [[NTCelestialSprite alloc] init];
        [sun setAngularSize:[DemoConfig floatFor:@"celestialSunSize"]];
        [sun setColor:[self colorWithRed:255 green:238 blue:170 alpha:255]];
        [sun setSoftness:0.35f];
        [sun setDirection:[DemoSky sunAzimuthForHour:hour latitude:latitude]
                 altitude:[DemoSky sunAltitudeForHour:hour latitude:latitude]
                 distance:0];
        [layer add:sun];
    }

    if ([DemoConfig boolFor:@"celestialMoon"]) {
        NTCelestialSprite *moon = [[NTCelestialSprite alloc] init];
        [moon setAngularSize:[DemoConfig floatFor:@"celestialMoonSize"]];
        [moon setColor:[self colorWithRed:235 green:235 blue:225 alpha:255]];
        [moon setSoftness:0.2f];
        // Roughly opposite the sun, which is close enough for a full moon and enough to see the
        // arc and the sprite behave.
        [moon setDirection:[DemoSky sunAzimuthForHour:fmodf(hour + 12.0f, 24.0f) latitude:latitude]
                  altitude:[DemoSky sunAltitudeForHour:fmodf(hour + 12.0f, 24.0f) latitude:latitude]
                  distance:0];
        [layer add:moon];
    }

    // The day arc: the path the sun walks across the sky, as a circle on the dome. A circle about
    // the celestial pole is exactly what a day's motion is, so this is the arc rather than a
    // sampled polyline.
    if ([DemoConfig boolFor:@"celestialArc"]) {
        NTCelestialArc *arc = [[NTCelestialArc alloc] init];
        [arc setCircle:(latitude >= 0 ? 0.0f : 180.0f)
          axisAltitude:(float)fabs(latitude)
                radius:90.0f];
        [arc setWidth:[DemoConfig floatFor:@"celestialArcWidth"]];
        [arc setColor:[self colorWithRed:255 green:210 blue:120 alpha:200]];
        [arc setBelowHorizonVisible:YES];
        [layer add:arc];
    }
    if ([DemoConfig boolFor:@"celestialMoonArc"]) {
        NTCelestialArc *arc = [[NTCelestialArc alloc] init];
        // The moon's path is tilted a few degrees off the sun's; enough to tell them apart.
        [arc setCircle:(latitude >= 0 ? 0.0f : 180.0f)
          axisAltitude:(float)fabs(latitude) + 5.0f
                radius:90.0f];
        [arc setWidth:[DemoConfig floatFor:@"celestialArcWidth"]];
        [arc setColor:[self colorWithRed:170 green:190 blue:230 alpha:170]];
        [arc setBelowHorizonVisible:YES];
        [layer add:arc];
    }
}

/**
 * The brightest stars, by azimuth/altitude at the map's latitude.
 *
 * The Android demo carries a real catalogue (DemoStarCatalogue, ~300 entries with RA/dec and
 * magnitudes) and converts through hour angle. This is a compact stand-in: enough bright stars
 * and figures to see the field render, size-by-magnitude work and the labels place, without
 * porting the catalogue wholesale. Positions are indicative, not astrometric.
 */
+ (void)addStars:(NTCelestialLayer *)layer latitude:(double)latitude {
    // name, azimuth, altitude, magnitude
    NSArray *catalogue = @[
        @[@"Polaris",   @0.0f,   @(latitude), @1.98f],
        @[@"Vega",      @70.0f,  @62.0f,  @0.03f],
        @[@"Deneb",     @45.0f,  @70.0f,  @1.25f],
        @[@"Altair",    @110.0f, @45.0f,  @0.77f],
        @[@"Arcturus",  @250.0f, @55.0f,  @(-0.05f)],
        @[@"Capella",   @330.0f, @48.0f,  @0.08f],
        @[@"Sirius",    @180.0f, @22.0f,  @(-1.46f)],
        @[@"Betelgeuse",@195.0f, @40.0f,  @0.50f],
        @[@"Rigel",     @186.0f, @28.0f,  @0.13f],
        @[@"Aldebaran", @215.0f, @52.0f,  @0.85f],
        @[@"Procyon",   @160.0f, @35.0f,  @0.34f],
        @[@"Spica",     @230.0f, @30.0f,  @0.97f],
        @[@"Antares",   @205.0f, @18.0f,  @1.09f],
        @[@"Pollux",    @145.0f, @58.0f,  @1.14f],
        @[@"Regulus",   @260.0f, @42.0f,  @1.35f],
    ];

    float brightest = [DemoConfig floatFor:@"starsSize"];
    float perMagnitude = 0.55f;
    float faintest = 1.4f;

    for (NSArray *star in catalogue) {
        float magnitude = [star[3] floatValue];
        // Brighter star, bigger dot: magnitudes run backwards, hence the subtraction.
        float size = fmaxf(faintest, brightest - (magnitude + 1.5f) * perMagnitude);

        NTCelestialSprite *sprite = [[NTCelestialSprite alloc] init];
        [sprite setScreenSize:size];
        [sprite setColor:[self colorWithRed:255 green:255 blue:245 alpha:230]];
        [sprite setSoftness:0.6f];
        [sprite setDirection:[star[1] floatValue] altitude:[star[2] floatValue] distance:0];
        if ([DemoConfig boolFor:@"starsLabels"]) {
            [sprite setMetaDataElement:@"name" element:[[NTVariant alloc] initWithString:star[0]]];
        }
        [layer add:sprite];
    }

    if ([DemoConfig boolFor:@"starsEquator"]) {
        // The celestial equator: a great circle 90 degrees from the pole.
        NTCelestialArc *equator = [[NTCelestialArc alloc] init];
        [equator setCircle:(latitude >= 0 ? 0.0f : 180.0f)
              axisAltitude:(float)fabs(latitude)
                    radius:90.0f];
        [equator setWidth:1.0f];
        [equator setColor:[self colorWithRed:120 green:170 blue:220 alpha:120]];
        [equator setBelowHorizonVisible:NO];
        [layer add:equator];
    }
}

@end
