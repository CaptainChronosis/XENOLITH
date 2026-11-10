# Build on Windows

Python 3.12, Pillow and the JieLi Windows 2.5.3 toolchain are required.
`tools/TOOLCHAIN_MANIFEST.json` contains the compiler acquisition URL and pinned
hashes. Toolchain binaries are not included. The complete modified fm1-base and
concrete BSP source are in `vendor/fm1-base`; no submodule checkout is needed.
The minimal licensed SDK packaging resources are in `sdk-resources`.

```powershell
py -3.12 -m venv .venv
.venv/Scripts/python -m pip install -r requirements.txt
.venv/Scripts/python tools/build_windows.py --toolchain 'C:/path/to/pi32'
```

The compiler directory contains `bin/clang.exe`. Alternatively set
`JIELI_PI32_ROOT` and use `Build Windows.bat`. Outputs are under `app/build`;
a failed build never launches or installs firmware.

The adapter verifies compiler and SDK hashes. The application still uses its
existing development identity `FM-1_29000`; the adapter does not create a new
version identity. Changing `app/fm1.toml`, fonts or source requires rebuilding
and testing the new binary. [Build evidence](evidence/BUILD.json) names the
packaged input and verification scope.

The independent control tests need a host C compiler available as `gcc` (or set
`CC` for the first command). They do not open an audio device or MIDI port:

```powershell
py -3.12 tools/tests/verify_midi_controls.py
py -3.12 tools/tests/verify_midi_app_host.py
py -3.12 -m unittest discover -s tools/tests -p test_update.py
```

The first command covers note ownership, sequencer clocks and recording. The
second extracts the actual application's MIDI pump and queue consumers, using
mocked ports. These tests do not establish hardware timing or TRS electrical
compatibility.
