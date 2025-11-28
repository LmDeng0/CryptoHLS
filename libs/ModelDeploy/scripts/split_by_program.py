#!/usr/bin/env python3
import json, random, argparse
random.seed(42)

def kernel_key(s):
    k = s.get("top") or s.get("top_function") or ""
    src = s.get("source_hash") or s.get("src_path") or ""
    return f"{k}|{src}"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--infile", required=True)
    ap.add_argument("--train_out", required=True)
    ap.add_argument("--val_out", required=True)
    ap.add_argument("--test_out", required=True)
    ap.add_argument("--val_ratio", type=float, default=0.1)
    ap.add_argument("--test_ratio", type=float, default=0.1)
    args = ap.parse_args()

    rows = [json.loads(l) for l in open(args.infile, "r", encoding="utf-8") if l.strip()]
    groups = {}
    for s in rows:
        groups.setdefault(kernel_key(s), []).append(s)

    keys = list(groups.keys())
    random.shuffle(keys)
    n = len(keys); n_val = int(n*args.val_ratio); n_test = int(n*args.test_ratio)
    valk = set(keys[:n_val]); testk = set(keys[n_val:n_val+n_test])
    traink = set(keys) - valk - testk

    def dump(keys_set, path):
        with open(path,"w",encoding="utf-8") as f:
            for k in keys_set:
                for s in groups[k]:
                    f.write(json.dumps(s, ensure_ascii=False)+"\n")

    dump(traink, args.train_out)
    dump(valk, args.val_out)
    dump(testk, args.test_out)

if __name__ == "__main__":
    main()
