/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_MVTVALUECONVERTER_H_
#define _MASSIF_MVTVALUECONVERTER_H_

#include "core/Variant.h"

#include <mapnikvt/Value.h>

namespace massif {

    struct MVTValueConverter {
        Variant operator() (std::monostate) const { return Variant(); }

        Variant operator() (const std::shared_ptr<const mvt::ValueArray>& val) const {
            std::vector<Variant> elements;
            if (val) {
                for (auto it = val->elements.begin(); it != val->elements.end(); it++) {
                    elements.push_back(std::visit(MVTValueConverter(), *it));
                }
            }
            return Variant(elements);
        }

        Variant operator() (const std::shared_ptr<const mvt::ValueObject>& val) const {
            std::map<std::string, Variant> members;
            if (val) {
                for (auto it = val->members.begin(); it != val->members.end(); it++) {
                    members[it->first] = std::visit(MVTValueConverter(), it->second);
                }
            }
            return Variant(members);
        }

        template <typename T> Variant operator() (T val) const { return Variant(val); }
    };

}

#endif
