#pragma once

#include <Shlwapi.h>

namespace phlipbot
{
struct Input final {
public:
  Input() = default;
  ~Input() = default;
  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;

  void HookInput(HWND const hwnd);
  void UnhookInput(HWND const hwnd);

  bool is_hooked{false};
};
}