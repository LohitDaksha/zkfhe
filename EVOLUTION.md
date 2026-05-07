# EVOLUTION.md — Changes Since Initial Commit

This document tracks all changes made to the zkOpenFHE repository since the initial commit (`2fb0d2d`) on the `bench` branch.

---

## Build System Fixes

### `third-party/libsnark/CMakeLists.txt` (modified submodule)
- **Changed `WITH_PROCPS` default from `ON` to `OFF`.**
  - `libprocps` is an optional memory-profiling dependency that is not installed on most systems (including SCC).
  - The CMake command-line flag `-DWITH_PROCPS=OFF` was being silently ignored due to CMake policy `CMP0077`, so the fix had to be applied directly in the submodule's `CMakeLists.txt`.

---

## New Example Files

### `src/pke/examples/verifiable-simple-integers-bgvrns-v2.cpp` [NEW]
- A **new working example** that demonstrates the zkOpenFHE proof system using the current API.
- Follows the exact pattern from the unit tests (`UnitTestLibsnarkGadgets.cpp`):
  1. Define the FHE circuit as a lambda
  2. Run in `PROOFSYSTEM_MODE_CONSTRAINT_GENERATION`
  3. Run in `PROOFSYSTEM_MODE_WITNESS_GENERATION`
  4. Check `ps.pb.is_satisfied()`
- Circuit: `EvalMultNoRelin(ct1, ct2)` followed by `Relinearize(...)`.
- **Findings:**
  - Add, Sub, and MultNoRelin all produce `satisfied: true` ✅
  - Relinearize produces `satisfied: false` ❌ — this appears to be a bug in the upstream `RelinearizeConstraint`/`RelinearizeWitness` implementation.

### `src/pke/examples/verifiable-simple-integers-bgvrns-v3.cpp` [NEW]
- Variant of the verifiable example for batch job execution.

---

## Job Scripts

### `jobs/run_v3.sh` [NEW]
- Batch job script for running the v3 example on the SCC cluster.

### `jobs/README.md` [NEW]
- Documentation for the job scripts.

---

## Notes

- The original `src/pke/examples/verifiable-simple-integers-bgvrns.cpp` is **unchanged** — it uses the old API (`ConstrainPublicInput`, `ConstrainRelin`, `ConstrainSubstraction`) which no longer compiles with the current `LibsnarkProofSystem` class. It is preserved as a historical reference.
- The `CMakeLists.txt` (root) and `src/pke/CMakeLists.txt` already contained the `PERFORMANCE OFF` and `snark ff gmp` linker fixes in the initial commit of this branch.

---

## Branch: `fix/relin-constraint` — Relinearization Bug Investigation

### Summary

Systematic investigation into why `Relinearize` produces `satisfied: false` in the R1CS constraint system, while all other operations (Add, Sub, MultNoRelin) produce `satisfied: true`.

### Proof System Operation Verification Results

| Operation | `satisfied` | Constraints | Time |
|-----------|:-----------:|:-----------:|------|
| EvalAdd only | ✅ true | 0 | <1s |
| EvalAdd + EvalSub | ✅ true | 0 | <1s |
| EvalMultNoRelin | ✅ true | 32,768 | ~1s |
| EvalMultNoRelin + Relinearize | ❌ false | ~14.2M | ~5min |

### Bug #1: Copy-Paste Typo in `ConstrainFastKeySwitchCore` (FIXED)

**File:** `src/pke/lib/proofsystem/proofsystem_libsnark.cpp`

Two identical bugs found at lines 2041 and 2177 (in both overloads of `ConstrainFastKeySwitchCore`):

```diff
- g0_add.generate_r1cs_constraints();
- g0_add.generate_r1cs_witness();
+ g1_add.generate_r1cs_constraints();
+ g1_add.generate_r1cs_witness();
```

**Impact:** `g1_add` was created to accumulate the `ct1` (a-vector) component of the key switch, but `g0_add` (the `ct0`/b-vector gadget) was called for constraint and witness generation instead. This caused:
- Missing constraints for the `ct1` accumulation path
- Duplicate constraint generation on the `ct0` path
- Incorrect witness values for `ct1`

**Status:** Fix applied. Constraint count changed (14,230,276 → 14,230,288), confirming the fix had an effect. However, `satisfied` remains `false`, indicating additional issues.

### Bug #2: Deeper Constraint Construction Issue (UNDER INVESTIGATION)

Even with only constraint generation (no witness phase), `satisfied: false`. This means the bug is in how the R1CS constraints are being constructed inside the relinearization pipeline, not in witness replay.

**Possible root causes under investigation:**
1. Incorrect linear combination composition in `ConstrainKeySwitchPrecomputeCore` — the digit decomposition constraints may not correctly capture the CRT decompose → NTT → modulus-switch pipeline
2. Mismatch between the FHE operation (performed by `cryptoContext->Relinearize()`) and the constraint-side operations (which manually trace through `KeySwitchBV` internals)
3. The `ConstrainFastKeySwitchCore` functions create gadgets as local variables, call `generate_r1cs_witness()` eagerly, but don't store them in `witness_metadata` — this means any later gadget replay only covers a subset of the witness

**Key observation:** Unit tests for individual components (`key_switch_precompute_core`, `key_switch_fast_key_switch_core`, `switch_modulus`, `ntt`, `intt`) all pass `is_satisfied()`. But there is **no dedicated end-to-end `relin` unit test** in the test suite, suggesting the full pipeline was never integration-tested.

**Note:** This is NOT a theoretical limitation — relinearization is fully expressible as R1CS constraints (it's polynomial arithmetic: CRT decomposition, NTTs, modular arithmetic, and inner products with public evaluation keys). The zkFHE paper explicitly covers relinearization in their verifiable FHE framework.

### Files Modified on This Branch

| File | Change |
|------|--------|
| `src/pke/lib/proofsystem/proofsystem_libsnark.cpp` | Fixed `g0_add`→`g1_add` typo at lines 2041 and 2177 |
| `src/pke/examples/verifiable-simple-integers-bgvrns-v2.cpp` | Updated to constraint-only test for debugging |

