#ifndef IO_HANDLING_HPP
#define IO_HANDLING_HPP


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "utility.hpp"
#include "grammar.hpp"

using namespace std;



void read_grammar(const string &filename, Grammar &G);
string grammar_to_string(const Grammar &G);

// Logger
struct Logger {
    ofstream out;
    Logger(const string &fname);
    void snapshot(const string &title, const Grammar &G);
    void info(const string &s);
};


#endif