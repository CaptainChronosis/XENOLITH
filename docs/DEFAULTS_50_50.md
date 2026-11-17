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

Public naming remains V1 / v1.0. On 17 November 2026, the v1.0 tag was
corrected to the matching 50/50 source, as required for the Firmware Hub's
source mirror. Its automatic source archives and the attached
`XENOLITH-1.0-corresponding-source-3cf3093b.zip` both describe the corrected
firmware. The original 60/40 commit remains in repository history:
`d74efeeebfb6f873da3d5e2b579bfabd0455af02`.
Already-downloaded offline installers retain their old payload; download a
fresh installer to obtain this correction.
