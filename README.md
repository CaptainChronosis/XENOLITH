# XENOLITH

![Xenolith boot emblem](docs/card.png)

Experimental cell-based FM synthesizer for the M-VAVE FM-1.

Xenolith combines up to 64 paired oscillator cells with parallel, serial,
branching and feedback connections. It includes selectable FM laws, up to
eight-note paraphony, modulation generators, sequencing, a multimode filter,
VCA and wavefolder, digital rate/quantization controls and stereo reverb. USB and
TRS MIDI input receive notes and external clock/transport.
The interface uses four labelled knobs, a dark display and the XX29 red palette.

- [Download firmware](firmware/XENOLITH1.fwsc)
- [Windows updater ZIP](downloads/XENOLITH-Windows-updater-85079c89.zip)
- [Installation](INSTALL.md) / [playing guide](docs/PLAY.md) / [build instructions](BUILD.md)

This is the 10 November 2026 development snapshot, package `85079c89`.
This candidate was installed from stock V15, changed to JIANT, and restored
through the Windows updater. Standard USB note and transport checks passed; the analogue recording confirms
the programmed note order at 120 and 90 BPM and release after Stop or clock loss.
[evidence/BUILD.json](evidence/BUILD.json) records the measured scope and limits.
Optional heavy processing reduces the admitted internal sample rate; the output
transport stays at 44.1 kHz. A complex graph is not guaranteed the same internal
rate as an eight-cell parallel patch.

The Windows updater requires Python 3.12 with the Windows `py` launcher, uses
the pinned FM-1 installer protocol and asks before writing. Its portable wrapper has offline tests, and its local-file installation path
was exercised on the connected device. Other firmware may erase saved patches.

The platform recognizes the original Puya and newer UC25HQ80 1 MiB flash IDs.
UC25HQ80 support is based on its manufacturer datasheet and offline tests; a
physical FM-1 of that revision has not been available for qualification.
The embedded repository and dictionary links point to this repository. No
Firmware Hub listing is claimed.

## MIDI status

USB MIDI from a computer and the TRS MIDI input receive notes on all channels.
The enabled sequencer follows 24-PPQN Clock after MIDI Start or Continue; bare
clock pulses never start it. Stop or clock loss releases the sequence, while
local PLAY returns to internal BPM. MIDI notes can enter tonic step recording.

Velocity is fixed; Note On with zero velocity releases the note. CC120/123
release the originating channel's notes. Pitch bend, sustain, other CCs, MIDI
output and thru are not implemented. Direct USB-keyboard hosting and Bluetooth
are disabled. See the [playing guide](docs/PLAY.md#midi-connections).

## Source and licences

The complete application and concrete modified platform/BSP sources are included.
The isolated exported source rebuilt byte-for-byte to the bundled firmware.
The combined instrument is GPL-3.0-or-later; individual framework, font and SDK
components retain their original terms. See [LICENSE](LICENSE),
[component notices](app/THIRD_PARTY.md) and [credits](app/CREDITS.md).
