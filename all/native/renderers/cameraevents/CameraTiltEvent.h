/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_CAMERATILTEVENT_H_
#define _MASSIF_CAMERATILTEVENT_H_

#include "renderers/cameraevents/CameraEvent.h"

namespace massif {

    class CameraTiltEvent : public CameraEvent {
    public:
        CameraTiltEvent();
        virtual ~CameraTiltEvent();
    
        bool isKeepRotation() const;
        void setKeepRotation(bool keepRotation);
    
        float getTilt() const;
        void setTilt(float tilt);
    
        float getTiltDelta() const;
        void setTiltDelta(float tiltDelta);
    
        bool isUseDelta() const;
    
        void calculate(Options& options, ViewState& viewState);

    private:
        bool _keepRotation;

        float _tilt;
    
        float _tiltDelta;
    
        bool _useDelta;
    };
    
}

#endif
