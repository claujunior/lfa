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


// Logger
struct Logger {
    ofstream out;
    Logger(const string &fname) { out.open(fname); if (!out) throw runtime_error("Não foi possível criar log"); }
    void snapshot(const string &title, const Grammar &G) {
        out << "==== [" << title << "] ====\n";
        out << grammar_to_string(G) << "\n";
    }
    void info(const string &s) { out << s << "\n"; }
};


void read_grammar(const string &filename, Grammar &G);
string grammar_to_string(const Grammar &G);


#endif