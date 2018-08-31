#pragma once

// A simple PID controller implementation

namespace phlipbot
{
struct PID {
  explicit PID() noexcept = default;
  explicit PID(float gain_p, float gain_i, float gain_d) noexcept;

  PID(PID const&) = delete;
  PID& operator=(PID const&) = delete;

  void Reset();

  // Compute the control output from the process error and change in time.
  float Update(float const error, float const dt);

  float gain_p = 0.0f;
  float gain_i = 0.0f;
  float gain_d = 0.0f;

private:
  float prev_error = 0.0f;
  float prev_i = 0.0f;
};
}