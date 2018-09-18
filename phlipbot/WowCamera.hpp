#pragma once

#include <boost/optional.hpp>

#include "wow_constants.hpp"

namespace phlipbot
{
struct WowCamera {
  WowCamera(CGCamera* camera) : camera(camera){};
  ~WowCamera() = default;
  WowCamera(WowCamera const&) = default;
  WowCamera& operator=(WowCamera const&) = default;

  static boost::optional<WowCamera> GetActiveCamera();

  inline float GetFov() const { return (camera->*camera->vmt->GetFov)(); }

  inline vec3 Forward() const
  {
    vec3 result;
    (camera->*camera->vmt->Forward)(&result);
    return result;
  }
  inline vec3 Right() const
  {
    vec3 result;
    (camera->*camera->vmt->Right)(&result);
    return result;
  }
  inline vec3 Up() const
  {
    vec3 result;
    (camera->*camera->vmt->Up)(&result);
    return result;
  }

  mat4x4 Projection() const;
  mat4x4 View() const;

  CGCamera* camera;
};
}