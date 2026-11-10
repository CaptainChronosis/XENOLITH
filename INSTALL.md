# Install Xenolith

For Windows, download the updater ZIP from the README and extract it completely.
Install Python 3.11 or newer with its Windows `py` launcher if it is not already
available. Run `Setup updater.bat` once, then `Update.bat`.

1. Connect the FM-1 with a USB data cable and close other MIDI flashers.
2. Choose **1** for the bundled Xenolith package or **2** to read device identity.
3. Select the FM-1 from the numbered MIDI ports. Check the displayed package
   identity and SHA-256; answer `y` only when ready to install.
4. Leave the cable attached until the updater reports completion. If it stops in
   update mode, run the installer again to resume; retain its error message.

**3** selects another local `.fwsc` package using the same installer. Compatibility
is firmware-specific: this menu is not a promise that every third-party or stock
transition works. It does not include or modify M-VAVE's official rescue tools.
[Official/Hub recovery information](https://thefirmwarehub.com/tools/rescue/).

The bundled package is checked against `firmware/package.json` before MIDI opens.
The pinned installer checks package identity and loader presence and checks the
installed identity after restarting. An identity ACK is not a binary readback.
Preserve valued patches separately before switching firmware.

For an offline check, run `python tools/update.py --check-package`.
This opens no MIDI devices. Dependencies are installed into `.venv-updater`,
inside the extracted folder; no administrator access or browser extension is
required by this wrapper.

The wrapper is new and offline-tested. This exact firmware has separate physical
validation using the established workstation installer; the portable ZIP's own
physical installation remains to be checked. Newer UC25HQ80 units are not yet
qualified by this build.
