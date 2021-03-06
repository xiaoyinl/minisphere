minisphere Release Checklist
============================

Prepare the Release
-------------------

The following changes should be made in a separate commit.  The first line of
the commit message should be `minisphere X.Y.Z`, where `X.Y.Z` is the version
number of the release, and a tag `vX.Y.Z` should be created which points to
this new commit.

* Version number in `VERSION`
* Version number in Win32 resources (`msvs/*.rc`)
* Version number in `src/shared/version.h`
* Version number in `src/plugin/PluginMain.cs`
* Version number in `src/plugin/Properties/AssemblyInfo.cs`
* Version number and release date in `README.md`
* Version number and release date in manual pages (`manpages/*`)
* Version number, release date, and changelog entries in `CHANGELOG.md`
* Version number in `setup/minisphere.iss`


Build the Release
-----------------

* In Windows using Visual Studio 2017:
    - Run `git clean -xdf`, then build the following project configurations:
        + **minisphere:** x64 Redist, x64 Console, Win32 Redist, Win32 Console
        + **Cell:** x64 Console, Win32 Console
        + **SSj:** x64 Console, Win32 Console
        + **Plugin:** AnyCPU Release
    - Copy the latest Sphere Studio "Release" build into `msw/ide/`
    - Compile `setup/minisphere.iss` using the latest version of Inno Setup
    - `minisphereSetup-X.Y.Z.exe` will be in `setup/`

* Using a 64-bit installation of Ubuntu 16.04:
    - Run `make clean all dist`
    - `minisphere_X.Y.Z.tar.gz` will be in `dist/`


Unleash the Beast!
------------------

* Drink a bunch of Monster drinks, at least 812 cans, and then...

* Post a release to GitHub pointing at the new Git tag, and upload the
  following files built above:
    - `minisphereSetup-X.Y.Z.exe`
    - `minisphere-X.Y.Z.tar.gz`
