# Job Scripts

SGE batch scripts for running zkOpenFHE verifiable FHE examples on the BU Shared Computing Cluster (SCC).

## Scripts

### `run_v3.sh`

Runs the **verifiable-simple-integers-bgvrns-v3** example — a complete end-to-end verifiable FHE pipeline that:

1. Sets up a BGVrns crypto context (additions only, no multiplicative depth)
2. Encrypts 4 plaintext vectors
3. Evaluates a simple FHE circuit: 3 homomorphic additions → `((ct1 + ct2) + ct3) + ct4`
4. Builds an R1CS constraint system from the FHE circuit using `LibsnarkProofSystem`
5. Generates a witness (variable assignments) for the R1CS
6. Checks R1CS satisfiability
7. Runs **trusted setup** (`r1cs_ppzksnark_generator`) to produce proving and verification keys
8. Runs the **SNARK prover** (`r1cs_ppzksnark_prover`) to generate a succinct proof
9. Runs the **SNARK verifier** (`r1cs_ppzksnark_verifier_strong_IC`) to verify the proof

Each phase is timed so you can identify performance bottlenecks.

#### Resource Requests

| Parameter | Value | Description |
|-----------|-------|-------------|
| `-P` | `he` | Project allocation |
| `-pe omp` | `28` | OpenMP cores |
| `-l h_rt` | `04:00:00` | Wall-clock time limit |
| `-m beas` | — | Email on begin, end, abort, suspend |

#### Usage

```bash
# From the repository root
qsub jobs/run_v3.sh
```

#### Output

Logs are written to `zkfhe_v3_<JOB_ID>.log` in the working directory from which `qsub` was invoked.

## Prerequisites

The binary must be built before submitting:

```bash
cd build
cmake ..
cmake --build . --target verifiable-simple-integers-bgvrns-v3 -j$(nproc)
```

## Source Code

The corresponding C++ source lives at:

```
src/pke/examples/verifiable-simple-integers-bgvrns-v3.cpp
```
