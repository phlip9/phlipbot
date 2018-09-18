#pragma once

#include <d3d9.h>

#include "WowCamera.hpp"

namespace phlipbot
{
struct WorldRender {
  WorldRender() = default;
  ~WorldRender() = default;
  WorldRender(WorldRender const&) = delete;
  WorldRender& operator=(WorldRender const&) = delete;

  void SetRenderState(IDirect3DDevice9* device,
                      D3DVIEWPORT9 const& vp,
                      D3DMATRIX const& view,
                      D3DMATRIX const& proj);
  void Render(IDirect3DDevice9* device,
              D3DVIEWPORT9 const& vp,
              WowCamera const& camera);
  void RenderTest(IDirect3DDevice9* device, WowCamera const& camera);

  static D3DMATRIX D3DMatrixFromGlm(mat4x4 const& m);
};
}