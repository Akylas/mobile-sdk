/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_TASK_H_
#define _MASSIF_TASK_H_

#include <memory>

namespace massif {

    class Task : public std::enable_shared_from_this<Task> {
    public:
        virtual ~Task() { }

        void operator()();

    protected:
        Task() { }

        virtual void run() = 0;
    };

}

#endif
