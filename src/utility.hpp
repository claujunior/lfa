#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>
#include <vector>


using namespace std;

string trim(const string &s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
string to_lower_copy(const string &s) {
    string t = s; for (auto &c : t) c = (char)tolower((unsigned char)c); return t;
}
void split_tokens_list(const string &inside, vector<string> &out) {
    string cur;
    for (size_t i = 0; i < inside.size(); ++i) {
        char c = inside[i];
        if (c == ',' ) {
            string t = trim(cur);
            if (!t.empty()) out.push_back(t);
            cur.clear();
        } else cur.push_back(c);
    }
    string t = trim(cur);
    if (!t.empty()) out.push_back(t);
}

#endif