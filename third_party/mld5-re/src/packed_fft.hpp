#pragma once
#include <vector>

namespace melodyne_re {
struct PackedFftResult {
    bool ok = false;
    const char* error = nullptr;
    std::vector<float> values;
};
// MATHEMATICAL replacement, not recovered Intel/MKL butterfly instructions and
// NOT a last-bit-native-parity claim. Restricted aligned wrapper contract,
// real single precision, N floats, power-of-two 8<=N<=4096.
//
// With F[k]=sum_n x[n]*exp(-i*2*pi*k*n/N), output is
// [2/N*F[0], -2/N*F[N/2], 2/N*Re(F[1]), -2/N*Im(F[1]), ...].
// Thus positive unit cosine/sine both have +1 at their respective interior bin.
// Slot1 is NEGATIVE scaled Nyquist, NOT DC's imaginary part.
//
// native_unaligned=true is rejected: saved inverse's unaligned branch only
// initializes half the destination, so is not the aligned mathematical contract.
PackedFftResult packed_real_forward_math(const std::vector<float>& samples,
                                        bool native_unaligned = false);
// Input packing is identical. Undo wrapper scale/sign by alternating +.5,-.5,
// then UNNORMALIZED inverse real DFT. Round-trip ideally returns input.
PackedFftResult packed_real_inverse_math(const std::vector<float>& packed,
                                        bool native_unaligned = false);
} // namespace melodyne_re
