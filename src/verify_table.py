#!/usr/bin/env python3
"""Independent verifier for exact_unique_products.cpp CSV output.

The C++ solver already checks its incumbent pairs internally.  This script is
intentionally separate and simple: it only verifies the displayed A,B witnesses,
not the solver's upper-bound proof.
"""

from __future__ import annotations

import argparse
import ast
import csv
from pathlib import Path


def has_unique_products(a_values: list[int], b_values: list[int]) -> bool:
    seen: dict[int, tuple[int, int]] = {}
    for a in a_values:
        for b in b_values:
            product = a * b
            if product in seen:
                return False
            seen[product] = (a, b)
    return True


def parse_list(text: str) -> list[int]:
    return list(ast.literal_eval(text.replace(" ", ",")))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path)
    args = parser.parse_args()

    failures: list[str] = []
    with args.csv_path.open(newline="") as f:
        for row in csv.DictReader(f):
            n = int(row["N"])
            a_values = parse_list(row["A"])
            b_values = parse_list(row["B"])
            claimed = int(row["M"])
            if any(x < 1 or x > n for x in a_values + b_values):
                failures.append(f"N={n}: witness contains value outside [N]")
            if len(a_values) * len(b_values) != claimed:
                failures.append(f"N={n}: |A||B| does not equal M")
            if not has_unique_products(a_values, b_values):
                failures.append(f"N={n}: products are not unique")

    if failures:
        for failure in failures:
            print(failure)
        return 1

    print(f"verified witnesses in {args.csv_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
