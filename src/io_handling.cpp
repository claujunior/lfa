#include "io_handling.hpp"

Logger::Logger(const string &fname) {
    out.open(fname);
    if (!out) throw runtime_error("Não foi possível criar log em " + fname);
}
void Logger::snapshot(const string &title, const Grammar &G) {
    out << "==== [" << title << "] ====\n";
    out << grammar_to_string(G) << "\n\n";
}
void Logger::info(const string &s) {
    out << s << "\n";
}

// Tolerant: accepts accents, multiple lines, automatic additions with warnings.
/// @brief Robust parser for the format with blocks: Variaveis = {...}, Alfabeto = {...}, Inicial = X, Regras: A -> A01B | & 
/// @param filename File to read from
/// @param G Grammar object to populate
void read_grammar(const string &filename, Grammar &G)
{
    ifstream in(filename);
    if (!in) throw runtime_error("Não foi possível abrir " + filename);
    vector<string> lines;
    string line;
    while (getline(in, line)) {
        auto p = line.find('#');
        if (p != string::npos) line = line.substr(0, p);
        lines.push_back(line);
    }
    in.close();

    auto find_line_idx = [&](const string &key)->int {
        string keyl = to_lower_copy(key);
        for (int i = 0; i < (int)lines.size(); ++i) {
            if (to_lower_copy(lines[i]).find(keyl) != string::npos) return i;
        }
        return -1;
    };

    // 1) Variaveis
    int idx = find_line_idx("variave");
    if (idx == -1) throw runtime_error("Formato inválido: linha 'Variaveis' não encontrada.");
    string collected;
    auto posBrace = lines[idx].find('{');
    if (posBrace != string::npos) {
        collected = lines[idx].substr(posBrace+1);
        int j = idx;
        bool found = (lines[idx].find('}') != string::npos);
        while (!found && ++j < (int)lines.size()) {
            collected += " " + lines[j];
            if (lines[j].find('}') != string::npos) { found = true; break; }
        }
        auto rb = collected.find('}');
        if (rb != string::npos) collected = collected.substr(0, rb);
    } else {
        int j = idx+1; bool started=false;
        for (; j < (int)lines.size(); ++j) {
            if (!started) {
                if (lines[j].find('{') != string::npos) {
                    started=true; auto p = lines[j].find('{'); collected = lines[j].substr(p+1);
                    if (lines[j].find('}') != string::npos) { auto rb = collected.find('}'); if (rb!=string::npos) collected = collected.substr(0,rb); break; }
                }
            } else {
                collected += " " + lines[j];
                if (lines[j].find('}') != string::npos) { break; }
            }
        }
        if (!started) throw runtime_error("Formato inválido em 'Variaveis': chaves não encontradas.");
        auto rb = collected.find('}');
        if (rb != string::npos) collected = collected.substr(0, rb);
    }
    vector<string> vars;
    split_tokens_list(collected, vars);
    if (vars.empty()) {
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

    // 3) Inicial
    idx = find_line_idx("inicial");
    if (idx == -1) idx = find_line_idx("start");
    if (idx == -1) throw runtime_error("Formato inválido: linha 'Inicial' não encontrada.");
    {
        string ln = lines[idx];
        auto eq = ln.find('=');
        if (eq == string::npos) {
            // talvez "Inicial : S" ou "Inicial S"
            auto pos = ln.find_first_of(" \t", (int)ln.find_first_not_of(" \t"));
            string after = ln.substr(pos);
            string token = trim(after);
            if (token.empty()) throw runtime_error("Formato inválido na linha Inicial");
            G.S = token;
        } else {
            string after = ln.substr(eq+1);
            G.S = trim(after);
        }
        if (!G.V.count(G.S)) {
            cerr << "Aviso: símbolo inicial '" << G.S << "' não estava na lista de Variaveis — adicionando automaticamente.\n";
            G.V.insert(G.S);
        }
    }

    // 4) Regras:
    idx = find_line_idx("regras");
    if (idx == -1) idx = find_line_idx("regra");
    if (idx == -1) throw runtime_error("Formato inválido: seção 'Regras' não encontrada.");

    for (int i = idx+1; i < (int)lines.size(); ++i) {
        string ln = trim(lines[i]);
        if (ln.empty()) continue;
        auto arrow = ln.find("->");
        if (arrow == string::npos) continue;
        string lhs = trim(ln.substr(0, arrow));
        string rhsall = trim(ln.substr(arrow+2));
        if (lhs.empty() || rhsall.empty()) continue;
        if (!G.V.count(lhs)) {
            cerr << "Aviso: LHS '" << lhs << "' não estava em Variaveis — adicionando automaticamente.\n";
            G.V.insert(lhs);
        }
        // split alternatives by '|'
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
            RHS r;
            size_t p = 0;
            while (p < alt.size()) {
                bool matched = false;
                // match variables longest-first
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
                // try terminals longest-first
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
                // else single character as terminal (if present in alphabet, or add it)
                string t(1, alt[p]);
                if (!G.T.count(t)) {
                    cerr << "Aviso: símbolo '" << t << "' não estava em Alfabeto — adicionando automaticamente.\n";
                    G.T.insert(t);
                }
                r.push_back(t);
                p++;
            }
            G.P[lhs].push_back(r);
        }
    }
}


//// @brief Convert a Grammar object to its string representation pretty-printed.
/// @param G Grammar to convert to string.
/// @return String representation of the grammar.
string grammar_to_string(const Grammar &G) {
    ostringstream oss;
    oss << "Start: " << G.S << "\n";
    // iterate V in sorted order for determinism
    vector<string> vars(G.V.begin(), G.V.end());
    sort(vars.begin(), vars.end());
    for (auto &A : vars) {
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
                    if(G.isTerminal(rhs[i])) oss << '\'' << rhs[i] << '\'';
                    else oss << rhs[i];
                }
            }
        }
        oss << "\n";
    }
    return oss.str();
}
