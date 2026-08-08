/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CELESTIALSPRITE_H_
#define _CARTO_CELESTIALSPRITE_H_

#include "celestial/CelestialObject.h"

#include <memory>

namespace carto {
    class Bitmap;

    /**
     * A flat, camera-facing object in the sky: a disc, a point of light, an icon overhead.
     *
     * Its size is given either as an ANGULAR size in degrees - which is what a real body has, so it
     * grows and shrinks with the field of view like everything else in the world - or as a fixed
     * SCREEN size in pixels, which is what an icon or a marker wants. A sprite with no bitmap is
     * drawn as a soft disc in its own color, which is enough for most bodies and costs no
     * texture at all.
     */
    class CelestialSprite : public CelestialObject {
    public:
        CelestialSprite();
        virtual ~CelestialSprite();

        /**
         * Returns the angular size of the sprite.
         * @return The angular diameter in degrees, or 0 if the sprite is sized in pixels.
         */
        float getAngularSize() const;
        /**
         * Sets the angular size of the sprite, the way a real body is measured - the disc then
         * covers the same angle whatever the field of view.
         * @param degrees The angular diameter in degrees.
         */
        void setAngularSize(float degrees);

        /**
         * Returns the screen size of the sprite.
         * @return The diameter in pixels, or 0 if the sprite is sized by angle.
         */
        float getScreenSize() const;
        /**
         * Sets a fixed on-screen size, in pixels, independent of the field of view. Use this for
         * icons and markers rather than for bodies.
         * @param pixels The diameter in pixels.
         */
        void setScreenSize(float pixels);

        /**
         * Returns the bitmap of the sprite.
         * @return The bitmap, or null if the sprite is drawn as a plain disc.
         */
        std::shared_ptr<Bitmap> getBitmap() const;
        /**
         * Sets the bitmap of the sprite. Sprites are batched per bitmap, so objects that share one
         * bitmap - a catalogue of thousands, for instance - cost a single draw call.
         * @param bitmap The new bitmap, or null to draw a plain disc.
         */
        void setBitmap(const std::shared_ptr<Bitmap>& bitmap);

        /**
         * Returns the edge softness of a disc sprite.
         * @return The softness, 0 for a hard edge and 1 for a fully soft one.
         */
        float getSoftness() const;
        /**
         * Sets the edge softness of a disc sprite. It has no effect on a sprite with a bitmap.
         * @param softness The softness, 0 for a hard edge and 1 for a fully soft one.
         */
        void setSoftness(float softness);

        /**
         * Returns the extra radius that responds to a click.
         * @return The extra click radius in degrees.
         */
        float getClickRadius() const;
        /**
         * Sets an extra angular radius that responds to a click, added to the sprite's own size. A
         * sprite half a pixel across is impossible to hit otherwise.
         * @param degrees The extra click radius in degrees.
         */
        void setClickRadius(float degrees);

    private:
        float _angularSize;
        float _screenSize;
        std::shared_ptr<Bitmap> _bitmap;
        float _softness;
        float _clickRadius;
    };

}

#endif
