/-
N=5 C++-style Lean4Web check.

This file is not the "direct list of all A,B candidates" style.
It mirrors the main idea of `prove_fixed_size.cpp`:

* choose a fixed A;
* record the ratios that occur between two elements of A;
* build the induced conflict condition on possible B-elements;
* check the maximum independent-set size on the B-side.

For N=5, the maximum independent set is computed by enumerating B masks.
That is much simpler than the C++ branch-and-bound code, but it checks the
same graph criterion.
-/

namespace ProductDistinctN05CppStyle

/-! ## Basic product-distinct check for the lower-bound witness -/

def containsNat (xs : List Nat) (x : Nat) : Bool :=
  xs.any (fun y => y == x)

def noRepeats : List Nat -> Bool
  | [] => true
  | x :: xs => !containsNat xs x && noRepeats xs

def productsWith (a : Nat) : List Nat -> List Nat
  | [] => []
  | b :: bs => a * b :: productsWith a bs

def products : List Nat -> List Nat -> List Nat
  | [], _ => []
  | a :: as, B => productsWith a B ++ products as B

def productDistinct (A B : List Nat) : Bool :=
  noRepeats (products A B)

def pairValue (A B : List Nat) : Nat :=
  A.length * B.length

/-! ## Bit masks for finite subsets of [N] -/

def bitU (k : Nat) : UInt64 :=
  (1 : UInt64) <<< UInt64.ofNat k

/-
Membership test for a bit-mask set.

The number x is represented by bit (x - 1).  For example:

* x = 1 uses bit 0;
* x = 2 uses bit 1;
* x = 5 uses bit 4.

`hasElem mask x` returns true exactly when that bit is on in `mask`.

The expression

    mask &&& bitU (x - 1)

keeps only the bit for x.  If the result is nonzero, x belongs to the set.
-/
def hasElem (mask : UInt64) (x : Nat) : Bool :=
  ((mask &&& bitU (x - 1)) != 0)

def addElem (mask : UInt64) (x : Nat) : UInt64 :=
  mask ||| bitU (x - 1)

def maskOf (xs : List Nat) : UInt64 :=
  xs.foldl addElem 0

def popcountUpTo (n : Nat) (mask : UInt64) : Nat :=
  Id.run do
    let mut count := 0
    for x in [1:n+1] do
      if hasElem mask x then
        count := count + 1
    return count

def allMasksOfSize (n targetSize : Nat) : List UInt64 :=
  Id.run do
    let limit := Nat.shiftLeft 1 n
    let mut masks := []
    for maskNat in [0:limit] do
      let mask := UInt64.ofNat maskNat
      if popcountUpTo n mask == targetSize then
        masks := mask :: masks
    return masks.reverse

/-! ## C++-style ratio graph on the B-side -/

abbrev Ratio := Nat × Nat

def ratioKey (x y : Nat) : Ratio :=
  let lo := if x <= y then x else y
  let hi := if x <= y then y else x
  let g := Nat.gcd lo hi
  (hi / g, lo / g)

def containsRatio (rs : List Ratio) (r : Ratio) : Bool :=
  rs.any (fun s => s == r)

/-
Ratios between pairs of chosen A-elements.

Here `aMask` is a bit mask for A.  For example, when n = 5, the mask

    0b10110

means A = {2,3,5}: bit 1, bit 2, and bit 4 are on.  The helper
`hasElem aMask x` tests whether x is in A.

This function scans all pairs x < y in [n].  If both x and y are in A, it stores
the reduced integer ratio y/x, written by `ratioKey x y`.

This is the Lean analogue of the C++ `QMask q`: it records which multiplicative
ratios are already present inside A.
-/
def ratiosForA (n : Nat) (aMask : UInt64) : List Ratio :=
  Id.run do
    let mut rs := []
    for x in [1:n+1] do
      -- Is x one of the selected A-elements?
      if hasElem aMask x then
        for y in [x+1:n+1] do
          -- Is y also one of the selected A-elements?
          if hasElem aMask y then
            -- Since x < y here, this records the ratio y/x, reduced by gcd.
            rs := ratioKey x y :: rs
    return rs

/-
Why ratios appear:

For fixed A, a repeated product would mean

    a1 * b1 = a2 * b2

with two different A-values and two different B-values.  Rearranging gives

    a2 / a1 = b1 / b2

up to swapping the two values on either side.  So a collision occurs exactly
when a ratio already present between two A-values is also present between two
B-values.

Thus, after A is fixed, two B-values conflict if their reduced ratio is one of
the reduced ratios already seen inside A.
-/
def bValuesConflict (aRatios : List Ratio) (x y : Nat) : Bool :=
  if x == y then
    false
  else
    containsRatio aRatios (ratioKey x y)

/- A B-mask is independent if it contains no conflicting pair.  This is the
same graph criterion used by the C++ maximum-independent-set step. -/
def independentBMask (n : Nat) (aRatios : List Ratio) (bMask : UInt64) : Bool :=
  Id.run do
    let mut ok := true
    for x in [1:n+1] do
      if ok && hasElem bMask x then
        for y in [x+1:n+1] do
          if ok && hasElem bMask y && bValuesConflict aRatios x y then
            ok := false
    return ok

/-
For one fixed A, compute the largest possible size of B.

`aMask` encodes the fixed set A.  The products repeat exactly when two chosen
B-values have a ratio already seen between two chosen A-values.  Therefore the
allowed B's are exactly the independent sets in the B-side conflict graph.

For N=5, we can compute this maximum by checking every B-mask directly:

* build the ratio list coming from A;
* enumerate every subset B of [n], encoded as `bMask`;
* keep only independent B-masks;
* return the largest size among them.

In the C++ program this maximum-independent-set step is optimized by
branch-and-bound.  Here it is deliberately written plainly because N=5 is tiny.
-/
def maxIndependentBSizeForA (n : Nat) (aMask : UInt64) : Nat :=
  Id.run do
    -- Ratios already present inside A.  These determine which pairs of
    -- B-values conflict.
    let aRatios := ratiosForA n aMask
    -- All subsets of [n] are represented by masks from 0 to 2^n - 1.
    let limit := Nat.shiftLeft 1 n
    let mut best := 0
    for bMaskNat in [0:limit] do
      let bMask := UInt64.ofNat bMaskNat
      -- If this B-mask has no conflicting pair, it is a valid choice for B
      -- with respect to the fixed A.
      if independentBMask n aRatios bMask then
        let size := popcountUpTo n bMask
        if best < size then
          best := size
    return best

/-
Test one fixed size pair `(targetA, targetB)`.

To prove that no valid pair has these sizes, we do this:

* enumerate every set A subset [n] with |A| = targetA;
* for this fixed A, build the B-side conflict graph from the ratios inside A;
* compute the largest possible size of an independent B in that graph;
* check that this largest possible B-size is still smaller than targetB.

If this is true for every A of size targetA, then no valid pair A,B with
|A| = targetA and |B| = targetB exists.

This follows the fixed-A / B-side graph idea used by `prove_fixed_size.cpp`.
-/
def fixedSizeInfeasible (n targetA targetB : Nat) : Bool :=
  (allMasksOfSize n targetA).all
    (fun aMask =>
      /-
      For this fixed A, `maxIndependentBSizeForA n aMask` is the largest number
      of B-elements that can be chosen without creating a repeated product.

      So if

          maxIndependentBSizeForA n aMask < targetB

      then this particular A cannot be extended to any valid B of size
      targetB.  The surrounding `.all` requires this for every A of size
      targetA.
      -/
      decide (maxIndependentBSizeForA n aMask < targetB))

/-! ## Size-pair reduction -/

def allSizePairsWithLargerSideFirstAbove
    (n lowerBound : Nat) : List (Nat × Nat) :=
  Id.run do
    let mut pairs := []
    for a in [0:n+1] do
      for b in [0:a+1] do
        if lowerBound < a * b then
          pairs := pairs ++ [(a, b)]
    return pairs

/-! ## Final check -/

/-
The final theorem has the same six-condition shape as the readable files.

Lower-bound clauses:

1. The displayed witness A = [1,2,3], B = [1,4,5] is product-distinct.
2. The same witness has value |A| * |B| = 9.

Upper-bound clauses:

3. The size pairs with product greater than 9 are exactly the displayed list.
4. Every such size pair contains one of the frontier sizes (5,2) or (4,3).
5. No valid pair exists with sizes (5,2).
   This is checked by the fixed-A / B-side graph method, following the C++
   style.

6. No valid pair exists with sizes (4,3).
   This is checked by the fixed-A / B-side graph method, following the C++
   style.
-/
theorem N05_cpp_style_six_conditions :
    productDistinct [1, 2, 3] [1, 4, 5] = true ∧
    pairValue [1, 2, 3] [1, 4, 5] = 9 ∧
    allSizePairsWithLargerSideFirstAbove 5 9 =
      [(4, 3), (4, 4), (5, 2), (5, 3), (5, 4), (5, 5)] ∧
    ([(4, 3), (4, 4), (5, 2), (5, 3), (5, 4), (5, 5)].all
      (fun p =>
        (decide (5 <= p.1) && decide (2 <= p.2)) ||
        (decide (4 <= p.1) && decide (3 <= p.2)))) = true ∧
    fixedSizeInfeasible 5 5 2 = true ∧
    fixedSizeInfeasible 5 4 3 = true := by
  native_decide

#check N05_cpp_style_six_conditions
#print axioms N05_cpp_style_six_conditions

/- Small visible values for Lean4Web. -/
#eval maxIndependentBSizeForA 5 (maskOf [1, 2, 3, 4, 5])
#eval fixedSizeInfeasible 5 5 2
#eval fixedSizeInfeasible 5 4 3

end ProductDistinctN05CppStyle
