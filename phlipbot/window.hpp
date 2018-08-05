#pragma once

#include <windows.h>

#include <boost/optional.hpp>

#include <hadesmem/process.hpp>

namespace phlipbot
{
HWND& GetCurrentWindow();

void SetCurrentWindow(HWND hwnd);

boost::optional<HWND> FindMainWindow(hadesmem::Process const& process);

void LogWindowTitle(HWND const wnd);
}