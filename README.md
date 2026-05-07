# zkOpenFHE — OpenFHE with Zero-Knowledge Proofs of Computation

Fully Homomorphic Encryption (FHE) is a powerful cryptographic primitive that enables performing computations over encrypted data without having access to the secret key.
However, FHE only achieves a relatively weak security notion (IND-CPA or IND-CPA-D), which is often insufficient for real-world deployments. 

The goal of zkOpenFHE is to be a drop-in replacement for the excellent [OpenFHE](https://github.com/openfheorg/openfhe-development) library, with the additional ability to prove the correct evaluation of an FHE circuit using a (zero-knowledge) succinct non-interactive argument of knowledge ((zk)SNARK). 

To achieve this, we mirror OpenFHE's interfaces and augment them under the hood with automatic constraint generation and extended witness computation, using [libsnark](https://github.com/scipr-lab/libsnark).

---

## Table of Contents

- [Quick Start](#quick-start)
- [Usage](#usage)
- [Building from Source](#building-from-source)
  - [Prerequisites](#prerequisites)
  - [Build Steps](#build-steps)
  - [Troubleshooting](#troubleshooting)
- [Running Examples](#running-examples)
- [Running Benchmarks](#running-benchmarks)
- [Project Structure](#project-structure)
- [Links and Resources](#links-and-resources)
- [Contributing](#contributing)

---

## Quick Start

```bash
# Clone the repository (with submodules)
git clone --recursive https://github.com/zkfhe/zkopenfhe.git
cd zkopenfhe

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run an example
./bin/examples/pke/verifiable-simple-integers-bgvrns
```

---

## Usage

zkOpenFHE mirrors the standard OpenFHE API. You simply wrap your computation in a `LibsnarkProofSystem` to automatically generate ZKP constraints:

<table>
<tr>
<th>OpenFHE</th>
<th>zkOpenFHE</th>
</tr>
<tr>
<td>

```cpp


c = cryptoContext->Encrypt(secretKey, x);


c_rot = cryptoContext->EvalRotate(c, 1);

c2 = cryptoContext->EvalMultNoRelin(c, c_rot);
```
</td>
<td>

```cpp
proofSystem = LibsnarkProofSystem(cryptoContext);

c = cryptoContext->Encrypt(secretKey, x);
proofSystem.PublicInput(c);

c_rot = proofSystem->EvalRotate(c, 1);

c2 = proofSystem->EvalMultNoRelin(c, c_rot);
```
</td>
</tr>
</table>

A `ProofSystem` has three modes:

| Mode | Description |
|------|-------------|
| `PROOFSYSTEM_MODE_EVALUATION` | Evaluate the FHE circuit, just like OpenFHE |
| `PROOFSYSTEM_MODE_CONSTRAINT_GENERATION` | Generate R1CS constraints for the FHE circuit |
| `PROOFSYSTEM_MODE_WITNESS_GENERATION` | Generate the extended witness needed for proving |

---

## Building from Source

### Prerequisites

| Dependency | Notes |
|------------|-------|
| **C++17 compiler** | GCC ≥ 9.0 or Clang ≥ 10 |
| **CMake** | ≥ 3.5.1 |
| **GMP** | GNU Multiple Precision Arithmetic Library (`libgmp-dev`) |
| **OpenMP** | Usually bundled with your compiler |
| **Git** | For fetching submodules |

**On Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libgmp-dev git
```

**On RHEL/CentOS/Rocky:**
```bash
sudo yum install -y gcc gcc-c++ cmake gmp-devel git
```

### Build Steps

**1. Clone the repository with all submodules:**
```bash
git clone --recursive https://github.com/zkfhe/zkopenfhe.git
cd zkopenfhe
```

If you've already cloned without `--recursive`, initialize submodules manually:
```bash
git submodule update --init --recursive
```

**2. Create a build directory and configure:**
```bash
mkdir build && cd build
cmake ..
```

**3. Build everything (examples + benchmarks + tests):**
```bash
make -j$(nproc)
```

Or build only specific targets:
```bash
# Build a single example
make verifiable-simple-integers-bgvrns -j$(nproc)

# Build a single benchmark
make bench_logistic_regression -j$(nproc)

# Build all examples
make allpkeexamples -j$(nproc)

# Build all benchmarks
make allbenchmark -j$(nproc)
```

### Troubleshooting

#### `libprocps` not found

If CMake fails with:
```
Package 'libprocps', required by 'virtual:world', not found
```

This dependency is optional (used only for memory profiling in libsnark). It has already been disabled by default in this fork. If you encounter this on a fresh build, pass the flag explicitly:
```bash
cmake -DWITH_PROCPS=OFF ..
```

#### `plugin needed to handle lto object` / Undefined references to `libff::bn128_*`

If you see linker errors like `undefined reference to libff::bn128_pp::init_public_params()` along with `plugin needed to handle lto object` warnings, this means Link Time Optimization (LTO) is producing object files that your system's `ar`/`ranlib` cannot process.

This has been fixed in this fork by disabling the `PERFORMANCE` (LTO) flag. If you still encounter it, explicitly disable it:
```bash
cmake -DPERFORMANCE=OFF ..
```

---

## Running Examples

After building, all example binaries are located in `build/bin/examples/pke/`.

### Verifiable FHE Examples (ZKP)

These examples demonstrate constraint generation and proof verification for FHE circuits:

| Example | Binary | Description |
|---------|--------|-------------|
| [verifiable-simple-integers-bgvrns](src/pke/examples/verifiable-simple-integers-bgvrns.cpp) | `bin/examples/pke/verifiable-simple-integers-bgvrns` | Basic BGV integer operations with R1CS constraint generation and satisfiability check |
| [sok-outsourcing-numconstraints](src/pke/examples/sok-outsourcing-numconstraints.cpp) | `bin/examples/pke/sok-outsourcing-numconstraints` | Outsourced computation constraint counting |
| [sok-2pc-numconstraints](src/pke/examples/sok-2pc-numconstraints.cpp) | `bin/examples/pke/sok-2pc-numconstraints` | Two-party computation constraint counting |
| [sok-logistic-regression-inference](src/pke/examples/sok-logistic-regression-inference.cpp) | `bin/examples/pke/sok-logistic-regression-inference` | Logistic regression inference with proofs |
| [sok-nn-batched](src/pke/examples/sok-nn-batched.cpp) | `bin/examples/pke/sok-nn-batched` | Batched neural network inference with proofs |
| [sok-tracing-protect](src/pke/examples/sok-tracing-protect.cpp) | `bin/examples/pke/sok-tracing-protect` | Tracing protection scenario |

**Run an example:**
```bash
cd build
./bin/examples/pke/verifiable-simple-integers-bgvrns
```

> **Note:** Constraint generation for FHE circuits is computationally intensive. Expect runtimes of several minutes depending on the circuit complexity and your hardware.

### Standard OpenFHE Examples

These are the original OpenFHE examples (no ZKP) and can be useful for learning the base API:

| Example | Binary |
|---------|--------|
| [simple-integers-bgvrns](src/pke/examples/simple-integers-bgvrns.cpp) | `bin/examples/pke/simple-integers-bgvrns` |
| [simple-integers](src/pke/examples/simple-integers.cpp) | `bin/examples/pke/simple-integers` |
| [simple-real-numbers](src/pke/examples/simple-real-numbers.cpp) | `bin/examples/pke/simple-real-numbers` |
| [depth-bgvrns](src/pke/examples/depth-bgvrns.cpp) | `bin/examples/pke/depth-bgvrns` |
| [rotation](src/pke/examples/rotation.cpp) | `bin/examples/pke/rotation` |

---

## Running Benchmarks

After building, benchmark binaries are located in `build/bin/benchmark/`.

| Benchmark | Binary | Description |
|-----------|--------|-------------|
| [bench_logistic_regression](benchmark/src/bench_logistic_regression.cpp) | `bin/benchmark/bench_logistic_regression` | End-to-end logistic regression: FHE eval, constraint generation, and witness generation |
| [bench_libsnark](benchmark/src/bench_libsnark.cpp) | `bin/benchmark/bench_libsnark` | Standalone libsnark performance on your system |
| [sok-2pc](benchmark/src/sok-2pc.cpp) | `bin/benchmark/sok-2pc` | Two-party computation benchmark |
| [sok-flipped2pc](benchmark/src/sok-flipped2pc.cpp) | `bin/benchmark/sok-flipped2pc` | Flipped two-party computation benchmark |
| [sok-outsourcingnonbatched](benchmark/src/sok-outsourcingnonbatched.cpp) | `bin/benchmark/sok-outsourcingnonbatched` | Non-batched outsourcing benchmark |

**Run a benchmark:**
```bash
cd build
./bin/benchmark/bench_logistic_regression
```

Benchmarks use [Google Benchmark](https://github.com/google/benchmark) and support standard flags:
```bash
# Run with a specific number of iterations
./bin/benchmark/bench_logistic_regression --benchmark_repetitions=3

# Output results as JSON
./bin/benchmark/bench_logistic_regression --benchmark_format=json
```

---

## Project Structure

```
zkOpenFHE/
├── src/
│   ├── core/                         # OpenFHE core library
│   ├── pke/                          # Public-key encryption library
│   │   ├── include/
│   │   │   └── proofsystem/          # ★ ZKP proof system headers
│   │   │       ├── proofsystem.h           # Base ProofSystem abstraction
│   │   │       └── proofsystem_libsnark.h  # Libsnark R1CS implementation
│   │   ├── lib/
│   │   │   └── proofsystem/          # ★ ZKP proof system implementation
│   │   └── examples/                 # Example programs
│   └── binfhe/                       # Binary FHE library
├── benchmark/src/                    # Benchmark programs
├── third-party/
│   ├── libsnark/                     # libsnark ZKP backend
│   ├── cereal/                       # Serialization
│   ├── google-benchmark/             # Google Benchmark
│   └── google-test/                  # Google Test
└── CMakeLists.txt                    # Root build configuration
```

---

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_EXAMPLES` | `ON` | Build example programs |
| `BUILD_BENCHMARKS` | `ON` | Build benchmark programs |
| `BUILD_UNITTESTS` | `ON` | Build unit tests |
| `WITH_OPENMP` | `ON` | Enable OpenMP parallelism |
| `WITH_PROCPS` | `OFF` | Use libprocps for memory profiling (requires `libprocps-dev`) |
| `NATIVE_SIZE` | `64` | Native integer bit width (64 or 128) |
| `PROOFSYSTEM_R1CS_NTT` | `LINALG` | R1CS arithmetization mode for NTT (`OPENFHE` or `LINALG`) |

Example with custom options:
```bash
cmake -DBUILD_BENCHMARKS=OFF -DPROOFSYSTEM_R1CS_NTT=OPENFHE ..
```

---

## Links and Resources

- **Project website:** [zkFHE.github.io](https://zkfhe.github.io)
- **OpenFHE documentation:** [openfhe-development.readthedocs.io](https://openfhe-development.readthedocs.io)
- **libsnark:** [github.com/scipr-lab/libsnark](https://github.com/scipr-lab/libsnark)

### Platform-Specific Installation

- [Linux](https://openfhe-development.readthedocs.io/en/latest/sphinx_rsts/intro/installation/linux.html)
- [macOS](https://openfhe-development.readthedocs.io/en/latest/sphinx_rsts/intro/installation/macos.html) — Note: the libsnark backend may not build on Apple Silicon.
- [Windows](https://openfhe-development.readthedocs.io/en/latest/sphinx_rsts/intro/installation/windows.html)

---

## Contributing

If you'd like to contribute, please [reach out](mailto:christian.knabenhans@epfl.ch)!
We're also very grateful if you [report issues](https://github.com/zkfhe/zkopenfhe/issues), big or small, or if you'd like to contribute some examples.
