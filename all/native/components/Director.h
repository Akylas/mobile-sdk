/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_DIRECTOR_H_
#define _MASSIF_DIRECTOR_H_

namespace massif {

    class Director {
    public:
        virtual ~Director() { }
        
        virtual void retainDirector() = 0;
        virtual void releaseDirector() = 0;

        virtual void* getDirectorObject() const = 0;
    };

}

#endif
