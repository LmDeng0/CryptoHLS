#!/usr/bin/env python3
import json, argparse, numpy as np, joblib
from pathlib import Path

def load_models(model_dir):
    models = [ joblib.load(f"{model_dir}/rf_head{i}.joblib") for i in range(6) ]
    meta = json.load(open(f"{model_dir}/meta.json","r",encoding="utf-8"))
    return models, meta["feature_order"]

def make_feat_from_candidate(cand, feat_order):
    # cand 来自 DSE 的候选（未跑 HLS），结构与训练样本中的 solution 同源
    p_est = float(cand.get("estimated_period_ns", 0.0))
    sol = cand.get("solution", [])
    kinds = [ (s.get("kind") or "").lower() for s in sol ]
    def has(k): return 1 if k in kinds else 0
    def first_arg(k, key="factor", default=0):
        for s in sol:
            if (s.get("kind") or "").lower()==k and key in s:
                try: return float(s[key])
                except: return default
        return default
    top = (cand.get("top") or cand.get("top_function") or "").lower()
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
        "est_period": p_est,
        "kernel_aes": 1 if "aes" in top else 0,
        "kernel_sha1": 1 if "sha1" in top else 0,
        "n_nodes": float(cand.get("graph_stats",{}).get("n_nodes",0)),
        "n_edges": float(cand.get("graph_stats",{}).get("n_edges",0)),
    }
    return np.array([feats[k] for k in feat_order], dtype=float)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--candidates", required=True)  # jsonl
    ap.add_argument("--model_dir", required=True)   # runs/rf_baseline
    ap.add_argument("--topk", type=int, default=256)
    ap.add_argument("--out", required=True)
    ap.add_argument("--weights", default="1.0,1.0,0.0,0.0,0.0,0.0",
                    help="w for [lat, ii, lut, ff, bram, dsp]")
    args = ap.parse_args()

    models, feat_order = load_models(args.model_dir)
    weights = np.array([float(x) for x in args.weights.split(",")], dtype=float)

    cands = [json.loads(l) for l in open(args.candidates,"r",encoding="utf-8") if l.strip()]
    X = np.stack([make_feat_from_candidate(c, feat_order) for c in cands], axis=0)

    preds = np.stack([m.predict(X) for m in models], axis=1)  # [B,6]
    # 目标评分：可按需求调整
    score = preds[:,0]*np.maximum(preds[:,1],1.0) + np.dot(preds[:,2:], weights[2:])
    # 前 topk
    idx = np.argsort(score)[:args.topk]

    with open(args.out,"w",encoding="utf-8") as f:
        for i in idx:
            f.write(json.dumps(cands[i], ensure_ascii=False) + "\n")
