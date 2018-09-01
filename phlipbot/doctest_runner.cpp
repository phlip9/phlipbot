#define DOCTEST_CONFIG_IMPLEMENTATION_IN_DLL
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

// doctest_runner.cpp exports a test runner that we can run, which includes any
// tests written in other phlipbot .cpp files.
//
// phlipbot_unittest links phlipbot.dll to then run this test runner.
//
// Note that unittests are not run while injected into the WoW.exe process!
