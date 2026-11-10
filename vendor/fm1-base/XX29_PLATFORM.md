# XX29 FM-1 platform

This is the dedicated local platform for Xenolith and later XX29 instruments.
It is a pinned fork of `fm1-base` with its own `felucca-bsp` submodule checkout,
not a fork of the complete Felucca synth/sequencer application.

The application remains in `XENOLITH/source`. Build and launch through the
workstation's Xenolith tools. This repository supplies the application ABI,
device support, toolchain inputs and packaging machinery. It does not contain a
second editable copy of Xenolith.

## Ancestry and maintenance

| Component | Starting revision | Local branch |
|---|---|---|
| fm1-base | `d8ff8f41aa511e03d70a7c565c9fa0f1dd18ae2f` | `xx29/platform` |
| felucca-bsp (`bsp/`) | `305c372c3ec18f3917744e608998a0d7fd700135` | `xx29/platform` |

Both Git repositories have independent object storage and history. The original
`04_WORKBENCH/fm1-base` and its BSP remain unchanged. The remotes named `upstream`
record provenance; this local fork has not been published and has no automatic
update process. Future upstream fixes can be reviewed and selectively applied.
There is no requirement to follow each Felucca release or keep this fork rebased.
The `bsp` gitlink pins the selected BSP revision; do not use submodule `--remote`
as part of routine builds.

All inherited licences and notices remain. Keep platform changes separate from
instrument changes. No PLL, voltage, USB-MIDI loader handshake or package-layout
change was made when establishing this fork.

## Initial dual-core contract

The existing BSP already implements CPU1 startup before audio/timers, early hold
before RAM initialisation, stack/EMU guards, a checked mailbox, cross-core `csync`,
core-aware faults and retirement. These are not new XX29 fixes. See
[the review disposition](docs/XX29_ARGON_REVIEW_2026-11-01.md).

The simple initial application use is a foreground FFT job over an immutable
capture, followed immediately by `plat_cpu2_wait()`. The audio ISR keeps complete
ownership of synthesis, envelope and reverb and never waits for this worker.
The foreground cannot read its analyzer or enter storage/OTA until the join
returns. A failed worker must be retired before its scratch is invalidated and
recomputed on CPU0. This is display-work distribution, not doubled audio capacity.

Use `cpu2 = true` and `cpu2_fallback = true` only in a deliberately tested
application. Start with `plat_cpu2_set(1)` in `app_init`; do not restart a retired
worker during playback. The current 8 ms timeout begins at `wait()` entry, so
dispatch must be followed immediately by that call in this design. Longer FFTs
remain on CPU0 until measured against the bound. An asynchronous worker or audio
partition would need an additional deadline/ownership design and new tests.

`device/main_fm1.c` services update/restore in its foreground loop outside
`app_frame()`. The bounded wait calls no yield or storage function. CPU1's idle
loop is in RAM, so after a successful join it does not fetch worker code from
flash. Application code must retain these invariants. The emulator does not
model physical cache or bus coherence and cannot certify an individual unit's
CPU1 reliability.

## Verification boundary

Fork creation itself changes no C, assembly, linker, loader or build-tool source.
`XX29_PROVENANCE.json` records the baseline. Application builds and emulator
results are retained under `XENOLITH/verification` and `XENOLITH/sessions`; they
must identify this platform and the selected BSP revision. No physical flash was
performed to establish this repository.

Subsequent local repairs are recorded separately. The
[audio startup repair](docs/XX29_AUDIO_START_2026-11-01.md) defers DMA until its
interrupt service is ready; its baseline diagnostic and verification boundary
are retained with the instrument sessions.
