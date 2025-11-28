# import os
# import json
# import random
# import subprocess
# import numpy as np

# DATASET = "hls_dataset.jsonl"
# PREDICT_SCRIPT = "libs/ModelDeploy/scripts/predict.py"
# GRAPH_DIR = "./build/build"
# N_SAMPLES = 20  # 抽取样本数

# def run_predict(nodes_csv, edges_csv, solution_json):
#     cmd = [
#         "python3", PREDICT_SCRIPT,
#         "--graph-nodes", nodes_csv,
#         "--graph-edges", edges_csv,
#         "--solution", json.dumps(solution_json)
#     ]
#     result = subprocess.run(cmd, capture_output=True, text=True)
#     if result.returncode != 0:
#         print("Error running predict.py:", result.stderr)
#         return None
#     try:
#         return json.loads(result.stdout.strip())
#     except Exception as e:
#         print("JSON parse error:", result.stdout)
#         return None

# def relative_error(pred, real):
#     eps = 1e-6
#     return abs(pred - real) / (real + eps) * 100

# def main():
#     # 读取数据集
#     with open(DATASET, "r") as f:
#         lines = [json.loads(line) for line in f if line.strip()]

#     # 随机抽 N 条
#     samples = random.sample(lines, min(N_SAMPLES, len(lines)))

#     errors_latency, errors_interval, errors_lut = [], [], []

#     for entry in samples:
#         nodes_csv = os.path.join(GRAPH_DIR, entry["graph_nodes_file"])
#         edges_csv = os.path.join(GRAPH_DIR, entry["graph_edges_file"])
#         solution = entry.get("solution", [])
#         real = entry.get("real_ppa", {})

#         # 跑预测
#         pred = run_predict(nodes_csv, edges_csv, solution)
#         if pred is None:
#             continue

#         # 真实值
#         real_latency = float(real.get("latency_avg", 0))
#         real_interval = float(real.get("interval_avg", 0))
#         real_lut = float(real.get("lut", 0))

#         # 预测值
#         pred_latency = pred["predicted_latency"]
#         pred_interval = pred["predicted_interval"]
#         pred_lut = pred["predicted_lut"]

#         # 误差
#         err_lat = relative_error(pred_latency, real_latency)
#         err_int = relative_error(pred_interval, real_interval)
#         err_lut = relative_error(pred_lut, real_lut)

#         errors_latency.append(err_lat)
#         errors_interval.append(err_int)
#         errors_lut.append(err_lut)

#         print(f"\n=== Sample ===")
#         print(f"Real: latency={real_latency}, interval={real_interval}, lut={real_lut}")
#         print(f"Pred: latency={pred_latency:.2f}, interval={pred_interval:.2f}, lut={pred_lut:.2f}")
#         print(f"Error: latency={err_lat:.2f}%, interval={err_int:.2f}%, lut={err_lut:.2f}%")

#     print("\n=== Summary ===")
#     print(f"Latency MAPE : {np.mean(errors_latency):.2f}%")
#     print(f"Interval MAPE: {np.mean(errors_interval):.2f}%")
#     print(f"LUT MAPE     : {np.mean(errors_lut):.2f}%")
#     print(f"Overall MAPE : {(np.mean(errors_latency)+np.mean(errors_interval)+np.mean(errors_lut))/3:.2f}%")

# if __name__ == "__main__":
#     main()


#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import json
import argparse
import subprocess
import numpy as np
import random
import csv

DEFAULT_DATASET = "hls_dataset.jsonl"
DEFAULT_GRAPH_DIR = "./build/build"
DEFAULT_PREDICT = "libs/ModelDeploy/scripts/predict.py"

def run_predict(predict_py, nodes_csv, edges_csv, solution_json):
    cmd = [
        "python3", predict_py,
        "--graph-nodes", nodes_csv,
        "--graph-edges", edges_csv,
        "--solution", json.dumps(solution_json, ensure_ascii=False)
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print("[predict.py ERROR]", res.stderr.strip())
        return None
    try:
        return json.loads(res.stdout.strip())
    except Exception:
        print("[predict.py OUTPUT PARSE FAIL]\n", res.stdout[:500])
        return None

def ape(pred, real):
    eps = 1e-6
    return abs(pred - real) / (abs(real) + eps) * 100.0

def load_dataset(path):
    items = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line: 
                continue
            items.append(json.loads(line))
    return items

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", default=DEFAULT_DATASET, help="hls_dataset.jsonl 路径")
    ap.add_argument("--graph-dir", default=DEFAULT_GRAPH_DIR, help="图CSV所在目录")
    ap.add_argument("--predict", default=DEFAULT_PREDICT, help="predict.py 路径")
    ap.add_argument("--samples", type=int, default=50, help="抽样数量（<=0 表示全量）")
    ap.add_argument("--seed", type=int, default=123, help="随机种子（用于可复现实验）")
    ap.add_argument("--csv", default="eval_results.csv", help="保存逐条结果到CSV")
    ap.add_argument("--filter-min-lat", type=float, default=None, help="仅评估 latency >= 此值 的样本")
    ap.add_argument("--filter-max-lat", type=float, default=None, help="仅评估 latency <= 此值 的样本")
    ap.add_argument("--filter-min-lut", type=float, default=None, help="仅评估 lut >= 此值 的样本")
    ap.add_argument("--filter-max-lut", type=float, default=None, help="仅评估 lut <= 此值 的样本")
    args = ap.parse_args()

    random.seed(args.seed)
    np.random.seed(args.seed)

    lines = load_dataset(args.dataset)

    # 可选：简单范围过滤，便于分别看常见区间与极端区间
    def keep(entry):
        r = entry.get("real_ppa", {})
        lat = float(r.get("latency_avg", 0))
        lut = float(r.get("lut", 0))
        if args.filter_min_lat is not None and lat < args.filter_min_lat: return False
        if args.filter_max_lat is not None and lat > args.filter_max_lat: return False
        if args.filter_min_lut is not None and lut < args.filter_min_lut: return False
        if args.filter_max_lut is not None and lut > args.filter_max_lut: return False
        return True

    lines = [e for e in lines if keep(e)]
    if not lines:
        print("没有匹配过滤条件的样本.")
        return

    # 抽样（不重复）
    if args.samples > 0 and args.samples < len(lines):
        lines = random.sample(lines, args.samples)

    rows = []
    for i, entry in enumerate(lines, 1):
        nodes_csv = os.path.join(args.graph_dir, entry["graph_nodes_file"])
        edges_csv = os.path.join(args.graph_dir, entry["graph_edges_file"])
        solution = entry.get("solution", [])
        real = entry.get("real_ppa", {})

        pred = run_predict(args.predict, nodes_csv, edges_csv, solution)
        if pred is None:
            continue

        real_latency  = float(real.get("latency_avg", 0))
        real_interval = float(real.get("interval_avg", 0))
        real_lut      = float(real.get("lut", 0))

        pred_latency  = float(pred.get("predicted_latency", 0))
        pred_interval = float(pred.get("predicted_interval", 0))
        pred_lut      = float(pred.get("predicted_lut", 0))

        err_lat = ape(pred_latency,  real_latency)
        err_int = ape(pred_interval, real_interval)
        err_lut = ape(pred_lut,      real_lut)

        rows.append({
            "graph_nodes_file": entry["graph_nodes_file"],
            "graph_edges_file": entry["graph_edges_file"],
            "latency_real": real_latency,
            "latency_pred": pred_latency,
            "latency_ape%": err_lat,
            "interval_real": real_interval,
            "interval_pred": pred_interval,
            "interval_ape%": err_int,
            "lut_real": real_lut,
            "lut_pred": pred_lut,
            "lut_ape%": err_lut
        })

        print(f"[{i}/{len(lines)}] "
              f"lat R={real_latency:.2f} P={pred_latency:.2f} E={err_lat:.2f}% | "
              f"II  R={real_interval:.2f} P={pred_interval:.2f} E={err_int:.2f}% | "
              f"LUT R={real_lut:.2f} P={pred_lut:.2f} E={err_lut:.2f}%")

    if not rows:
        print("无有效评估结果（predict失败或数据缺失）")
        return

    # 统计
    lat_apes = np.array([r["latency_ape%"] for r in rows])
    int_apes = np.array([r["interval_ape%"] for r in rows])
    lut_apes = np.array([r["lut_ape%"] for r in rows])

    def stats(x):
        return {
            "mean": float(np.mean(x)),
            "median": float(np.median(x)),
            "p90": float(np.percentile(x, 90)),
        }

    s_lat = stats(lat_apes)
    s_int = stats(int_apes)
    s_lut = stats(lut_apes)

    print("\n=== Summary ===")
    print(f"Latency   APE%  -> mean {s_lat['mean']:.2f} | median {s_lat['median']:.2f} | p90 {s_lat['p90']:.2f}")
    print(f"Interval  APE%  -> mean {s_int['mean']:.2f} | median {s_int['median']:.2f} | p90 {s_int['p90']:.2f}")
    print(f"LUT       APE%  -> mean {s_lut['mean']:.2f} | median {s_lut['median']:.2f} | p90 {s_lut['p90']:.2f}")
    print(f"Overall mean of means: { (s_lat['mean'] + s_int['mean'] + s_lut['mean'])/3.0 :.2f}%")

    # 保存CSV
    out_csv = args.csv
    os.makedirs(os.path.dirname(out_csv) or ".", exist_ok=True)
    with open(out_csv, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"\nSaved detailed results to: {out_csv}")

    # 打印最差的若干例子（按 Interval APE% 排序，也可以换成 latency/lut）
    worst_k = min(10, len(rows))
    worst = sorted(rows, key=lambda r: r["interval_ape%"], reverse=True)[:worst_k]
    print("\n=== Worst by Interval APE% ===")
    for r in worst:
        print(f"{r['graph_nodes_file']} | II real={r['interval_real']:.2f}, "
              f"pred={r['interval_pred']:.2f}, APE={r['interval_ape%']:.2f}% | "
              f"lat APE={r['latency_ape%']:.2f}%, LUT APE={r['lut_ape%']:.2f}%")

if __name__ == "__main__":
    main()
