#!/usr/bin/env python3
import json, argparse, numpy as np
from pathlib import Path
from sklearn.ensemble import RandomForestRegressor
from sklearn.metrics import mean_absolute_percentage_error
import joblib

def make_feat(sample):
    p = sample.get("real_ppa", {})
    sol = sample.get("solution", [])
    kinds = [ (s.get("kind") or "").lower() for s in sol ]
    def has(k): return 1 if k in kinds else 0
    def first_arg(k, key="factor", default=0):
        for s in sol:
            if (s.get("kind") or "").lower()==k and key in s: 
                try: return float(s[key])
                except: return default
        return default
    feats = {
        "n_pragmas": len(sol),
        "has_pipeline": has("pipeline"),
        "has_unroll": has("unroll"),
        "has_partition": has("array_partition"),
        "has_dataflow": has("dataflow"),
        "has_stream": has("stream"),
        "unroll_factor": first_arg("unroll","factor",0),
        "partition_factor": first_arg("array_partition","factor",0),
        "partition_dim": first_arg("array_partition","dim",0),
        "est_period": float(p.get("estimated_period_ns",0.0)),
        "kernel_aes": 1 if "aes" in (sample.get("top","")+sample.get("top_function","")).lower() else 0,
        "kernel_sha1": 1 if "sha1" in (sample.get("top","")+sample.get("top_function","")).lower() else 0,
        "n_nodes": float(sample.get("graph_stats",{}).get("n_nodes",0)),
        "n_edges": float(sample.get("graph_stats",{}).get("n_edges",0)),
    }
    x = np.array([feats[k] for k in sorted(feats.keys())], dtype=float)
    y = np.array([
        float(p.get("latency_cycles_avg",0)),
        float(p.get("interval_avg",0)),
        float(p.get("lut",0)), float(p.get("ff",0)),
        float(p.get("bram",0)), float(p.get("dsp",0)),
    ], dtype=float)
    return x, y

def load_jsonl(path):
    X, Y = [], []
    for l in open(path, "r", encoding="utf-8"):
        if not l.strip(): continue
        s = json.loads(l)
        x,y = make_feat(s)
        if np.all(y>0):
            X.append(x); Y.append(y)
    return np.vstack(X), np.vstack(Y)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--train", required=True)
    ap.add_argument("--val", required=True)
    ap.add_argument("--out_dir", required=True)
    args = ap.parse_args()
    Path(args.out_dir).mkdir(parents=True, exist_ok=True)

    Xtr, Ytr = load_jsonl(args.train)
    Xv,  Yv  = load_jsonl(args.val)

    models = []
    for i in range(6):
        rf = RandomForestRegressor(n_estimators=600, max_depth=None, n_jobs=-1, random_state=42)
        rf.fit(Xtr, Ytr[:,i])
        pred = rf.predict(Xv)
        mape = mean_absolute_percentage_error(Yv[:,i], pred)
        print(f"[RF] Head{i} MAPE={mape*100:.2f}%")
        joblib.dump(rf, f"{args.out_dir}/rf_head{i}.joblib")
        models.append(rf)

    # 存特征顺序方便在线预测
    feat_order = sorted(["n_pragmas","has_pipeline","has_unroll","has_partition","has_dataflow","has_stream",
                         "unroll_factor","partition_factor","partition_dim","est_period",
                         "kernel_aes","kernel_sha1","n_nodes","n_edges"])
    with open(f"{args.out_dir}/meta.json","w",encoding="utf-8") as f:
        json.dump({"feature_order": feat_order}, f, ensure_ascii=False)
