// Desktop tests for the st-wavedistort unit.
// Host build (needs the SDK headers + CMSIS; -fpermissive/-w only quiet CMSIS's own 32-bit pointer casts on a 64-bit PC):
//   gcc -c -std=c11 -fpermissive -w -I. -I../common -I../../ext/CMSIS/CMSIS/Include -DSTM32H725xE -DCORTEX_USE_FPU=TRUE -DARM_MATH_CM7 -D__FPU_PRESENT header.c -o header.o
//   g++ -O2 -std=c++11 -fpermissive -w -I. -I../common -I../../ext/CMSIS/CMSIS/Include -DSTM32H725xE -DCORTEX_USE_FPU=TRUE -DARM_MATH_CM7 -D__FPU_PRESENT test_effect.cc header.o -o test_effect && ./test_effect
#include "effect.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

extern "C" const genericfx_unit_header_t unit_header;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

// ---- op-by-op transcription of the patch (double precision), one channel -----------------------
// strength/gain are the slider values 0..1; rh_scalelog = exp(v*(log(max)-log(min)) + log(min)).
static double scalelog(double v, double mn, double mx) { return exp(v * (log(mx) - log(mn)) + log(mn)); }
static double pd_cos(double cycles) { return cos(2.0 * M_PI * cycles); }  // cos~ takes cycles

static double pd_patch(double x, double strength, double gain) {
  const double k = scalelog(strength, 0.1, 5.0);   // right inlet of "*~ 0.6"
  const double g = scalelog(gain, 0.1, 10.0);      // right inlet of the final "*~"
  const double u = x * k;                          // *~
  const double c = pd_cos(fabs(u) / 3.14 + 0.25);  // abs~ -> /~ 3.14 -> +~ 0.25 -> cos~
  const double s = (x > 0.0) ? 1.0 : 0.0;          // expr~ $v1>0
  const double sg = s * -2.0 + 1.0;                // *~ -2 -> +~ 1
  return (sg * c) * 0.2 * g;                       // *~ -> *~ 0.2 -> *~ gain
}

int main() {
  // ---- header sanity -------------------------------------------------------------------------
  const genericfx_unit_header_t &h = unit_header;
  CHECK(h.common.num_params == 3, "num_params");
  CHECK(h.common.params[0].init == 490 && h.common.params[1].init == 490 && h.common.params[2].init == 1000, "init values");
  CHECK(h.common.params[0].max == 1000 && h.common.params[1].max == 1000, "max values");
  CHECK(h.common.dev_id == 0x6D696C69 && h.common.unit_id == 2, "ids");

  // ---- real Effect driven with the integer hardware values ----------------------------------
  const int N = 4800;
  const double sv[] = {0, 0.1, 0.49, 0.8, 1.0};
  double worst = 0;
  for (double st : sv) for (double gn : sv) {
    Effect e;
    e.init(nullptr);
    e.setParameter(Effect::STRENGTH, (int32_t)lround(st * 1000));
    e.setParameter(Effect::GAIN, (int32_t)lround(gn * 1000));
    e.setParameter(Effect::MIX, 1000);
    // snap smoothing: run a settle block, then measure
    std::vector<float> in(N * 2), out(N * 2);
    for (int i = 0; i < N; ++i) { in[2*i] = in[2*i+1] = 0.9f * sinf(2 * 3.14159265f * 220.f * i / 48000.f) * (i % 2 ? 1.f : 0.5f); }
    e.process(in.data(), out.data(), N);  // settle (smoothing is already at target after init+snap only for defaults)
    for (int r = 0; r < 20; ++r) e.process(in.data(), out.data(), N);  // 2 s: 5 ms smoothing fully settled
    for (int i = 0; i < N; ++i) {
      const double ref = pd_patch(in[2*i], lround(st * 1000) / 1000.0, lround(gn * 1000) / 1000.0);
      // The float one-pole smoothers stop about 1e-5 (relative, -99 dB) short of their targets once the
      // per-sample step falls below one float ulp. A relative error in strength shifts the sine's phase in
      // proportion to its argument, so the tolerance is scaled by output level and argument size.
      const double kk = scalelog(lround(st * 1000) / 1000.0, 0.1, 5.0), gg = scalelog(lround(gn * 1000) / 1000.0, 0.1, 10.0);
      const double tol = 3e-5 * 0.2 * gg * (1.0 + 2.0 * kk * fabs((double)in[2*i]));
      const double err = fabs(out[2*i] - ref) / tol;  // <= 1 passes
      if (err > worst) worst = err;
      CHECK(out[2*i] == out[2*i+1], "channels differ");
    }
  }
  printf("worst |effect - patch| over grid, as a fraction of the tolerance: %.3g\n", worst);
  CHECK(worst < 1.0, "effect deviates from patch transcription (%g)", worst);

  // ---- MIX = 0 is exactly dry ---------------------------------------------------------------
  {
    Effect e; e.init(nullptr);
    e.setParameter(Effect::MIX, 0);
    std::vector<float> in(N * 2), out(N * 2);
    for (int i = 0; i < N * 2; ++i) in[i] = 0.7f * sinf(0.01f * i);
    for (int r = 0; r < 20; ++r) e.process(in.data(), out.data(), N);
    double d = 0; for (int i = 0; i < N * 2; ++i) d = fmax(d, fabs(out[i] - in[i]));
    printf("mix 0 max |out-in|: %.3g\n", d);
    CHECK(d < 1e-6, "mix 0 not dry");
  }

  // ---- silence in -> silence out, odd symmetry, bounded -------------------------------------
  {
    Effect e; e.init(nullptr);
    std::vector<float> in(N * 2, 0.f), out(N * 2, 1.f);
    e.process(in.data(), out.data(), N);
    double m = 0; for (float v : out) m = fmax(m, fabs(v));
    CHECK(m == 0.0, "silence not silent (%g)", m);
    e.setParameter(Effect::STRENGTH, 1000); e.setParameter(Effect::GAIN, 1000);
    for (int i = 0; i < N * 2; ++i) in[i] = ((rand() / (float)RAND_MAX) * 2.f - 1.f) * 4.f;  // hot, beyond full scale
    for (int r = 0; r < 20; ++r) e.process(in.data(), out.data(), N);
    m = 0; for (float v : out) { CHECK(v == v, "NaN"); m = fmax(m, fabs(v)); }
    printf("max |out| at max strength/gain with +-4 noise: %.3f (bound 0.2*10=2.0)\n", m);
    CHECK(m <= 2.0001, "output exceeds the 0.2*gain bound");
  }

  // ---- parameter move produces no jumps (smoothing) ----------------------------------------
  {
    Effect e; e.init(nullptr);
    std::vector<float> in(N * 2), out(N * 2);
    for (int i = 0; i < N * 2; ++i) in[i] = 0.3f;  // DC so any step is the gain/strength moving
    for (int r = 0; r < 5; ++r) e.process(in.data(), out.data(), N);
    e.setParameter(Effect::GAIN, 1000);
    e.process(in.data(), out.data(), N);
    double jump = 0; for (int i = 1; i < N; ++i) jump = fmax(jump, fabs(out[2*i] - out[2*(i-1)]));
    printf("largest per-sample step after a full gain move: %.4f\n", jump);
    CHECK(jump < 0.02, "gain move not smooth");
  }

  printf(fails ? "\n%d FAILURE(S)\n" : "\nall tests passed\n", fails);
  return fails != 0;
}
