#import "DemoSky.h"
#import "DemoConfig.h"

@implementation DemoSky

/** Hours from solar noon, signed, wrapped to [-12, 12]. */
static float hourAngle(float hour) {
    float delta = hour - 12.0f;
    while (delta > 12.0f) { delta -= 24.0f; }
    while (delta < -12.0f) { delta += 24.0f; }
    return delta;
}

+ (float)sunAltitudeForHour:(float)hour latitude:(double)latitude {
    // Peak altitude at noon falls off with latitude; a cosine over the day is close enough to
    // give a believable sunrise, midday and sunset without an ephemeris.
    float peak = (float)(90.0 - fabs(latitude) * 0.75);
    float t = hourAngle(hour) / 6.0f;   // ±1 at 06:00 and 18:00
    return peak * cosf(t * (float)M_PI_2);
}

+ (float)sunAzimuthForHour:(float)hour latitude:(double)latitude {
    // East at sunrise, south at noon (northern hemisphere), west at sunset.
    float noonAzimuth = latitude >= 0 ? 180.0f : 0.0f;
    return fmodf(noonAzimuth + hourAngle(hour) * 15.0f + 360.0f, 360.0f);
}

+ (NTColor *)colorWithRed:(int)red green:(int)green blue:(int)blue {
    return [[NTColor alloc] initWithR:red g:green b:blue a:255];
}

+ (void)applyDayCycle:(NTMapView *)mapView hour:(float)hour {
    NTOptions *options = [mapView getOptions];
    NTProjection *projection = [options getBaseProjection];
    NTMapPos *centre = [projection toWgs84:[mapView getFocusPos]];
    double latitude = [centre getY];

    float altitude = [self sunAltitudeForHour:hour latitude:latitude];
    float azimuth = [self sunAzimuthForHour:hour latitude:latitude];

    NTLightOptions *light = [[NTLightOptions alloc] init];
    [light setSunAzimuth:azimuth];
    [light setSunAltitude:altitude];
    // Dim towards and past the horizon rather than cutting out, so dusk reads as dusk.
    float daylight = fmaxf(0.0f, fminf(1.0f, (altitude + 6.0f) / 20.0f));
    [light setSunIntensity:[DemoConfig floatFor:@"sunIntensity"] * daylight];
    [light setAmbientIntensity:[DemoConfig floatFor:@"ambient"] * (0.25f + 0.75f * daylight)];
    [light setShadowStrength:[DemoConfig floatFor:@"shadow"] * daylight];
    [options setLightOptions:light];

    NTSkyOptions *sky = [options getSkyOptions];
    [sky setEnabled:[DemoConfig boolFor:@"sky"]];
    if (daylight <= 0.0f) {
        [sky setSkyColor:[self colorWithRed:12 green:16 blue:34]];
        [sky setHorizonColor:[self colorWithRed:30 green:36 blue:60]];
    } else if (daylight < 0.45f) {
        // Low sun: warm horizon under a still-blue zenith.
        [sky setSkyColor:[self colorWithRed:70 green:96 blue:150]];
        [sky setHorizonColor:[self colorWithRed:226 green:140 blue:88]];
    } else {
        [sky setSkyColor:[self colorWithRed:96 green:152 blue:214]];
        [sky setHorizonColor:[self colorWithRed:196 green:216 blue:236]];
    }
    [options setSkyOptions:sky];
}

@end
