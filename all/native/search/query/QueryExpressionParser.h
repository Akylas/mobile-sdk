/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_QUERYEXPRESSIONPARSER_H_
#define _MASSIF_QUERYEXPRESSIONPARSER_H_

#include "search/query/QueryExpression.h"

#include <string>
#include <memory>

namespace massif {

    /**
     * Parser for SQL-like query expression.
     */
    class QueryExpressionParser {
    public:
        /**
         * Parse the query string and return corresponding parsed expression.
         * @param expr The string expression to parse.
         * @return The parsed query expression object.
         */
        static std::shared_ptr<QueryExpression> parse(const std::string& expr);

    private:
        QueryExpressionParser();
    };

}

#endif
