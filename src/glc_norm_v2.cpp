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

    std::vector<std::string> vars;
    vars.reserve(G.P.size());
    for (auto &p : G.P) vars.push_back(p.first);

    if (vars.empty()) {
        log.info("Nenhum não-terminal encontrado. Nada a ordenar.\n");
        return;
    }

    // 2. Ordenar alfabeticamente (ABCD..., ou S,A,B,C se S vem primeiro)
    std::sort(vars.begin(), vars.end());

    log.info("Ordem escolhida para variáveis:");
    for (auto &v : vars) log.info("  " + v);
    log.info("");
    log.snapshot("Após order_variables (GNF)", G);

    bool changed = true;
    while (changed) {
        changed = false;

        for (auto &p : G.P) {
            Symbol A = p.first;
            auto &rhs_list = p.second;

            std::vector<RHS> new_list;

            for (auto &rhs : rhs_list) {
                if (rhs.empty()) {
                    // não deveria acontecer depois da eliminação de ε, mas ignoramos
                    new_list.push_back(rhs);
                    continue;
                }

                Symbol X = rhs[0];

                // se começa com variável, expandir
                if (!G.isTerminal(X)) {
                    changed = true;
                    for (auto &prod_of_X : G.P[X]) {
                        RHS expanded = prod_of_X;
                        expanded.insert(expanded.end(), rhs.begin() + 1, rhs.end());
                        new_list.push_back(expanded);
                    }
                } else {
                    new_list.push_back(rhs);
                }
            }

            rhs_list = std::move(new_list);
        }
    }
    log.snapshot("Após eliminação de prefixos variáveis (GNF)", G);
    log.info("GNF (tentativa): etapas concluídas.");
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
