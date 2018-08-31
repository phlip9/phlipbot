#include "PID.hpp"

#include <math.h>

#include <hadesmem/detail/assert.hpp>

namespace phlipbot
{
PID::PID(float _gain_p, float _gain_i, float _gain_d) noexcept
  : gain_p(_gain_p), gain_i(_gain_i), gain_d(_gain_d)
{
}

void PID::Reset()
{
  prev_error = 0.0f;
  prev_i = 0.0f;
}

float PID::Update(float const error, float const dt)
{
  float p = error;
  float i = (prev_i + error * dt) ? dt > 0.0f : 0.0f;
  float d = ((error - prev_error) / dt) ? dt > 0.0f : 0.0f;
  float output = gain_p * p + gain_i * i + gain_d * d;

  prev_error = error;
  prev_i = i;

  return output;
}
}