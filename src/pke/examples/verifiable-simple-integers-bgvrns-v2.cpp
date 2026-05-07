//==================================================================================
// Verifiable BGVrns example — mult + relin, constraint-only test.
// Tests if constraint generation alone (with eager witness) produces satisfied=true.
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

    LibsnarkProofSystem ps(cryptoContext);

    // Only constraint generation — no witness phase
    cout << "Constraint generation (with eager witness)..." << endl;
    ps.SetMode(PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);
    ps.PublicInput(ctxt1);
    ps.PublicInput(ctxt2);
    auto ct_mul   = ps.EvalMultNoRelin(ctxt1, ctxt2);
    auto ct_relin = ps.Relinearize(ct_mul);
    cout << "  done (" << ps.pb.num_constraints() << " constraints)" << endl;

    // Check WITHOUT witness generation phase
    cout << endl;
    cout << "===== R1CS Statistics =====" << endl;
    cout << "#inputs:      " << ps.pb.num_inputs() << endl;
    cout << "#variables:   " << ps.pb.num_variables() << endl;
    cout << "#constraints: " << ps.pb.num_constraints() << endl;

    bool satisfied = ps.pb.is_satisfied();
    cout << "satisfied (constraint-gen only): " << std::boolalpha << satisfied << endl;

    return !satisfied;
}
