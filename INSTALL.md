# XENOLITH / Installation

Download the installer for your platform from the [README](README.md) or
[XX29](https://xx29.org/). The new packages include private Python and MIDI
dependencies, source and component notices; no separate Python installation
or Internet connection is required to use them.

## Start the updater

- **Windows 10/11 x64:** run `XENOLITH-Updater-1.0-Windows-x64.exe`.
  No ZIP extraction, setup script or administrator account is needed.
  The EXE is unsigned; Windows may show an unknown-publisher prompt.
- **macOS Apple Silicon (11+) or Intel (10.15+):** extract the matching archive,
  then open `Update XENOLITH.command` in Terminal. These packages are unnotarized.
  Review any download block in Privacy & Security; the updater does not remove
  quarantine flags or change Gatekeeper settings.
- **Linux x86_64:** extract the archive with `tar -xzf`, open a terminal in its
  folder and run `./Update-XENOLITH.sh`. Requires glibc 2.28+, ALSA
  `libasound.so.2`, `libstdc++.so.6` (GLIBCXX_3.4.21+) and access to the ALSA
  MIDI sequencer. Do not run as root. See the included README for details.

The macOS and Linux packages are **experimental**. Archive checks and 19 shared
wrapper checks passed on Windows; native execution and hardware testing on
those platforms have not been done. The Windows EXE passed 16 offline launcher
checks and a physical installation on one FM-1 on 16 November 2026, with all
17 saved objects identical before and after. This does not qualify every host
setup or FM-1 revision. The newer UC25HQ80 chip is recognized but has not been
tested on physical hardware.

## Install and preserve saved patches

1. Save panel edits on the instrument, stop playback, connect one FM-1 with a
   USB data cable and close other MIDI applications.
2. Choose **Install**, select the FM-1 port and check the displayed package and
   device identity before confirming.
3. When Xenolith is running, installation first backs up its saved settings and
   all 16 slots. A backup failure blocks writing. Other firmware's patches
   cannot be backed up through this saved-object protocol; the updater requires
   explicit confirmation before proceeding without that backup.
4. Leave USB connected until transfer and restart identity verification finish.
   Retain any error message if the updater stops.

Backups and operation receipts remain outside the downloaded EXE/archive:
Windows uses `%LOCALAPPDATA%\XX29\XenolithUpdater`; macOS uses
`~/Library/Application Support/XX29/XENOLITH Updater/backups`; Linux uses
`${XDG_DATA_HOME:-~/.local/share}/xx29/xenolith-updater/backups`.
Use the Windows EXE's `--paths` option for its exact backup directory.
Keep a separate copy of important backups. Restore replaces the saved objects
and requires confirmation plus physical SELECT on the FM-1. These backups
exclude unsaved edits, firmware, bootloader, calibration and BLE data.

A compatible USB-MIDI update loader is required. There is no blanket requirement
to return through stock V15; compatibility depends on the firmware currently
installed. The updater is not a universal ROM recovery tool and does not install
an alternate bootloader. It does not include or modify M-VAVE's official rescue
tools. [Official/Hub recovery information](https://thefirmwarehub.com/tools/rescue/).

The bundled firmware and runtime are checked before MIDI opens. The pinned
installer checks package identity and loader presence and checks the installed
identity after restarting. An identity ACK is not a binary readback.
Add `--check` to the platform launcher for an offline integrity/import check;
this opens no MIDI devices.

## Legacy Windows ZIP / repository scripts

The original [Windows updater ZIP](downloads/XENOLITH-Windows-updater-85079c89.zip)
and the repository's BAT scripts still require Python 3.12 with the Windows
`py` launcher. Extract the ZIP completely, run `Setup updater.bat` once, then
`Update.bat`. Setup explicitly uses 3.12 and installs prebuilt packages into
`.venv-updater`; it does not require a C++ compiler. If an older setup created
that folder with another Python version, rename it and run setup again.

Its menu uses **1** for bundled Xenolith, **2** to read device identity and
**3** for another local `.fwsc` package. Select the numbered MIDI port and
answer `y` only when ready. For its offline package check, run
`python tools/update.py --check-package`. Its local-file installation path was
exercised on the connected FM-1; setup on a clean Windows PC has not been
qualified. Use the new EXE for the bundled runtime and saved-object backup flow.
