#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

namespace {

constexpr int MAX_N = 64;
constexpr int MAX_WORDS = 32;

struct QMask {
    array<uint64_t, MAX_WORDS> w{};

    bool operator==(const QMask &other) const {
        return w == other.w;
    }
};

struct QMaskHash {
    int words;

    size_t operator()(const QMask &m) const {
        uint64_t h = 1469598103934665603ull;
        for (int i = 0; i < words; ++i) {
            uint64_t x = m.w[i];
            h ^= x;
            h *= 1099511628211ull;
            h ^= x >> 32;
            h *= 1099511628211ull;
        }
        return static_cast<size_t>(h);
    }
};

struct QMaskEq {
    int words;

    bool operator()(const QMask &a, const QMask &b) const {
        for (int i = 0; i < words; ++i) {
            if (a.w[i] != b.w[i]) return false;
        }
        return true;
    }
};

struct MISResult {
    int size = 0;
    uint64_t set = 0;
};

struct Seed {
    int n = 0;
    int product = 0;
    uint64_t a_mask = 0;
    uint64_t b_mask = 0;
};

string strip_quotes(string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

vector<string> split_csv_simple(const string &line) {
    vector<string> out;
    string cur;
    bool in_quote = false;
    for (char c : line) {
        if (c == '"') {
            in_quote = !in_quote;
            cur.push_back(c);
        } else if (c == ',' && !in_quote) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

uint64_t parse_mask_list(string text) {
    text = strip_quotes(text);
    uint64_t mask = 0;
    for (char &c : text) {
        if (c == '[' || c == ']') c = ' ';
    }
    stringstream ss(text);
    int x;
    while (ss >> x) {
        if (x >= 1 && x <= 64) mask |= 1ull << (x - 1);
    }
    return mask;
}

vector<Seed> read_seed_csv(const string &path) {
    vector<Seed> seeds;
    if (path.empty()) return seeds;
    ifstream in(path);
    if (!in) {
        cerr << "Could not open seed CSV: " << path << "\n";
        exit(1);
    }
    string line;
    getline(in, line);
    while (getline(in, line)) {
        if (line.empty()) continue;
        auto fields = split_csv_simple(line);
        if (fields.size() < 7) continue;
        Seed seed;
        seed.n = stoi(fields[0]);
        seed.product = stoi(fields[2]);
        seed.a_mask = parse_mask_list(fields[5]);
        seed.b_mask = parse_mask_list(fields[6]);
        seeds.push_back(seed);
    }
    return seeds;
}

struct Solver {
    int N = 0;
    int ratio_count = 0;
    int words = 0;
    vector<vector<int>> ratio_id;
    vector<array<uint64_t, MAX_N>> ratio_adj;
    unordered_map<QMask, MISResult, QMaskHash, QMaskEq> mis_cache;

    int best = 0;
    uint64_t best_a = 0;
    uint64_t best_b = 0;
    uint64_t dfs_nodes = 0;
    uint64_t mis_nodes = 0;
    chrono::steady_clock::time_point started;
    double time_limit_sec = 0.0;
    bool timed_out = false;

    explicit Solver(int n, double limit)
        : N(n),
          ratio_id(n + 1, vector<int>(n + 1, -1)),
          mis_cache(0, QMaskHash{1}, QMaskEq{1}),
          started(chrono::steady_clock::now()),
          time_limit_sec(limit) {
        build_ratios();
        words = (ratio_count + 63) / 64;
        mis_cache = unordered_map<QMask, MISResult, QMaskHash, QMaskEq>(
            0, QMaskHash{words}, QMaskEq{words});
    }

    static int popcount(uint64_t x) {
        return __builtin_popcountll(x);
    }

    bool check_timeout() {
        if (time_limit_sec <= 0) return false;
        if ((dfs_nodes & 8191ull) != 0) return false;
        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - started).count();
        if (elapsed > time_limit_sec) {
            timed_out = true;
            return true;
        }
        return false;
    }

    void set_bit(QMask &m, int bit) const {
        m.w[bit >> 6] |= 1ull << (bit & 63);
    }

    void or_inplace(QMask &a, const QMask &b) const {
        for (int i = 0; i < words; ++i) a.w[i] |= b.w[i];
    }

    void build_ratios() {
        unordered_map<string, int> id;
        for (int a = 1; a <= N; ++a) {
            for (int b = a + 1; b <= N; ++b) {
                int g = gcd(a, b);
                int num = b / g;
                int den = a / g;
                string key = to_string(num) + "/" + to_string(den);
                auto it = id.find(key);
                int idx;
                if (it == id.end()) {
                    idx = static_cast<int>(id.size());
                    id.emplace(key, idx);
                } else {
                    idx = it->second;
                }
                ratio_id[a][b] = ratio_id[b][a] = idx;
            }
        }
        ratio_count = static_cast<int>(id.size());
        if ((ratio_count + 63) / 64 > MAX_WORDS) {
            cerr << "Too many ratio classes for MAX_WORDS; lower N or raise MAX_WORDS.\n";
            exit(2);
        }
        ratio_adj.assign(ratio_count, {});
        for (int k = 0; k < ratio_count; ++k) {
            ratio_adj[k].fill(0);
        }
        for (int b1 = 1; b1 <= N; ++b1) {
            for (int b2 = b1 + 1; b2 <= N; ++b2) {
                int k = ratio_id[b1][b2];
                ratio_adj[k][b1 - 1] |= 1ull << (b2 - 1);
                ratio_adj[k][b2 - 1] |= 1ull << (b1 - 1);
            }
        }
    }

    array<uint64_t, MAX_N> build_adj(const QMask &q) const {
        array<uint64_t, MAX_N> adj{};
        adj.fill(0);
        for (int wi = 0; wi < words; ++wi) {
            uint64_t x = q.w[wi];
            while (x) {
                int bit = __builtin_ctzll(x);
                int k = wi * 64 + bit;
                x &= x - 1;
                if (k >= ratio_count) continue;
                for (int i = 0; i < N; ++i) {
                    adj[i] |= ratio_adj[k][i];
                }
            }
        }
        return adj;
    }

    uint64_t greedy_independent(const array<uint64_t, MAX_N> &adj, uint64_t cand) const {
        uint64_t result = 0;
        while (cand) {
            int best_v = -1;
            int best_deg = MAX_N + 1;
            uint64_t tmp = cand;
            while (tmp) {
                int v = __builtin_ctzll(tmp);
                tmp &= tmp - 1;
                int deg = popcount(adj[v] & cand);
                if (deg < best_deg) {
                    best_deg = deg;
                    best_v = v;
                }
            }
            uint64_t bit = 1ull << best_v;
            result |= bit;
            cand &= ~bit;
            cand &= ~adj[best_v];
        }
        return result;
    }

    MISResult max_independent_set(const QMask &q) {
        auto it = mis_cache.find(q);
        if (it != mis_cache.end()) return it->second;

        auto adj = build_adj(q);
        uint64_t all = (N == 64) ? ~0ull : ((1ull << N) - 1ull);
        uint64_t greedy = greedy_independent(adj, all);
        int best_size = popcount(greedy);
        uint64_t best_set = greedy;

        auto rec = [&](auto &&self, uint64_t cand, int cur_size, uint64_t cur_set) -> void {
            ++mis_nodes;
            if (cur_size + popcount(cand) <= best_size) return;
            if (!cand) {
                if (cur_size > best_size) {
                    best_size = cur_size;
                    best_set = cur_set;
                }
                return;
            }

            bool has_edge = false;
            uint64_t tmp = cand;
            while (tmp) {
                int v = __builtin_ctzll(tmp);
                tmp &= tmp - 1;
                if (adj[v] & cand) {
                    has_edge = true;
                    break;
                }
            }
            if (!has_edge) {
                int total = cur_size + popcount(cand);
                if (total > best_size) {
                    best_size = total;
                    best_set = cur_set | cand;
                }
                return;
            }

            int branch_v = -1;
            int branch_deg = -1;
            tmp = cand;
            while (tmp) {
                int v = __builtin_ctzll(tmp);
                tmp &= tmp - 1;
                int deg = popcount(adj[v] & cand);
                if (deg > branch_deg) {
                    branch_deg = deg;
                    branch_v = v;
                }
            }

            uint64_t bit = 1ull << branch_v;
            self(self, cand & ~bit & ~adj[branch_v], cur_size + 1, cur_set | bit);
            self(self, cand & ~bit, cur_size, cur_set);
        };

        rec(rec, all, 0, 0);
        MISResult result{best_size, best_set};
        mis_cache.emplace(q, result);
        return result;
    }

    vector<int> mask_to_vec(uint64_t mask) const {
        vector<int> out;
        for (int i = 0; i < N; ++i) {
            if ((mask >> i) & 1ull) out.push_back(i + 1);
        }
        return out;
    }

    string vec_to_string(const vector<int> &v) const {
        stringstream ss;
        ss << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) ss << " ";
            ss << v[i];
        }
        ss << "]";
        return ss.str();
    }

    void dfs(int next_a, vector<int> &chosen, const QMask &q, uint64_t a_mask) {
        if (timed_out) return;
        ++dfs_nodes;
        if (check_timeout()) return;

        MISResult b = max_independent_set(q);
        int a_size = static_cast<int>(chosen.size());
        int product = a_size * b.size;
        if (product > best) {
            best = product;
            best_a = a_mask;
            best_b = b.set;
        }

        int remaining = N - next_a + 1;
        int max_a_size = a_size + remaining;
        // Since the problem is symmetric in A and B, an optimum can be oriented
        // so that |A| >= |B|.  Within this subtree final |A| is at most
        // max_a_size, so final |B| never needs to exceed max_a_size either.
        if (max_a_size * min(b.size, max_a_size) <= best) return;
        if (next_a > N) return;

        QMask q_include = q;
        for (int a : chosen) {
            int rid = ratio_id[next_a][a];
            set_bit(q_include, rid);
        }

        chosen.push_back(next_a);
        dfs(next_a + 1, chosen, q_include, a_mask | (1ull << (next_a - 1)));
        chosen.pop_back();
        dfs(next_a + 1, chosen, q, a_mask);
    }

    bool verify_pair(uint64_t a_mask, uint64_t b_mask) const {
        unordered_map<int, pair<int, int>> seen;
        for (int a = 1; a <= N; ++a) {
            if (((a_mask >> (a - 1)) & 1ull) == 0) continue;
            for (int b = 1; b <= N; ++b) {
                if (((b_mask >> (b - 1)) & 1ull) == 0) continue;
                int prod = a * b;
                if (seen.count(prod)) return false;
                seen[prod] = {a, b};
            }
        }
        return true;
    }

    void solve() {
        vector<int> chosen;
        QMask empty;
        dfs(1, chosen, empty, 0);
        if (!timed_out && !verify_pair(best_a, best_b)) {
            cerr << "Internal verifier failed for N=" << N << "\n";
            exit(3);
        }
    }
};

string csv_escape(const string &s) {
    string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

}  // namespace

int main(int argc, char **argv) {
    int start_n = 1;
    int max_n = 20;
    double limit = 0.0;
    string out_path;
    string seed_csv_path;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--start-n" && i + 1 < argc) {
            start_n = stoi(argv[++i]);
        } else if (arg == "--max-n" && i + 1 < argc) {
            max_n = stoi(argv[++i]);
        } else if (arg == "--time-limit" && i + 1 < argc) {
            limit = stod(argv[++i]);
        } else if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "--seed-csv" && i + 1 < argc) {
            seed_csv_path = argv[++i];
        } else {
            cerr << "Usage: " << argv[0]
                 << " [--start-n N] [--max-n N] [--time-limit seconds-per-N]"
                 << " [--out table.csv] [--seed-csv table.csv]\n";
            return 1;
        }
    }

    if (max_n < 1 || max_n > MAX_N) {
        cerr << "--max-n must be between 1 and " << MAX_N << "\n";
        return 1;
    }
    if (start_n < 1 || start_n > max_n) {
        cerr << "--start-n must be between 1 and --max-n\n";
        return 1;
    }

    ostream *out = &cout;
    ofstream file;
    if (!out_path.empty()) {
        file.open(out_path);
        if (!file) {
            cerr << "Could not open output file: " << out_path << "\n";
            return 1;
        }
        out = &file;
    }

    vector<Seed> seeds = read_seed_csv(seed_csv_path);

    *out << "N,status,M,a_size,b_size,A,B,ratio_classes,dfs_nodes,mis_cache,mis_nodes,seconds\n";
    for (int n = start_n; n <= max_n; ++n) {
        auto t0 = chrono::steady_clock::now();
        Solver solver(n, limit);
        for (const auto &seed : seeds) {
            if (seed.n <= n && seed.product > solver.best &&
                solver.verify_pair(seed.a_mask, seed.b_mask)) {
                solver.best = seed.product;
                solver.best_a = seed.a_mask;
                solver.best_b = seed.b_mask;
            }
        }
        solver.solve();
        auto t1 = chrono::steady_clock::now();
        double seconds = chrono::duration<double>(t1 - t0).count();
        auto avec = solver.mask_to_vec(solver.best_a);
        auto bvec = solver.mask_to_vec(solver.best_b);
        string status = solver.timed_out ? "timeout_lower_bound" : "exact";
        *out << n << ","
             << status << ","
             << solver.best << ","
             << avec.size() << ","
             << bvec.size() << ","
             << csv_escape(solver.vec_to_string(avec)) << ","
             << csv_escape(solver.vec_to_string(bvec)) << ","
             << solver.ratio_count << ","
             << solver.dfs_nodes << ","
             << solver.mis_cache.size() << ","
             << solver.mis_nodes << ","
             << seconds << "\n";
        out->flush();
    }
    return 0;
}
