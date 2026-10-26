// wavedistort_core.h - platform-independent core of the st-wavedistort effect.
//
// Ported from the Pure Data patch st-wavedistort.pd. Traced from its "workings" subpatch, per channel:
//
//   k = 0.1 * 50^strength        strength 0..1: log scale 0.1 .. 5      (pd rh_scalelog)
//   g = 0.1 * 100^gain           gain     0..1: log scale 0.1 .. 10     (pd rh_scalelog)
//   u = x * k                                                            (*~ with k in its right inlet)
//   p = |u| / 3.14 + 0.25                                                (abs~, /~ 3.14, +~ 0.25)
//   c = cos(2 pi p)                                                      (cos~ works in cycles)
//   s = (x > 0) ? 1 : 0                                                  (expr~ $v1>0)
//   y = ((1 - 2 s) * c) * 0.2 * g                                        (*~ -2, +~ 1, *~, *~ 0.2, *~ g)
//
// The abs~ / sign / cos~ combination collapses to a plain sine:
//   cos(2 pi (|u|/3.14 + 1/4)) = -sin(2 pi |u| / 3.14), and multiplying by -sign(x) gives sin(2 pi u / 3.14).
// So the patch is a sine waveshaper (a wavefolder once k * x is large):
//   y = 0.2 * g * sin(w * k * x),   w = 2 pi / 3.14  (= 2.0010, the patch divides by 3.14, not pi)
//
// The patch has no dry/wet control; MIX is added here because the NTS-3's depth strip is normally dry/wet.
// At MIX = 100 % the output is exactly the patch's.
//
// No SDK dependencies, so it can be unit tested on a desktop.
#pragma once

#include <stdint.h>
#include <math.h>

namespace wavedistort {

static constexpr float kOmega = 6.2831853f / 3.14f;  // 2 pi / 3.14, see above
static constexpr float kOutScale = 0.2f;              // the patch's final "*~ 0.2"

struct Fx {
  static const int kMaxCh = 2;

  // targets, set from the parameter callbacks (k and g are the already-mapped multipliers)
  float k_target = 0.68f;   // strength 0.49 -> 0.1 * 50^0.49
  float g_target = 0.955f;  // gain     0.49 -> 0.1 * 100^0.49
  float mix_target = 1.f;

  // smoothed state (5 ms one-pole, so moving the pad does not zipper)
  float k = 0.68f, g = 0.955f, mix = 1.f;
  float smooth = 0.004f;
  int channels = 2;

  void init(float samplerate, int channels_) {
    // Set every field explicitly: on the device the unit may be a static object whose
    // constructor/initializers are not guaranteed to have run.
    channels = channels_ > kMaxCh ? kMaxCh : channels_;
    smooth = 1.f - expf(-1.f / (0.005f * samplerate));
    k_target = 0.68f;
    g_target = 0.955f;
    mix_target = 1.f;
    snap();
  }

  void snap() {
    k = k_target;
    g = g_target;
    mix = mix_target;
  }

  // `in` / `out` are interleaved with `channels` samples per frame.
  void process(const float *in, float *out, uint32_t frames) {
    for (uint32_t n = 0; n < frames; ++n) {
      k += smooth * (k_target - k);
      g += smooth * (g_target - g);
      mix += smooth * (mix_target - mix);
      const float kw = kOmega * k;
      const float gain = kOutScale * g;
      for (int c = 0; c < channels; ++c) {
        const float x = in[n * channels + c];
        const float y = gain * sinf(kw * x);
        out[n * channels + c] = x + mix * (y - x);
      }
    }
  }
};

}  // namespace wavedistort
