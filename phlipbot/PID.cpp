#include "PID.hpp"

#include <doctest.h>

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

namespace test
{
TEST_CASE("PID works")
{
  PID pid{1, 1, 1};
  float output = pid.Update(1.0f, 0.1f);

  CHECK(output > 0.0f);
}

TEST_CASE("PID works with dt = 0")
{
  PID pid{1, 1, 1};
  float output = pid.Update(1.0f, 0.0f);

  CHECK(output > 0.0f);
}
}
}
