# Agent guidance

## Documentation

- Write README.md for people evaluating and using the library: clear benefits,
  a screenshot, a working quick start, and links to deeper documentation.
- Keep API and packaging details in docs/reference.md and sample-specific
  instructions in examples/sample-app/README.md. These are also human-facing docs.
- Put agent instructions and maintenance constraints here. Do not add task
  reports, implementation diaries, or explanations aimed at LLMs to the README.
- Use PowerShell syntax for Windows command examples. Check relative links when
  moving documentation. Treat recorded binary sizes as measurements, not promises.

## Project map

- include/small_installer/: public header and implementation headers.
- tools/si_pack.cpp: archive and locale packer.
- examples/sample-app/: complete app, installer, uninstaller, resources and locales.
- tests/: transaction, file removal, silent install and silent uninstall checks.
- external/slim-ui/: rendering submodule; third_party/lzma/: compression sources.

## Build and validation

Use Visual Studio 2022, x64, and CMake 3.23 or newer. Initialize the Slim UI
submodule with `git submodule update --init --recursive` if needed.

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config MinSizeRel
ctest --test-dir build -C MinSizeRel --output-on-failure
```

`examples/sample-app/build.ps1` builds and packs `build/SampleAppSetup.exe`.
Preserve the MinSizeRel no-CRT configuration and compiler security support.
For documentation-only edits, verify examples and links; rebuilding is unnecessary.

## Installation behavior to preserve

- The configured executable name identifies processes terminated before
  `on_pre_install`, clean-reinstall removal and extraction.
- A replacement failure must not reach the finished page or launch the app.
  Interactive installs offer Retry/Cancel; silent installs return failure.
- Uninstall progress tracks handled files, including nested directories, and
  reaches 100% visibly before revealing completion. Keep UI interpolation on the
  UI thread and file work on the uninstall worker.
- Remove directory reparse points as links without traversing them. Exclude the
  running uninstaller from normal removal and preserve its cleanup handoff.
- Keep English first when packing locales so missing translations fall back to it.
