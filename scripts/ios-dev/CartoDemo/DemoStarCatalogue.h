#import <Foundation/Foundation.h>

/**
 * The bright-star catalogue and the constellation figures the star demo draws. The counterpart of
 * scripts/android-dev's DemoStarCatalogue.java, with the same entries in the same order.
 *
 * DATA, not code. Positions are J2000 right ascension (hours) and declination (degrees) with the
 * visual magnitude, for every star down to about magnitude 3 plus the fainter ones a figure needs.
 * Precession from J2000 to today is about a third of a degree and is not applied - a star is drawn
 * a few pixels across, so it does not show.
 *
 * A figure is a list of segments between star NAMES: a typo is then a missing line and a log
 * warning rather than a silently wrong sky.
 */
@interface DemoStarCatalogue : NSObject

/** "name|right ascension (hours)|declination (degrees)|visual magnitude" */
+ (NSArray<NSString *> *)stars;
/** Figure name -> flat list of star names, taken in pairs as segments. */
+ (NSDictionary<NSString *, NSArray<NSString *> *> *)figures;
/** The figure names in drawing order (a dictionary has none). */
+ (NSArray<NSString *> *)figureNames;

@end
