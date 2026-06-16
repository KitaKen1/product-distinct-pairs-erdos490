#!/usr/bin/env python3
"""Run fixed-size upper-bound checks for a claimed lower bound.

If a witness gives M(N) >= lb, then to prove M(N) = lb it is enough to rule
out |A||B| >= lb + 1.  By swapping A and B, assume |A| >= |B|.  This first
restricts |A| to ceil(sqrt(lb + 1)) <= |A| <= N.  For each possible |A|, the
smallest |B| that could still beat lb is ceil((lb + 1) / |A|).

These pairs are then reduced to the undominated frontier: if (a0,b0) has
a0 <= a and b0 <= b, ruling out (a0,b0) already rules out (a,b), since taking
subsets preserves product-distinctness.  When calling the fixed-size prover,
this helper may swap the two sizes so that the prover branches over the smaller
side.
"""

from __future__ import annotations

import argparse
import csv
import math
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


def parse_status_line(line: str) -> dict[str, str]:
    parts = next(csv.reader([line]))
    return dict(zip(parts[0::2], parts[1::2]))


def minimal_checks(n: int, lb: int) -> list[tuple[int, int]]:
    candidates: list[tuple[int, int]] = []
    threshold = lb + 1
    min_a = math.isqrt(threshold - 1) + 1
    for a in range(min_a, n + 1):
        b = math.ceil(threshold / a)
        if b <= a and b <= n:
            candidates.append((a, b))

    checks: list[tuple[int, int]] = []
    for a, b in candidates:
        dominated = any(a0 <= a and b0 <= b for a0, b0 in candidates if (a0, b0) != (a, b))
        if not dominated:
            checks.append((a, b))
    return sorted(checks, reverse=True)


def read_seed(path: Path) -> dict[tuple[int, int], str]:
    if not path.exists():
        return {}
    rows: dict[tuple[int, int], str] = {}
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if not row.get("status_line"):
                continue
            parsed = parse_status_line(row["status_line"])
            a, b = int(parsed["A"]), int(parsed["B"])
            rows[(a, b)] = row["status_line"]
            rows[(b, a)] = row["status_line"]
    return rows


def run_one(binary: Path, n: int, a: int, b: int, time_limit: float) -> str:
    solve_a, solve_b = sorted((a, b))
    cmd = [str(binary), "--n", str(n), "--a", str(solve_a), "--b", str(solve_b)]
    if time_limit > 0:
        cmd += ["--time-limit", str(time_limit)]
    proc = subprocess.run(cmd, text=True, capture_output=True, check=False)
    output = (proc.stdout or proc.stderr).strip().splitlines()
    if not output:
        raise RuntimeError(f"no output for A={a}, B={b}, returncode={proc.returncode}")
    line = output[-1]
    parsed = parse_status_line(line)
    if parsed.get("status") != "INFEASIBLE":
        raise RuntimeError(f"check A={a}, B={b} did not prove infeasible: {line}")
    return line


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--n", type=int, required=True)
    parser.add_argument("--lb", type=int, required=True)
    parser.add_argument("--binary", type=Path, default=Path("./prove_fixed_size"))
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seed-from", type=Path, action="append", default=[])
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--time-limit", type=float, default=0.0)
    args = parser.parse_args()

    needed = minimal_checks(args.n, args.lb)
    rows = {}
    for seed in args.seed_from:
        rows.update(read_seed(seed))

    missing = [(a, b) for a, b in needed if (a, b) not in rows]
    if missing:
        print("missing checks:", " ".join(f"A={a},B={b}" for a, b in missing), flush=True)
    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
        futures = {
            pool.submit(run_one, args.binary, args.n, a, b, args.time_limit): (a, b)
            for a, b in missing
        }
        for future in as_completed(futures):
            a, b = futures[future]
            line = future.result()
            rows[(a, b)] = line
            print(f"proved A={a}, B={b}: {line}", flush=True)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["r", "target_B", "status_line"])
        for a, b in needed:
            writer.writerow([a, b, rows[(a, b)]])

    print(f"wrote {args.out}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
