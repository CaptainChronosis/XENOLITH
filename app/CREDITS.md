# Credits

## Xenolith-specific work

- **The VT323 Project Authors / Peter Hull**: VT323 2.000, used for Xenolith's
  LCD lettering under SIL OFL 1.1. The unchanged font, licence and pinned
  Google/fonts provenance are retained in `assets/fonts/`; this instrument's
  `tools/gen_vt323.py` rasterizes it without replacing other firmware fonts.
- **James McCartney and the SuperCollider project**: the LFNoise2 recurrence and
  constructor behavior in `xenolith_core.c` are adapted from SuperCollider 3.14.1
  `server/plugins/NoiseUGens.cpp`, Copyright (c) 2002 James McCartney,
  GPL-2.0-or-later. The exact source URL and notice are retained in that file.
  The combined instrument uses the later-version option under GPL-3.0-or-later;
  `LICENSE` and `COPYING_SUPERCOLLIDER` contain the selected GPL version 3 text.
- **The operator's Zusammenbruch XX29 DSP_v1.46**: the original eight-cell
  Xenolith FM equations, control meanings and X_BUS inspiration. The original
  SuperCollider file remains unchanged; this firmware's compact exhaust is a
  new realization rather than a GVerb port.
- **Julius O. Smith III**: published explanations of sinusoidal FM/Bessel
  sidebands, Hann-window analysis, Schroeder allpasses and feedback delay
  networks informed the independently written analysis and exhaust modules.
  Research references are recorded with the workstation's test receipts;
  no claim of endorsement or copied implementation is made.

## Retained fm1-base and component credits

- **Leo Kuroshita** ([@kurogedelic](https://github.com/kurogedelic), [Hügelton Instruments](https://hugelton.com)):
  Felucca, the firmware this base grew from, and felucca-bsp, which it is built on. Much of fm1-base is his code,
  used under MPL-2.0 with his permission ([hugelton/felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)).
- **Charles Vestal** ([@charlesvestal](https://github.com/charlesvestal)): fm1-base, and the platform layer of the
  FM-1 firmwares it comes from (X0X, FoMni-1, CTL-1 and the others).
- **keremimo** ([@keremimo](https://github.com/keremimo)): the MIDI clock and transport sync in felucca-bsp's
  `usb.c` ([hugelton/felucca-bsp#3](https://github.com/hugelton/felucca-bsp/pull/3)).
- **Henri David** (hdavid): the own Bluetooth stack in `bt/` and felucca-bsp's radio
  drivers, from [Optimist](https://github.com/w0ts/optimist): BLE MIDI, pairing, the central role, and Bluetooth
  audio (BR/EDR, Secure Simple Pairing, A2DP), written from the Bluetooth specifications and measurements of the
  FM-1's radio. Offered under GPL-3.0-only or MPL-2.0 ([w0ts/optimist#55](https://github.com/w0ts/optimist/issues/55)),
  used here under MPL-2.0.
- JieLi's AC79 SDK (Zhuhai Jieli Technology, Apache-2.0): the radio's calibration that felucca-bsp's
  `chip/fm1_rf_cal.h` rewrites in C, and the radio's start-up tables (`chip/ble_rf_tables_v15.h`, as M-VAVE's stock
  firmware runs them).
- Android's Bluetooth stack (AOSP, Broadcom): the SBC encoder that Bluetooth audio sends (Apache-2.0).
- **Kenneth MacKay**: micro-ecc, the elliptic-curve keys of Bluetooth pairing (BSD-2-Clause).
- **Chris Farrell** ([@seajaysec](https://github.com/seajaysec)): from [fm-static](https://github.com/seajaysec/fm-static),
  the web installer's finding of an FM-1 under any port name, M-VAVE's Open firmware's identities, the package
  checksum check and the return check after resuming an update, used under MPL-2.0 with his permission
  ([seajaysec/fm-static#1](https://github.com/seajaysec/fm-static/issues/1#issuecomment-6410795859)).
- The idea of running FM-1 firmware in the browser: [Groove OS](https://www.groove-os.com/) by Peter Gombos (no
  code from it).
- Fonts: Barlow by The Barlow Project Authors and Terminus by Dimitar Toshkov Zhekov (both SIL OFL 1.1).
