#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os
import sys
import shutil
import json
import csv
import re
import subprocess
from pathlib import Path
from typing import Dict, Optional, List

REPORT_KEYS = [
    "Latency min", "Latency max", "Latency avg",
    "Interval min", "Interval max", "Interval avg",
    "LUT", "FF", "BRAM_18K", "URAM", "DSP",
    "Target Period (ns)", "Estimated Clock Period (ns)"
]

def run_cmd(cmd, cwd=None):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return p.returncode, p.stdout

def write_tcl(tcl_path: Path, top: str, source: Path, part: str, clock: float, proj_name: str):
    tcl = f"""
open_project {proj_name}
set_top {top}
add_files {source}
open_solution "sol1"
set_part {{{part}}}
create_clock -period {clock} -name default
csynth_design
exit
""".strip() + "\n"
    tcl_path.write_text(tcl)

def find_csynth_report(proj_dir: Path, top: str) -> Optional[Path]:
    candidates = list(proj_dir.rglob(f"{top}_csynth.rpt"))
    if candidates:
        candidates.sort(key=lambda p: len(str(p)))
        return candidates[-1]
    any_rpts = list(proj_dir.rglob("*_csynth.rpt"))
    if any_rpts:
        any_rpts.sort(key=lambda p: len(str(p)))
        return any_rpts[-1]
    return None

def _parse_perf_block(txt: str, res: Dict[str, float]) -> None:
    # 兼容两种格式：
    # 1) "Latency (cycles)  min  123  max  456  avg  200"
    # 2) "Latency (cycles): min = 123, max = 456, avg = 200"
    lat_patterns = [
        r"Latency\s*\(cycles\)\s*min\s*(\d+)\s*max\s*(\d+)\s*avg\s*([\d\.]+)",
        r"Latency\s*\(cycles\)\s*:\s*min\s*=\s*(\d+)\s*,\s*max\s*=\s*(\d+)\s*,\s*avg\s*=\s*([\d\.]+)"
    ]
    ii_patterns = [
        r"Interval\s*\(II\)\s*min\s*(\d+)\s*max\s*(\d+)\s*avg\s*([\d\.]+)",
        r"Interval\s*\(II\)\s*:\s*min\s*=\s*(\d+)\s*,\s*max\s*=\s*(\d+)\s*,\s*avg\s*=\s*([\d\.]+)"
    ]
    for pat in lat_patterns:
        m = re.search(pat, txt, re.I)
        if m:
            res["Latency min"] = float(m.group(1))
            res["Latency max"] = float(m.group(2))
            res["Latency avg"] = float(m.group(3))
            break
    for pat in ii_patterns:
        m = re.search(pat, txt, re.I)
        if m:
            res["Interval min"] = float(m.group(1))
            res["Interval max"] = float(m.group(2))
            res["Interval avg"] = float(m.group(3))
            break

    # 时钟信息（有些版本不带）
    m_target = re.search(r"Target\s+CP\s*\(ns\)\s*=\s*([\d\.]+)", txt, re.I)
    if m_target:
        res["Target Period (ns)"] = float(m_target.group(1))
    m_est = re.search(r"Estimated\s+Clock\s+Period\s*\(ns\)\s*=\s*([\d\.]+)", txt, re.I)
    if m_est:
        res["Estimated Clock Period (ns)"] = float(m_est.group(1))

def _split_bar_row(line: str) -> List[str]:
    # 把 "|  a  |  b  |" 拆成 ["a","b"]
    parts = [p.strip() for p in line.strip().strip("|").split("|")]
    return parts

def _parse_util_table(txt: str, res: Dict[str, float]) -> None:
    # 找资源利用小节的 Summary 表头：
    # | Name | BRAM_18K | DSP | FF | LUT | URAM |
    header_idx = -1
    lines = txt.splitlines()
    for i, line in enumerate(lines):
        if re.search(r"^\s*\|\s*Name\s*\|", line):
            header_idx = i
            break
    if header_idx < 0:
        return

    header_cols = _split_bar_row(lines[header_idx])
    # 期待 header_cols 形如 ["Name","BRAM_18K","DSP","FF","LUT","URAM"]
    # 往下找以 "| Total |" 开头的一行
    total_line = None
    for j in range(header_idx+1, min(header_idx+50, len(lines))):
        if re.match(r"^\s*\|\s*Total\s*\|", lines[j]):
            total_line = lines[j]
            break
    if not total_line:
        return

    total_cols = _split_bar_row(total_line)
    # 对齐列：Name 列为 total_cols[0]，资源列从 1 开始
    name_to_value = {}
    for k in range(1, min(len(header_cols), len(total_cols))):
        col_name = header_cols[k]
        val_str = total_cols[k]
        if re.fullmatch(r"\d+", val_str):
            name_to_value[col_name.upper()] = float(val_str)

    # 按我们关心的 key 映射
    mapping = {
        "LUT": ["LUT"],
        "FF": ["FF", "REGISTERS", "CLB REGISTERS"],
        "BRAM_18K": ["BRAM_18K", "BRAM"],
        "URAM": ["URAM"],
        "DSP": ["DSP", "DSP48E", "DSP48"]
    }
    for out_key, aliases in mapping.items():
        for alias in aliases:
            if alias.upper() in name_to_value:
                res[out_key] = name_to_value[alias.upper()]
                break

def parse_csynth_report(rpt_path: Path) -> Dict[str, float]:
    txt = rpt_path.read_text(errors="ignore")
    res: Dict[str, float] = {}

    _parse_perf_block(txt, res)
    _parse_util_table(txt, res)

    return res

def run_hls_once(source: Path, top: str, part: str, clock: float, work_root: Path, tag: str) -> Dict[str, float]:
    proj_dir = work_root / f"hls_eval_{tag}"
    if proj_dir.exists():
        shutil.rmtree(proj_dir)
    proj_dir.mkdir(parents=True, exist_ok=True)

    tcl_path = proj_dir / "run.tcl"
    write_tcl(tcl_path, top=top, source=source.resolve(), part=part, clock=clock, proj_name="hls_eval")

    run_tcl_abs = str(tcl_path.resolve())
    print(f"[INFO] Running vitis_hls with {run_tcl_abs}")
    code, out = run_cmd(["vitis_hls", "-f", run_tcl_abs], cwd=proj_dir)
    (proj_dir / "vitis_hls_stdout.log").write_text(out)

    rpt = find_csynth_report(proj_dir, top)
    if code != 0 or not rpt or not rpt.exists():
        print(out)
        raise RuntimeError(f"[{tag}] Vitis HLS 运行失败或报告未找到：{rpt}")

    metrics = parse_csynth_report(rpt)
    metrics["__report_path"] = str(rpt)
    metrics["__work_dir"] = str(proj_dir)
    return metrics

def fmt(v):
    if v is None:
        return "-"
    if isinstance(v, float) and abs(v - int(v)) < 1e-9:
        v = int(v)
    return str(v)

def print_compare_table(base: Dict[str, float], opt: Dict[str, float]):
    print("\n=== Vitis HLS Comparison (Baseline vs Optimized) ===")
    w = 28
    print(f"{'Metric':{w}} | {'Baseline':>12} | {'Optimized':>12} | {'Delta (Opt-Base)':>16}")
    print("-"*(w+3+12+3+12+3+16))
    keys = REPORT_KEYS
    for k in keys:
        b = base.get(k)
        o = opt.get(k)
        d = (o - b) if (b is not None and o is not None) else None
        print(f"{k:{w}} | {fmt(b):>12} | {fmt(o):>12} | {fmt(d):>16}")

def save_json_csv(base: Dict[str,float], opt: Dict[str,float], json_path: Optional[Path], csv_path: Optional[Path]):
    if json_path:
        json_path.parent.mkdir(parents=True, exist_ok=True)
        with open(json_path, "w") as f:
            json.dump({"baseline": base, "optimized": opt}, f, indent=2)
    if csv_path:
        csv_path.parent.mkdir(parents=True, exist_ok=True)
        with open(csv_path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["metric", "baseline", "optimized", "delta"])
            for k in REPORT_KEYS:
                b = base.get(k)
                o = opt.get(k)
                d = (o - b) if (b is not None and o is not None) else None
                w.writerow([k, b, o, d])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--optimized", required=True)
    ap.add_argument("--top", required=True)
    ap.add_argument("--part", required=True)
    ap.add_argument("--clock", type=float, default=10.0)
    ap.add_argument("--work", default="build/hls_compare_out")
    ap.add_argument("--json", default=None)
    ap.add_argument("--csv", default=None)
    args = ap.parse_args()

    work_root = Path(args.work)
    work_root.mkdir(parents=True, exist_ok=True)

    baseline_src = Path(args.baseline)
    optimized_src = Path(args.optimized)
    if not baseline_src.exists():
        print(f"[ERR] baseline 源文件不存在：{baseline_src}")
        sys.exit(1)
    if not optimized_src.exists():
        print(f"[ERR] optimized 源文件不存在：{optimized_src}")
        sys.exit(1)

    print("[INFO] Running baseline...")
    base = run_hls_once(baseline_src, args.top, args.part, args.clock, work_root, tag="baseline")
    print("[INFO] Running optimized...")
    opt  = run_hls_once(optimized_src, args.top, args.part, args.clock, work_root, tag="optimized")

    print_compare_table(base, opt)

    json_path = Path(args.json) if args.json else None
    csv_path  = Path(args.csv)  if args.csv  else None
    save_json_csv(base, opt, json_path, csv_path)

    print("\n[INFO] Reports:")
    print("  Baseline rpt:", base.get("__report_path", "-"))
    print("  Optimized rpt:", opt.get("__report_path", "-"))
    print("[INFO] Work dirs:")
    print("  Baseline dir:", base.get("__work_dir", "-"))
    print("  Optimized dir:", opt.get("__work_dir", "-"))

if __name__ == "__main__":
    main()
