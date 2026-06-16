#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace {

constexpr int MAX_N = 64;
constexpr int MAX_WORDS = 32;

struct QMask {
    array<uint64_t, MAX_WORDS> w{};
    bool operator==(const QMask &other) const { return w == other.w; }
};

struct QMaskHash {
    int words;
    size_t operator()(const QMask &m) const {
        uint64_t h = 1469598103934665603ull;
        for (int i = 0; i < words; ++i) {
            uint64_t x = m.w[i];
            h ^= x;
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

struct Prover {
    int N;
    int target_a;
    int target_b;
    int ratio_count = 0;
    int words = 0;
    vector<vector<int>> ratio_id;
    vector<array<uint64_t, MAX_N>> ratio_adj;
    unordered_map<QMask, MISResult, QMaskHash, QMaskEq> mis_cache;
    uint64_t dfs_nodes = 0;
    uint64_t mis_nodes = 0;
    bool found = false;
    uint64_t found_a = 0;
    uint64_t found_b = 0;
    chrono::steady_clock::time_point started;
    double time_limit_sec;
    bool timed_out = false;
    string order_name;
    vector<int> order;

    Prover(int n, int a, int b, double limit, string order_kind)
        : N(n),
          target_a(a),
          target_b(b),
          ratio_id(n + 1, vector<int>(n + 1, -1)),
          mis_cache(0, QMaskHash{1}, QMaskEq{1}),
          started(chrono::steady_clock::now()),
          time_limit_sec(limit),
          order_name(std::move(order_kind)) {
        build_ratios();
        build_order();
        words = (ratio_count + 63) / 64;
        mis_cache = unordered_map<QMask, MISResult, QMaskHash, QMaskEq>(
            0, QMaskHash{words}, QMaskEq{words});
    }

    static int popcount(uint64_t x) { return __builtin_popcountll(x); }

    bool check_timeout() {
        if (time_limit_sec <= 0) return false;
        if ((dfs_nodes & 8191ull) != 0) return false;
        double elapsed = chrono::duration<double>(chrono::steady_clock::now() - started).count();
        if (elapsed > time_limit_sec) {
            timed_out = true;
            return true;
        }
        return false;
    }

    void set_bit(QMask &m, int bit) const { m.w[bit >> 6] |= 1ull << (bit & 63); }

    void build_ratios() {
        unordered_map<string, int> id;
        for (int a = 1; a <= N; ++a) {
            for (int b = a + 1; b <= N; ++b) {
                int g = gcd(a, b);
                string key = to_string(b / g) + "/" + to_string(a / g);
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
        ratio_adj.assign(ratio_count, {});
        for (int k = 0; k < ratio_count; ++k) ratio_adj[k].fill(0);
        for (int b1 = 1; b1 <= N; ++b1) {
            for (int b2 = b1 + 1; b2 <= N; ++b2) {
                int k = ratio_id[b1][b2];
                ratio_adj[k][b1 - 1] |= 1ull << (b2 - 1);
                ratio_adj[k][b2 - 1] |= 1ull << (b1 - 1);
            }
        }
    }

    void build_order() {
        order.resize(N);
        iota(order.begin(), order.end(), 1);
        if (order_name == "asc") return;
        if (order_name == "desc") {
            reverse(order.begin(), order.end());
            return;
        }
        if (order_name == "score") {
            vector<int> score(N + 1, 0);
            vector<int> seen(ratio_count, 0);
            for (int a = 1; a <= N; ++a) {
                int stamp = a;
                for (int b = 1; b <= N; ++b) {
                    if (a == b) continue;
                    int id = ratio_id[a][b];
                    if (seen[id] != stamp) {
                        seen[id] = stamp;
                        ++score[a];
                    }
                }
            }
            sort(order.begin(), order.end(), [&](int x, int y) {
                if (score[x] != score[y]) return score[x] > score[y];
                return x > y;
            });
            return;
        }
        cerr << "unknown order " << order_name << ", using asc\n";
        order_name = "asc";
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
                for (int i = 0; i < N; ++i) adj[i] |= ratio_adj[k][i];
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
        uint64_t best_set = greedy_independent(adj, all);
        int best_size = popcount(best_set);

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

    void dfs(int pos, vector<int> &chosen, const QMask &q, uint64_t a_mask) {
        if (found || timed_out) return;
        ++dfs_nodes;
        if (check_timeout()) return;

        int chosen_count = static_cast<int>(chosen.size());
        int remaining = N - pos;
        if (chosen_count > target_a) return;
        if (chosen_count + remaining < target_a) return;

        MISResult b = max_independent_set(q);
        if (b.size < target_b) return;
        if (chosen_count == target_a) {
            found = true;
            found_a = a_mask;
            found_b = b.set;
            return;
        }
        if (pos >= N) return;

        int next_a = order[pos];
        QMask include_q = q;
        for (int a : chosen) set_bit(include_q, ratio_id[next_a][a]);

        chosen.push_back(next_a);
        dfs(pos + 1, chosen, include_q, a_mask | (1ull << (next_a - 1)));
        chosen.pop_back();
        dfs(pos + 1, chosen, q, a_mask);
    }

    void run() {
        vector<int> chosen;
        QMask empty;
        dfs(0, chosen, empty, 0);
    }
};

}  // namespace

int main(int argc, char **argv) {
    int n = 34;
    int target_a = 16;
    int target_b = 15;
    double limit = 0.0;
    string order = "asc";
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--n" && i + 1 < argc) n = stoi(argv[++i]);
        else if (arg == "--a" && i + 1 < argc) target_a = stoi(argv[++i]);
        else if (arg == "--b" && i + 1 < argc) target_b = stoi(argv[++i]);
        else if (arg == "--time-limit" && i + 1 < argc) limit = stod(argv[++i]);
        else if (arg == "--order" && i + 1 < argc) order = argv[++i];
        else {
            cerr << "Usage: " << argv[0] << " --n N --a A --b B [--time-limit seconds] [--order asc|desc|score]\n";
            return 1;
        }
    }
    auto t0 = chrono::steady_clock::now();
    Prover prover(n, target_a, target_b, limit, order);
    prover.run();
    double seconds = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
    cout << "N," << n << ",A," << target_a << ",B," << target_b
         << ",status," << (prover.found ? "FEASIBLE" : (prover.timed_out ? "TIMEOUT" : "INFEASIBLE"))
         << ",dfs_nodes," << prover.dfs_nodes
         << ",mis_cache," << prover.mis_cache.size()
         << ",mis_nodes," << prover.mis_nodes
         << ",seconds," << seconds;
    if (prover.found) {
        cout << ",witness_A,\"" << prover.vec_to_string(prover.mask_to_vec(prover.found_a)) << "\""
             << ",witness_B,\"" << prover.vec_to_string(prover.mask_to_vec(prover.found_b)) << "\"";
    }
    cout << "\n";
    return prover.found ? 1 : (prover.timed_out ? 2 : 0);
}
