#include "utility.hpp"

std::string trim(const std::string &s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string to_lower_copy(const std::string &s) {
    std::string t = s; for (auto &c : t) c = (char)tolower((unsigned char)c); return t;
}
void split_tokens_list(const std::string &inside, std::vector<std::string> &out) {
    std::string cur;
    for (size_t i = 0; i < inside.size(); ++i) {
        char c = inside[i];
        if (c == ',' ) {
            std::string t = trim(cur);
            if (!t.empty()) out.push_back(t);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    std::string t = trim(cur);
    if (!t.empty()) out.push_back(t);
}