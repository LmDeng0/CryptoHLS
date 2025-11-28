#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Basic dataset cleaning for CryptoHLS HLS jsonl logs.

Usage example (from project root /home/CryptoHLS):

  python3 libs/ModelDeploy/scripts/clean_dataset.py \
    --inputs hls_dataset.jsonl sha1_hls_dataset.jsonl aes_hls_dataset.jsonl \
    --output mk_hls_dataset_clean.jsonl

规则（v1 简化版）：
  - 只保留 real_ppa 中 latency_avg / interval_avg / lut 三个字段都存在且为正数的样本
  - 过滤掉任何含有 'undef' / 'NaN' / 空串的 PPA 数值
  - 过滤掉 latency_avg 或 lut 为 0 的样本
  - 按百分位裁剪极端值（可选开关，默认开）
"""

import argparse
import json
import math
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple


def safe_float(x) -> Optional[float]:
    if x is None:
        return None
    if isinstance(x, (int, float)):
        return float(x)
    s = str(x).strip().lower()
    if s in ("", "undef", "nan", "inf", "-inf"):
        return None
    try:
        return float(s)
    except Exception:
        return None


def load_jsonl(path: Path) -> List[Dict[str, Any]]:
    items: List[Dict[str, Any]] = []
    with path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                items.append(json.loads(line))
            except Exception as e:
                print(f"[clean_dataset] WARN: skip invalid json line in {path.name}: {e}")
    return items


def basic_filter(record: Dict[str, Any]) -> Tuple[bool, str]:
    """
    返回 (是否保留, 原因/空串)
    """
    if "real_ppa" not in record or not isinstance(record["real_ppa"], dict):
        return False, "no_real_ppa"

    ppa = record["real_ppa"]
    lat = safe_float(ppa.get("latency_avg"))
    ii  = safe_float(ppa.get("interval_avg"))
    lut = safe_float(ppa.get("lut"))

    if lat is None or ii is None or lut is None:
        return False, "ppa_missing"

    if lat <= 0:
        return False, "lat_le_zero"
    if lut <= 0:
        return False, "lut_le_zero"
    if ii < 0:
        return False, "ii_lt_zero"

    # 过滤 interval_avg 为 0 的“完全组合逻辑”样本（对 GNN 很难学，先丢）
    if ii == 0:
        return False, "ii_eq_zero"

    # solution 字段必须存在且非空
    sol = record.get("solution")
    if not isinstance(sol, list) or len(sol) == 0:
        return False, "no_solution"

    # solution 中不允许出现 function 为空串的 pragma
    for cmd in sol:
        if not isinstance(cmd, dict):
            return False, "bad_cmd"
        f = cmd.get("function", "")
        if not isinstance(f, str):
            return False, "bad_func_field"
        if f.strip() == "":
            return False, "empty_func"

    return True, ""


def compute_percentiles(vals: List[float], ps: List[float]) -> Dict[float, float]:
    if not vals:
        return {p: math.nan for p in ps}
    vals_sorted = sorted(vals)
    n = len(vals_sorted)
    out = {}
    for p in ps:
        if n == 1:
            out[p] = vals_sorted[0]
            continue
        k = (n - 1) * p / 100.0
        f = math.floor(k)
        c = math.ceil(k)
        if f == c:
            out[p] = vals_sorted[int(k)]
        else:
            d0 = vals_sorted[int(f)] * (c - k)
            d1 = vals_sorted[int(c)] * (k - f)
            out[p] = d0 + d1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--inputs",
        nargs="+",
        required=True,
        help="Input jsonl files (one or more). e.g. hls_dataset.jsonl sha1_hls_dataset.jsonl",
    )
    ap.add_argument(
        "--output",
        required=True,
        help="Output cleaned jsonl file, e.g. mk_hls_dataset_clean.jsonl",
    )
    ap.add_argument(
        "--no_clip",
        action="store_true",
        help="Disable percentile-based extreme value clipping",
    )
    args = ap.parse_args()

    in_paths = [Path(p).resolve() for p in args.inputs]
    out_path = Path(args.output).resolve()

    print("[clean_dataset] Inputs:")
    for p in in_paths:
        print("  -", p)
    print("[clean_dataset] Output:", out_path)

    # 1) 读入所有样本
    raw_records: List[Dict[str, Any]] = []
    for p in in_paths:
        if not p.exists():
            print(f"[clean_dataset] WARN: input file not found: {p}")
            continue
        items = load_jsonl(p)
        print(f"[clean_dataset] Loaded {len(items)} records from {p.name}")
        raw_records.extend(items)

    print(f"[clean_dataset] Total raw records: {len(raw_records)}")

    # 2) 基础过滤
    kept: List[Dict[str, Any]] = []
    stats_drop = {}
    for rec in raw_records:
        ok, reason = basic_filter(rec)
        if ok:
            kept.append(rec)
        else:
            stats_drop[reason] = stats_drop.get(reason, 0) + 1

    print(f"[clean_dataset] After basic filter: {len(kept)} kept, {len(raw_records) - len(kept)} dropped")
    if stats_drop:
        print("[clean_dataset] Drop reasons:")
        for k, v in sorted(stats_drop.items(), key=lambda kv: kv[1], reverse=True):
            print(f"  - {k}: {v}")

    if not kept:
        print("[clean_dataset] No samples survived after basic filter, abort.")
        return

    # 3) 计算 PPA 分布并裁剪极端值
    lat_list = []
    ii_list = []
    lut_list = []
    for rec in kept:
        ppa = rec["real_ppa"]
        lat_list.append(float(ppa["latency_avg"]))
        ii_list.append(float(ppa["interval_avg"]))
        lut_list.append(float(ppa["lut"]))

    lat_stats = compute_percentiles(lat_list, [0, 25, 50, 75, 90, 95, 99, 99.9])
    ii_stats  = compute_percentiles(ii_list,  [0, 25, 50, 75, 90, 95, 99, 99.9])
    lut_stats = compute_percentiles(lut_list, [0, 25, 50, 75, 90, 95, 99, 99.9])

    print("[clean_dataset] Latency percentiles:", lat_stats)
    print("[clean_dataset] Interval percentiles:", ii_stats)
    print("[clean_dataset] LUT percentiles    :", lut_stats)

    if args.no_clip:
        final_records = kept
        print("[clean_dataset] Percentile clipping disabled (--no_clip).")
    else:
        lat_hi = lat_stats[99.9]
        lut_hi = lut_stats[99.9]
        # interval 理论上不裁剪（用来区分 II=1,2,4），这里只过滤极端大值
        ii_hi = ii_stats[99.9]

        final_records = []
        drop_extreme = 0
        for rec in kept:
            ppa = rec["real_ppa"]
            lat = float(ppa["latency_avg"])
            ii  = float(ppa["interval_avg"])
            lut = float(ppa["lut"])
            if lat > lat_hi * 10:   # 非常极端的 latency
                drop_extreme += 1
                continue
            if lut > lut_hi * 10:   # 非常极端的 lut
                drop_extreme += 1
                continue
            if ii  > ii_hi  * 10:
                drop_extreme += 1
                continue
            final_records.append(rec)

        print(f"[clean_dataset] Extreme clipping dropped {drop_extreme} samples.")
        print(f"[clean_dataset] Final kept samples: {len(final_records)}")

    # 4) 写出结果
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        for rec in final_records:
            f.write(json.dumps(rec) + "\n")

    print("[clean_dataset] Done. Cleaned dataset written to:", out_path)


if __name__ == "__main__":
    main()
