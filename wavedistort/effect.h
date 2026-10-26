#pragma once
/*
 *  File: effect.h
 *
 *  st-wavedistort for the NTS-3 kaoss pad kit: a sine waveshaper / wavefolder.
 *  X axis: STRENGTH (how hard the signal is driven into the sine). Y axis: GAIN (output level).
 *  FX depth strip: MIX (dry/wet).
 *
 *  Port of the Pure Data patch st-wavedistort.pd. The DSP lives in wavedistort_core.h (no SDK
 *  dependencies, unit tested on a desktop); this file only adapts it to the logue SDK Processor interface.
 */
#include "processor.h"
#include "unit_genericfx.h"
#include "wavedistort_core.h"

class Effect : public Processor
{
public:
  // No delay lines needed, so ask for no SDRAM (the template default is a large buffer).
  uint32_t getBufferSize() const override final { return 0U; }

  // Parameter indices. Must match the order of the descriptors in header.c.
  enum
  {
    STRENGTH = 0U,
    GAIN,
    MIX,
    NUM_PARAMS
  };

  // The patch's two log scales ("rh_scalelog"): value 0..1 -> min * (max / min)^value.
  static constexpr float kStrengthMin = 0.1f;
  static constexpr float kStrengthMax = 5.f;
  static constexpr float kGainMin = 0.1f;
  static constexpr float kGainMax = 10.f;
  static constexpr float kScale = 1000.f;  // parameter units per 1.0 (shown as 0.000 .. 1.000)

  inline void setParameter(uint8_t index, int32_t value) override final
  {
    switch (index)
    {
    case STRENGTH:
      fx_.k_target = kStrengthMin * powf(kStrengthMax / kStrengthMin, value / kScale);
      break;

    case GAIN:
      fx_.g_target = kGainMin * powf(kGainMax / kGainMin, value / kScale);
      break;

    case MIX:
      // 0.0 .. 100.0 % in tenths of a percent
      fx_.mix_target = value / kScale;
      break;

    default:
      break;
    }
  }

  // life-cycle
  void init(float *allocated_buffer) override final
  {
    (void)allocated_buffer;  // not used: no SDRAM requested
    fx_.init(getSampleRate(), 2);
    // Start from the header's default values, in case the host does not push every parameter first.
    for (uint8_t i = 0; i < NUM_PARAMS; ++i)
      setParameter(i, unit_header.common.params[i].init);
    fx_.snap();
  }

  void teardown() override final {}

  // audio: interleaved stereo in / out
  void process(const float *__restrict in, float *__restrict out, uint32_t frames) override final
  {
    fx_.process(in, out, frames);
  }

private:
  wavedistort::Fx fx_;
};
