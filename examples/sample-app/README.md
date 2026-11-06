# Sample App

A complete setup program for a small application. It installs `SampleApp.exe`, and everything here is meant to be copied and changed: nothing depends on the sample's name.

```powershell
.\build.ps1
```

This builds the application, the installer and the uninstaller, packs them, and writes `build\SampleAppSetup.exe` at the repository's root. The script prints the final file size.

## What it shows

| File | What it shows |
|------|---------------|
| `installer.c` | The wizard's pages, registry entries, a Start Menu shortcut, a `sample-app://` link handler, a minimum Windows version, and the callbacks around installation |
| `confetti.h` | A finished-page animation through `SiFinishedAnim`, paced by the wizard's vsync pump |
| `welcome_bubbles.h` | A floating bubble effect over the welcome image, respecting the Windows animation setting |
| `uninstaller.c` | The confirmation page, what the library removes by itself, and what a callback has to remove |
| `installer.rc`, `uninstaller.rc` | The icon, the embedded images, the manifest and the version information |
| `version.h` | The version, in one place for the code and the resources |
| `locales/` | The wizard's strings in 30 languages, plus the sample's own |
| `build.ps1` | The order of packing: the uninstaller first, then the installer around it |
| `app/` | The application itself, which only says that it runs |

## Make it yours

1. Replace the names in `installer.c` and `uninstaller.c`: `cfg.name`, `cfg.publisher`, `cfg.exe`, the install folder, the registry key, the shortcut and the protocol.
2. Generate a new GUID for `cfg.app_id`, the same in both files. It identifies your installed application, so an update finds the installation it replaces.
3. Put your own files in `assets/`. The welcome image is shown tall, on the left. The icon must keep resource ID 1.
4. Put your license in `locales/en.txt`, under `licenseText`, and in any language you have it for; the others show English.
5. In `build.ps1`, add your application's files with `--add`.

To try a silent install, run the setup program with `--silent`; `uninstall.exe --silent` removes it the same way. An update starts it with `--silent-update`, which also relaunches the application with `cfg.silent_update_launch_args`.

Sign `uninstall.exe` before it is packed into the installer, and the installer after packing: signing changes a file, so it can't be done the other way round.
