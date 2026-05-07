//==================================================================================
// Verifiable BGVrns example — mult + relin circuit.
//==================================================================================

#include "openfhe.h"
#include "proofsystem/proofsystem_libsnark.h"
#include <iostream>

using std::cout, std::endl;
using namespace lbcrypto;

int main() {
    CCParams<CryptoContextBGVRNS> parameters;
    parameters.SetMultiplicativeDepth(1);
    parameters.SetPlaintextModulus(65537);
    parameters.SetScalingTechnique(FIXEDMANUAL);
    parameters.SetKeySwitchTechnique(KeySwitchTechnique::BV);

    CryptoContext<DCRTPoly> cryptoContext = GenCryptoContext(parameters);
    cryptoContext->Enable(PKE);
    cryptoContext->Enable(KEYSWITCH);
    cryptoContext->Enable(LEVELEDSHE);

    KeyPair<DCRTPoly> keyPair = cryptoContext->KeyGen();
    cryptoContext->EvalMultKeyGen(keyPair.secretKey);

    Plaintext plaintext1 = cryptoContext->MakePackedPlaintext({1, 2, 3, 4});
    Plaintext plaintext2 = cryptoContext->MakePackedPlaintext({5, 6, 7, 8});

    auto ctxt1 = cryptoContext->Encrypt(keyPair.publicKey, plaintext1);
    auto ctxt2 = cryptoContext->Encrypt(keyPair.publicKey, plaintext2);

    cout << "N: " << cryptoContext->GetRingDimension() << endl;
    cout << "Encryption: done" << endl;

    // Circuit: multiply then relinearize
    LibsnarkProofSystem ps(cryptoContext);

    auto circuit = [&](Ciphertext<DCRTPoly> ct1, Ciphertext<DCRTPoly> ct2) {
        ps.PublicInput(ct1);
        ps.PublicInput(ct2);
        auto ct_mul   = ps.EvalMultNoRelin(ct1, ct2);
        auto ct_relin = ps.Relinearize(ct_mul);
        return ct_relin;
    };

    // Phase 1: Constraint Generation
    cout << "Constraint generation..." << endl;
    ps.SetMode(PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);
    auto out = circuit(ctxt1, ctxt2);
    cout << "  done (" << ps.pb.num_constraints() << " constraints, "
         << ps.pb.num_variables() << " variables)" << endl;

    // Phase 2: Witness Generation
    cout << "Witness generation..." << endl;
    ps.SetMode(PROOFSYSTEM_MODE_WITNESS_GENERATION);
    circuit(ctxt1, ctxt2);
    cout << "  done" << endl;

    // Check
    cout << endl;
    cout << "===== R1CS Statistics =====" << endl;
    cout << "#inputs:      " << ps.pb.num_inputs() << endl;
    cout << "#variables:   " << ps.pb.num_variables() << endl;
    cout << "#constraints: " << ps.pb.num_constraints() << endl;

    bool satisfied = ps.pb.is_satisfied();
    cout << "satisfied:    " << std::boolalpha << satisfied << endl;

    return !satisfied;
}
