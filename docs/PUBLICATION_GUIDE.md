# Publishing Xenolith

Checked 10 November 2026. Repository: [CaptainChronosis/XENOLITH](https://github.com/CaptainChronosis/XENOLITH).

No Firmware Hub submission has been sent by this workflow. Use the final release's
evidence for its tested package and remaining limits; this is the publishing procedure.

Use a tagged release with complete source and a directly attached `.fwsc`. A ZIP
is useful for the Windows updater, but is not a substitute for the source tree.
Keep this workstation's recordings, research library, private session logs and
toolchain installation outside the public repository.

## The five different things on GitHub

| Item | Purpose | Where to change it |
|---|---|---|
| **About** | Short description, website and search topics | Gear beside About on the repository's main page |
| **README.md** | Main introduction, pictures, installation and documentation links | Edit the file |
| **fmhub.json** | Firmware Hub card description, category, image and links | Edit the root JSON file on `main` |
| **Commit** | Recorded revision of files | Created when changes are saved to Git |
| **Release + tag** | A named version, its exact source revision and downloadable firmware | Releases  ->  Draft a new release |

Suggested About text: **Experimental cell-based FM synthesizer for the M-VAVE FM-1.**
Suggested topics: `fm-1`, `m-vave`, `synthesizer`, `fm-synthesis`, `firmware`.
About and README do not change the USB identity or the firmware's compiled
metadata. Those come from source and require a new build when changed.

## Editing your words and pictures

Open `README.md`, use the pencil button, make the change, select Preview, then
Commit changes with a short description. For a documentation-only change on your
own unprotected repository, saving directly to `main` is sufficient; a branch and
pull request are useful for changes you want to review separately.
[GitHub's file-editing guide](https://docs.github.com/en/repositories/working-with-files/managing-files/editing-files).

Put a new image under `docs/`, then use a relative Markdown link:

```markdown
![Xenolith on the FM-1](docs/xenolith-device.jpg)
[Playing guide](docs/PLAY.md)
[Installation](INSTALL.md)
```

Keep filename spelling and capitalisation consistent. Use ordinary PNG/JPEG for
images and Markdown or PDF for longer documents. A README image can be larger
than the Hub's square card image; they serve different layouts.
[GitHub's README guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-readmes).

Your edits should remain authoritative. Before a later export, pull the current
GitHub revision, inspect the difference, and merge only the new source/package
files. Do not replace a hand-edited README or `fmhub.json` with an older generated
copy. Keep a separate publication checkout; never initialise Git over the whole
FM-1 workstation. A useful instruction to Codex is: "Preserve my README and card
wording; show any proposed changes to those files separately."

## Why open source is the sensible route here

Xenolith already contains licensed source whose distribution obligations apply
to the combined firmware. The specific GPL connection is the **LFNoise2 recurrence
and constructor adapted from SuperCollider**, not a blanket assumption that
everything descended from Felucca is GPL. `app/xenolith_core.c` retains James
McCartney's GPL-2.0-or-later provenance; Xenolith distributes the combined work
under **GPL-3.0-or-later**, using that later-version permission.
[Original SuperCollider file](https://github.com/supercollider/supercollider/blob/Version-3.14.1/server/plugins/NoiseUGens.cpp).

Recipients may study, modify and redistribute the covered software, including
commercially, while respecting the licence. They do not gain write access to your
repository or authority to label their fork your official release. You can keep
maintainer control, decline proposed changes and publish at your own pace. The
amount of code or tokens needed to understand a project is not an access control.
[GPL text](https://www.gnu.org/licenses/gpl-3.0.html).

For this instrument, openness also makes bug reports, hardware compatibility and
long-term rebuilds more credible. Keep complete corresponding source beside each
binary: application, modified platform and BSP, build scripts, table/font
generators and inputs, required packaging resources, notices and build directions.
A general-purpose unmodified compiler can be documented separately rather than
bundled; the export pins its acquisition details and hash. The exact tagged source
must build the released binary, rather than merely resemble the latest work.

Individual files retain their own terms. The wrapper and much of the pinned
`fm1-base`/`felucca-bsp` code are MPL-2.0, including upstream relicensing permissions
recorded in `vendor/fm1-base/LICENSING.md`. Keep those notices. MPL works at the
file level; do not rewrite all third-party headers as GPL or claim sole authorship.
[Mozilla's licence explanation](https://www.mozilla.org/en-US/MPL/2.0/FAQ/).

Minimum credit record:

| Contributor/component | What Xenolith uses |
|---|---|
| CaptainChronosis / XX29 | Instrument design, original SuperCollider patch and Xenolith implementation |
| James McCartney / SuperCollider | Adapted LFNoise2 recurrence and constructor; GPL provenance |
| Charles Vestal / fm1-base | Application/platform framework and template |
| Leo Kuroshita / Hügelton / Felucca BSP | Hardware support and retained platform code |
| keremimo / Kerem Kılıç | MIDI clock and transport synchronisation in the BSP's USB code |
| VT323 Project Authors / Peter Hull | Font; SIL OFL 1.1 |
| JieLi and other retained components | Licensed SDK packaging resources and component notices |

Keep the fuller `app/CREDITS.md`, `app/THIRD_PARTY.md`, framework/BSP notices and
all accompanying licence files. Research influences belong in a separate
acknowledgement paragraph: a Smith or Radiotron reference does not imply copied
code, endorsement, or authorship of Xenolith. This exhaust is not a GVerb port.

## Making the release

1. Select the verified package and its matching source export. Check the package
   SHA-256 against its receipt; retain the same `.fwsc` unchanged.
2. Commit the source, build inputs, notices, guide and final metadata. Confirm the
   release tag will point to this commit, not an older snapshot.
3. On GitHub choose **Releases  ->  Draft a new release**. Create a version tag such
   as `v0.1.0` at that commit. Use a plain title and short notes describing changes,
   installation and tested limits.
4. Attach the `.fwsc` directly, `SHA256SUMS.txt`, and optionally the Windows updater
   ZIP. The ZIP should contain that exact package, its checksum and instructions.
5. Mark an initial experimental version as a pre-release. Save the draft until
   the hardware transition checks and source export are complete; then publish.

GitHub automatically provides source archives for the tagged revision. Those are
additional conveniences; the repository must still contain readable files. If
immutable releases are enabled, assemble all assets before publication.
[GitHub's release instructions](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository).

GitHub accepts ZIP files, but uploading one source ZIP creates one opaque file;
it does not turn its contents into a normal repository. Use Git/GitHub Desktop or
the prepared file upload to preserve the directory tree. Firmware users generally
need only the `.fwsc` or updater ZIP, not the complete source checkout.

## Browser playback and the Jukebox

No hosted Xenolith browser demo is planned for this release. Leave `play` out of
`fmhub.json`; a browser demo is optional and does not affect ordinary firmware
listing or browser-based installation. Local flashing uses the supplied updater.
[Hub requirements](https://thefirmwarehub.com/submit/).

The [FM-1 Jukebox](https://thefirmwarehub.com/dj/) is a separate collection of
recorded music. A firmware submission does not automatically add a track or a
playable Xenolith instrument. A voluntary track upload requires a Hub sign-in,
your own FM-1 recording, MP3/WAV up to 30 MB, and the displayed playback/mixing
permission. Its limit is one track per day. Nothing has been uploaded here.

The Hub also offers a separate source-conversion route for its VIBE FM player
and plugins. That requires conversion and checking; a firmware listing alone is
not evidence of Xenolith compatibility. It is outside this release's scope.
[Conversion service](https://thefirmwarehub.com/convert/).

## Firmware Hub submission

For the normal hosted installation route, provide public licensed source, the
GitHub Release asset, unique identity, included update loader, real-device
transition tests, and root `fmhub.json`. Test entry from V15 and another firmware,
and exit to another firmware. Explain patch incompatibility. Recheck
`FM-1_29000` in the identity checker before submitting; a previous free result is
not a reservation. Newer UC25HQ80 flash support needs attention alongside Puya.
[Current submission requirements](https://thefirmwarehub.com/submit/).

The card currently uses `kinds: ["synth"]`, your author name, a short plain-English
description, `docs/card.png`, and a readable rust-red accent. The picture must be
square, 240-1024 pixels, PNG/WebP/JPEG and no more than 300 KB. Optional browser,
editor and cheat-sheet links must actually exist; omit unavailable ones. Validate
the JSON and use the Hub's card checker. Card edits and later releases are normally
picked up within three hours.
[Metadata specification](https://thefirmwarehub.com/fmhub.md).

The Hub also offers link-only cards for closed-source projects with their own
installer, and its FAQ allows a package served without a GitHub Release. Neither
exception removes Xenolith's existing source-licence obligations. The tagged
release route above is clearer for this project.

Send links, not the binaries, to **zven@thefirmwarehub.com**. Draft only:

```text
Subject: FM-1 firmware submission - Xenolith

Firmware: Xenolith
Author: CaptainChronosis / XX29
Repository: https://github.com/CaptainChronosis/XENOLITH
Release: [published release URL]
Identity: FM-1_29000
Combined licence: GPL-3.0-or-later; component notices retained
Card metadata: repository-root fmhub.json

Hardware checks: [exact tested package, V15 entry, other-firmware entry,
outgoing update and observed USB identity; state any untested chip revision]

Installation and playing instructions are linked from the README.
Please let me know if anything further is needed for listing.
```

The separate [aroum firmware directory](https://github.com/aroum/fm1-custom-fw)
can be approached afterwards with a concise issue or README pull request linking
the repository, release and description. That is a separate curated listing;
Firmware Hub acceptance does not automatically update it.

## Release readiness

Before publishing, check this short list:

- The release tag contains the complete buildable source and component notices.
- The attached firmware matches the package hash in `evidence/BUILD.json`.
- The same candidate has the required real-device entry and outgoing-update tests.
- `README.md`, `INSTALL.md` and `docs/PLAY.md` describe that candidate's controls.
- The updater uses Python 3.12 and installs binary dependency wheels; its package
  is the same `.fwsc` attached directly to the release.
- `fmhub.json` has a working image and omits links which are not being provided.

A successful build or emulator session does not fill in a missing hardware test.
Keep measured limits in [build and device evidence](../evidence/BUILD.json).
The player guide should contain controls, not old test transcripts.

Save Markdown and JSON as UTF-8. For PowerShell uploads, read with
`Get-Content -Raw -Encoding UTF8`; for Python use `read_text(encoding="utf-8")`.
Preview the actual GitHub page after upload. Incorrectly decoding UTF-8 as a
Windows code page produces strings such as the broken dash previously visible
in the public playing guide. ASCII punctuation is used in that guide to reduce
this risk, but contributor names should retain their correct spelling.
