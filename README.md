# Small Installer

**Modern looks. Tiny installers.**

Give your Windows app a modern, polished installer in an extremely compact executable. Describe the pages, branding and install behavior you want to your LLM, let it adapt the sample, and build your setup program.

Underneath, Small Installer is a header-only C11 library that bundles your application and uninstaller into one executable, with no separate C runtime installation required.

[Get started](#get-started) · [Sample app](examples/sample-app) · [Developer reference](docs/reference.md) · [MIT license](LICENSE)

Used by [Termy](https://termy.ai).

## Why Small Installer?

- **Describe it to your LLM.** Tell it which pages, branding, shortcuts and install behavior you need, and have it adapt the sample app. A small C configuration makes it easy to review the result and ask for changes.
- **A modern look, made yours.** Add your icon, welcome image and accent color, with optional dark mode and page transitions. Rendering comes from [Slim UI](https://github.com/live-sound/slim-ui), using Direct2D and DirectWrite.
- **Extremely compact.** A minimal three-page installer measures about **122 KiB**, or **169 KiB with an uninstaller**. Your application files add their LZMA-compressed size on top. See the [build conditions and measurements](docs/reference.md#size).
- **An installation in your users' language.** The wizard follows the Windows locale. The sample includes translations in 30 languages.
- **The Windows essentials.** Configure shortcuts, registry entries, URL protocol handlers, per-user or administrator installation, and reinstall options.
- **Room for your own flow.** Add custom pages, callbacks and optional downloads, or run installs, updates and uninstalls silently.

![Sample App setup, showing the finished page of the installer](examples/sample-app/screenshot.png)

## Get started

Start with the sample: a tiny application, a complete installer and uninstaller, images, and translations. Clone the repository, then open it in your LLM coding tool.

You'll need **Windows**, **Visual Studio 2022 with the C++ desktop tools and Windows SDK**, **CMake 3.23 or newer**, and **Git**. Run these commands in PowerShell:

```powershell
git clone --recursive https://github.com/live-sound/small-installer.git
cd small-installer
```

Already cloned the repository? Run `git submodule update --init --recursive` to fetch Slim UI before building.

### Make it yours

Tell your LLM where to find your application files and artwork, then describe what you want. For example:

> Adapt examples/sample-app into a per-user installer for MyApp. Use a blue accent, a license page and a Start Menu shortcut. Offer to launch the app when setup finishes. Include an uninstaller and update the build script to pack my application files. Ask me for any missing app details or file locations.

Review the changes, then build and try the installer. To adjust it, ask for a change such as “Use a dark theme” or “Add a welcome page with my logo.” The [sample app guide](examples/sample-app/README.md) explains the files and settings, and the [developer reference](docs/reference.md#configure-your-installer) covers configuring the installer directly in C.

### Build and try it

From the repository folder, run:

```powershell
.\examples\sample-app\build.ps1
```

The script builds and packs the installer, then prints the output path. The unmodified sample produces **`build\SampleAppSetup.exe`**; you can build it first to explore the wizard before making changes. Run your generated setup program to check the pages and installation behavior.

## Updates and unattended installs

Small Installer detects an existing installation using your app's `app_id`. Keep that identifier stable across releases so updates find the right installation.

```powershell
.\MyAppSetup.exe --silent
.\MyAppSetup.exe --silent --launch
.\MyAppSetup.exe --silent-update
```

`--silent` installs without a window; `--launch` starts the app after success. `--silent-update` installs and then relaunches the app. The uninstaller also accepts `--silent` and keeps application data. See the [command-line reference](docs/reference.md#command-line) for details.

Setup terminates running processes matching the configured app executable name before replacing files. If a file remains locked, interactive setup offers Retry or Cancel; silent setup fails with a nonzero exit code. See [replacement behavior](docs/reference.md#replacing-an-existing-installation) when planning your update flow.

## Why no visual designer?

Because it's faster and easier to tell your LLM what you need. Small Installer keeps the configuration in ordinary C code, so you can describe a change, review the result and build it.

## Explore further

| I want to… | Start here |
|------------|------------|
| Adapt a working installer | [Sample app guide](examples/sample-app/README.md) |
| Choose pages and configure the wizard | [API reference](docs/reference.md#api-reference) |
| Add languages | [Internationalization](docs/reference.md#internationalization) |
| Bundle files into the setup program | [si_pack](docs/reference.md#si_pack-tool) |
| Understand the internals | [Architecture](docs/reference.md#architecture) |

## License

[MIT](LICENSE), including the sample application, images and locale files. Small Installer uses [Slim UI](https://github.com/live-sound/slim-ui) (MIT) and the LZMA SDK (public domain).
