#!/usr/bin/env python3
import json, argparse, math
from pathlib import Path

REQ_PPA_KEYS = ["latency_cycles_avg","interval_avg","lut","ff","bram","dsp","estimated_period_ns"]

def is_bad_number(x):
    if x is None: return True
    if isinstance(x, str) and x.strip().lower() in {"nan","undef"}: return True
    if isinstance(x, float) and (math.isnan(x) or math.isinf(x)): return True
    return False

def read_jsonl(path):
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line=line.strip()
            if line: yield json.loads(line)

def infer_kernel_name(sample):
    top = sample.get("top") or sample.get("top_function")
    if top: return top
    sol = sample.get("solution", [])
    fnames = {s.get("function") for s in sol if s.get("function")}
    return fnames.pop() if len(fnames)==1 else None

def collect_stat(vals):
    if not vals: return None
    vals = sorted(vals)
    def pct(q):
        if len(vals)==1: return vals[0]
        k = int((q/100.0)*(len(vals)-1))
        return vals[k]
    return {"p99_9": pct(99.9)}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--inputs", nargs="+", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--log", default="clean_log.txt")
    args = ap.parse_args()

    raw = []
    for p in args.inputs:
        raw.extend(read_jsonl(p))

    # 收集面积类统计，用于极值裁剪
    res_keys = ["lut","ff","bram","dsp"]
    res_vals = {k:[] for k in res_keys}
    for s in raw:
        ppa = (s.get("real_ppa") or {})
        if not all(k in ppa for k in REQ_PPA_KEYS): continue
        if any(is_bad_number(ppa[k]) for k in REQ_PPA_KEYS): continue
        for k in res_keys:
            try: res_vals[k].append(float(ppa[k]))
            except: pass
    stats = {k: collect_stat(v) for k,v in res_vals.items()}

    kept = 0; dropped = 0; reasons = {}
    with open(args.out,"w",encoding="utf-8") as out:
        for s in raw:
            ppa = (s.get("real_ppa") or {})
            reason = None
            if not all(k in ppa for k in REQ_PPA_KEYS):
                reason = "missing_ppa_keys"
            elif any(is_bad_number(ppa[k]) for k in REQ_PPA_KEYS):
                reason = "nan_or_undef"
            if reason is None:
                try:
                    if float(ppa["interval_avg"]) <= 0:
                        reason = "illegal_ii"
                except:
                    reason = "parse_error"

            if reason is None:
                # 单样本函数一致性检查
                sol = s.get("solution", [])
                fnames = {x.get("function") for x in sol if x.get("function")}
                kname = infer_kernel_name(s)
                if kname is None or len(fnames) > 1 or (len(fnames)==1 and kname not in fnames):
                    reason = "mixed_functions"

            if reason is None:
                # 面积长尾裁剪
                try:
                    for rk in res_keys:
                        if stats[rk] and float(ppa[rk]) > stats[rk]["p99_9"]:
                            reason = f"extreme_{rk}"; break
                except:
                    reason = "parse_error"

            if reason:
                dropped += 1
                reasons[reason] = reasons.get(reason,0)+1
                continue

            kept += 1
            out.write(json.dumps(s, ensure_ascii=False) + "\n")

    with open(args.log,"w",encoding="utf-8") as lf:
        lf.write(f"TOTAL={len(raw)} KEPT={kept} DROPPED={dropped}\n")
        for k,v in sorted(reasons.items(), key=lambda x:-x[1]):
            lf.write(f"{k}: {v}\n")

if __name__ == "__main__":
    main()
