//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

/*
  Verifiable BGVrns example with full SNARK proof generation and verification.

  Demonstrates:
    1. Standard FHE evaluation (three additions)
    2. Constraint generation via LibsnarkProofSystem
    3. Witness generation via LibsnarkProofSystem
    4. R1CS satisfiability check
    5. Trusted setup (key generation)
    6. Proof generation (prover)
    7. Proof verification (verifier)
 */

#include "openfhe.h"
#include "proofsystem/proofsystem_libsnark.h"
#include <iostream>
#include <chrono>

using std::cout, std::endl;
using namespace lbcrypto;

int main() {
    // =========================================================================
    // Step 1 - Set CryptoContext
    // =========================================================================
    CCParams<CryptoContextBGVRNS> parameters;
    parameters.SetMultiplicativeDepth(0);   // additions only, no mult depth needed
    parameters.SetPlaintextModulus(65537);
    parameters.SetScalingTechnique(FIXEDMANUAL);
    parameters.SetKeySwitchTechnique(KeySwitchTechnique::BV);

    CryptoContext<DCRTPoly> cryptoContext = GenCryptoContext(parameters);
    cryptoContext->Enable(PKE);
    cryptoContext->Enable(KEYSWITCH);
    cryptoContext->Enable(LEVELEDSHE);

    // =========================================================================
    // Step 2 - Key Generation
    // =========================================================================
    KeyPair<DCRTPoly> keyPair = cryptoContext->KeyGen();

    // Print parameters
    cout << "===== Parameters =====" << endl;
    cout << "N: " << cryptoContext->GetRingDimension() << endl;
    cout << "t: " << cryptoContext->GetCryptoParameters()->GetPlaintextModulus() << endl;
    cout << "Q: " << cryptoContext->GetCryptoParameters()->GetElementParams()->GetModulus() << endl;
    cout << "KeySwitchTechnique: " << parameters.GetKeySwitchTechnique() << endl;
    cout << endl;

    // =========================================================================
    // Step 3 - Encryption
    // =========================================================================
    std::vector<int64_t> vec1 = {1, 2, 3, 4};
    std::vector<int64_t> vec2 = {10, 20, 30, 40};
    std::vector<int64_t> vec3 = {100, 200, 300, 400};
    std::vector<int64_t> vec4 = {1000, 2000, 3000, 4000};

    Plaintext pt1 = cryptoContext->MakePackedPlaintext(vec1);
    Plaintext pt2 = cryptoContext->MakePackedPlaintext(vec2);
    Plaintext pt3 = cryptoContext->MakePackedPlaintext(vec3);
    Plaintext pt4 = cryptoContext->MakePackedPlaintext(vec4);

    auto ct1 = cryptoContext->Encrypt(keyPair.publicKey, pt1);
    auto ct2 = cryptoContext->Encrypt(keyPair.publicKey, pt2);
    auto ct3 = cryptoContext->Encrypt(keyPair.publicKey, pt3);
    auto ct4 = cryptoContext->Encrypt(keyPair.publicKey, pt4);

    cout << "Encryption: done" << endl;

    // =========================================================================
    // Step 4 - FHE Evaluation: Three additions
    //   Compute: ((ct1 + ct2) + ct3) + ct4
    //   Expected result: {1111, 2222, 3333, 4444}
    // =========================================================================
    auto ctAdd12  = cryptoContext->EvalAdd(ct1, ct2);
    auto ctAdd123 = cryptoContext->EvalAdd(ctAdd12, ct3);
    auto ctResult = cryptoContext->EvalAdd(ctAdd123, ct4);

    // Decrypt to verify correctness
    Plaintext ptResult;
    cryptoContext->Decrypt(keyPair.secretKey, ctResult, &ptResult);
    ptResult->SetLength(vec1.size());
    cout << "FHE result: " << ptResult << endl;

    // =========================================================================
    // Step 5 - Proof: Constraint Generation
    // =========================================================================
    auto t_start = std::chrono::high_resolution_clock::now();

    LibsnarkProofSystem ps(cryptoContext);
    ps.SetMode(PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);

    // Declare public inputs
    ps.PublicInput(ct1);
    ps.PublicInput(ct2);
    ps.PublicInput(ct3);
    ps.PublicInput(ct4);

    // Mirror the FHE circuit: three additions
    auto ps_add12  = ps.EvalAdd(ct1, ct2);
    auto ps_add123 = ps.EvalAdd(ps_add12, ct3);
    auto ps_result = ps.EvalAdd(ps_add123, ct4);

    // Declare public output and finalize
    auto vars_out = *ps.ConstrainPublicOutput(ps_result);
    ps.FinalizeOutputConstraints(ps_result, vars_out);

    auto t_constrain = std::chrono::high_resolution_clock::now();
    double dt_constrain = std::chrono::duration<double>(t_constrain - t_start).count();
    cout << "Constraint generation: done (" << dt_constrain << " s)" << endl;

    // =========================================================================
    // Step 6 - Proof: Witness Generation
    // =========================================================================
    ps.SetMode(PROOFSYSTEM_MODE_WITNESS_GENERATION);

    ps.PublicInput(ct1);
    ps.PublicInput(ct2);
    ps.PublicInput(ct3);
    ps.PublicInput(ct4);

    auto wg_add12  = ps.EvalAdd(ct1, ct2);
    auto wg_add123 = ps.EvalAdd(wg_add12, ct3);
    auto wg_result = ps.EvalAdd(wg_add123, ct4);

    auto t_witness = std::chrono::high_resolution_clock::now();
    double dt_witness = std::chrono::duration<double>(t_witness - t_constrain).count();
    cout << "Witness generation: done (" << dt_witness << " s)" << endl;

    // =========================================================================
    // Step 7 - R1CS Satisfiability Check
    // =========================================================================
    const auto& pb = ps.pb;
    const r1cs_constraint_system<FieldT> constraint_system = pb.get_constraint_system();

    cout << endl;
    cout << "===== R1CS Statistics =====" << endl;
    cout << "#inputs:      " << constraint_system.num_inputs() << endl;
    cout << "#variables:   " << constraint_system.num_variables() << endl;
    cout << "#constraints: " << constraint_system.num_constraints() << endl;

    bool satisfied = constraint_system.is_satisfied(pb.primary_input(), pb.auxiliary_input());
    cout << "R1CS satisfied: " << std::boolalpha << satisfied << endl;

    if (!satisfied) {
        cout << "ERROR: R1CS not satisfied, aborting proof generation." << endl;
        return 1;
    }

    // =========================================================================
    // Step 8 - Trusted Setup (Key Generation)
    // =========================================================================
    cout << endl << "===== SNARK Pipeline =====" << endl;

    auto t_setup_start = std::chrono::high_resolution_clock::now();

    r1cs_ppzksnark_keypair<default_r1cs_ppzksnark_pp> keypair =
        r1cs_ppzksnark_generator<default_r1cs_ppzksnark_pp>(constraint_system);

    auto t_setup_end = std::chrono::high_resolution_clock::now();
    double dt_setup = std::chrono::duration<double>(t_setup_end - t_setup_start).count();
    cout << "Trusted setup: done (" << dt_setup << " s)" << endl;

    // =========================================================================
    // Step 9 - Proof Generation (Prover)
    // =========================================================================
    auto t_prove_start = std::chrono::high_resolution_clock::now();

    r1cs_ppzksnark_proof<default_r1cs_ppzksnark_pp> proof =
        r1cs_ppzksnark_prover<default_r1cs_ppzksnark_pp>(
            keypair.pk, pb.primary_input(), pb.auxiliary_input());

    auto t_prove_end = std::chrono::high_resolution_clock::now();
    double dt_prove = std::chrono::duration<double>(t_prove_end - t_prove_start).count();
    cout << "Proof generation: done (" << dt_prove << " s)" << endl;

    // =========================================================================
    // Step 10 - Proof Verification (Verifier)
    // =========================================================================
    auto t_verify_start = std::chrono::high_resolution_clock::now();

    bool verified = r1cs_ppzksnark_verifier_strong_IC<default_r1cs_ppzksnark_pp>(
        keypair.vk, pb.primary_input(), proof);

    auto t_verify_end = std::chrono::high_resolution_clock::now();
    double dt_verify = std::chrono::duration<double>(t_verify_end - t_verify_start).count();

    cout << "Proof verification: done (" << dt_verify << " s)" << endl;
    cout << endl;
    cout << "===== Final Result =====" << endl;
    cout << "VERIFIED: " << std::boolalpha << verified << endl;

    return !(satisfied && verified);
}
