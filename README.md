phlipbot
========

TODO(phlip9): fill out description


## Build

### Install and Build Boost v1.68

+ Download boost from
[Boost v1.68](https://www.boost.org/users/history/version_1_68_0.html).

+ Extract it somewhere and add the directory to your env as `BOOST_ROOT`.
For example, `BOOST_ROOT="C:\Program Files\boost\boost_1_68_0` in your Environment Variables.

+ Compile the non-header-only boost libraries:

```sh
cd $BOOST_ROOT
./boostrap.bat
./b2
```

+ TODO(phlip9): Use `vcpkg` or something.

### Build phlipbot

+ Clone the repo

```sh
git clone git@github.com:phlip9/phlipbot.git
```

+ Pull the submodules:

```sh
cd phlipbot
git submodule update --init --recursive
```

+ Open `phlipbot.sln` in VS.

+ Select `Win32/x86` as the build target.

+ Build all projects.


## Usage

```sh
$ cd build/Debug/Win32

# inject phlipbot.dll into a running WoW.exe
$ ./phlipbot_launcher inject

# eject phlipbot.dll from a running WoW.exe
$ ./phlipbot_launcher eject

# watch for changes in phlipbot.dll, then eject any currently inject dll, and
# inject the new dll.
# useful when developing for quicker iteration time.
$ ./phlipbot_launcher watch
```

Press `<Shift-F9>` to toggle display of the GUI.


### Useful Tools:

+ [DebugViewer++](https://github.com/CobaltFusion/DebugViewPP)
for viewing debug output from the injected dll running in the WoW.exe process.
