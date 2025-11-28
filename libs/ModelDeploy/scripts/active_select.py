#!/usr/bin/env python3
import json, argparse, numpy as np

def pareto_mask(preds):
    # preds: [B,6]  -> 采用 latency*max(II,1)+1e-6*LUT 作为近似
    obj = preds[:,0] * np.maximum(preds[:,1],1.0) + 1e-6*preds[:,2]
    thr = np.percentile(obj, 30)  # 保留前30%作为近Pareto
    return obj <= thr

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--preds", required=True, help="jsonl of {pred:[6], var:[6], cand:{...}}")
    ap.add_argument("--topk", type=int, default=256)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    rows = [json.loads(l) for l in open(args.preds,"r",encoding="utf-8") if l.strip()]
    preds = np.array([r["pred"] for r in rows], dtype=float)
    vars_ = np.array([r.get("var",[0]*6) for r in rows], dtype=float)
    unc = vars_.mean(axis=1)
    pmask = pareto_mask(preds)

    order = np.argsort(-unc)  # 高不确定优先
    selected = []
    for i in order:
        if pmask[i]:
            selected.append(i)
        if len(selected) >= args.topk: break

    with open(args.out,"w",encoding="utf-8") as f:
        for i in selected:
            f.write(json.dumps(rows[i]["cand"], ensure_ascii=False)+"\n")
