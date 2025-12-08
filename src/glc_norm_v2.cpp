// glc_norm.cpp
// Compilar: g++ -std=c++17 -O2 glc_norm.cpp -o glc_norm
// Uso: ./glc_norm gramatica.txt [cnf|gnf] log.txt

#include <bits/stdc++.h>
#include "utility.hpp"
#include "grammar.hpp"
#include "io_handling.hpp"

using namespace std;


// Compute nullable set
static set<Symbol> compute_nullable(const Grammar &G) {
    set<Symbol> nullable;
    bool changed=true;
    while (changed) {
        changed=false;
        for (auto &A : G.V) {
            if (nullable.count(A)) continue;
            if (!G.P.count(A)) continue;
            for (auto &rhs : G.P.at(A)) /*precisa de at() pq é const*/ {
                if (rhs == RHS{"&"}) { nullable.insert(A); changed=true; break; }
                bool allnull=true;
                for (auto &X : rhs) {
                    if (G.isTerminal(X) || !nullable.count(X)) { allnull=false; break; }
                }
                if (allnull) { nullable.insert(A); changed=true; break; }
            }
        }
    }
    return nullable;
}

// Remove epsilon-productions (fixed, safe). Preserves language; introduces new start S0 if original start nullable.
static void remove_epsilon(Grammar &G, Logger &log) {
    log.info("Remoção de regras-ε: início.");
    auto nullable = compute_nullable(G);
    log.info("Variáveis nulas (nullable):");
    for (auto &x : nullable) log.info("  " + x);

    bool start_nullable = nullable.count(G.S);
    Symbol originalStart = G.S;
    if (start_nullable) {
        // create new start symbol S0 not colliding
        Symbol S0;
        int k = 0;
        do { S0 = originalStart + "_S0_" + to_string(++k); } while (G.V.count(S0));
        G.V.insert(S0);
        // add S0 -> originalStart and S0 -> &
        G.P[S0].push_back(RHS{ originalStart });
        G.P[S0].push_back(RHS{"&"});
        G.S = S0;
        log.info("Start era nullable: criado novo start '" + S0 + "' com " + S0 + "->" + originalStart + " e " + S0 + "->&");
    }

    // Make a copy of productions to iterate safely
    Productions oldP = G.P;
    Productions newP;

    // For each production A -> X1 X2 ... Xn (non-empty or empty)
    for (auto &A : G.V) {
        set<vector<Symbol>> accum;
        // ensure we process original productions only (if A had none, skip)
        if (!oldP.count(A)) continue;
        for (auto &rhs : oldP[A]) {
            if (rhs.empty()) {
                // explicit epsilon: we'll drop it unless it's for the current start symbol G.S (created above)
                continue;
            }
            // find positions that are nullable
            vector<int> nullablePos;
            for (size_t i=0;i<rhs.size();++i) {
                if (!G.isTerminal(rhs[i]) && nullable.count(rhs[i])) nullablePos.push_back((int)i);
            }
            // enumerate subsets of nullable positions
            int m = (int)nullablePos.size();
            int combos = 1 << m;
            for (int mask = 0; mask < combos; ++mask) {
                vector<Symbol> newrhs;
                for (size_t i = 0; i < rhs.size(); ++i) {
                    bool remove = false;
                    for (int j = 0; j < m; ++j) if ((mask>>j)&1) if ((int)i == nullablePos[j]) { remove = true; break; }
                    if (!remove) newrhs.push_back(rhs[i]);
                }
                // If newrhs becomes [A] (single symbol same as LHS), skip to avoid self unit-production A->A
                if (newrhs.size()==1 && newrhs[0] == A) continue;
                accum.insert(newrhs);
            }
        }
        // Copy accum into newP
        for (auto &v : accum) {
            newP[A].push_back(v);
        }
    }

    // Remove any explicit epsilon productions except the newly created start (if any)
    // (Note: if we created S0 it already has an epsilon production in G.P[S0] from above)
    // Overwrite G.P with newP for processed variables; keep productions for possible new start
    for (auto &A : G.V) {
        G.P[A].clear();
    }
    for (auto &pr : newP) {
        for (auto &rhs : pr.second) G.P[pr.first].push_back(rhs);
    }
    // ensure if we created S0, keep its epsilon (it is already registered)
    // if original grammar didn't have start nullable, we must ensure no variable has epsilon production
    // Remove empty RHS entries (epsilon) that may accidentally remain (shouldn't, but be safe)
    for (auto &A : G.V) {
        auto &vec = G.P[A];
        vec.erase(remove_if(vec.begin(), vec.end(), [](const RHS &r){ return r.empty(); }), vec.end());
    }

    log.info("Remoção de regras-ε: finalizada.");
    log.snapshot("Após remoção de ε-productions", G);
}

// Remove unit-productions A -> B (single nonterminal)
static void remove_unit_productions(Grammar &G, Logger &log) {
    log.info("Remoção de unit-productions: início.");
    // compute unit closures
    map<Symbol, set<Symbol>> closure;
    for (auto &A : G.V) {
        closure[A].insert(A);
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto &B : vector<Symbol>(closure[A].begin(), closure[A].end())) {
                if (!G.P.count(B)) continue;
                for (auto &rhs : G.P[B]) {
                    if (rhs.size() == 1 && !G.isTerminal(rhs[0])) {
                        if (!closure[A].count(rhs[0])) { closure[A].insert(rhs[0]); changed = true; }
                    }
                }
            }
        }
    }
    Productions newP;
    for (auto &A : G.V) {
        set<vector<Symbol>> acc;
        for (auto &B : closure[A]) {
            if (!G.P.count(B)) continue;
            for (auto &rhs : G.P[B]) {
                if (rhs.size() == 1 && !G.isTerminal(rhs[0])) continue;
                acc.insert(rhs);
            }
        }
        for (auto &rhs : acc) newP[A].push_back(rhs);
    }
    G.P = newP;
    log.info("Remoção de unit-productions: finalizada.");
    log.snapshot("Após remoção de unit-productions", G);
}

// Remove useless symbols (non-generating and non-reachable)
static void remove_useless_symbols(Grammar &G, Logger &log) {
    log.info("Remoção de símbolos inúteis: início.");
    // generating variables: those that derive a string of terminals
    set<Symbol> gen;
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto &A : G.V) {
            if (gen.count(A)) continue; // A já é gerador
            if (!G.P.count(A)) continue; // A não gera nada
            for (auto &rhs : G.P[A]) { 
                bool ok = true;
                for (auto &X : rhs) {
                    if (!G.isTerminal(X) && !gen.count(X)) {
                        ok = false;
                        break;
                    }
                }
                if (ok) { gen.insert(A); changed = true; break; }
            }
        }
    }
    log.info("Geradores:");
    for (auto &x : gen) log.info("  " + x);

    // remove productions that contain non-generating variables
    for (auto it = G.P.begin(); it != G.P.end();) {
        if (!gen.count(it->first)) { it = G.P.erase(it); continue; }
        auto &vec = it->second;
        vec.erase(remove_if(vec.begin(), vec.end(), [&](const RHS &r){
            for (auto &X : r) if (!G.isTerminal(X) && !gen.count(X)) return true;
            return false;
        }), vec.end());
        ++it;
    }
    // keep only V that are generating
    set<Symbol> newV;
    for (auto &x : gen) newV.insert(x);
    G.V = newV;    

    // reachable from start
    set<Symbol> reach;
    reach.insert(G.S);
    changed = true;

    while (changed) {
        changed = false;
        for (auto &A : vector<Symbol>(reach.begin(), reach.end())) {
            if (!G.P.count(A)) continue;
            for (auto &rhs : G.P[A]) {
                std::cout << "Analisando produção de " << A << ": " ;
                for (auto &s : rhs) std::cout << s << " \n";
                for (auto &X : rhs) {
                    if (!G.isTerminal(X) && !reach.count(X)) { reach.insert(X); changed = true; }
                }
            }
        }
    }
    log.info("Alcançáveis:");
    for (auto &x : reach) log.info("  " + x);

    // intersection
    set<Symbol> finalV;
    for (auto &x : G.V) if (reach.count(x)) finalV.insert(x);
    G.V = finalV;
    // remove productions with LHS not in V
    for (auto it = G.P.begin(); it != G.P.end();) {
        if (!G.V.count(it->first)) it = G.P.erase(it);
        else ++it;
    }

    log.info("Remoção de símbolos inúteis: finalizada.");
    log.snapshot("Após remoção de símbolos inúteis", G);
}

// Replace terminals in RHS length >=2 with fresh variables
static void replace_terminals_in_long_productions(Grammar &G, Logger &log) {
    log.info("Substituição de terminais em produções longas: início.");
    map<Symbol, Symbol> termVar;
    int cnt = 0;
    // collect productions to modify
    Productions newP = G.P;
    for (auto &A : vector<Symbol>(G.V.begin(), G.V.end())) {
        if (!G.P.count(A)) continue;
        for (auto &rhs : G.P[A]) {
            if (rhs.size() >= 2) {
                for (auto &X : rhs) {
                    if (G.isTerminal(X)) {
                        if (!termVar.count(X)) {
                            Symbol Vn;
                            do { Vn = "T_" + to_string(++cnt); } while (G.V.count(Vn));
                            G.V.insert(Vn);
                            termVar[X] = Vn;
                            newP[Vn].push_back(RHS{X});
                        }
                    }
                }
            }
        }
    }
    // apply replacements
    for (auto &A : vector<Symbol>(G.V.begin(), G.V.end())) {
        for (auto &rhs : newP[A]) {
            if (rhs.size() >= 2) {
                for (auto &X : rhs) if (G.isTerminal(X)) X = termVar[X];
            }
        }
    }
    G.P = newP;
    log.info("Substituição de terminais em produções longas: finalizada.");
    log.snapshot("Após substituição de terminais em produções longas", G);
}

// Binarize RHS length > 2
static void binarize(Grammar &G, Logger &log) {
    log.info("Binarização: início.");
    Productions newP;
    int cnt = 0;
    for (auto &A : vector<Symbol>(G.V.begin(), G.V.end())) {
        if (!G.P.count(A)) continue;
        for (auto &rhs : G.P[A]) {
            if (rhs.size() <= 2) {
                newP[A].push_back(rhs);
                continue;
            }
            // create chain
            // A -> X0 Y1
            // Y1 -> X1 Y2
            // ...
            // Yk -> Xk-1 Xk
            vector<Symbol> sym = rhs;
            Symbol current = A;
            for (size_t i = 0; i + 2 < sym.size(); ++i) {
                Symbol Yi;
                do { Yi = "N_" + to_string(++cnt); } while (G.V.count(Yi));
                G.V.insert(Yi);
                // production for current -> sym[i] Yi
                newP[current].push_back(RHS{ sym[i], Yi });
                current = Yi;
            }
            // last two
            size_t m = sym.size();
            newP[current].push_back(RHS{ sym[m-2], sym[m-1] });
        }
    }
    // also preserve productions for variables that didn't have entries in newP (ensure presence)
    for (auto &v : G.V) {
        if (newP.count(v) == 0) {
            // keep existing if any
            if (G.P.count(v)) {
                for (auto &rhs : G.P[v]) if (rhs.size() <= 2) newP[v].push_back(rhs);
            }
        }
    }
    G.P = newP;
    log.info("Binarização: finalizada.");
    log.snapshot("Após binarização (CNF-ready)", G);
}

// Convert to CNF
static void to_cnf(Grammar &G, Logger &log) {
    log.snapshot("Gramática original", G);
    remove_epsilon(G, log);
    remove_unit_productions(G, log);
    remove_useless_symbols(G, log);
    replace_terminals_in_long_productions(G, log);
    binarize(G, log);
    log.info("CNF: etapas concluídas.");
    log.snapshot("Gramática em (aproximação de) CNF", G);
}

// Minimal practical GNF attempt (kept simple)
static void to_gnf(Grammar &G, Logger &log) {
    log.snapshot("Gramática original", G);
    remove_epsilon(G, log);
    remove_unit_productions(G, log);
    remove_useless_symbols(G, log);
    replace_terminals_in_long_productions(G, log);

    log.info("Início do processo prático para GNF (Greibach Normal Form - Forma Normal de Greibach).\n");

    // order nonterminals
    vector<Symbol> order(G.V.begin(), G.V.end());
    auto itS = find(order.begin(), order.end(), G.S);
    if (itS != order.end()) rotate(order.begin(), itS, itS+1);
    log.info("Ordem de nao-terminais usada para GNF:");
    for (auto &x: order) log.info("  " + x);

    bool changed = true;
    int iter = 0;
    while (changed && iter < 2000) {
        ++iter;
        changed = false;
        for (size_t i=0;i<order.size();++i) {
            Symbol A = order[i];
            if (!G.P.count(A)) continue;
            vector<RHS> newRhs;
            for (auto &rhs : G.P[A]) {
                if (rhs.empty()) continue;
                Symbol first = rhs[0];
                // if first is nonterminal that appears BEFORE A in order, substitute
                auto pos = find(order.begin(), order.begin()+i, first);
                if (!G.isTerminal(first) && pos != order.begin()+i) {
                    // substitute first with its productions
                    if (!G.P.count(first)) { newRhs.push_back(rhs); continue; }
                    for (auto &beta : G.P[first]) {
                        RHS newr = beta;
                        newr.insert(newr.end(), rhs.begin()+1, rhs.end());
                        newRhs.push_back(newr);
                    }
                    changed = true;
                } else newRhs.push_back(rhs);
            }
            sort(newRhs.begin(), newRhs.end());
            newRhs.erase(unique(newRhs.begin(), newRhs.end()), newRhs.end());
            G.P[A] = newRhs;
        }
    }
    log.info("Substituições por ordem concluídas (iterações = " + to_string(iter) + ").");
    log.snapshot("Após substituições iniciais para GNF", G);

    // Eliminar recursão esquerda por variáveis A (prático)
    for (auto &A : order) {
        if (!G.P.count(A)) continue;
        vector<RHS> alpha, beta;
        for (auto &rhs : G.P[A]) {
            if (!rhs.empty() && rhs[0] == A) {
                RHS tail(rhs.begin()+1, rhs.end());
                alpha.push_back(tail);
            } else beta.push_back(rhs);
        }
        if (!alpha.empty()) {
            Symbol Aprime;
            int k = 0;
            do { Aprime = A + "_G" + to_string(++k); } while (G.V.count(Aprime));
            G.V.insert(Aprime);
            vector<RHS> newA, newAprime;
            for (auto &b : beta) {
                RHS nb = b;
                nb.push_back(Aprime);
                newA.push_back(nb);
            }
            for (auto &a : alpha) {
                RHS na = a;
                na.push_back(Aprime);
                newAprime.push_back(na);
            }
            newAprime.push_back(RHS()); // epsilon
            G.P[A] = newA;
            G.P[Aprime] = newAprime;
            log.info("Eliminada recursão esquerda para " + A + " introduzindo " + Aprime);
        }
    }
    log.snapshot("Após eliminação de recursão à esquerda (tentaiva)", G);

    // final check
    bool fail = false;
    for (auto &A : G.V) {
        for (auto &rhs : G.P[A]) {
            if (rhs.empty()) continue;
            if (!G.isTerminal(rhs[0])) {
                bool replaced = false;
                for (auto &x : G.P[rhs[0]]) if (!x.empty() && G.isTerminal(x[0])) replaced = true;
                if (!replaced) fail = true;
            }
        }
    }
    if (fail) log.info("ATENÇÃO: Não foi possível garantir que todas as produções iniciem por terminal automaticamente (caso complexo).");
    else log.info("Todas as produções começam por terminal: GNF alcançada (prático).");

    log.snapshot("Gramática pós-processada tentativa GNF", G);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Uso: " << argv[0] << " gramatica.txt [cnf|gnf] output_log.txt\n";
        return 1;
    }
    string infile = argv[1];
    string mode = argv[2];
    string logf = argv[3];
    Grammar G;
    read_grammar(infile, G);
    Logger logger(logf);
    if (mode == "cnf") {
        to_cnf(G, logger);
        logger.info("NORMALIZACAO: CNF finalizada.");
    } else if (mode == "gnf") {
        to_gnf(G, logger);
        logger.info("NORMALIZACAO: GNF (tentativa) finalizada. Revise o log.");
    } else {
        cerr << "Modo desconhecido: use cnf ou gnf\n";
        return 1;
    }
    logger.out.close();
    cout << "Processo finalizado. Log em: " << logf << "\n";

    return 0;
}
