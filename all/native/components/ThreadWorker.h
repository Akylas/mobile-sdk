/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_THREADWORKER_H_
#define _MASSIF_THREADWORKER_H_

namespace massif {

    class ThreadWorker {
    public:
        virtual ~ThreadWorker() { }
    
        virtual void operator()() = 0;
    
    protected:
        ThreadWorker() { }
    };
    
}

#endif
