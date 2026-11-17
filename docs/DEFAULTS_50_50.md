# V1 default correction — 17 November 2026

Factory LOAD ONSET and SPAN are both 50%. The compiled change is limited to
those two defaults in `app/xenolith_governor.c`; the accepted REC lamp flicker,
DSP algorithms, controls and patch schema are unchanged.

Firmware SHA-256:
`3cf3093b31f88696461dfd3bb4e366328ba0780212366715690785c9d58f2611`

The complete source export rebuilt byte-for-byte to the 611,496-byte package.
The corrected firmware was installed and checked on one FM-1 on 17 November;
all 17 saved objects matched before and after. This does not establish hardware
qualification for every FM-1 revision or native macOS/Linux installation.

Public naming remains V1 / v1.0. The original v1.0 tag is unchanged; its GitHub
automatic Source code archives describe the original 60/40 build. Use the
attached `XENOLITH-1.0-corresponding-source-3cf3093b.zip` for the corrected
firmware. Current main also contains the corrected source and firmware.
Already-downloaded offline installers retain their old payload; download a
fresh installer to obtain this correction.
