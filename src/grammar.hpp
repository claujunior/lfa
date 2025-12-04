#ifndef GRAMMAR_HPP
#define GRAMMAR_HPP

#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

using Symbol = string;
using RHS = vector<Symbol>;
using Productions = map<Symbol, vector<RHS>>;




struct Grammar {
    set<Symbol> V; // nao-terminais
    set<Symbol> T; // terminais
    Symbol S; // start
    Productions P;
};

#endif