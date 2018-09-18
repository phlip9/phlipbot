#include "WorldRender.hpp"

#include <hadesmem/detail/smart_handle.hpp>
#include <hadesmem/detail/trace.hpp>
#include <hadesmem/error.hpp>

using glm::transpose;

using hadesmem::detail::SmartComHandle;

using hadesmem::ErrorCodeWinHr;
using hadesmem::ErrorString;

namespace phlipbot
{
void WorldRender::SetRenderState(IDirect3DDevice9* device,
                                 D3DVIEWPORT9 const& vp,
                                 D3DMATRIX const& view,
                                 D3DMATRIX const& proj)
{
  device->SetRenderState(D3DRS_ZENABLE, D3DZBUFFERTYPE::D3DZB_TRUE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMPFUNC::D3DCMP_LESSEQUAL);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND::D3DBLEND_INVSRCALPHA);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL::D3DCULL_NONE);
  device->SetRenderState(D3DRS_LIGHTING, FALSE);
  // device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  // device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  // device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  // device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
  // device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
  // device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
  // device->SetRenderState(D3DRS_FOGENABLE, FALSE);
  // device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
  // device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
  // device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  // device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
  // device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);

  device->SetTexture(0, nullptr);

  device->SetVertexShader(nullptr);
  device->SetPixelShader(nullptr);

  device->SetViewport(&vp);

  device->SetTransform(D3DTRANSFORMSTATETYPE::D3DTS_VIEW, &view);
  device->SetTransform(D3DTRANSFORMSTATETYPE::D3DTS_PROJECTION, &proj);
}

void WorldRender::Render(IDirect3DDevice9* device,
                         D3DVIEWPORT9 const& vp,
                         WowCamera const& camera)
{
  // Save the current device state so we can restore it after our we render
  IDirect3DStateBlock9* state_block = nullptr;
  auto const create_sb_hr = device->CreateStateBlock(D3DSBT_ALL, &state_block);
  if (FAILED(create_sb_hr)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"device->CreateStateBlock failed"}
                        << ErrorCodeWinHr{create_sb_hr});
  }
  SmartComHandle state_block_cleanup{state_block};

  mat4x4 const glm_view = camera.View();
  D3DMATRIX const d3d_view = D3DMatrixFromGlm(glm_view);

  mat4x4 const glm_proj = camera.Projection();
  D3DMATRIX const d3d_proj = D3DMatrixFromGlm(glm_proj);

  SetRenderState(device, vp, d3d_view, d3d_proj);
  RenderTest(device, camera);

  // Restore the original device state
  auto const apply_sb_hr = state_block->Apply();
  if (FAILED(apply_sb_hr)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(hadesmem::Error{}
                                    << ErrorString{"state_block->Apply failed"}
                                    << ErrorCodeWinHr{apply_sb_hr});
  }
}

void WorldRender::RenderTest(IDirect3DDevice9* /*device*/,
                             WowCamera const& /*camera*/)
{
}

D3DMATRIX WorldRender::D3DMatrixFromGlm(mat4x4 const& m)
{
  mat4x4 const mT = transpose(m);
  return *reinterpret_cast<D3DMATRIX const*>(&mT);
}
}