// glc_norm.cpp
// Compilar: g++ -std=c++17 -O2 glc_norm.cpp -o glc_norm
// Uso: ./glc_norm gramatica.txt cnf output_log.txt
//      ./glc_norm gramatica.txt gnf output_log.txt

#include <bits/stdc++.h>
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

static bool isNonTerminal(const Symbol &s) {
    if (s.empty()) return false;
    // Convenção: começa com letra maiúscula = não-terminal
    return isupper((unsigned char)s[0]);
}

static vector<string> split_tokens(const string &s) {
    // tokens separados por espaços (trim)
    vector<string> out;
    string cur;
    for (size_t i=0;i<s.size();++i) {
        char c = s[i];
        if (isspace((unsigned char)c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else if (c == '|' || c == '-' || c == '>' ) {
            // keep punctuation as tokens only when '->' sequence; we'll handle
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            if (c == '|') out.push_back("|");
            else if (c == '-') {
                // expect "->"
                if (i+1 < s.size() && s[i+1] == '>') { out.push_back("->"); ++i; }
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Substitua sua função read_grammar por esta versão mais robusta:
static string to_lower_copy(const string &s) {
    string t = s; for (auto &c : t) c = (char)tolower((unsigned char)c); return t;
}
static string trim(const string &s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
static void split_tokens_list(const string &inside, vector<string> &out) {
    string cur;
    for (size_t i = 0; i < inside.size(); ++i) {
        char c = inside[i];
        if (c == ',' ) {
            string t = trim(cur);
            if (!t.empty()) out.push_back(t);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    string t = trim(cur);
    if (!t.empty()) out.push_back(t);
}

static void read_grammar(const string &filename, Grammar &G) {
    ifstream in(filename);
    if (!in) throw runtime_error("Não foi possível abrir " + filename);
    // ler todo arquivo
    vector<string> lines;
    string line;
    while (getline(in, line)) {
        // remover comentários após '#'
        auto p = line.find('#');
        if (p != string::npos) line = line.substr(0, p);
        lines.push_back(line);
    }
    in.close();

    // helpers para localizar seções (case-insensitive)
    auto find_line_idx = [&](const string &key)->int {
        string keyl = to_lower_copy(key);
        for (int i = 0; i < (int)lines.size(); ++i) {
            if (to_lower_copy(lines[i]).find(keyl) != string::npos) return i;
        }
        return -1;
    };

    // 1) Variaveis (aceita "Variaveis" ou "Variáveis")
    int idx = find_line_idx("variave");
    if (idx == -1) throw runtime_error("Formato inválido: linha 'Variaveis' não encontrada.");
    // coletar texto entre { ... } possivelmente em várias linhas
    string collected;
    auto posBrace = lines[idx].find('{');
    if (posBrace != string::npos) {
        // pegar da primeira linha depois da '{'
        collected = lines[idx].substr(posBrace+1);
        // procurar '}' em linhas seguintes se necessário
        int j = idx;
        bool found = (lines[idx].find('}') != string::npos);
        while (!found && ++j < (int)lines.size()) {
            collected += " " + lines[j];
            if (lines[j].find('}') != string::npos) { found = true; break; }
        }
        // agora extrair até '}'
        auto rb = collected.find('}');
        if (rb != string::npos) collected = collected.substr(0, rb);
    } else {
        // caso sem chaves na mesma linha, pegar a próxima linha que contenha '{'
        int j = idx+1;
        bool started = false, finished = false;
        for (; j < (int)lines.size(); ++j) {
            if (!started) {
                if (lines[j].find('{') != string::npos) {
                    started = true;
                    auto p = lines[j].find('{');
                    collected = lines[j].substr(p+1);
                    if (lines[j].find('}') != string::npos) { finished = true; auto rb = collected.find('}'); if (rb!=string::npos) collected = collected.substr(0,rb); break; }
                }
            } else {
                collected += " " + lines[j];
                if (lines[j].find('}') != string::npos) { finished = true; break; }
            }
        }
        if (!started) throw runtime_error("Formato inválido em 'Variaveis': chaves não encontradas.");
        // retirar tudo após '}'
        auto rb = collected.find('}');
        if (rb != string::npos) collected = collected.substr(0, rb);
    }
    // split por ',' ou espaços
    vector<string> vars;
    split_tokens_list(collected, vars);
    if (vars.empty()) {
        // tentar tokens por espaço
        stringstream ss(collected);
        string tok;
        while (ss >> tok) vars.push_back(tok);
    }
    for (auto &v : vars) {
        string t = trim(v);
        if (!t.empty()) G.V.insert(t);
    }

    // 2) Alfabeto
    idx = find_line_idx("alfabeto");
    if (idx == -1) throw runtime_error("Formato inválido: linha 'Alfabeto' não encontrada.");
    // extrair entre { }
    collected.clear();
    posBrace = lines[idx].find('{');
    if (posBrace != string::npos) {
        collected = lines[idx].substr(posBrace+1);
        int j = idx; bool found = (lines[idx].find('}')!=string::npos);
        while (!found && ++j < (int)lines.size()) { collected += " " + lines[j]; if (lines[j].find('}')!=string::npos) { found=true; break; } }
        auto rb = collected.find('}');
        if (rb != string::npos) collected = collected.substr(0, rb);
    } else {
        int j = idx+1; bool started=false;
        for (; j < (int)lines.size(); ++j) {
            if (!started) {
                if (lines[j].find('{')!=string::npos) { started=true; auto p=lines[j].find('{'); collected = lines[j].substr(p+1); if (lines[j].find('}')!=string::npos){ auto rb=collected.find('}'); collected = collected.substr(0,rb); break; } }
            } else {
                collected += " " + lines[j];
                if (lines[j].find('}')!=string::npos) break;
            }
        }
        if (!started) throw runtime_error("Formato inválido em 'Alfabeto': chaves não encontradas.");
        auto rb = collected.find('}'); if (rb!=string::npos) collected = collected.substr(0,rb);
    }
    vector<string> terms;
    split_tokens_list(collected, terms);
    if (terms.empty()) {
        stringstream ss(collected);
        string tk;
        while (ss >> tk) terms.push_back(tk);
    }
    for (auto &t : terms) {
        string s = trim(t);
        if (!s.empty()) G.T.insert(s);
    }

    // 3) Inicial (aceita "Inicial" ou "Start")
    idx = find_line_idx("inicial");
    if (idx == -1) idx = find_line_idx("start");
    if (idx == -1) throw runtime_error("Formato inválido: linha 'Inicial' não encontrada.");
    {
        string ln = lines[idx];
        auto eq = ln.find('=');
        if (eq == string::npos) {
            // talvez esteja no formato "Inicial : S" ou apenas "Inicial S"
            string after = ln.substr( ln.find_first_not_of(" \t", (int)ln.find_first_of(" \t")) );
            string token = trim(after);
            if (token.empty()) throw runtime_error("Formato inválido na linha Inicial");
            G.S = token;
        } else {
            string after = ln.substr(eq+1);
            G.S = trim(after);
        }
        // se inicial não estiver em V, não falhar: adicionar e avisar
        if (!G.V.count(G.S)) {
            cerr << "Aviso: símbolo inicial '" << G.S << "' não estava na lista de Variaveis — adicionando automaticamente.\n";
            G.V.insert(G.S);
        }
    }

    // 4) Regras:
    idx = find_line_idx("regras");
    if (idx == -1) {
        // talvez 'Regras:' esteja sem acento ou com 'Producoes'
        idx = find_line_idx("regra");
    }
    if (idx == -1) throw runtime_error("Formato inválido: seção 'Regras' não encontrada.");

    // ler linhas após 'Regras' até EOF
    for (int i = idx+1; i < (int)lines.size(); ++i) {
        string ln = trim(lines[i]);
        if (ln.empty()) continue;
        // procurar '->'
        auto arrow = ln.find("->");
        if (arrow == string::npos) continue; // pular linhas não-produção
        string lhs = trim(ln.substr(0, arrow));
        string rhsall = trim(ln.substr(arrow+2));
        if (lhs.empty() || rhsall.empty()) continue;
        // validar lhs: se não estiver em V, avisar e adicionar
        if (!G.V.count(lhs)) {
            cerr << "Aviso: LHS '" << lhs << "' não estava em Variaveis — adicionando automaticamente.\n";
            G.V.insert(lhs);
        }
        // rhs pode ser '|' separado? no seu formato cada produção é um RHS simples; se tiver alternância, tratar '|'
        vector<string> alts;
        string cur;
        for (size_t k = 0; k < rhsall.size(); ++k) {
            char c = rhsall[k];
            if (c == '|') { alts.push_back(trim(cur)); cur.clear(); }
            else cur.push_back(c);
        }
        if (!cur.empty()) alts.push_back(trim(cur));
        for (auto &alt : alts) {
            if (alt == "&") { G.P[lhs].push_back(RHS()); continue; }
            // agora precisamos tokenizar alt que pode ser tipo "A01BC" -> combinar variaveis e terminais
            RHS r;
            size_t p = 0;
            while (p < alt.size()) {
                bool matched = false;
                // tentar casar nome de variável mais longo primeiro
                // coletar variáveis em vetor ordenado por tamanho decrescente
                vector<string> vars_vec(G.V.begin(), G.V.end());
                sort(vars_vec.begin(), vars_vec.end(), [](const string &a, const string &b){ return a.size() > b.size(); });
                for (auto &v : vars_vec) {
                    if (alt.size() - p >= v.size() && alt.substr(p, v.size()) == v) {
                        r.push_back(v);
                        p += v.size();
                        matched = true;
                        break;
                    }
                }
                if (matched) continue;
                // senão, pegar símbolo terminal (1 caractere ou multi, tentar casar terminals mais longos)
                vector<string> terms_vec(G.T.begin(), G.T.end());
                sort(terms_vec.begin(), terms_vec.end(), [](const string &a, const string &b){ return a.size() > b.size(); });
                bool termMatched = false;
                for (auto &t : terms_vec) {
                    if (alt.size() - p >= t.size() && alt.substr(p, t.size()) == t) {
                        r.push_back(t);
                        p += t.size();
                        termMatched = true;
                        break;
                    }
                }
                if (termMatched) continue;
                // se chegou aqui, tentar aceitar caractere simples como terminal (e avisar)
                string t(1, alt[p]);
                if (G.T.count(t)) {
                    r.push_back(t); p++;
                } else {
                    // aceitar mesmo assim (adiciona ao alfabeto) e avisar
                    cerr << "Aviso: símbolo '" << t << "' não estava em Alfabeto — adicionando automaticamente.\n";
                    G.T.insert(t);
                    r.push_back(t);
                    p++;
                }
            } // fim tokenização RHS
            G.P[lhs].push_back(r);
        }
    }
}


// pretty print grammar
static string grammar_to_string(const Grammar &G) {
    ostringstream oss;
    oss << "Start: " << G.S << "\n";
    for (auto &A : G.V) {
        if (G.P.count(A)==0) continue;
        oss << A << " -> ";
        bool first = true;
        for (auto &rhs : G.P.at(A)) {
            if (!first) oss << " | ";
            first = false;
            if (rhs.empty()) oss << "&";
            else {
                for (size_t i=0;i<rhs.size();++i) {
                    if (i) oss << " ";
                    oss << rhs[i];
                }
            }
        }
        oss << "\n";
    }
    return oss.str();
}

// utilities to log
struct Logger {
    ofstream out;
    Logger(const string &fname) { out.open(fname); if (!out) throw runtime_error("Não foi possível criar log"); }
    void snapshot(const string &title, const Grammar &G) {
        out << "==== [" << title << "] ====\n";
        out << grammar_to_string(G) << "\n\n";
    }
    void info(const string &s) { out << s << "\n"; }
};

// Helper: compute nullable set (variavel que gera epsilon)
static set<Symbol> compute_nullable(const Grammar &G) {
    set<Symbol> nullable;
    bool changed=true;
    while (changed) {
        changed=false;
        for (auto &A : G.V) {
            if (nullable.count(A)) continue;
            for (auto &rhs : G.P.count(A)?G.P.at(A):vector<RHS>()) {
                if (rhs.empty()) { nullable.insert(A); changed=true; break; }
                bool allnull=true;
                for (auto &X : rhs) {
                    if (!isNonTerminal(X) || !nullable.count(X)) { allnull=false; break; }
                }
                if (allnull) { nullable.insert(A); changed=true; break; }
            }
        }
    }
    return nullable;
}

// Remove epsilon-productions (preservando linguagem, exceto possivel inclusão de S->& handled)
static void remove_epsilon(Grammar &G, Logger &log) {
    log.info("Remoção de regras-ε: início.");
    auto nullable = compute_nullable(G);
    log.info("Variáveis nulas (nullable):");
    for (auto &x : nullable) log.info("  " + x);
    Grammar H = G;
    // Para cada produção A -> X1 X2 ... Xn, gerar todas as combinações onde omitimos Xi que são nulos
    Productions newP;
    for (auto &A : G.V) {
        vector<RHS> newRHSs;
        for (auto &rhs : G.P.count(A)?G.P.at(A):vector<RHS>()) {
            size_t n = rhs.size();
            // for empty (epsilon) -> handled: skip epsilon here (we will remove it)
            if (n == 0) continue;
            // gerar 2^k subsets: mas apenas para símbolos não-terminais nulos
            vector<RHS> acc;
            acc.push_back(RHS());
            for (size_t i=0;i<n;++i) {
                Symbol X = rhs[i];
                vector<RHS> next;
                bool isNull = isNonTerminal(X) && nullable.count(X);
                for (auto &prefix : acc) {
                    // keep X
                    RHS withX = prefix; withX.push_back(X); next.push_back(withX);
                    // omit X if nullable
                    if (isNull) {
                        RHS withoutX = prefix; next.push_back(withoutX);
                    }
                }
                acc.swap(next);
            }
            // adicionar todos os acc que não são vazios (permitir vazios? apenas quando A era o start e epsilon original permitido)
            for (auto &r : acc) {
                // se r vazio, então geraremos epsilon — mas só se A != S (depois decidimos)
                newRHSs.push_back(r);
            }
        }
        // remover duplicates
        sort(newRHSs.begin(), newRHSs.end());
        newRHSs.erase(unique(newRHSs.begin(), newRHSs.end()), newRHSs.end());
        // se algum rhs ficou vazio, ele representa epsilon; guardamos
        newP[A] = newRHSs;
    }
    // agora, decidir sobre S->&: se S ou alguma variável pode gerar epsilon originalmente, então manter S->& explicitamente.
    bool originalGeneratesEps = false;
    if (G.P.count(G.S)) {
        for (auto &r : G.P.at(G.S)) if (r.empty()) originalGeneratesEps = true;
    }
    G.P = newP;
    // se originalmente S =>* ε, garantir que S -> & esteja presente (ou criar S0)
    if (originalGeneratesEps) {
        // garantir produção S -> & (RHS empty)
        if (G.P[G.S].end() == find_if(G.P[G.S].begin(), G.P[G.S].end(), [](const RHS& r){ return r.empty(); })) {
            G.P[G.S].push_back(RHS());
        }
    } else {
        // remover produções vazias (epsilon) se existirem
        for (auto &A : G.V) {
            auto &vec = G.P[A];
            vec.erase(remove_if(vec.begin(), vec.end(), [](const RHS &r){ return r.empty(); }), vec.end());
        }
    }
    log.info("Remoção de regras-ε: finalizada.");
    log.snapshot("Após remoção de ε-productions", G);
}

// Remove unit-productions A -> B
static void remove_unit_productions(Grammar &G, Logger &log) {
    log.info("Remoção de unit-productions: início.");
    // para cada A, compute conjunto de reachable via unit-productions
    map<Symbol, set<Symbol>> unit;
    for (auto &A : G.V) {
        unit[A].insert(A);
        bool changed=true;
        while (changed) {
            changed=false;
            for (auto &B : vector<Symbol>(unit[A].begin(), unit[A].end())) {
                if (G.P.count(B)==0) continue;
                for (auto &rhs : G.P[B]) {
                    if (rhs.size()==1 && isNonTerminal(rhs[0])) {
                        if (!unit[A].count(rhs[0])) { unit[A].insert(rhs[0]); changed=true; }
                    }
                }
            }
        }
    }
    Productions newP;
    for (auto &A : G.V) {
        vector<RHS> out;
        set<string> seen;
        for (auto &B : unit[A]) {
            if (G.P.count(B)==0) continue;
            for (auto &rhs : G.P[B]) {
                if (rhs.size()==1 && isNonTerminal(rhs[0])) continue; // unit, ignore
                if (seen.insert(grammar_to_string(Grammar{G.V,G.T,G.S,{{B,{rhs}}}})).second) {
                    out.push_back(rhs);
                }
            }
        }
        newP[A] = out;
    }
    G.P = newP;
    log.info("Remoção de unit-productions: finalizada.");
    log.snapshot("Após remoção de unit-productions", G);
}

// Remove symbols inúteis (não alcançáveis e não geradores)
static void remove_useless_symbols(Grammar &G, Logger &log) {
    log.info("Remoção de símbolos inúteis: início.");
    // primeiro: símbolos que geram uma cadeia de terminais (geradores)
    set<Symbol> gen;
    bool changed=true;
    while (changed) {
        changed=false;
        for (auto &A : G.V) {
            if (gen.count(A)) continue;
            for (auto &rhs : G.P.count(A)?G.P.at(A):vector<RHS>()) {
                bool ok=true;
                for (auto &X : rhs) {
                    if (isNonTerminal(X) && !gen.count(X)) { ok=false; break; }
                    if (!isNonTerminal(X)) { /* terminal ok */ }
                }
                if (ok) { gen.insert(A); changed=true; break; }
            }
        }
    }
    log.info("Geradores:"); for (auto &x:gen) log.info("  "+x);
    // remover produções com não-geradores
    for (auto it = G.P.begin(); it != G.P.end();) {
        if (!gen.count(it->first)) { it = G.P.erase(it); continue; }
        auto &vec = it->second;
        vec.erase(remove_if(vec.begin(), vec.end(), [&](const RHS &r){
            for (auto &X : r) if (isNonTerminal(X) && !gen.count(X)) return true;
            return false;
        }), vec.end());
        ++it;
    }
    // manter só V que são geradores
    set<Symbol> newV;
    for (auto &x : gen) newV.insert(x);
    G.V = newV;
    // agora: símbolos alcançáveis a partir de S
    set<Symbol> reach;
    reach.insert(G.S);
    changed=true;
    while (changed) {
        changed=false;
        for (auto &A : vector<Symbol>(reach.begin(), reach.end())) {
            if (G.P.count(A)==0) continue;
            for (auto &rhs : G.P.at(A)) {
                for (auto &X : rhs) {
                    if (isNonTerminal(X) && !reach.count(X)) { reach.insert(X); changed=true; }
                }
            }
        }
    }
    log.info("Alcançáveis:"); for (auto &x: reach) log.info("  "+x);
    // manter só V = reach ∩ gen
    set<Symbol> finalV;
    for (auto &x : G.V) if (reach.count(x)) finalV.insert(x);
    G.V = finalV;
    // remover produções com LHS não em V
    for (auto it = G.P.begin(); it != G.P.end();) {
        if (!G.V.count(it->first)) { it = G.P.erase(it); continue; }
        ++it;
    }
    log.info("Remoção de símbolos inúteis: finalizada.");
    log.snapshot("Após remoção de símbolos inúteis", G);
}

// Introduz variáveis para terminais quando aparecem em contextos com >=2 símbolos
static void replace_terminals_in_long_productions(Grammar &G, Logger &log) {
    log.info("Substituição de terminais em produções longas: início.");
    // criar map terminal -> variable
    map<Symbol, Symbol> termVar;
    int cnt = 0;
    for (auto &A : vector<Symbol>(G.V.begin(), G.V.end())) {
        // ensure counter starts distinct
    }
    for (auto &A : G.V) {
        for (auto &rhs : G.P[A]) {
            if (rhs.size() >= 2) {
                for (auto &X : rhs) {
                    if (!isNonTerminal(X)) {
                        if (!termVar.count(X)) {
                            // criar nova variavel
                            Symbol Vn;
                            do {
                                Vn = "T_" + to_string(++cnt);
                            } while (G.V.count(Vn));
                            G.V.insert(Vn);
                            termVar[X] = Vn;
                            // adicionar Vn -> terminal
                            G.P[Vn].push_back(RHS{X});
                        }
                    }
                }
            }
        }
    }
    // substituir nos rhs
    for (auto &A : G.V) {
        for (auto &rhs : G.P[A]) {
            if (rhs.size() >= 2) {
                for (auto &X : rhs) {
                    if (!isNonTerminal(X)) {
                        X = termVar[X];
                    }
                }
            }
        }
    }
    log.info("Substituição de terminais em produções longas: finalizada.");
    log.snapshot("Após substituição de terminais em produções longas", G);
}

// Binarização: transformar A -> X1 X2 X3 ... em A -> X1 Y1; Y1 -> X2 Y2; ... Yk-1 -> Xk Xk+1
static void binarize(Grammar &G, Logger &log) {
    log.info("Binarização: início.");
    int cnt = 0;
    Productions newP;
    for (auto &A : G.V) {
        for (auto &rhs : G.P[A]) {
            if (rhs.size() <= 2) {
                newP[A].push_back(rhs);
            } else {
                // criar cadeia de variaveis auxiliares
                Symbol prev = A;
                vector<Symbol> symbols = rhs;
                // para cada grupo produzir
                Symbol left = symbols[0];
                // criamos uma cadeia de novos variaveis: X1 X2 X3 X4 => A -> X1 Y1; Y1 -> X2 Y2; Y2 -> X3 X4
                Symbol currLHS = A;
                for (size_t i=0;i+2<symbols.size();++i) {
                    Symbol X = symbols[i];
                    Symbol Y;
                    // novo var
                    do { Y = "N_" + to_string(++cnt); } while (G.V.count(Y));
                    G.V.insert(Y);
                    newP[currLHS].push_back(RHS{ symbols[i], Y });
                    currLHS = Y;
                }
                // os dois ultimos
                size_t m = symbols.size();
                newP[currLHS].push_back(RHS{ symbols[m-2], symbols[m-1] });
            }
        }
    }
    // juntar com producoes para variaveis criadas dinamicamente (como as T_ and N_ já foram adicionadas)
    for (auto &A : G.V) {
        if (newP.count(A)) {
            // replace
            G.P[A] = newP[A];
        } else {
            // manter producoes (caso A seja auxiliado e tenha producoes originais)
            if (!G.P.count(A)) continue;
            // se G.P[A] tem todas com size <=2, já foi mantido
        }
    }
    log.info("Binarização: finalizada.");
    log.snapshot("Após binarização (CNF-ready)", G);
}

// Conversão completa para CNF
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

// --- GNF: tentativa prática (substituição seguindo ordenação) ---
static void to_gnf(Grammar &G, Logger &log) {
    log.snapshot("Gramática original", G);
    // pré-processo: remover epsilon, unit, inúteis
    remove_epsilon(G, log);
    remove_unit_productions(G, log);
    remove_useless_symbols(G, log);

    // order nonterminals (arbitrário: vetor)
    vector<Symbol> order(G.V.begin(), G.V.end());
    // garantir que start fica em primeiro
    auto itS = find(order.begin(), order.end(), G.S);
    if (itS != order.end()) { rotate(order.begin(), itS, itS+1); }

    log.info("Ordem de nao-terminais usada para GNF:");
    for (auto &x: order) log.info("  " + x);

    // iterativamente substituir produções que iniciam com non-terminal de menor índice
    // objetivo: para cada A, todas as producoes comecem com terminal
    bool changed = true;
    int iter = 0;
    while (changed && iter < 2000) {
        ++iter;
        changed = false;
        for (size_t i=0;i<order.size();++i) {
            Symbol A = order[i];
            vector<RHS> newRhs;
            for (auto &rhs : G.P[A]) {
                if (rhs.empty()) continue; // skip epsilon (should be removed)
                Symbol first = rhs[0];
                if (isNonTerminal(first) && find(order.begin(), order.begin()+i, first) != order.begin()+i) {
                    // first is a non-terminal that appears BEFORE A in the order -> substitute
                    // substituir first por suas producoes
                    for (auto &beta : G.P[first]) {
                        RHS newR = beta;
                        newR.insert(newR.end(), rhs.begin()+1, rhs.end());
                        newRhs.push_back(newR);
                    }
                    changed = true;
                } else {
                    newRhs.push_back(rhs);
                }
            }
            // simplifica e atualiza
            sort(newRhs.begin(), newRhs.end());
            newRhs.erase(unique(newRhs.begin(), newRhs.end()), newRhs.end());
            G.P[A] = newRhs;
        }
    }
    log.info("Substituições por ordem concluídas (iterações = " + to_string(iter) + ").");
    log.snapshot("Após substituições iniciais para GNF", G);

    // Agora: eliminar recursão à esquerda para cada A: producoes A -> A α  e A -> β
    for (auto &A : order) {
        vector<RHS> alpha; // producoes que comecam com A (recursivas)
        vector<RHS> beta;  // producoes que comecam com terminal ou com outra variavel diferente de A
        for (auto &rhs : G.P[A]) {
            if (!rhs.empty() && rhs[0] == A) {
                // extrair tail
                RHS tail(rhs.begin()+1, rhs.end());
                alpha.push_back(tail);
            } else {
                beta.push_back(rhs);
            }
        }
        if (!alpha.empty()) {
            // criar A' novo
            Symbol Aprime;
            int cnt = 0;
            do { Aprime = A + "_G" + to_string(++cnt); } while (G.V.count(Aprime));
            G.V.insert(Aprime);
            vector<RHS> newA, newAprime;
            // para cada beta: A -> beta Aprime
            for (auto &b : beta) {
                RHS nb = b;
                nb.push_back(Aprime);
                newA.push_back(nb);
            }
            // para cada alpha: Aprime -> alpha Aprime | epsilon
            for (auto &a : alpha) {
                RHS na = a;
                na.push_back(Aprime);
                newAprime.push_back(na);
            }
            // adicionar epsilon ao Aprime
            newAprime.push_back(RHS()); // epsilon
            G.P[A] = newA;
            G.P[Aprime] = newAprime;
            log.info("Eliminada recursão esquerda para " + A + " introduzindo " + Aprime);
        }
    }
    log.snapshot("Após eliminação de recursão à esquerda (tentaiva)", G);

    // Depois de tudo, garantir que todas producoes comecem por terminal: se alguma começar por variavel, tentamos substituir uma vez mais
    bool fail = false;
    for (auto &A : G.V) {
        for (auto &rhs : G.P[A]) {
            if (rhs.empty()) continue;
            if (isNonTerminal(rhs[0])) {
                // tentativa final de substituição: se rhs[0] tem producoes que iniciam por terminal, substituir
                bool can = true;
                bool replaced = false;
                for (auto &X : G.P[rhs[0]]) {
                    if (!X.empty() && !isNonTerminal(X[0])) { replaced = true; break; }
                }
                if (!replaced) { fail = true; }
            }
        }
    }
    if (fail) {
        log.info("ATENÇÃO: Não foi possível garantir que todas as produções iniciem por terminal automaticamente (caso complexo).");
    } else {
        log.info("Todas as produções começam por terminal: GNF alcançada (prático).");
    }
    log.snapshot("Gramática pós-processada tentativa GNF", G);
}

// Main
int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    try {
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
    } catch (exception &e) {
        cerr << "Erro: " << e.what() << "\n";
        return 2;
    }
    return 0;
}