# XENOLITH

![Xenolith boot emblem](docs/card.png)

Experimental cell-based FM synthesizer for the M-VAVE FM-1.

Xenolith combines up to 64 paired oscillator cells with parallel, serial,
branching and feedback connections. It includes selectable FM laws, up to
eight-note paraphony, modulation generators, sequencing, a multimode filter,
VCA and wavefolder, digital rate/quantization controls and stereo reverb.
The interface uses four labelled knobs, a dark display and the XX29 red palette.

- [Download firmware](firmware/XENOLITH1.fwsc)
- [Windows updater ZIP](downloads/XENOLITH-Windows-updater-601c8d1b.zip)
- [Installation](INSTALL.md) / [playing guide](docs/PLAY.md) / [build instructions](BUILD.md)

This is the 10 November 2026 development snapshot, package `601c8d1b`.
The firmware has been tested on the developer's FM-1 through its mixer output.
[evidence/BUILD.json](evidence/BUILD.json) records the measured scope and limits.
Optional heavy processing reduces the admitted internal sample rate; the output
transport stays at 44.1 kHz. A complex graph is not guaranteed the same internal
rate as an eight-cell parallel patch.

The Windows updater uses the pinned FM-1 installer protocol and asks before
writing. Its portable wrapper has offline tests; physical installation through
this exported wrapper has not yet been tested. The firmware itself was installed
through the workstation's tested updater. Other firmware may erase saved patches.

Newer UC25HQ80 flash-chip units are not yet qualified by this snapshot; the current
platform recognises the older Puya part for writable storage. The embedded
repository/dictionary metadata still contains development placeholders. This
repository is the source location for this snapshot; those embedded fields will
be corrected in a subsequent tested build. No Firmware Hub listing is claimed.

## Source and licences

The complete application and concrete modified platform/BSP sources are included.
The isolated exported source rebuilt byte-for-byte to the bundled firmware.
The combined instrument is GPL-3.0-or-later; individual framework, font and SDK
components retain their original terms. See [LICENSE](LICENSE),
[component notices](app/THIRD_PARTY.md) and [credits](app/CREDITS.md).
