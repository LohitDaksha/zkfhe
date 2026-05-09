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

### Files Modified on This Branch (Investigation Phase)

| File | Change |
|------|--------|
| `src/pke/lib/proofsystem/proofsystem_libsnark.cpp` | Fixed `g0_add`→`g1_add` typo at lines 2041 and 2177 |
| `src/pke/examples/verifiable-simple-integers-bgvrns-v2.cpp` | Updated to constraint-only test for debugging |

---

## Branch: `fix/relin-constraint` — Relinearization Bug Root-Cause Fix

### Summary

After systematic investigation, four root-cause bugs were found and fixed. The `relin` unit test now produces `satisfied: true` (5,661,036 constraints). The full EvalMultNoRelin + Relinearize circuit also produces `satisfied: true` (14,230,525 constraints).

### Bug #3: `relin` Unit Test Called `PublicInput` in Evaluation Mode

**File:** `src/pke/unittest/utproofsystem/UnitTestLibsnarkGadgets.cpp`

The test called `ps.PublicInput(in)` and `GetMetadata<...>(in)` before `SetMode(CONSTRAINT_GENERATION)`, so `PublicInput` ran as a no-op and immediately threw "metadata not set".

**Fix:** Removed the premature `PublicInput` + `GetMetadata` calls. The `eval` lambda already handles `PublicInput` in the correct mode. Also fixed `eval(ctxt1)` → `eval(in)` (was operating on a 2-poly ciphertext instead of the 3-poly relin input).

### Bug #4: `PublicInputWitness` Looped Over Wrong Dimension

**File:** `src/pke/lib/proofsystem/proofsystem_libsnark.cpp`

```diff
- for (size_t j = 0; j < num_polys; j++) {   // e.g. j < 3 for a 3-poly ciphertext
+ for (size_t j = 0; j < num_limbs; j++) {   // correct: iterate CRT limbs (e.g. 1 for post-ModReduce)
```

For a 3-poly, 1-limb ciphertext (the relin input after `ModReduceInPlace`), the old loop tried to access `c_i.GetElementAtIndex(1)` on a vector of size 1, causing an out-of-range exception at the start of witness generation.

### Bug #5: `PublicInput` Did Not Set Witness Values During Constraint Generation

**File:** `src/pke/include/proofsystem/proofsystem.h`

`PublicInputConstraint` allocated `pb_variable<FieldT>` for each coefficient but left their values as 0. Gadgets called inline during constraint generation (NTT butterflies, `LazyMulModGadget` in `ConstrainFastKeySwitchCore`) computed witnesses from these zero inputs. Since `LazyMulModGadget` gadgets are stack-local (not stored in `witnessMetadata.gadgets`) they could not be re-run in witness generation. Their zero outputs violated the constraint `digit * bv = mul_out` once the INTT gadgets (which ARE stored) updated `digit` to the correct non-zero value.

**Fix:** In the `PROOFSYSTEM_MODE_CONSTRAINT_GENERATION` case of `PublicInput`, also call `SetMetadata(ciphertext, m.witness_metadata)` and `PublicInputWitness(ciphertext)`. This ensures that inline `generate_r1cs_witness()` calls during constraint generation operate on real values.

```cpp
case PROOFSYSTEM_MODE_CONSTRAINT_GENERATION:
    m = PublicInputConstraint(ciphertext);
    SetMetadata<ConstraintMetadata>(ciphertext, m);
    SetMetadata(ciphertext, m.witness_metadata);   // NEW
    wire_id_to_metadata[GetWireId(ciphertext)] = m.witness_metadata;
    PublicInputWitness(ciphertext);                // NEW
    break;
```

**Side effect:** Also fixed `set_format_coefficient` unit test (was getting zero INTT inputs → zero outputs → `expect_equal_mod` failure).

### Bug #6: `constrain_addmod_lazy` Self-Aliasing Corrupted `in1` Data

**File:** `src/pke/lib/proofsystem/proofsystem_libsnark.cpp`

`RelinearizeConstraint` calls:
```cpp
constrain_addmod_lazy(out_metadata, 0, tmp_metadata, 0, out_metadata, 0, ...);
```
i.e., `in1 == &out` and `index_1 == index_out`. The lazy branch executed `out[index_out][j] = vector<...>()` before reading `in1[index_1][j]`, overwriting the vector in place and causing the resulting LC to be only `in2` instead of `in1 + in2`.

**Fix:** Save `in1` data before any writes to `out`:
```cpp
const auto saved_in1_lc     = in1[index_1];
const auto saved_in1_maxval = in1.max_value[index_1];
// ... use saved_in1_lc / saved_in1_maxval everywhere in1[index_1] was read
```

### End-to-End Verification Results

| Test | Before | After |
|------|--------|-------|
| `libsnark_openfhe_gadgets.relin` unit test | ❌ crash / false | ✅ 5,661,036 constraints, satisfied |
| `set_format_coefficient` unit test | ❌ false (all-zero INTT output) | ✅ satisfied |
| `key_switch_precompute_core` unit test | ❌ "metadata not set" crash | ✅ satisfied |
| `verifiable-simple-integers-bgvrns-v2` (EvalMult + Relin) | ❌ false | ✅ 14,230,525 constraints, satisfied |

### Files Modified (Fix Phase)

| File | Change |
|------|--------|
| `src/pke/unittest/utproofsystem/UnitTestLibsnarkGadgets.cpp` | Fixed `relin` and `key_switch_precompute_core` tests |
| `src/pke/lib/proofsystem/proofsystem_libsnark.cpp` | Fixed `PublicInputWitness` loop dimension; fixed `constrain_addmod_lazy` aliasing |
| `src/pke/include/proofsystem/proofsystem.h` | Fixed `PublicInput` to set witness values eagerly during constraint generation |
| `src/pke/examples/verifiable-simple-integers-bgvrns-v2.cpp` | Updated to two-phase (constraint gen + witness gen) circuit |

