# import argparse
# import subprocess
# import os
# import json
# import re
# import xml.etree.ElementTree as ET
# from datetime import datetime
# import shutil

# print("--- EXECUTING HLS_RUNNER_PY [DEBUG VERSION V7 with Clock+Latency+Interval Fallback + SafeParse] ---")

# # 配置
# VITIS_HLS_PATH = "/home/Xilinx/Vitis_HLS/2021.1/bin/vitis_hls"
# FPGA_PART = "xcvu9p-flga2104-2L-e"

# # Tcl 模板
# tcl_template = """
# open_project -reset {project_dir}
# add_files {cpp_file} -cflags "-std=c++17"
# set_top {top_function}
# open_solution -reset solution1
# set_part {fpga_part}
# create_clock -period 10 -name default
# csynth_design
# exit
# """

# def get_xml_text(element, path):
#     """从XML安全取值"""
#     if element is None:
#         return None
#     node = element.find(path)
#     if node is not None and node.text not in (None, ""):
#         return node.text
#     return None

# def _pick_first(elem, paths):
#     """尝试多个路径，取第一个非空值"""
#     for p in paths:
#         val = get_xml_text(elem, p)
#         if val is not None:
#             return val
#     return None

# # ---------------- Safe converters ----------------
# def safe_int(x, default=0):
#     try:
#         if x is None:
#             return default
#         if isinstance(x, (int, float)):
#             return int(x)
#         x = str(x).strip()
#         if x.lower() in ("undef", "nan", ""):
#             return default
#         return int(float(x))
#     except Exception:
#         return default

# def safe_float(x, default=0.0):
#     try:
#         if x is None:
#             return default
#         if isinstance(x, (int, float)):
#             return float(x)
#         x = str(x).strip()
#         if x.lower() in ("undef", "nan", ""):
#             return default
#         return float(x)
#     except Exception:
#         return default

# # ---------------- RPT fallback ----------------
# _RPT_LAT = re.compile(r"Latency.*?\|\s*(\d+)\s*\|\s*(\d+)", re.IGNORECASE | re.DOTALL)
# _RPT_INT = re.compile(r"Interval.*?\|\s*(\d+)\s*\|\s*(\d+)", re.IGNORECASE | re.DOTALL)
# _RPT_CLK = re.compile(r"\|\s*ap_clk\s*\|\s*([\d\.]+)\s*ns\s*\|\s*([\d\.]+)\s*ns", re.IGNORECASE)

# def parse_rpt_fallback(top_function, project_dir):
#     rpt_path = os.path.join(project_dir, "solution1", "syn", "report", f"{top_function}_csynth.rpt")
#     if not os.path.exists(rpt_path):
#         return {}
#     txt = open(rpt_path, "r", errors="ignore").read()
#     res = {}
#     m = _RPT_LAT.search(txt)
#     if m:
#         res["latency_min"], res["latency_max"] = int(m.group(1)), int(m.group(2))
#         res["latency_avg"] = int(round((res["latency_min"] + res["latency_max"]) / 2))
#     m2 = _RPT_INT.search(txt)
#     if m2:
#         res["interval_min"], res["interval_max"] = int(m2.group(1)), int(m2.group(2))
#         res["interval_avg"] = int(round((res["interval_min"] + res["interval_max"]) / 2))
#     m3 = _RPT_CLK.search(txt)
#     if m3:
#         res["target_period_ns"], res["estimated_period_ns"] = float(m3.group(1)), float(m3.group(2))
#     return res

# # ---------------- XML 优先，RPT 兜底 ----------------
# def parse_hls_report(top_function, project_dir):
#     report_xml = os.path.join(project_dir, "solution1", "syn", "report", f"{top_function}_csynth.xml")
#     data = {}

#     if os.path.exists(report_xml):
#         try:
#             tree = ET.parse(report_xml)
#             root = tree.getroot()
#             perf = root.find('.//PerformanceEstimates')
#             area = root.find('.//AreaEstimates')

#             # latency
#             data["latency_min"] = safe_int(_pick_first(perf, ['.//MinimumLatency', './/Latency/Min']))
#             data["latency_max"] = safe_int(_pick_first(perf, ['.//MaximumLatency', './/Latency/Max']))
#             data["latency_avg"] = safe_int(_pick_first(perf, ['.//Average-caseLatency', './/Latency/Avg-case']))
#             if data["latency_avg"] == 0 and data["latency_min"] and data["latency_max"]:
#                 data["latency_avg"] = (data["latency_min"] + data["latency_max"]) // 2

#             # interval
#             data["interval_min"] = safe_int(_pick_first(perf, ['.//Interval/Min', './/MinimumInterval']))
#             data["interval_max"] = safe_int(_pick_first(perf, ['.//Interval/Max', './/MaximumInterval']))
#             data["interval_avg"] = safe_int(_pick_first(perf, ['.//Avg-caseInterval', './/Interval/Avg-case']))
#             if data["interval_avg"] == 0 and data["interval_min"] and data["interval_max"]:
#                 data["interval_avg"] = (data["interval_min"] + data["interval_max"]) // 2

#             # area
#             data["ff"]   = safe_int(_pick_first(area, ['.//FF']))
#             data["lut"]  = safe_int(_pick_first(area, ['.//LUT']))
#             data["bram"] = safe_int(_pick_first(area, ['.//BRAM_18K', './/BRAM']))
#             data["dsp"]  = safe_int(_pick_first(area, ['.//DSP']))

#             # clock
#             data["target_period_ns"] = safe_float(_pick_first(perf, ['.//TargetCP', './/ClockPeriod/Target']))
#             data["estimated_period_ns"] = safe_float(_pick_first(perf, [
#                 './/EstimatedCP', './/ClockPeriod/Estimated', './/EstimatedClockPeriod'
#             ]))

#         except Exception as e:
#             print(f"[hls_runner.py] XML parse error: {e}")

#     # fallback with rpt
#     rpt_data = parse_rpt_fallback(top_function, project_dir)
#     for k, v in rpt_data.items():
#         if not data.get(k):
#             data[k] = v

#     # latency in ns
#     cp = data.get("estimated_period_ns") or data.get("target_period_ns") or 0.0
#     if cp and data.get("latency_avg", 0):
#         data["latency_ns_avg"] = data["latency_avg"] * cp

#     return data if data else None

# # ---------------- HLS 执行 ----------------
# def run_hls_and_parse(cpp_file_path, top_function, project_dir):
#     tcl_content = tcl_template.format(
#         project_dir=project_dir,
#         cpp_file=cpp_file_path,
#         top_function=top_function,
#         fpga_part=FPGA_PART
#     )
#     os.makedirs(project_dir, exist_ok=True)
#     tcl_path = os.path.join(project_dir, "run_hls.tcl")
#     open(tcl_path, "w").write(tcl_content)

#     print(f"[hls_runner.py] Starting HLS synthesis in '{project_dir}'...")
#     try:
#         process = subprocess.run([VITIS_HLS_PATH, "-f", tcl_path],
#                                  capture_output=True, text=True, timeout=900)
#         if process.returncode != 0:
#             print(f"[hls_runner.py] WARNING: HLS exited with {process.returncode}")

#         report_xml = os.path.join(project_dir, "solution1/syn/report", f"{top_function}_csynth.xml")
#         report_rpt = os.path.join(project_dir, "solution1/syn/report", f"{top_function}_csynth.rpt")
#         if not (os.path.exists(report_xml) or os.path.exists(report_rpt)):
#             print("[hls_runner.py] ERROR: Report missing!")
#             return False, None

#         ppa_data = parse_hls_report(top_function, project_dir)
#         if ppa_data:
#             print("[hls_runner.py] Success.")
#             return True, ppa_data
#         return False, None
#     except Exception as e:
#         print(f"[hls_runner.py] Exception: {e}")
#         return False, None
#     finally:
#         if os.path.exists(project_dir):
#             shutil.rmtree(project_dir)

# # ---------------- 主程序 ----------------
# def main():
#     parser = argparse.ArgumentParser()
#     parser.add_argument("optimized_cpp", help="Path to optimized C++")
#     parser.add_argument("--top", required=True)
#     parser.add_argument("--solution", required=True)
#     parser.add_argument("--graph-nodes", required=True)
#     parser.add_argument("--graph-edges", required=True)
#     parser.add_argument("--dataset-file", required=True)
#     parser.add_argument("--project-dir", required=True)
#     args = parser.parse_args()

#     success, real_ppa = run_hls_and_parse(args.optimized_cpp, args.top, args.project_dir)
#     if success and real_ppa:
#         record = {
#             "timestamp": datetime.now().isoformat(),
#             "graph_nodes_file": os.path.basename(args.graph_nodes),
#             "graph_edges_file": os.path.basename(args.graph_edges),
#             "solution": json.loads(args.solution),
#             "real_ppa": real_ppa
#         }
#         with open(args.dataset_file, "a") as f:
#             f.write(json.dumps(record) + "\n")
#         print("[hls_runner.py] Data point saved.")
#     else:
#         print("[hls_runner.py] Data point not saved.")

# if __name__ == "__main__":
#     main()


#!/usr/bin/env python3
# /home/CryptoHLS/libs/ModelDeploy/scripts/hls_runner.py
import argparse
import subprocess
import os
import json
import re
import xml.etree.ElementTree as ET
from datetime import datetime
import shutil
import hashlib

print("--- EXECUTING HLS_RUNNER_PY [Cache+Clock+Latency+Interval Fallback + SafeParse V9] ---")

# =========================
# 配置
# =========================
VITIS_HLS_PATH = os.environ.get("VITIS_HLS_BIN", "/home/Xilinx/Vitis_HLS/2021.1/bin/vitis_hls")
FPGA_PART = os.environ.get("CRYPTOHLS_PART", "xcvu9p-flga2104-2L-e")
CLOCK_PERIOD_NS = float(os.environ.get("CRYPTOHLS_CLOCK_NS", "10"))
CACHE_DIR = os.environ.get("HLS_CACHE_DIR", "/home/CryptoHLS/.hls_cache")
KEEP_HLS = os.environ.get("CRYPTOHLS_KEEP_HLS", "0") == "1"
RETRY_ON_FAIL = int(os.environ.get("CRYPTOHLS_RETRY", "1"))  # 失败重试次数

# =========================
# Tcl 模板
# =========================
tcl_template = """
open_project -reset {project_name}
add_files {cpp_file} -cflags "-std=c++17"
set_top {top_function}
open_solution -reset solution1
set_part {fpga_part}
create_clock -period {clock_ns} -name default
csynth_design
exit
"""

# =========================
# 安全 XML 工具
# =========================
def get_xml_text(element, path):
    if element is None:
        return None
    node = element.find(path)
    if node is not None and node.text not in (None, ""):
        return node.text
    return None

def _pick_first(elem, paths):
    for p in paths:
        val = get_xml_text(elem, p)
        if val is not None:
            return val
    return None

# ---------------- Safe converters ----------------
def safe_int(x, default=0):
    try:
        if x is None:
            return default
        if isinstance(x, (int, float)):
            return int(x)
        s = str(x).strip()
        if s.lower() in ("undef", "nan", ""):
            return default
        return int(float(s))
    except Exception:
        return default

def safe_float(x, default=0.0):
    try:
        if x is None:
            return default
        if isinstance(x, (int, float)):
            return float(x)
        s = str(x).strip()
        if s.lower() in ("undef", "nan", ""):
            return default
        return float(s)
    except Exception:
        return default

# =========================
# RPT fallback 解析
# =========================
_RPT_LAT = re.compile(r"Latency\s*\(cycles\).*?\|\s*(\d+)\s*\|\s*(\d+)", re.IGNORECASE | re.DOTALL)
_RPT_INT = re.compile(r"Interval\s*\(cycles\).*?\|\s*(\d+)\s*\|\s*(\d+)", re.IGNORECASE | re.DOTALL)
_RPT_CLK = re.compile(r"\|\s*ap_clk\s*\|\s*([\d\.]+)\s*ns\s*\|\s*([\d\.]+)\s*ns", re.IGNORECASE)

def parse_rpt_fallback(top_function, project_dir):
    rpt_path = os.path.join(project_dir, "solution1", "syn", "report", f"{top_function}_csynth.rpt")
    if not os.path.exists(rpt_path):
        return {}
    try:
        txt = open(rpt_path, "r", errors="ignore").read()
    except Exception:
        return {}
    res = {}
    m = _RPT_LAT.search(txt)
    if m:
        res["latency_min"], res["latency_max"] = int(m.group(1)), int(m.group(2))
        res["latency_avg"] = int(round((res["latency_min"] + res["latency_max"]) / 2))
    m2 = _RPT_INT.search(txt)
    if m2:
        res["interval_min"], res["interval_max"] = int(m2.group(1)), int(m2.group(2))
        res["interval_avg"] = int(round((res["interval_min"] + res["interval_max"]) / 2))
    m3 = _RPT_CLK.search(txt)
    if m3:
        res["target_period_ns"], res["estimated_period_ns"] = float(m3.group(1)), float(m3.group(2))
    return res

# =========================
# XML 优先，RPT 兜底
# =========================
def parse_hls_report(top_function, project_dir):
    report_xml = os.path.join(project_dir, "solution1", "syn", "report", f"{top_function}_csynth.xml")
    data = {}

    if os.path.exists(report_xml):
        try:
            tree = ET.parse(report_xml)
            root = tree.getroot()
            perf = root.find('.//PerformanceEstimates')
            area = root.find('.//AreaEstimates')

            # latency
            data["latency_min"] = safe_int(_pick_first(perf, ['.//MinimumLatency', './/Latency/Min']))
            data["latency_max"] = safe_int(_pick_first(perf, ['.//MaximumLatency', './/Latency/Max']))
            data["latency_avg"] = safe_int(_pick_first(perf, ['.//Average-caseLatency', './/Latency/Avg-case']))
            if data.get("latency_avg", 0) == 0 and data.get("latency_min") and data.get("latency_max"):
                data["latency_avg"] = (data["latency_min"] + data["latency_max"]) // 2

            # interval
            data["interval_min"] = safe_int(_pick_first(perf, ['.//Interval/Min', './/MinimumInterval']))
            data["interval_max"] = safe_int(_pick_first(perf, ['.//Interval/Max', './/MaximumInterval']))
            data["interval_avg"] = safe_int(_pick_first(perf, ['.//Avg-caseInterval', './/Interval/Avg-case']))
            if data.get("interval_avg", 0) == 0 and data.get("interval_min") and data.get("interval_max"):
                data["interval_avg"] = (data["interval_min"] + data["interval_max"]) // 2

            # area
            data["ff"]   = safe_int(_pick_first(area, ['.//FF']))
            data["lut"]  = safe_int(_pick_first(area, ['.//LUT']))
            data["bram"] = safe_int(_pick_first(area, ['.//BRAM_18K', './/BRAM']))
            data["dsp"]  = safe_int(_pick_first(area, ['.//DSP']))

            # clock
            data["target_period_ns"] = safe_float(_pick_first(perf, ['.//TargetCP', './/ClockPeriod/Target']))
            data["estimated_period_ns"] = safe_float(_pick_first(perf, [
                './/EstimatedCP', './/ClockPeriod/Estimated', './/EstimatedClockPeriod'
            ]))

        except Exception as e:
            print(f"[hls_runner.py] XML parse error: {e}")

    # fallback with rpt
    rpt_data = parse_rpt_fallback(top_function, project_dir)
    for k, v in rpt_data.items():
        if not data.get(k):
            data[k] = v

    # latency in ns
    cp = data.get("estimated_period_ns") or data.get("target_period_ns") or 0.0
    if cp and data.get("latency_avg", 0):
        data["latency_ns_avg"] = data["latency_avg"] * cp

    # 合法性判定
    if not data:
        return None
    if data.get("latency_avg", 0) <= 0 and data.get("interval_avg", 0) <= 0 and data.get("lut", 0) <= 0:
        # 完全缺失视为无效
        return None
    return data

# =========================
# 方案 canonical 化 & 缓存
# =========================
def canonical_solution(sol_list):
    """将 solution 排序、参数键排序，便于做缓存/去重"""
    norm = []
    for s in (sol_list or []):
        params = s.get("params", {}) or {}
        params_sorted = {k: params[k] for k in sorted(params.keys())}
        norm.append({
            "function": s.get("function", ""),
            "line": int(s.get("line", 0) or 0),
            "type": s.get("type", ""),
            "params": params_sorted
        })
    # 排序：function, line, type
    norm.sort(key=lambda x: (x["function"], x["line"], x["type"]))
    return norm

def canonical_key(top, part, clock_ns, sol_list):
    payload = {
        "top": (top or "").lower(),
        "part": (part or "").lower(),
        "clock": float(clock_ns or 0.0),
        "solution": canonical_solution(sol_list),
    }
    s = json.dumps(payload, sort_keys=True, ensure_ascii=False)
    return hashlib.sha1(s.encode("utf-8")).hexdigest()

def cache_get(key):
    try:
        os.makedirs(CACHE_DIR, exist_ok=True)
        path = os.path.join(CACHE_DIR, key + ".json")
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
    except Exception:
        return None
    return None

def cache_put(key, result):
    try:
        os.makedirs(CACHE_DIR, exist_ok=True)
        path = os.path.join(CACHE_DIR, key + ".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False)
    except Exception as e:
        print(f"[hls_runner.py] cache write failed: {e}")

# =========================
# HLS 执行
# =========================
def run_hls_once(cpp_file_path, top_function, project_dir, clock_ns):
    workdir = os.path.abspath(os.path.dirname(project_dir))
    project_name = os.path.basename(os.path.abspath(project_dir))
    tcl_content = tcl_template.format(
        project_name=project_name, 
        project_dir=project_dir,
        cpp_file=cpp_file_path,
        top_function=top_function,
        fpga_part=FPGA_PART,
        clock_ns=clock_ns
    )
    os.makedirs(project_dir, exist_ok=True)
    tcl_path = os.path.join(project_dir, "run_hls.tcl")
    with open(tcl_path, "w") as f:
        f.write(tcl_content)

    print(f"[hls_runner.py] Starting HLS synthesis in '{project_dir}'...")
    process = subprocess.run([VITIS_HLS_PATH, "-f", tcl_path],
                             capture_output=True, text=True, timeout=1800, cwd=workdir)
    if process.returncode != 0:
        print(f"[hls_runner.py] WARNING: HLS exited with {process.returncode}")
    report_xml = os.path.join(project_dir, "solution1/syn/report", f"{top_function}_csynth.xml")
    report_rpt = os.path.join(project_dir, "solution1/syn/report", f"{top_function}_csynth.rpt")
    if not (os.path.exists(report_xml) or os.path.exists(report_rpt)):
        print("[hls_runner.py] ERROR: Report missing!")
        return False, None, process.stdout + "\n" + process.stderr

    ppa_data = parse_hls_report(top_function, project_dir)
    if ppa_data:
        print("[hls_runner.py] Success.")
        return True, ppa_data, process.stdout + "\n" + process.stderr
    return False, None, process.stdout + "\n" + process.stderr

def run_hls_and_parse(cpp_file_path, top_function, project_dir, clock_ns, sol_list):
    # 先查缓存
    key = canonical_key(top_function, FPGA_PART, clock_ns, sol_list)
    hit = cache_get(key)
    if hit is not None and hit.get("ok"):
        print("[hls_runner.py] Cache hit.")
        return True, hit.get("real_ppa"), "CACHE_HIT"

    # 运行（失败重试 RETRY_ON_FAIL 次）
    tries = max(1, 1 + RETRY_ON_FAIL)
    logs = ""
    ok, ppa = False, None
    for t in range(tries):
        ok, ppa, logs = run_hls_once(cpp_file_path, top_function, project_dir, clock_ns)
        if ok:
            break

    # 清理目录
    if not KEEP_HLS and os.path.exists(project_dir):
        try:
            shutil.rmtree(project_dir)
        except Exception as e:
            print(f"[hls_runner.py] WARN: cleanup failed: {e}")

    # 写缓存（仅成功）
    if ok and ppa:
        cache_put(key, {"ok": True, "real_ppa": ppa, "log": logs})

    return ok, ppa, logs

# =========================
# 主程序
# =========================
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("optimized_cpp", help="Path to optimized C++")
    parser.add_argument("--top", required=True)
    parser.add_argument("--solution", required=True)
    parser.add_argument("--graph-nodes", required=True)
    parser.add_argument("--graph-edges", required=True)
    parser.add_argument("--dataset-file", required=True)
    parser.add_argument("--project-dir", required=True)
    parser.add_argument("--clock-ns", type=float, default=CLOCK_PERIOD_NS)
    args = parser.parse_args()

    # 解析 solution
    try:
        sol_list = json.loads(args.solution)
    except Exception as e:
        print(f"[hls_runner.py] ERROR: invalid solution json: {e}")
        return

    # 跑 HLS + 解析
    success, real_ppa, logs = run_hls_and_parse(
        args.optimized_cpp, args.top, args.project_dir, args.clock_ns, sol_list
    )

    if success and real_ppa:
        record = {
            "timestamp": datetime.now().isoformat(),
            "graph_nodes_file": os.path.basename(args.graph_nodes),
            "graph_edges_file": os.path.basename(args.graph_edges),
            "solution": canonical_solution(sol_list),
            "real_ppa": real_ppa
        }
        with open(args.dataset_file, "a") as f:
            f.write(json.dumps(record) + "\n")
        print("[hls_runner.py] Data point saved.")
    else:
        print("[hls_runner.py] Data point NOT saved.")
        # 可以根据需要把失败日志落到单独文件
        # with open(args.dataset_file + ".fail.log", "a") as f:
        #     f.write(f"[{datetime.now().isoformat()}] {logs}\n")

if __name__ == "__main__":
    main()
