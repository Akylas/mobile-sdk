#pragma once

#include <string>
#include <vector>

namespace routing {

    /**
     * Split a string by a delimiter character.
     */
    inline std::vector<std::string> splitKeys(const std::string& s, char delim = '.') {
        std::vector<std::string> result;
        std::string cur;
        for (char c : s) {
            if (c == delim) { result.push_back(cur); cur.clear(); }
            else cur += c;
        }
        result.push_back(cur);
        return result;
    }

} // namespace routing
