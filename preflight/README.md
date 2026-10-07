# Red Warden Pre-Flight

A dock for OBS Studio that checks your setup before you go live. Each check is a green tick, an
amber warning or a red problem, with one plain sentence saying what to fix.

Pre-Flight is part of the [Red Warden Stream Kit](https://redwardenstudios.com/division/bastion/stream-kit/),
next to Red Warden Multistream. It works on its own or together with Multistream. Windows only.

## Checks

- **Mic unmuted**: whether your chosen microphone is muted in OBS.
- **Mic test**: you press Test mic and speak; it listens for 5 seconds and measures the signal level.
- **Desktop audio**: whether your desktop / game audio source is muted (only if you chose one).
- **Starting scene**: whether the scene you picked is the one live in OBS.
- **Main stream**: whether OBS's own stream (Settings > Stream) has a server and a key.
- **Multistream**: whether at least one Red Warden Multistream destination is switched on.
- **Recording**: whether OBS will record with your stream, the recording folder is writable, and how
  much disk space is left.
- **OBS keeping up**: dropped frames over the last 30 seconds.
- **Your checklist**: your own items, ticked by hand.

**Check & Go Live** runs the list and starts OBS's main stream when nothing is red; otherwise it
lists the problems and lets you go live anyway. Pre-Flight never blocks OBS's own Start Streaming
button and never stops a stream that has started.

## Building from source

Windows, Visual Studio 2022, CMake 3.30 or newer. The project follows the OBS plugin template.

```
cmake --preset windows-x64
cmake --build build_x64 --config RelWithDebInfo
```

The unit tests (no OBS needed) are built with the plugin and end up in `build_x64\RelWithDebInfo\`.
`tools\run-all.ps1` builds and runs the unit tests and the test harnesses. The installer and zip are
built by the Kit project (`tools\package.ps1` in the repository root).

## License

GPL-2.0-or-later, see `LICENSE`. The GPL covers the code, not the name: "Red Warden" and the Red
Warden logos are trademarks of Red Warden Studios LLC. If you distribute a modified version, give it
a different name and remove the logos.

## Support

support@redwardenstudios.com
