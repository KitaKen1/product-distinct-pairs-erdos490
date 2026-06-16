# Exact Value Table for M(N) = max |A||B| (Related to Erdos Problems #490)

This repository contains the table of values `M(1),...,M(35)`, together with
the code and data files used to compute and check them.

The values are:

```text
1, 2, 4, 6, 9, 12, 16, 20, 25, 28,
35, 40, 48, 50, 55, 60, 72, 78, 91, 98,
105, 105, 120, 128, 144, 144, 153, 162, 180, 190,
210, 220, 231, 231, 242
```

## Problem setting and Small Example

### Problem setting

For each `N >= 1`, compute `M(N)`.  Here

```text
M(N) = max |A| |B|,
```

where `A` and `B` range over subsets of `[N] = {1,...,N}`.  The condition is
that all products `ab`, with `a in A` and `b in B`, are distinct.

### Small example

For `N = 6`, both sets are chosen from `{1,2,3,4,5,6}`.  Take

```text
A = {1,2,3,4}
B = {1,5,6}
```

Then `|A||B| = 4*3 = 12`, and the products are

```text
1, 2, 3, 4, 5, 10, 15, 20, 6, 12, 18, 24
```

all distinct.  Thus this pair witnesses the value `M(6)=12`.

## Computing method

The computation has two parts.

1: For the lower bound, we give an explicit pair of sets `A,B` for each `N` and
verify directly that all products are distinct.  The witness table in the
appendix records these displayed constructions.

2: For the upper bound, we search for a contradiction to the next larger value.
If the displayed construction has value `L`, the upper-bound task is to prove
that no product-distinct pair with `|A||B| >= L+1` exists.

### 1. Witness lower-bound check for `N = 1,...,35`

The witness table in the appendix stores one displayed pair `A,B` for each
`N=1,...,35`.  The lower-bound check is direct:

- every element of `A` and `B` is in `[N]`;
- `|A||B|` equals the listed value `M(N)`;
- all products `ab` are distinct.

This proves the listed value is at least attainable.  It is only a lower-bound
check; it does not prove maximality.

### 2. Upper-bound check

Suppose the witness table gives value `L` for a fixed `N`.  To prove
`M(N)=L`, it remains to rule out every product-distinct pair `A,B subset [N]`
with

```text
|A||B| >= L+1.
```

The problem is symmetric in `A` and `B`, so we may assume `|A| >= |B|`.  The
size conditions are

```text
|A| >= |B|,
|A| <= N,
|B| <= N,
|A||B| >= L+1.
```

For the minimal frontier, it is enough to consider

```text
|B| <= ceil(sqrt(L+1)) <= |A| <= N.
```

For each possible value of `|B|`, the smallest `|A|` that could still beat `L`
is `ceil((L+1)/|B|)`.  This gives candidate size `(|A|, |B|)` pairs.  After
removing `(|A|, |B|)` pairs already covered by smaller ones, the remaining
minimal size `(|A|, |B|)` pairs are checked by exact exhaustive search.

#### Small example

For `N=20`, the displayed witness has `L=98`.  A larger value would have to
satisfy `|A||B| >= 99`, so `|B| <= ceil(sqrt(99))=10 <= |A| <= 20`.
The minimal size `(|A|, |B|)` pairs are:

```text
(20,5), (17,6), (15,7), (13,8), (11,9), (10,10)
```

Thus exactness reduces to six fixed-size searches.

## Visual check

For a visual example, open the GitHub Pages dashboard in `index.html`.  It shows
one selectable witness for each `N = 1,...,35`, with the selected pair displayed
as two sets and as a product grid.  The full list of displayed witnesses is in
`assets/table_n35.csv`.

## Reproducing the checks

First build the two C++ programs:

```sh
g++ -O3 -std=c++17 src/exact_unique_products.cpp -o exact_unique_products
g++ -O3 -std=c++17 src/prove_fixed_size.cpp -o prove_fixed_size
```

### Lower-bound check

The lower-bound check verifies the displayed witnesses in the table:

```sh
python3 src/verify_table.py assets/table_n35.csv
```

### Upper-bound check

For a small reproducible example, recompute the exact value for `N=23`:

```sh
./exact_unique_products --start-n 23 --max-n 23 --seed-csv assets/table_n35.csv
```

The output should include the CSV row starting with `23,exact,120`.

Then run the corresponding fixed-size upper-bound checks using the listed value
`L=120`:

```sh
python3 src/run_minimal_bound.py --n 23 --lb 120 --binary ./prove_fixed_size --out /tmp/fixed_bound_n23.csv
```

This checks the minimal size `(|A|, |B|)` pairs that could still beat `120`.
The largest exact searches are not instant; the repository keeps only the main
value/witness table as a data file.

## Files

```text
assets/
  table_n35.csv                    value table and displayed witnesses

src/
  exact_unique_products.cpp        exact search
  prove_fixed_size.cpp             fixed-size infeasibility checker
  verify_table.py                  witness verifier
  run_minimal_bound.py             helper for minimal upper-bound checks

index.html                         static browser dashboard
```

## AI usage disclosure

Some code, calculations, and drafting were assisted by Codex 5.5 xhigh and
ChatGPT 5.5 Pro.

## Reference

- Erdos Problems #490 forum thread:
  <https://www.erdosproblems.com/forum/thread/490>

## Appendix: witness table

| N | M(N) | \|A\| | \|B\| | A | B |
|---:|---:|---:|---:|---|---|
| 1 | 1 | 1 | 1 | `{1}` | `{1}` |
| 2 | 2 | 1 | 2 | `{1}` | `{1, 2}` |
| 3 | 4 | 2 | 2 | `{1, 2}` | `{1, 3}` |
| 4 | 6 | 2 | 3 | `{1, 2}` | `{1, 3, 4}` |
| 5 | 9 | 3 | 3 | `{1, 2, 3}` | `{1, 4, 5}` |
| 6 | 12 | 4 | 3 | `{1, 2, 3, 4}` | `{1, 5, 6}` |
| 7 | 16 | 4 | 4 | `{1, 2, 3, 4}` | `{1, 5, 6, 7}` |
| 8 | 20 | 5 | 4 | `{1, 2, 3, 4, 6}` | `{1, 5, 7, 8}` |
| 9 | 25 | 5 | 5 | `{1, 2, 3, 4, 6}` | `{1, 5, 7, 8, 9}` |
| 10 | 28 | 7 | 4 | `{1, 2, 3, 4, 5, 6, 8}` | `{1, 7, 9, 10}` |
| 11 | 35 | 7 | 5 | `{1, 2, 3, 4, 5, 6, 8}` | `{1, 7, 9, 10, 11}` |
| 12 | 40 | 8 | 5 | `{1, 2, 3, 4, 5, 6, 8, 12}` | `{1, 7, 9, 10, 11}` |
| 13 | 48 | 8 | 6 | `{1, 2, 3, 4, 5, 6, 8, 12}` | `{1, 7, 9, 10, 11, 13}` |
| 14 | 50 | 10 | 5 | `{1, 2, 3, 4, 5, 6, 7, 8, 10, 12}` | `{1, 9, 11, 13, 14}` |
| 15 | 55 | 11 | 5 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12}` | `{1, 11, 13, 14, 15}` |
| 16 | 60 | 12 | 5 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14}` | `{1, 11, 13, 15, 16}` |
| 17 | 72 | 12 | 6 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14}` | `{1, 11, 13, 15, 16, 17}` |
| 18 | 78 | 13 | 6 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 18}` | `{1, 11, 13, 15, 16, 17}` |
| 19 | 91 | 13 | 7 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 18}` | `{1, 11, 13, 15, 16, 17, 19}` |
| 20 | 98 | 14 | 7 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 18, 20}` | `{1, 11, 13, 15, 16, 17, 19}` |
| 21 | 105 | 15 | 7 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 16, 18}` | `{1, 11, 13, 17, 19, 20, 21}` |
| 22 | 105 | 15 | 7 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 16, 18}` | `{1, 11, 13, 17, 19, 20, 21}` |
| 23 | 120 | 15 | 8 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 16, 18}` | `{1, 11, 13, 17, 19, 20, 21, 23}` |
| 24 | 128 | 16 | 8 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 16, 18, 24}` | `{1, 11, 13, 17, 19, 20, 21, 23}` |
| 25 | 144 | 18 | 8 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 18, 20, 24}` | `{1, 13, 17, 19, 21, 22, 23, 25}` |
| 26 | 144 | 18 | 8 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 18, 20, 24}` | `{1, 13, 17, 19, 21, 22, 23, 25}` |
| 27 | 153 | 17 | 9 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 18, 20, 21, 24}` | `{1, 11, 13, 16, 17, 19, 23, 25, 27}` |
| 28 | 162 | 18 | 9 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 18, 20, 21, 24}` | `{1, 13, 17, 19, 22, 23, 25, 27, 28}` |
| 29 | 180 | 20 | 9 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18, 20, 21, 22, 24}` | `{1, 17, 19, 23, 25, 26, 27, 28, 29}` |
| 30 | 190 | 19 | 10 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 18, 20, 21, 24, 30}` | `{1, 13, 17, 19, 22, 23, 25, 27, 28, 29}` |
| 31 | 210 | 21 | 10 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18, 20, 21, 22, 24, 30}` | `{1, 17, 19, 23, 25, 26, 27, 28, 29, 31}` |
| 32 | 220 | 22 | 10 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 20, 21, 22, 24, 28, 30}` | `{1, 17, 19, 23, 25, 26, 27, 29, 31, 32}` |
| 33 | 231 | 21 | 11 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 18, 20, 21, 24, 28, 30, 33}` | `{1, 13, 17, 19, 22, 23, 25, 27, 29, 31, 32}` |
| 34 | 231 | 21 | 11 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 18, 20, 21, 24, 28, 30, 33}` | `{1, 13, 17, 19, 22, 23, 25, 27, 29, 31, 32}` |
| 35 | 242 | 22 | 11 | `{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 18, 20, 24, 27, 28, 30, 33, 35}` | `{1, 13, 17, 19, 21, 22, 23, 25, 29, 31, 32}` |

## Appendix: why not brute force?

A direct brute-force search would try every pair of subsets `A,B` of `[N]`.
There are `2^N` choices for `A` and `2^N` choices for `B`, so this is `4^N`
pairs.  For `N=35`, this is about `2^70`, or roughly `1.18 * 10^21`,
candidate pairs.  This number is not large by every combinatorial standard,
but it is too large for literal enumeration of all pairs: even at `10^9`
candidate pairs per second, it would take more than 30,000 years before
accounting for the cost of checking products.
