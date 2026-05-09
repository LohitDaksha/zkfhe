//==================================================================================
// Verifiable BGVrns example — 3-addition circuit with full SNARK proof.
//==================================================================================

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
    parameters.SetMultiplicativeDepth(1);
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
    cryptoContext->EvalMultKeyGen(keyPair.secretKey);

    cout << "===== Parameters =====" << endl;
    cout << "N: " << cryptoContext->GetRingDimension() << endl;
    cout << "t: " << cryptoContext->GetCryptoParameters()->GetPlaintextModulus() << endl;
    cout << "Q: " << cryptoContext->GetCryptoParameters()->GetElementParams()->GetModulus() << endl;
    cout << endl;

    // =========================================================================
    // Step 3 - Encryption
    // =========================================================================
    std::vector<int64_t> vec1 = {1, 2, 3, 4};
    std::vector<int64_t> vec2 = {10, 20, 30, 40};
    std::vector<int64_t> vec3 = {100, 200, 300, 400};
    std::vector<int64_t> vec4 = {1000, 2000, 3000, 4000};

    auto ct1 = cryptoContext->Encrypt(keyPair.publicKey, cryptoContext->MakePackedPlaintext(vec1));
    auto ct2 = cryptoContext->Encrypt(keyPair.publicKey, cryptoContext->MakePackedPlaintext(vec2));
    auto ct3 = cryptoContext->Encrypt(keyPair.publicKey, cryptoContext->MakePackedPlaintext(vec3));
    auto ct4 = cryptoContext->Encrypt(keyPair.publicKey, cryptoContext->MakePackedPlaintext(vec4));

    cout << "Encryption: done" << endl;

    // =========================================================================
    // Step 4 - FHE Evaluation: ((ct1 + ct2) + ct3) + ct4
    // =========================================================================
    auto ctAdd12  = cryptoContext->EvalAdd(ct1, ct2);
    auto ctAdd123 = cryptoContext->EvalAdd(ctAdd12, ct3);
    auto ctResult = cryptoContext->EvalAdd(ctAdd123, ct4);

    Plaintext ptResult;
    cryptoContext->Decrypt(keyPair.secretKey, ctResult, &ptResult);
    ptResult->SetLength(vec1.size());
    cout << "FHE result: " << ptResult << endl;

    // =========================================================================
    // Step 5 - Proof: define circuit as a lambda for reuse
    // =========================================================================
    LibsnarkProofSystem ps(cryptoContext);

    auto circuit = [&](Ciphertext<DCRTPoly> c1, Ciphertext<DCRTPoly> c2,
                       Ciphertext<DCRTPoly> c3, Ciphertext<DCRTPoly> c4) {
        ps.PublicInput(c1);
        ps.PublicInput(c2);
        ps.PublicInput(c3);
        ps.PublicInput(c4);
        auto a12  = ps.EvalAdd(c1, c2);
        auto a123 = ps.EvalAdd(a12, c3);
        auto res  = ps.EvalAdd(a123, c4);
        return res;
    };

    // Phase 1: Constraint Generation
    auto t0 = std::chrono::high_resolution_clock::now();
    ps.SetMode(PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);
    auto out = circuit(ct1, ct2, ct3, ct4);
    auto t1 = std::chrono::high_resolution_clock::now();
    cout << "Constraint generation: done ("
         << std::chrono::duration<double>(t1 - t0).count() << " s, "
         << ps.pb.num_constraints() << " constraints)" << endl;

    // Phase 2: Witness Generation
    ps.SetMode(PROOFSYSTEM_MODE_WITNESS_GENERATION);
    circuit(ct1, ct2, ct3, ct4);
    auto t2 = std::chrono::high_resolution_clock::now();
    cout << "Witness generation: done ("
         << std::chrono::duration<double>(t2 - t1).count() << " s)" << endl;

    // =========================================================================
    // Step 6 - R1CS Satisfiability Check
    // =========================================================================
    cout << endl;
    cout << "===== R1CS Statistics =====" << endl;
    cout << "#inputs:      " << ps.pb.num_inputs() << endl;
    cout << "#variables:   " << ps.pb.num_variables() << endl;
    cout << "#constraints: " << ps.pb.num_constraints() << endl;

    bool satisfied = ps.pb.is_satisfied();
    cout << "R1CS satisfied: " << std::boolalpha << satisfied << endl;

    if (!satisfied) {
        cout << "ERROR: R1CS not satisfied, aborting proof generation." << endl;
        return 1;
    }

    // =========================================================================
    // Step 7 - Trusted Setup
    // =========================================================================
    cout << endl << "===== SNARK Pipeline =====" << endl;

    const r1cs_constraint_system<FieldT> constraint_system = ps.pb.get_constraint_system();

    auto t3 = std::chrono::high_resolution_clock::now();
    r1cs_ppzksnark_keypair<default_r1cs_ppzksnark_pp> keypair =
        r1cs_ppzksnark_generator<default_r1cs_ppzksnark_pp>(constraint_system);
    auto t4 = std::chrono::high_resolution_clock::now();
    cout << "Trusted setup: done (" << std::chrono::duration<double>(t4 - t3).count() << " s)" << endl;

    // =========================================================================
    // Step 8 - Proof Generation
    // =========================================================================
    auto t5 = std::chrono::high_resolution_clock::now();
    r1cs_ppzksnark_proof<default_r1cs_ppzksnark_pp> proof =
        r1cs_ppzksnark_prover<default_r1cs_ppzksnark_pp>(
            keypair.pk, ps.pb.primary_input(), ps.pb.auxiliary_input());
    auto t6 = std::chrono::high_resolution_clock::now();
    cout << "Proof generation: done (" << std::chrono::duration<double>(t6 - t5).count() << " s)" << endl;

    // =========================================================================
    // Step 9 - Proof Verification
    // =========================================================================
    auto t7 = std::chrono::high_resolution_clock::now();
    bool verified = r1cs_ppzksnark_verifier_strong_IC<default_r1cs_ppzksnark_pp>(
        keypair.vk, ps.pb.primary_input(), proof);
    auto t8 = std::chrono::high_resolution_clock::now();
    cout << "Proof verification: done (" << std::chrono::duration<double>(t8 - t7).count() << " s)" << endl;

    cout << endl;
    cout << "===== Final Result =====" << endl;
    cout << "VERIFIED: " << std::boolalpha << verified << endl;

    return !(satisfied && verified);
}
