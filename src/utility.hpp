#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>
#include <vector>


using namespace std;

string trim(const string &s);

string to_lower_copy(const string &s);

void split_tokens_list(const string &inside, vector<string> &out);

#endif