# Argon review disposition — 1 November 2026

Compared against pinned fm1-base `d8ff8f4` and felucca-bsp `305c372`, not against
the review's renamed `app_2.c` attachment or assumed current upstream state.
This document is the platform-side review; application changes and validation
are recorded by Xenolith separately.

| Suggestion / claim | Actual source evidence and decision |
|---|---|
| Ramp VDC14 and SYSVDD to cure CPU1 misreads | No measured rail values, droop trace or causal diagnosis is supplied for this unit. `bsp/docs/CPU.md` describes an instruction-interaction failure and reports a tested SYSVDD change that did not cure it; that does not establish the cause of all CPU1 failures either. **No voltage or clock change.** |
| Hold CPU1 before BSS clear | Already at the start of `device/main_fm1.c:fm1_cstart`, before RAM setup, with `FM1B_CPU2`. Keep it. |
| Initialise CPU1 stack guards and EMU_CON | Already done in `bsp/chip/fm1_cpu1.h:fm1_cpu1_main`, including the 256-byte lower guard. Keep it. |
| Avoid CPU1 halting itself inside reboot | Already handled in `fm1_cpu1_fault_c`: identifies the core with `fm1_cnum()`, records the fault, parks CPU1 and leaves CPU0 to retire/restart it. Stack-range guessing is unnecessary. |
| Add mailbox canary and separate ownership | Already checked by `FM1_CPU1_CHECK`; CPU0/CPU1 words occupy separate 32-byte-aligned regions. Keep the source-proven implementation instead of replacing it with a different XOR constant or guessed cache-line width. |
| Replace every compiler barrier with `csync; ssync` | `X_PUBLISH_BARRIER` documents CPU0 ISR/foreground publication. It is not used as the worker fence. The platform already exports `plat_cpu2_sync()` backed by `csync`. Do not scatter unverified `ssync` instructions or change unrelated single-core publication. |
| Audio block is 32 frames | Active `XENOLITH/source/app.h` sets `APP_BLOCK=256`: 5.805 ms at 44.1 kHz. The quality function's `32u` is not the platform callback length. |
| AUTO scales down to22.05/11.025k when cells rise | AUTO's low end is native44.1k. Those lower rates are separate deliberate reduced-rate modes, not the next automatic cell-count tiers. |
| Split the bank and use two RNG streams | New streams change drift trajectories. A sound-preserving split also needs committed/working state, private output, ordered controls and fallback that cannot double-advance partially rendered cells. Defer this larger refactor. |
| Foreground FFT and audio worker share CPU1 | One mailbox exists. A hybrid scheduler needs explicit ownership; do not submit an audio job while an FFT is outstanding. Initial use is foreground FFT only. |
| Move quality state into faster RAM | Different linker sections do not by themselves prove different access latency. Defer without measurement; no arbitrary relocation. |
| Proposed Padé limiter is bounded to±0.95 | It is not bounded for large input: after its square term is clamped to9, output grows linearly as one third of input. Do not replace the existing output bound with this formula. |

The review's tube circuit comparisons are not measurements of the FM-1. The
useful engineering concerns are ownership, deadlines, failure recovery, shared
memory and flash access. These are addressed directly in the initial design.

## Why synchronous foreground FFT is the first use

The CPU1 worker owns the existing analyzer and frozen input until an immediate
bounded join returns. CPU0 can still service audio interrupts during that wait.
No interrupt reads or changes the FFT scratch; UI/LED reads occur afterwards.
On failure, the platform holds CPU1 and reports retirement before CPU0 resets
the analyzer and computes a fresh result. A successful return publishes the
finished result through the platform fence. No additional40KiB analyzer is
needed, and no output audio buffer depends on CPU1 completion.

Storage and OTA are foreground operations in this platform. The join does not
call `plat_yield`, the event loop or any store function. Thus a joined job is
complete/retired before those operations can turn flash off. This argument
applies only to synchronous use; it must be revisited before asynchronous jobs.

## Scope of the fork

This repository is independent maintenance infrastructure. The inherited CPU1
implementation already supplies the requirements of this first partition, so
there is no justification for a cosmetic rewrite of its low-level code. Future
changes can be made here with separate commits and provenance without waiting
for, or automatically accepting, a new Felucca release.
