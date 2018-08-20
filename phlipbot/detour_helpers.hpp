#pragma once

#include <memory>
#include <string>

#include <hadesmem/detail/trace.hpp>
#include <hadesmem/process.hpp>

// TODO(phlip9): probably a better way to static link libudis86
//
// VS builds libudis86 as a static library, which is correct.
// However, when we build phlipbot, which is a dll, udis86/extern.h incorrectly
// mangles the symbol names from, e.g., ud_init -> __imp__ud_init, thinking
// that udis86 is being build as a dll now.
//
// For now, this is the only location that includes hadesmem/patcher.hpp
// (which eventually includes libudis86.h), so just temporarily kill the
// _USRDLL define so that it correctly sets the symbol names.
#pragma push_macro("_USRDLL")
#undef _USRDLL
#include <hadesmem/patcher.hpp>
#pragma pop_macro("_USRDLL")

namespace phlipbot
{
template <typename T, typename U, typename V>
inline void DetourFn(hadesmem::Process const& process,
                     std::string const& name,
                     std::unique_ptr<T>& detour,
                     U const& orig_fn,
                     V const& detour_fn)
{
  if (!detour) {
    if (orig_fn) {
      detour.reset(new T(process, orig_fn, detour_fn));
      detour->Apply();
      HADESMEM_DETAIL_TRACE_FORMAT_A("%s detoured", name.c_str());
    } else {
      HADESMEM_DETAIL_TRACE_FORMAT_A("Could not find %s export", name.c_str());
    }
  } else {
    HADESMEM_DETAIL_TRACE_FORMAT_A("%s already detoured", name.c_str());
  }
}

template <typename T>
inline void UndetourFn(std::string const& name, std::unique_ptr<T>& detour)
{
  if (detour) {
    detour->Remove();
    HADESMEM_DETAIL_TRACE_FORMAT_A("%s undetoured", name.c_str());

    auto& ref_count = detour->GetRefCount();
    while (ref_count.load()) {
      HADESMEM_DETAIL_TRACE_FORMAT_A("Spinning on %s ref count", name.c_str());
    }
    HADESMEM_DETAIL_TRACE_FORMAT_A("%s free of refs", name.c_str());

    detour = nullptr;
  } else {
    HADESMEM_DETAIL_TRACE_FORMAT_A("%s is not detoured, skipping removal",
                                   name.c_str());
  }
}
} // namespace phlipbot