/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_CANCELABLETASK_H_
#define _MASSIF_CANCELABLETASK_H_

#include "components/Task.h"

#include <atomic>

namespace massif {

    class CancelableTask : public Task {
    public:
        virtual ~CancelableTask() { }
    
        bool isCanceled() const {
            return _canceled.load();
        }
    
        virtual void cancel() {
            _canceled.store(true);
        }
    
    protected:
        CancelableTask() :
            _canceled(false)
        {
        }

    private:
        std::atomic<bool> _canceled;
    };
    
}

#endif
