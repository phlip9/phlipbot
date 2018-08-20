phlipbot
========

## Build

### Install Boost v1.68

+ Download boost from
[Boost v1.68](https://www.boost.org/users/history/version_1_68_0.html).

+ Extract it somewhere and add the directory to your env as `BOOST_ROOT`.
For example, `BOOST_ROOT="C:\Program Files\boost\boost_1_68_0` in your Environment Variables.

+ TODO(phlip9): Use `vcpkg` or something.

### Build phlipbot

+ Clone the repo

+ Pull the submodules:

```
git submodule update --init --recursive
```

+ Open `phlipbot.sln` in VS (be sure to run VS as administrator).

+ Select Win32/x86 as the build target.

+ Build all projects.

+ Run `phlipbot_launcher` while WoW.exe is running to inject `phlipbot.dll`.


## Usage

Press Shift-F9 to toggle display of the GUI.


### Useful Tools:

+ [DebugViewer++](https://github.com/CobaltFusion/DebugViewPP)
for viewing debug output from the injected dll running in the WoW.exe process.
