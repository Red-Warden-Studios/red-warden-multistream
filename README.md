# Red Warden Stream Kit

**Free OBS Studio plugins for people who stream, in one signed installer.**

By [Red Warden Studios](https://redwardenstudios.com/division/bastion/stream-kit/). One download, one logo, one update notice, a checkbox per tool.

## Tools

- **Red Warden Multistream** - stream to several platforms at once from OBS's own Start Streaming button.
- **Red Warden Pre-Flight** - checks your setup before you go live.

## Install

Run `RedWardenStreamKit-<version>-Setup.exe`, tick the tools you want (both are on by default) and
close OBS when asked. Start OBS, then open the **Docks** menu. If the old standalone Multistream
installer is on your PC, the Kit removes it first; your destinations and stream keys are kept.

Re-run the installer to add or remove a tool later: an unchecked tool that was installed is removed.

## Uninstall

Windows Settings > Apps > **Red Warden Stream Kit (OBS plugins)**. Destinations, settings and
stream keys live in your OBS profile and Windows Credential Manager and are not touched.

## Manual install

The zip holds one folder per plugin (`rws-multistream`, `rws-preflight`). Close OBS and copy them
into `C:\ProgramData\obs-studio\plugins\` (or into a portable OBS's `plugins` folder).

## Updates

Each plugin checks `https://redwardenstudios.com/updates/stream-kit.json` once per OBS start and
shows one line when a newer Kit exists. Multistream shows it when it is installed; Pre-Flight shows
it only when Multistream is not. Nothing is downloaded or run automatically, and the check can be
turned off in each tool's settings.

## Repository layout

- `multistream/` - Red Warden Multistream (OBS plugin source).
- `preflight/` - Red Warden Pre-Flight (OBS plugin source).
- `installer/` - the Kit installer (Inno Setup script) and the update-notice file.
- `tools/` - packaging and installer test scripts, and the script that publishes this repository.

Releases are tagged `kit-v<version>`. The source for every binary in a release is the tagged commit.

## Building from source

Windows only: Visual Studio 2022 and CMake 3.30 or newer. Each plugin is built on its own from its
folder with CMake presets, as in the OBS plugin template:

```
cd multistream      (or: cd preflight)
cmake --preset windows-x64
cmake --build build_x64 --config RelWithDebInfo
```

Each plugin folder has its own README with details and its test scripts.

## Building the installer

`tools\package.ps1` builds nothing: it packages the existing RelWithDebInfo builds of both plugins
and refuses unless each plugin's tests passed against that exact DLL. `-NoSign` makes an unsigned
dry run. `tools\test-installer.ps1` checks the installer end to end in a scratch folder. These
scripts and `installer\stream-kit.iss` were written for our development layout, where the plugins
sit beside the Kit as `red-warden-multistream\` and `red-warden-preflight\`; in this repository they
are `multistream\` and `preflight\`, so adjust those paths if you want to build the installer here.
The plugins themselves build as described above.

## License

Each tool is GPL-2.0-or-later (see `LICENSE`). The GPL covers the code, not the name: "Red Warden"
and the Red Warden logos are trademarks of Red Warden Studios LLC and are not licensed (see
`NOTICE.txt`). If you distribute a modified version, give it a different name and remove the logos.

## Support

support@redwardenstudios.com
