#include "WowCamera.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

using boost::make_optional;
using boost::none;
using boost::optional;

using glm::lookAtRH;
using glm::perspectiveRH_ZO;

// TODO(phlip9): need to transpose view and projection matrices? probably since
//               d3dmatrix is row-major and glm::mat is column-major

namespace phlipbot
{
optional<WowCamera> WowCamera::GetActiveCamera()
{
  auto const* world_frame =
    reinterpret_cast<CGWorldFrame*>(offsets::Data::CGWorldFrame);

  if (world_frame == nullptr || world_frame->camera == nullptr) {
    return none;
  }

  auto* camera_ptr = reinterpret_cast<CGCamera*>(world_frame->camera);
  return make_optional<WowCamera>(camera_ptr);
}

mat4x4 WowCamera::Projection() const
{
  // TODO(phlip9): why fov * 0.6?
  // TODO(phlip9): transpose?
  return perspectiveRH_ZO(camera->fov * 0.6f, camera->aspect_ratio,
                          camera->near_clip, camera->far_clip);
}

mat4x4 WowCamera::View() const
{
  vec3 const& eye = camera->position;
  vec3 const at = eye + Forward();
  // TODO(phlip9): use the camera Up since there might be roll?
  vec3 const up{0, 0, 1};
  // TODO(phlip9): transpose?
  return lookAtRH(eye, at, up);
}
}