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
