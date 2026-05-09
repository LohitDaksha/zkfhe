//==================================================================================
// Verifiable BGVrns example — mult + relin, two-phase constraint+witness.
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

    auto circuit = [&](Ciphertext<DCRTPoly> c1, Ciphertext<DCRTPoly> c2) {
        ps.PublicInput(c1);
        ps.PublicInput(c2);
        auto ct_mul   = ps.EvalMultNoRelin(c1, c2);
        auto ct_relin = ps.Relinearize(ct_mul);
        return ct_relin;
    };

    cout << "Constraint generation..." << endl;
    ps.SetMode(PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);
    auto ct_out = circuit(ctxt1, ctxt2);
    cout << "  done (" << ps.pb.num_constraints() << " constraints)" << endl;

    cout << "Witness generation..." << endl;
    ps.SetMode(PROOFSYSTEM_MODE_WITNESS_GENERATION);
    circuit(ctxt1, ctxt2);
    cout << "  done" << endl;

    cout << endl;
    cout << "===== R1CS Statistics =====" << endl;
    cout << "#inputs:      " << ps.pb.num_inputs() << endl;
    cout << "#variables:   " << ps.pb.num_variables() << endl;
    cout << "#constraints: " << ps.pb.num_constraints() << endl;

    bool satisfied = ps.pb.is_satisfied();
    cout << "R1CS satisfied: " << std::boolalpha << satisfied << endl;

    return !satisfied;
}
