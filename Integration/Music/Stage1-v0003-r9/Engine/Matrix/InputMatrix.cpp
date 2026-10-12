#include "InputMatrix.h"
namespace entropy::music {
void InputMatrix::process(const StereoBlock &m, const StereoBlock &a, StereoBlock &out,
                          uint32_t validity) noexcept {
    if (validity == 1) {
        out = m;
        return;
    }
    if (validity == 2) {
        out = a;
        return;
    }
    if (!validity) {
        out = {};
        return;
    }
    for (unsigned i = 0; i < 1024; i++) {
        out.left[i] = m.left[i] + a.left[i];
        out.right[i] = m.right[i] + a.right[i];
    }
}
} // namespace entropy::music
