// #include "DSE.h"
// #include "Core.h"

// #include <algorithm>
// #include <cstdio>
// #include <cstdlib>
// #include <filesystem>
// #include <fstream>
// #include <iostream>
// #include <map>
// #include <random>
// #include <set>
// #include <sstream>
// #include <stdexcept>
// #include <string>
// #include <unordered_set>
// #include <vector>

// namespace fs = std::filesystem;

// // ============= Debug helper =============
// static bool g_debug = [](){
//     const char* p = std::getenv("CRYPTOHLS_DEBUG");
//     return (p && *p == '1');
// }();
// #define DBG(msg) do { if (g_debug) { std::cerr << "[DSE-DBG] " << msg << "\n"; } } while(0)

// // ===================== 小工具函数 =====================

// static std::string jsonEscape(const std::string& s) {
//     std::string out;
//     out.reserve(s.size() + 8);
//     for (char c : s) {
//         switch (c) {
//             case '\"': out += "\\\""; break;
//             case '\\': out += "\\\\"; break;
//             case '\n': out += "\\n";  break;
//             case '\r': out += "\\r";  break;
//             case '\t': out += "\\t";  break;
//             default:   out += c;      break;
//         }
//     }
//     return out;
// }

// // 将 Solution 序列化为 predict.py 期望的 JSON
// static std::string solutionToJson(const Solution& solution) {
//     std::ostringstream os;
//     os << "[";
//     bool first = true;
//     for (const auto& cmd : solution) {
//         if (!first) os << ",";
//         first = false;

//         os << "{";
//         os << "\"type\":\"" << jsonEscape(cmd.type) << "\"";

//         // params
//         os << ",\"params\":{";
//         bool pfirst = true;
//         for (const auto& kv : cmd.params) {
//             if (!pfirst) os << ",";
//             pfirst = false;
//             os << "\"" << jsonEscape(kv.first) << "\":"
//                << "\"" << jsonEscape(kv.second) << "\"";
//         }
//         os << "}";

//         os << ",\"line\":" << cmd.line;
//         if (!cmd.function.empty()) {
//             os << ",\"function\":\"" << jsonEscape(cmd.function) << "\"";
//         }
//         os << "}";
//     }
//     os << "]";
//     return os.str();
// }

// // 轻量解析 {"predicted_latency": X, "predicted_interval": Y, "predicted_lut": Z}
// static bool parsePredictOutput(const std::string& text, double& latency, double& interval, double& lut) {
//     auto findVal = [&](const std::string& key, double& dst) -> bool {
//         size_t pos = text.find(key);
//         if (pos == std::string::npos) return false;
//         pos = text.find(':', pos);
//         if (pos == std::string::npos) return false;
//         size_t beg = pos + 1;
//         while (beg < text.size() && (text[beg] == ' ' || text[beg] == '\t')) beg++;
//         size_t end = beg;
//         while (end < text.size() && text[end] != ',' && text[end] != '}') end++;
//         try {
//             dst = std::stod(text.substr(beg, end - beg));
//             return true;
//         } catch (...) { return false; }
//     };

//     double lat = 0.0, ii = 1.0, l = 0.0;
//     bool ok1 = findVal("predicted_latency", lat);
//     bool ok2 = findVal("predicted_interval", ii);
//     bool ok3 = findVal("predicted_lut", l);

//     if (ok1 && ok2 && ok3) {
//         latency  = lat;
//         interval = ii;
//         lut      = l;
//         return true;
//     }
//     return false;
// }

// // ===================== 去重：规范化 key & 全局已见集合 =====================

// static std::string canonicalKey(const Solution& solution) {
//     struct Item {
//         std::string function;
//         unsigned line;
//         std::string type;
//         std::vector<std::pair<std::string,std::string>> params;
//     };
//     std::vector<Item> items;
//     items.reserve(solution.size());
//     for (const auto& c : solution) {
//         Item it;
//         it.function = c.function;
//         it.line = c.line;
//         it.type = c.type;
//         it.params.reserve(c.params.size());
//         for (const auto& kv : c.params) it.params.emplace_back(kv.first, kv.second);
//         std::sort(it.params.begin(), it.params.end(),
//                   [](auto& a, auto& b){ return a.first < b.first; });
//         items.emplace_back(std::move(it));
//     }
//     std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){
//         if (a.function != b.function) return a.function < b.function;
//         if (a.line != b.line) return a.line < b.line;
//         return a.type < b.type;
//     });

//     std::ostringstream os;
//     os << "[";
//     bool first = true;
//     for (const auto& it : items) {
//         if (!first) os << ",";
//         first = false;
//         os << "{f:" << jsonEscape(it.function)
//            << ",l:" << it.line
//            << ",t:" << jsonEscape(it.type)
//            << ",p:{";
//         bool pf = true;
//         for (const auto& kv : it.params) {
//             if (!pf) os << ",";
//             pf = false;
//             os << jsonEscape(kv.first) << ":" << jsonEscape(kv.second);
//         }
//         os << "}}";
//     }
//     os << "]";
//     return os.str();
// }

// static std::unordered_set<std::string> g_seen;
// static std::string g_seen_path;

// static void loadSeenFromFileOnce() {
//     static bool inited = false;
//     if (inited) return;
//     inited = true;

//     const char* p = std::getenv("CRYPTOHLS_SEEN");
//     if (!p || !*p) return;
//     g_seen_path = p;
//     std::ifstream fin(g_seen_path);
//     if (!fin) return;
//     std::string line;
//     while (std::getline(fin, line)) {
//         if (!line.empty()) g_seen.insert(line);
//     }
// }

// static void appendSeenToFile(const std::string& key) {
//     if (g_seen_path.empty()) return;
//     std::ofstream fout(g_seen_path, std::ios::app);
//     if (!fout) return;
//     fout << key << "\n";
// }

// // ===================== CostModel 实现 =====================

// CostModel::CostModel(const FunctionCost& base) : base_cost(base) {}

// FunctionCost CostModel::predictCost(const Solution& solution) const {
//     auto resolve_predict_py = []() -> std::string {
//         if (const char* envp = std::getenv("CRYPTOHLS_PREDICT")) {
//             fs::path p = fs::path(envp);
//             if (fs::exists(p) && fs::is_regular_file(p)) return fs::weakly_canonical(p).string();
//         }
//         fs::path exe;
//         try { exe = fs::read_symlink("/proc/self/exe"); } catch (...) {}
//         fs::path exe_dir = exe.empty() ? fs::current_path() : exe.parent_path();
//         fs::path root    = exe_dir.parent_path();
//         std::vector<fs::path> candidates = {
//             "/home/CryptoHLS/libs/ModelDeploy/scripts/predict.py",
//             root / "libs/ModelDeploy/scripts/predict.py",
//             root / "ModelDeploy/scripts/predict.py",
//             fs::path("libs/ModelDeploy/scripts/predict.py"),
//             fs::path("./libs/ModelDeploy/scripts/predict.py"),
//             fs::path("../libs/ModelDeploy/scripts/predict.py"),
//             fs::path("ModelDeploy/scripts/predict.py"),
//             fs::path("./ModelDeploy/scripts/predict.py"),
//             fs::path("../ModelDeploy/scripts/predict.py")
//         };
//         for (const auto& c : candidates) {
//             if (fs::exists(c) && fs::is_regular_file(c))
//                 return fs::weakly_canonical(c).string();
//         }
//         return {};
//     }();

//     const char* graph_nodes = "initial_graph_nodes.csv";
//     const char* graph_edges = "initial_graph_edges.csv";

//     FunctionCost predicted_cost = base_cost;

//     std::string result;
//     if (!resolve_predict_py.empty()) {
//         const std::string solJson = solutionToJson(solution);
//         std::ostringstream cmd;
//         cmd << "timeout 30s python3 " << resolve_predict_py
//             << " --graph-nodes " << graph_nodes
//             << " --graph-edges " << graph_edges
//             << " --solution '" << solJson << "' 2>&1";
//         FILE* pipe = popen(cmd.str().c_str(), "r");
//         if (pipe) {
//             char buf[512];
//             while (fgets(buf, sizeof(buf), pipe)) result += buf;
//             pclose(pipe);
//         }
//     }

//     double latency = 0.0, interval = 1.0, lut = 0.0;
//     if (!result.empty() && parsePredictOutput(result, latency, interval, lut)) {
//         std::cerr << "[GNN] latency=" << latency << ", interval=" << interval << ", lut=" << lut << "\n";
//         predicted_cost.performance_score = std::max(0, static_cast<int>(std::round(
//             100000.0 / std::max(1.0, latency * interval)
//         )));
//         predicted_cost.area_score = static_cast<int>(std::round(lut));
//         return predicted_cost;
//     }

//     // fallback 启发式
//     for (const auto& cmd : solution) {
//         if (cmd.type == "UNROLL") {
//             auto it = cmd.params.find("factor");
//             if (it != cmd.params.end()) {
//                 try {
//                     int factor = std::stoi(it->second);
//                     predicted_cost.performance_score -= (factor * 2);
//                     predicted_cost.area_score        += (factor * 5);
//                 } catch (...) {}
//             }
//         } else if (cmd.type == "PIPELINE") {
//             predicted_cost.performance_score -= 10;
//             predicted_cost.area_score        += 5;
//         } else if (cmd.type == "ARRAY_PARTITION") {
//             predicted_cost.performance_score -= 5;
//             predicted_cost.area_score        += 15;
//         } else if (cmd.type == "INLINE") {
//             predicted_cost.performance_score -= 15;
//             predicted_cost.area_score        += 10;
//         } else if (cmd.type == "DATAFLOW") {
//             predicted_cost.performance_score -= 20;
//             predicted_cost.area_score        += 20;
//         } else if (cmd.type == "INTERFACE") {
//             predicted_cost.performance_score -= 5;
//             predicted_cost.area_score        += 2;
//         }
//     }
//     if (predicted_cost.performance_score < 0) predicted_cost.performance_score = 0;
//     if (predicted_cost.area_score < 0)        predicted_cost.area_score = 0;

//     return predicted_cost;
// }

// // ===================== 安全随机辅助 =====================

// static int safeUniformInt(int lo, int hi, std::mt19937& rng) {
//     if (hi < lo) hi = lo;
//     std::uniform_int_distribution<> d(lo, hi);
//     return d(rng);
// }

// static size_t safeRandIndex(size_t n, std::mt19937& rng) {
//     if (n == 0) return 0;
//     if (n == 1) return 0;
//     return (size_t)safeUniformInt(0, (int)n - 1, rng);
// }

// static int randPick(const std::vector<int>& vals, std::mt19937& rng) {
//     if (vals.empty()) return 1;
//     return vals[safeRandIndex(vals.size(), rng)];
// }

// static std::string pickOneVec(const std::vector<std::string>& vals, std::mt19937& rng) {
//     if (vals.empty()) return {};
//     return vals[safeRandIndex(vals.size(), rng)];
// }

// static void addIfMissing(Solution& sol,
//                          const std::string& type,
//                          const std::string& func,
//                          unsigned line_hint,
//                          const std::map<std::string,std::string>& params) {
//     for (auto& c : sol) if (c.type == type && c.function == func) return;
//     PragmaCommand cmd;
//     cmd.type     = type;
//     cmd.function = func;
//     cmd.line     = line_hint;
//     cmd.params   = params;
//     sol.push_back(std::move(cmd));
// }

// // ===================== 系统化模板 =====================

// struct SysTpl {
//     bool need_stream = false;
//     std::string type;
//     std::map<std::string,std::vector<std::string>> grid;
//     unsigned line_hint = 150;
// };

// static const std::vector<SysTpl>& getSystematicTemplates() {
//     static std::vector<SysTpl> TPL = {
//         {.need_stream=false, .type="PIPELINE",
//          .grid={{"II", {"", "1","2","4"}}}, .line_hint=170},
//         {.need_stream=false, .type="UNROLL",
//          .grid={{"factor", {"2","3","4","5","6","8","16"}}}, .line_hint=162},
//         {.need_stream=false, .type="ARRAY_PARTITION",
//          .grid={{"dim", {"1","2"}}, {"factor", {"2","4","8","16"}},
//                 {"type", {"block","cyclic","complete"}}}, .line_hint=153},
//         {.need_stream=false, .type="INLINE",
//          .grid={{}}, .line_hint=180},
//         {.need_stream=false, .type="INTERFACE",
//          .grid={{"mode", {"s_axilite","m_axi"}}}, .line_hint=150},
//         {.need_stream=true, .type="DATAFLOW",
//          .grid={{}}, .line_hint=155},
//         {.need_stream=false, .type="STREAM",
//          .grid={{"variable", {"state"}}, {"depth", {"8","16","32"}}}, .line_hint=154}
//     };
//     return TPL;
// }

// static size_t& sys_index() { static size_t idx = 0; return idx; }

// // 展开笛卡尔积第 k 项，任意一维为空则返回空，调用方应跳过
// static std::map<std::string,std::string> pickSysParams(const SysTpl& tpl, size_t k) {
//     std::map<std::string,std::string> out;
//     if (tpl.grid.empty()) return out;

//     size_t total = 1;
//     std::vector<std::pair<std::string,std::vector<std::string>>> items;
//     items.reserve(tpl.grid.size());
//     for (auto& kv : tpl.grid) {
//         if (kv.second.empty()) {
//             // 某一维无候选，返回空，由上层跳过该模板
//             return {};
//         }
//         items.push_back(kv);
//         // 防溢出：非常规情况保护
//         if (kv.second.size() > (size_t)1e6) return {};
//         total *= kv.second.size();
//         if (total == 0) return {};
//     }
//     if (total == 0) return {};
//     k %= total;

//     for (int i = (int)items.size()-1; i >= 0; --i) {
//         auto& name = items[i].first;
//         auto& vals = items[i].second;
//         size_t base = vals.size();
//         if (base == 0) return {};
//         size_t idx = (base == 1) ? 0 : (k % base);
//         k = (base == 1) ? k : (k / base);
//         out[name] = vals[idx];
//     }
//     return out;
// }


// Solution generateRandomSolution(const DesignSpace& designSpace, std::mt19937& rng) {
//     loadSeenFromFileOnce();

//     if (designSpace.empty()) return {};

//     std::map<unsigned, std::vector<OptimizationPoint>> points_by_line;
//     for (const auto& point : designSpace) {
//         points_by_line[point.line].push_back(point);
//     }

//     const int ATTEMPTS = 64;
//     Solution last_solution;

//     int sys_env = -1;
//     if (const char* p = std::getenv("CRYPTOHLS_SYSTEMATIC")) {
//         if (*p == '0') sys_env = 0;
//         else if (*p == '1') sys_env = 1;
//     }

//     for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
//         Solution solution;

//         std::uniform_real_distribution<> prob(0.0, 1.0);
//         bool aggressive_mode = (prob(rng) < 0.2);
//         bool coverage_mode   = (prob(rng) < 0.25);
//         bool systematic_mode = (sys_env == 1) ? true : (sys_env == 0 ? false : (prob(rng) < 0.2));

//         std::uniform_int_distribution<> coin_flip(0, 1);

//         // Step 1: 随机/激进采样
//         for (const auto& line_pair : points_by_line) {
//             const auto& points_on_line = line_pair.second;
//             if (points_on_line.empty()) continue;

//             bool pickThisLine = (aggressive_mode || coverage_mode || systematic_mode) ? true : (coin_flip(rng) == 1);
//             if (!pickThisLine) continue;

//             int max_pick = (int)points_on_line.size();
//             int num_to_pick = 1;
//             if (aggressive_mode) {
//                 num_to_pick = safeUniformInt(1, max_pick, rng);
//             }

//             std::set<int> chosen_indices;
//             while ((int)chosen_indices.size() < num_to_pick) {
//                 size_t idx = safeRandIndex(points_on_line.size(), rng);
//                 chosen_indices.insert((int)idx);
//                 if (points_on_line.size() == 1) break;
//             }

//             for (int idx : chosen_indices) {
//                 const auto& chosen_point = points_on_line[(size_t)idx];
//                 if (chosen_point.options.empty()) continue;

//                 size_t opt_idx = safeRandIndex(chosen_point.options.size(), rng);
//                 const auto& chosen_option = chosen_point.options[opt_idx];

//                 PragmaCommand cmd;
//                 cmd.type     = chosen_option.type;
//                 cmd.function = chosen_point.function;
//                 cmd.line     = chosen_point.line;

//                 for (const auto& param_pair : chosen_option.param_ranges) {
//                     const auto& values = param_pair.second;
//                     if (!values.empty()) {
//                         size_t vidx = safeRandIndex(values.size(), rng);
//                         cmd.params[param_pair.first] = values[vidx];
//                     }
//                 }
//                 solution.push_back(std::move(cmd));
//             }
//         }

//         // Step 1.5: 系统化模式
//         if (systematic_mode) {
//             const auto& TPL = getSystematicTemplates();
//             size_t &gidx = sys_index();
//             int add_n = 2 + (int)(gidx % 3);

//             for (int i = 0; i < add_n; ++i, ++gidx) {
//                 const SysTpl& tpl = TPL[gidx % TPL.size()];
//                 if (tpl.need_stream) {
//                     addIfMissing(solution, "STREAM", "aes_encrypt", 154,
//                                  {{"variable","state"},{"depth","16"}});
//                     addIfMissing(solution, "DATAFLOW", "aes_encrypt", 155, {});
//                 } else {
//                     auto params = pickSysParams(tpl, gidx);
//                     if (tpl.grid.size() > 0 && params.empty()) continue;
//                     PragmaCommand c;
//                     c.type = tpl.type;
//                     c.function = "aes_encrypt";
//                     c.line = tpl.line_hint;
//                     c.params = params;
//                     solution.push_back(std::move(c));
//                 }
//             }
//         }

//         // Step 2: 覆盖模式
//         if (coverage_mode) {
//             bool hasInterface=false, hasPartition=false, hasDataflow=false;
//             bool hasPipeline=false, hasUnroll=false, hasInline=false, hasStream=false;

//             for (auto& cmd : solution) {
//                 if (cmd.type == "INTERFACE") hasInterface = true;
//                 if (cmd.type == "ARRAY_PARTITION") hasPartition = true;
//                 if (cmd.type == "DATAFLOW") hasDataflow = true;
//                 if (cmd.type == "PIPELINE") hasPipeline = true;
//                 if (cmd.type == "UNROLL") hasUnroll = true;
//                 if (cmd.type == "INLINE") hasInline = true;
//                 if (cmd.type == "STREAM") hasStream = true;
//             }

//             if (!hasInterface) {
//                 addIfMissing(solution, "INTERFACE", "aes_encrypt",
//                              150 + (rng() % 10),
//                              {{"mode", (prob(rng) < 0.5 ? "s_axilite" : "m_axi")}});
//             }
//             if (!hasPartition) {
//                 int pfactor = randPick({2,3,4,5,6,8,16}, rng);
//                 addIfMissing(solution, "ARRAY_PARTITION", "aes_encrypt",
//                              153, {{"dim","1"},
//                                    {"factor", std::to_string(pfactor)},
//                                    {"type", (prob(rng) < 0.5 ? "cyclic" : "block")}});
//             }
//             if (!hasUnroll) {
//                 int uf = randPick({2,3,4,5,6,8,16}, rng);
//                 addIfMissing(solution, "UNROLL", "aes_encrypt", 160 + (rng() % 5),
//                              {{"factor", std::to_string(uf)}});
//             }
//             if (!hasPipeline) {
//                 addIfMissing(solution, "PIPELINE", "aes_encrypt", 170 + (rng() % 5), {});
//             }
//             if (!hasInline) {
//                 addIfMissing(solution, "INLINE", "aes_encrypt", 180 + (rng() % 5), {});
//             }
//             if (!hasDataflow) {
//                 if (!hasStream) {
//                     addIfMissing(solution, "STREAM", "aes_encrypt", 154,
//                                  {{"variable","state"}, {"depth","16"}});
//                     hasStream = true;
//                 }
//                 addIfMissing(solution, "DATAFLOW", "aes_encrypt", 155, {});
//             }
//         }

//         // Step 3: STREAM-DATAFLOW 约束
//         bool hasStreamOnState = false;
//         for (const auto& cmd : solution) {
//             if (cmd.type == "STREAM" && cmd.params.count("variable") &&
//                 cmd.params.at("variable") == "state") {
//                 hasStreamOnState = true;
//                 break;
//             }
//         }
//         if (!hasStreamOnState) {
//             solution.erase(
//                 std::remove_if(solution.begin(), solution.end(),
//                     [](const PragmaCommand& c){ return c.type == "DATAFLOW"; }),
//                 solution.end()
//             );
//         }

//         // Step 4: 
//         if (solution.empty() && !designSpace.empty()) {
//             const auto& point = designSpace.front();
//             if (!point.options.empty()) {
//                 const auto& option = point.options.front();
//                 PragmaCommand cmd;
//                 cmd.type     = option.type;
//                 cmd.function = point.function;
//                 cmd.line     = point.line;
//                 for (const auto& pr : option.param_ranges) {
//                     if (!pr.second.empty()) cmd.params[pr.first] = pr.second.front();
//                 }
//                 solution.push_back(std::move(cmd));
//             }
//         }

//         // Step 5: INLINE vs PIPELINE 去冲突
//         std::map<std::string, std::vector<std::string>> func_opts;
//         for (const auto& cmd : solution) func_opts[cmd.function].push_back(cmd.type);
//         for (const auto& kv : func_opts) {
//             const auto& types = kv.second;
//             bool hasInline = std::find(types.begin(), types.end(), "INLINE")   != types.end();
//             bool hasPipe   = std::find(types.begin(), types.end(), "PIPELINE") != types.end();
//             if (hasInline && hasPipe) {
//                 std::uniform_int_distribution<> d(0, 1);
//                 const std::string removeType = (d(rng) == 0) ? "INLINE" : "PIPELINE";
//                 solution.erase(
//                     std::remove_if(solution.begin(), solution.end(),
//                         [&](const PragmaCommand& c){
//                             return c.function == kv.first && c.type == removeType;
//                         }),
//                     solution.end()
//                 );
//             }
//         }

//         // Step 6: 过滤非法组合
//         solution.erase(
//             std::remove_if(solution.begin(), solution.end(),
//                 [](const PragmaCommand& c) {
//                     if (c.type == "UNROLL") {
//                         auto it = c.params.find("factor");
//                         if (it != c.params.end()) {
//                             int f = atoi(it->second.c_str());
//                             if (f <= 0) return true;
//                         }
//                     }
//                     if (c.type == "STREAM") {
//                         auto it = c.params.find("depth");
//                         if (it != c.params.end()) {
//                             int d = atoi(it->second.c_str());
//                             if (d <= 0) return true;
//                         }
//                     }
//                     if (c.type == "INTERFACE") {
//                         auto it = c.params.find("mode");
//                         if (it != c.params.end()) {
//                             std::string m = it->second;
//                             if (m != "s_axilite" && m != "m_axi") return true;
//                         }
//                     }
//                     if (c.type == "ARRAY_PARTITION") {
//                         auto it = c.params.find("factor");
//                         if (it != c.params.end()) {
//                             int f = atoi(it->second.c_str());
//                             if (f <= 0) return true;
//                         }
//                         auto it2 = c.params.find("type");
//                         if (it2 != c.params.end()) {
//                             std::string t = it2->second;
//                             if (t != "block" && t != "cyclic" && t != "complete") return true;
//                         }
//                     }
//                     return false;
//                 }),
//             solution.end()
//         );

//         // 去重
//         const std::string key = canonicalKey(solution);
//         last_solution = solution;
//         if (g_seen.find(key) == g_seen.end()) {
//             g_seen.insert(key);
//             appendSeenToFile(key);
//             return solution;
//         }
//     }

//     return last_solution;
// }


#include "DSE.h"
#include "Core.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ============= Debug helper =============
static bool g_debug = [](){
    const char* p = std::getenv("CRYPTOHLS_DEBUG");
    return (p && *p == '1');
}();
#define DBG(msg) do { if (g_debug) { std::cerr << "[DSE-DBG] " << msg << "\n"; } } while(0)

// ===================== 小工具函数 =====================

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// 将 Solution 序列化为 predict.py 期望的 JSON
static std::string solutionToJson(const Solution& solution) {
    std::ostringstream os;
    os << "[";
    bool first = true;
    for (const auto& cmd : solution) {
        if (!first) os << ",";
        first = false;

        os << "{";
        os << "\"type\":\"" << jsonEscape(cmd.type) << "\"";

        // params
        os << ",\"params\":{";
        bool pfirst = true;
        for (const auto& kv : cmd.params) {
            if (!pfirst) os << ",";
            pfirst = false;
            os << "\"" << jsonEscape(kv.first) << "\":"
               << "\"" << jsonEscape(kv.second) << "\"";
        }
        os << "}";

        os << ",\"line\":" << cmd.line;
        if (!cmd.function.empty()) {
            os << ",\"function\":\"" << jsonEscape(cmd.function) << "\"";
        }
        os << "}";
    }
    os << "]";
    return os.str();
}

// 轻量解析 {"predicted_latency": X, "predicted_interval": Y, "predicted_lut": Z}
static bool parsePredictOutput(const std::string& text, double& latency, double& interval, double& lut) {
    auto findVal = [&](const std::string& key, double& dst) -> bool {
        size_t pos = text.find(key);
        if (pos == std::string::npos) return false;
        pos = text.find(':', pos);
        if (pos == std::string::npos) return false;
        size_t beg = pos + 1;
        while (beg < text.size() && (text[beg] == ' ' || text[beg] == '\t')) beg++;
        size_t end = beg;
        while (end < text.size() && text[end] != ',' && text[end] != '}') end++;
        try {
            dst = std::stod(text.substr(beg, end - beg));
            return true;
        } catch (...) { return false; }
    };

    double lat = 0.0, ii = 1.0, l = 0.0;
    bool ok1 = findVal("predicted_latency", lat);
    bool ok2 = findVal("predicted_interval", ii);
    bool ok3 = findVal("predicted_lut", l);

    if (ok1 && ok2 && ok3) {
        latency  = lat;
        interval = ii;
        lut      = l;
        return true;
    }
    return false;
}

// ===================== 去重：规范化 key & 全局已见集合 =====================

static std::string canonicalKey(const Solution& solution) {
    struct Item {
        std::string function;
        unsigned line;
        std::string type;
        std::vector<std::pair<std::string,std::string>> params;
    };
    std::vector<Item> items;
    items.reserve(solution.size());
    for (const auto& c : solution) {
        Item it;
        it.function = c.function;
        it.line = c.line;
        it.type = c.type;
        it.params.reserve(c.params.size());
        for (const auto& kv : c.params) it.params.emplace_back(kv.first, kv.second);
        std::sort(it.params.begin(), it.params.end(),
                  [](auto& a, auto& b){ return a.first < b.first; });
        items.emplace_back(std::move(it));
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){
        if (a.function != b.function) return a.function < b.function;
        if (a.line != b.line) return a.line < b.line;
        return a.type < b.type;
    });

    std::ostringstream os;
    os << "[";
    bool first = true;
    for (const auto& it : items) {
        if (!first) os << ",";
        first = false;
        os << "{f:" << jsonEscape(it.function)
           << ",l:" << it.line
           << ",t:" << jsonEscape(it.type)
           << ",p:{";
        bool pf = true;
        for (const auto& kv : it.params) {
            if (!pf) os << ",";
            pf = false;
            os << jsonEscape(kv.first) << ":" << jsonEscape(kv.second);
        }
        os << "}}";
    }
    os << "]";
    return os.str();
}

static std::unordered_set<std::string> g_seen;
static std::string g_seen_path;

static void loadSeenFromFileOnce() {
    static bool inited = false;
    if (inited) return;
    inited = true;

    const char* p = std::getenv("CRYPTOHLS_SEEN");
    if (!p || !*p) return;
    g_seen_path = p;
    std::ifstream fin(g_seen_path);
    if (!fin) return;
    std::string line;
    while (std::getline(fin, line)) {
        if (!line.empty()) g_seen.insert(line);
    }
}

static void appendSeenToFile(const std::string& key) {
    if (g_seen_path.empty()) return;
    std::ofstream fout(g_seen_path, std::ios::app);
    if (!fout) return;
    fout << key << "\n";
}

// ===================== Kernel 规则 & 合法性过滤 =====================

struct KernelTemplate {
    std::unordered_set<std::string> valid_stream_vars;
};

static const std::unordered_map<std::string, KernelTemplate> KERNEL_RULES = {
    {"aes_encrypt", KernelTemplate{std::unordered_set<std::string>{"state","round_key","in","out"}}},
    {"sha1",        KernelTemplate{std::unordered_set<std::string>{"w_buf","h_state","msg_in","msg_out"}}},
};

// 基础参数合法性（原有规则）
static bool basicParamCheck(const PragmaCommand& c) {
    auto atoi_safe = [](const std::string& s)->int{
        try { return std::stoi(s); } catch (...) { return 0; }
    };

    if (c.type == "UNROLL") {
        auto it = c.params.find("factor");
        if (it != c.params.end()) {
            int f = atoi_safe(it->second);
            if (f <= 0) return false;
        }
    }
    if (c.type == "STREAM") {
        auto it = c.params.find("depth");
        if (it != c.params.end()) {
            int d = atoi_safe(it->second);
            if (d <= 0) return false;
        }
    }
    if (c.type == "INTERFACE") {
        auto it = c.params.find("mode");
        if (it != c.params.end()) {
            std::string m = it->second;
            if (m != "s_axilite" && m != "m_axi") return false;
        }
    }
    if (c.type == "ARRAY_PARTITION") {
        auto it = c.params.find("factor");
        if (it != c.params.end()) {
            int f = atoi_safe(it->second);
            if (f <= 0) return false;
        }
        auto it2 = c.params.find("type");
        if (it2 != c.params.end()) {
            std::string t = it2->second;
            if (t != "block" && t != "cyclic" && t != "complete") return false;
        }
    }
    return true;
}

// 统一合法性过滤（函数一致性、配对、白名单、互斥）
static bool filterIllegalPragmas(std::vector<PragmaCommand>& sol, const std::string& top_function) {
    // 0) 基本参数合法性
    for (const auto& c : sol) {
        if (!basicParamCheck(c)) return false;
    }

    // 1) function 必须全等于 top_function
    for (const auto& c : sol) {
        if (c.function != top_function) return false;
    }

    // 2) DATAFLOW ↔ STREAM 配对，且 STREAM.variable 在白名单
    bool hasDF = std::any_of(sol.begin(), sol.end(), [](const PragmaCommand& c){ return c.type=="DATAFLOW"; });
    if (hasDF) {
        bool hasST = std::any_of(sol.begin(), sol.end(), [](const PragmaCommand& c){ return c.type=="STREAM"; });
        if (!hasST) return false;
        auto it = KERNEL_RULES.find(top_function);
        if (it != KERNEL_RULES.end() && !it->second.valid_stream_vars.empty()) {
            for (const auto& c : sol) if (c.type=="STREAM") {
                auto iv = c.params.find("variable");
                if (iv == c.params.end()) return false;
                if (it->second.valid_stream_vars.count(iv->second)==0) return false;
            }
        }
    }

    // 3) 同函数下 INLINE 与 PIPELINE 互斥
    bool hasInl = false, hasPipe = false;
    for (const auto& c : sol) {
        if (c.type=="INLINE")   hasInl  = true;
        if (c.type=="PIPELINE") hasPipe = true;
    }
    if (hasInl && hasPipe) return false;

    return true;
}

// ===================== 外部覆盖 + 自动推断 =====================

static std::string externalTopFunction() {
    // 优先读取环境变量
    const char* p1 = std::getenv("CRYPTOHLS_TOP_FUNCTION");
    if (p1 && *p1) return std::string(p1);
    const char* p2 = std::getenv("TOP_FUNCTION");
    if (p2 && *p2) return std::string(p2);
    const char* p3 = std::getenv("CRYPTOHLS_TOP");
    if (p3 && *p3) return std::string(p3);
    return {};
}

static std::string inferTopFunction(const DesignSpace& designSpace) {
    if (designSpace.empty()) return "top";
    std::unordered_map<std::string, int> cnt;
    std::string best = designSpace.front().function;
    int bestc = 0;
    for (const auto& p : designSpace) {
        int c = ++cnt[p.function];
        if (c > bestc) { bestc = c; best = p.function; }
    }
    return best;
}

static std::string resolveTopFunction(const DesignSpace& designSpace) {
    std::string ext = externalTopFunction();
    if (!ext.empty()) {
        DBG("Top function override from env: " << ext);
        return ext;
    }
    std::string inf = inferTopFunction(designSpace);
    DBG("Top function inferred: " << inf);
    return inf;
}

// ===================== 安全随机辅助 =====================

static int safeUniformInt(int lo, int hi, std::mt19937& rng) {
    if (hi < lo) hi = lo;
    std::uniform_int_distribution<> d(lo, hi);
    return d(rng);
}

static size_t safeRandIndex(size_t n, std::mt19937& rng) {
    if (n == 0) return 0;
    if (n == 1) return 0;
    return (size_t)safeUniformInt(0, (int)n - 1, rng);
}

static int randPick(const std::vector<int>& vals, std::mt19937& rng) {
    if (vals.empty()) return 1;
    return vals[safeRandIndex(vals.size(), rng)];
}

static std::string pickOneVec(const std::vector<std::string>& vals, std::mt19937& rng) {
    if (vals.empty()) return {};
    return vals[safeRandIndex(vals.size(), rng)];
}

static void addIfMissing(Solution& sol,
                         const std::string& type,
                         const std::string& func,
                         unsigned line_hint,
                         const std::map<std::string,std::string>& params) {
    for (auto& c : sol) if (c.type == type && c.function == func) return;
    PragmaCommand cmd;
    cmd.type     = type;
    cmd.function = func;
    cmd.line     = line_hint;
    cmd.params   = params;
    sol.push_back(std::move(cmd));
}

// ===================== 系统化模板 =====================

struct SysTpl {
    bool need_stream = false;
    std::string type;
    std::map<std::string,std::vector<std::string>> grid;
    unsigned line_hint = 150;
};

static std::vector<SysTpl> getSystematicTemplatesFor(const std::string& top_function) {
    // 可按 kernel 定制；这里给通用版本，行号维持原值
    return {
        {.need_stream=false, .type="PIPELINE",
         .grid={{"II", {"","1","2","4"}}}, .line_hint=170},
        {.need_stream=false, .type="UNROLL",
         .grid={{"factor", {"2","3","4","5","6","8","16"}}}, .line_hint=162},
        {.need_stream=false, .type="ARRAY_PARTITION",
         .grid={{"dim", {"1","2"}}, {"factor", {"2","4","8","16"}},
                {"type", {"block","cyclic","complete"}}}, .line_hint=153},
        {.need_stream=false, .type="INLINE",
         .grid={{}}, .line_hint=180},
        {.need_stream=false, .type="INTERFACE",
         .grid={{"mode", {"s_axilite","m_axi"}}}, .line_hint=150},
        {.need_stream=true, .type="DATAFLOW",
         .grid={{}}, .line_hint=155},
        {.need_stream=false, .type="STREAM",
         .grid={{"variable", { /* 变量白名单后面会填充 */ }},
                {"depth", {"8","16","32"}}}, .line_hint=154}
    };
}

static size_t& sys_index() { static size_t idx = 0; return idx; }

// 展开笛卡尔积第 k 项，任意一维为空则返回空，调用方应跳过
static std::map<std::string,std::string> pickSysParams(const SysTpl& tpl, size_t k) {
    std::map<std::string,std::string> out;
    if (tpl.grid.empty()) return out;

    size_t total = 1;
    std::vector<std::pair<std::string,std::vector<std::string>>> items;
    items.reserve(tpl.grid.size());
    for (auto& kv : tpl.grid) {
        if (kv.second.empty()) {
            return {};
        }
        items.push_back(kv);
        if (kv.second.size() > (size_t)1e6) return {};
        total *= kv.second.size();
        if (total == 0) return {};
    }
    if (total == 0) return {};
    k %= total;

    for (int i = (int)items.size()-1; i >= 0; --i) {
        auto& name = items[i].first;
        auto& vals = items[i].second;
        size_t base = vals.size();
        if (base == 0) return {};
        size_t idx = (base == 1) ? 0 : (k % base);
        k = (base == 1) ? k : (k / base);
        out[name] = vals[idx];
    }
    return out;
}

// ===================== 生成 =====================

Solution generateRandomSolution(const DesignSpace& designSpace, std::mt19937& rng) {
    loadSeenFromFileOnce();
    if (designSpace.empty()) return {};

    // 1) 解析 top 函数名：外部优先，回落推断
    const std::string top_function = resolveTopFunction(designSpace);

    // 2) 每行聚类
    std::map<unsigned, std::vector<OptimizationPoint>> points_by_line;
    for (const auto& point : designSpace) {
        points_by_line[point.line].push_back(point);
    }

    const int ATTEMPTS = 64;
    Solution last_solution;

    int sys_env = -1;
    if (const char* p = std::getenv("CRYPTOHLS_SYSTEMATIC")) {
        if (*p == '0') sys_env = 0;
        else if (*p == '1') sys_env = 1;
    }

    // 变量白名单（供 STREAM.variable 使用）
    std::vector<std::string> stream_vars;
    auto kt = KERNEL_RULES.find(top_function);
    if (kt != KERNEL_RULES.end())
        stream_vars.assign(kt->second.valid_stream_vars.begin(), kt->second.valid_stream_vars.end());
    if (stream_vars.empty()) stream_vars.push_back("state"); // 回退

    for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
        Solution solution;

        std::uniform_real_distribution<> prob(0.0, 1.0);
        bool aggressive_mode = (prob(rng) < 0.2);
        bool coverage_mode   = (prob(rng) < 0.25);
        bool systematic_mode = (sys_env == 1) ? true : (sys_env == 0 ? false : (prob(rng) < 0.2));

        std::uniform_int_distribution<> coin_flip(0, 1);

        // Step 1: 随机/激进采样
        for (const auto& line_pair : points_by_line) {
            const auto& points_on_line = line_pair.second;
            if (points_on_line.empty()) continue;

            bool pickThisLine = (aggressive_mode || coverage_mode || systematic_mode) ? true : (coin_flip(rng) == 1);
            if (!pickThisLine) continue;

            int max_pick = (int)points_on_line.size();
            int num_to_pick = 1;
            if (aggressive_mode) {
                num_to_pick = safeUniformInt(1, max_pick, rng);
            }

            std::set<int> chosen_indices;
            while ((int)chosen_indices.size() < num_to_pick) {
                size_t idx = safeRandIndex(points_on_line.size(), rng);
                chosen_indices.insert((int)idx);
                if (points_on_line.size() == 1) break;
            }

            for (int idx : chosen_indices) {
                const auto& chosen_point = points_on_line[(size_t)idx];
                if (chosen_point.options.empty()) continue;

                size_t opt_idx = safeRandIndex(chosen_point.options.size(), rng);
                const auto& chosen_option = chosen_point.options[opt_idx];

                PragmaCommand cmd;
                cmd.type     = chosen_option.type;
                cmd.function = top_function;                  // 统一落到 top
                cmd.line     = chosen_point.line;

                for (const auto& param_pair : chosen_option.param_ranges) {
                    const auto& values = param_pair.second;
                    if (!values.empty()) {
                        size_t vidx = safeRandIndex(values.size(), rng);
                        cmd.params[param_pair.first] = values[vidx];
                    }
                }
                solution.push_back(std::move(cmd));
            }
        }

        // Step 1.5: 系统化模式
        if (systematic_mode) {
            auto TPL = getSystematicTemplatesFor(top_function);
            // 补齐模板里的 STREAM.variable 候选
            for (auto& t : TPL) {
                if (t.type == "STREAM") {
                    auto it = t.grid.find("variable");
                    if (it != t.grid.end()) it->second = stream_vars;
                }
            }
            size_t &gidx = sys_index();
            int add_n = 2 + (int)(gidx % 3);

            for (int i = 0; i < add_n; ++i, ++gidx) {
                const SysTpl& tpl = TPL[gidx % TPL.size()];
                if (tpl.need_stream) {
                    addIfMissing(solution, "STREAM", top_function, 154,
                                 {{"variable", pickOneVec(stream_vars, rng)}, {"depth","16"}});
                    addIfMissing(solution, "DATAFLOW", top_function, 155, {});
                } else {
                    auto params = pickSysParams(tpl, gidx);
                    if (!tpl.grid.empty() && params.empty()) continue;
                    PragmaCommand c;
                    c.type = tpl.type;
                    c.function = top_function;
                    c.line = tpl.line_hint;
                    c.params = params;
                    solution.push_back(std::move(c));
                }
            }
        }

        // Step 2: 覆盖模式
        if (coverage_mode) {
            bool hasInterface=false, hasPartition=false, hasDataflow=false;
            bool hasPipeline=false, hasUnroll=false, hasInline=false, hasStream=false;

            for (auto& cmd : solution) {
                if (cmd.type == "INTERFACE") hasInterface = true;
                if (cmd.type == "ARRAY_PARTITION") hasPartition = true;
                if (cmd.type == "DATAFLOW") hasDataflow = true;
                if (cmd.type == "PIPELINE") hasPipeline = true;
                if (cmd.type == "UNROLL") hasUnroll = true;
                if (cmd.type == "INLINE") hasInline = true;
                if (cmd.type == "STREAM") hasStream = true;
            }

            if (!hasInterface) {
                addIfMissing(solution, "INTERFACE", top_function,
                             150 + (rng() % 10),
                             {{"mode", (prob(rng) < 0.5 ? "s_axilite" : "m_axi")}});
            }
            if (!hasPartition) {
                int pfactor = randPick({2,3,4,5,6,8,16}, rng);
                addIfMissing(solution, "ARRAY_PARTITION", top_function,
                             153, {{"dim","1"},
                                   {"factor", std::to_string(pfactor)},
                                   {"type", (prob(rng) < 0.5 ? "cyclic" : "block")}});
            }
            if (!hasUnroll) {
                int uf = randPick({2,3,4,5,6,8,16}, rng);
                addIfMissing(solution, "UNROLL", top_function, 160 + (rng() % 5),
                             {{"factor", std::to_string(uf)}});
            }
            if (!hasPipeline) {
                addIfMissing(solution, "PIPELINE", top_function, 170 + (rng() % 5), {});
            }
            if (!hasInline) {
                addIfMissing(solution, "INLINE", top_function, 180 + (rng() % 5), {});
            }
            if (!hasDataflow) {
                if (!hasStream) {
                    addIfMissing(solution, "STREAM", top_function, 154,
                                 {{"variable", pickOneVec(stream_vars, rng)}, {"depth","16"}});
                    hasStream = true;
                }
                addIfMissing(solution, "DATAFLOW", top_function, 155, {});
            }
        }

        // Step 3: 若没有 STREAM(variable in whitelist)，去掉 DATAFLOW
        bool hasStreamOnWhitelisted = false;
        {
            auto it = KERNEL_RULES.find(top_function);
            for (const auto& cmd : solution) {
                if (cmd.type == "STREAM") {
                    auto iv = cmd.params.find("variable");
                    if (iv != cmd.params.end()) {
                        if (it == KERNEL_RULES.end() || it->second.valid_stream_vars.count(iv->second)) {
                            hasStreamOnWhitelisted = true; break;
                        }
                    }
                }
            }
        }
        if (!hasStreamOnWhitelisted) {
            solution.erase(
                std::remove_if(solution.begin(), solution.end(),
                    [](const PragmaCommand& c){ return c.type == "DATAFLOW"; }),
                solution.end()
            );
        }

        // Step 4: 空方案兜底
        if (solution.empty() && !designSpace.empty()) {
            const auto& point = designSpace.front();
            if (!point.options.empty()) {
                const auto& option = point.options.front();
                PragmaCommand cmd;
                cmd.type     = option.type;
                cmd.function = top_function;
                cmd.line     = point.line;
                for (const auto& pr : option.param_ranges) {
                    if (!pr.second.empty()) cmd.params[pr.first] = pr.second.front();
                }
                solution.push_back(std::move(cmd));
            }
        }

        // Step 5: INLINE vs PIPELINE 去冲突（同函数）
        std::map<std::string, std::vector<std::string>> func_opts;
        for (const auto& cmd : solution) func_opts[cmd.function].push_back(cmd.type);
        for (const auto& kv : func_opts) {
            const auto& types = kv.second;
            bool hasInline = std::find(types.begin(), types.end(), "INLINE")   != types.end();
            bool hasPipe   = std::find(types.begin(), types.end(), "PIPELINE") != types.end();
            if (hasInline && hasPipe) {
                std::uniform_int_distribution<> d(0, 1);
                const std::string removeType = (d(rng) == 0) ? "INLINE" : "PIPELINE";
                solution.erase(
                    std::remove_if(solution.begin(), solution.end(),
                        [&](const PragmaCommand& c){
                            return c.function == kv.first && c.type == removeType;
                        }),
                    solution.end()
                );
            }
        }

        // Step 6: 统一合法性过滤（函数一致 + 配对 + 白名单 + 基础参数）
        if (!filterIllegalPragmas(solution, top_function)) {
            DBG("Illegal solution filtered, retry...");
            continue; // 重采样
        }

        // 去重
        const std::string key = canonicalKey(solution);
        if (g_seen.find(key) == g_seen.end()) {
            g_seen.insert(key);
            appendSeenToFile(key);
            return solution;
        } else {
            DBG("Duplicate solution (seen), retry...");
        }

        // 保存最后一次方案作为兜底
        last_solution = solution;
    }

    return last_solution;
}

// ===================== CostModel 实现 =====================

CostModel::CostModel(const FunctionCost& base) : base_cost(base) {}

FunctionCost CostModel::predictCost(const Solution& solution) const {
    auto resolve_predict_py = []() -> std::string {
        if (const char* envp = std::getenv("CRYPTOHLS_PREDICT")) {
            fs::path p = fs::path(envp);
            if (fs::exists(p) && fs::is_regular_file(p)) return fs::weakly_canonical(p).string();
        }
        fs::path exe;
        try { exe = fs::read_symlink("/proc/self/exe"); } catch (...) {}
        fs::path exe_dir = exe.empty() ? fs::current_path() : exe.parent_path();
        fs::path root    = exe_dir.parent_path();
        std::vector<fs::path> candidates = {
            "/home/CryptoHLS/libs/ModelDeploy/scripts/predict.py",
            root / "libs/ModelDeploy/scripts/predict.py",
            root / "ModelDeploy/scripts/predict.py",
            fs::path("libs/ModelDeploy/scripts/predict.py"),
            fs::path("./libs/ModelDeploy/scripts/predict.py"),
            fs::path("../libs/ModelDeploy/scripts/predict.py"),
            fs::path("ModelDeploy/scripts/predict.py"),
            fs::path("./ModelDeploy/scripts/predict.py"),
            fs::path("../ModelDeploy/scripts/predict.py")
        };
        for (const auto& c : candidates) {
            if (fs::exists(c) && fs::is_regular_file(c))
                return fs::weakly_canonical(c).string();
        }
        return {};
    }();

    const char* graph_nodes = "initial_graph_nodes.csv";
    const char* graph_edges = "initial_graph_edges.csv";

    FunctionCost predicted_cost = base_cost;

    std::string result;
    if (!resolve_predict_py.empty()) {
        const std::string solJson = solutionToJson(solution);
        std::ostringstream cmd;
        cmd << "timeout 30s python3 " << resolve_predict_py
            << " --graph-nodes " << graph_nodes
            << " --graph-edges " << graph_edges
            << " --solution '" << solJson << "' 2>&1";
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) result += buf;
            pclose(pipe);
        }
    }

    double latency = 0.0, interval = 1.0, lut = 0.0;
    if (!result.empty() && parsePredictOutput(result, latency, interval, lut)) {
        std::cerr << "[GNN] latency=" << latency << ", interval=" << interval << ", lut=" << lut << "\n";
        predicted_cost.performance_score = std::max(0, static_cast<int>(std::round(
            100000.0 / std::max(1.0, latency * interval)
        )));
        predicted_cost.area_score = static_cast<int>(std::round(lut));
        return predicted_cost;
    }

    // fallback 启发式（原样保留）
    for (const auto& cmd : solution) {
        if (cmd.type == "UNROLL") {
            auto it = cmd.params.find("factor");
            if (it != cmd.params.end()) {
                try {
                    int factor = std::stoi(it->second);
                    predicted_cost.performance_score -= (factor * 2);
                    predicted_cost.area_score        += (factor * 5);
                } catch (...) {}
            }
        } else if (cmd.type == "PIPELINE") {
            predicted_cost.performance_score -= 10;
            predicted_cost.area_score        += 5;
        } else if (cmd.type == "ARRAY_PARTITION") {
            predicted_cost.performance_score -= 5;
            predicted_cost.area_score        += 15;
        } else if (cmd.type == "INLINE") {
            predicted_cost.performance_score -= 15;
            predicted_cost.area_score        += 10;
        } else if (cmd.type == "DATAFLOW") {
            predicted_cost.performance_score -= 20;
            predicted_cost.area_score        += 20;
        } else if (cmd.type == "INTERFACE") {
            predicted_cost.performance_score -= 5;
            predicted_cost.area_score        += 2;
        }
    }
    if (predicted_cost.performance_score < 0) predicted_cost.performance_score = 0;
    if (predicted_cost.area_score < 0)        predicted_cost.area_score = 0;

    return predicted_cost;
}
