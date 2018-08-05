#pragma once

#include <memory>
#include <string>

#include <hadesmem/patcher.hpp>
#include <hadesmem/process.hpp>
#include <hadesmem/detail/trace.hpp>

namespace phlipbot
{
template <typename T, typename U, typename V>
inline void DetourFn(
  hadesmem::Process const& process,
  std::string const& name,
  std::unique_ptr<T>& detour,
  U const& orig_fn,
  V const& detour_fn
) {
  if (!detour) {
    if (orig_fn) {
      detour.reset(new T(process, orig_fn, detour_fn));
      detour->Apply();
      HADESMEM_DETAIL_TRACE_FORMAT_A("%s detoured", name.c_str());
    }
    else {
      HADESMEM_DETAIL_TRACE_FORMAT_A("Could not find %s export", name.c_str());
    }
  }
  else {
    HADESMEM_DETAIL_TRACE_FORMAT_A("%s already detoured", name.c_str());
  }
}

template <typename T>
inline void UndetourFn(
  std::string const& name,
  std::unique_ptr<T>& detour
) {
  if (detour) {
    detour->Remove();
    HADESMEM_DETAIL_TRACE_FORMAT_A("%s undetoured", name.c_str());

    auto& ref_count = detour->GetRefCount();
    while (ref_count.load()) {
      HADESMEM_DETAIL_TRACE_FORMAT_A("Spinning on %s ref count", name.c_str());
    }
    HADESMEM_DETAIL_TRACE_FORMAT_A("%s free of refs", name.c_str());

    detour = nullptr;
  }
  else {
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "%s is not detoured, skipping removal", name.c_str());
  }
}
}