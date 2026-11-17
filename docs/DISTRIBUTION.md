# Xenolith publishing with bounded transfer

The prepared `publish-installers.yml` is a one-time, operator-authorized source-tag
correction for 17 November 2026. Copy it to the repository's existing
`.github/workflows/publish-installers.yml` alongside the reviewed documentation
changes. Its exact-path push trigger and manual dispatch both require current
`main`. It uses the workflow's `GITHUB_TOKEN`; local GitHub CLI authentication is
not assumed or established by preparing this file.

It validates the 11 existing release assets using GitHub's SHA-256 metadata,
checks the corrected governor source, firmware manifest and known Git blob, and
requires the original commit to remain in `main` history. It advances `v1.0` only
from the recorded original commit or accepts an already-correct identical target.
It replaces only the exact reviewed source-tag paragraph in the release notes,
preserving all surrounding text. Missing digest metadata or unexpected changes
stop the job. There is no binary download fallback, asset upload or asset deletion.
Rerunning the same workflow commit is supported; a later source-tag change needs
another explicit review of its expected old and new identities.

For future binary publication, build and verify once locally or in GitHub Actions.
Upload those retained outputs directly to the intended GitHub release with
`gh release upload`, or upload the already-built Actions artifacts in the release
job. Confirm authentication and exact repository/release before invoking a local
CLI command. Do not make release jobs fetch their packages from the website.

The website should hold text, links, checksums and redirects to GitHub release
assets. Routine verification reads release metadata and checks the pinned digests;
download binaries again only for a specifically required end-to-end check. Keep
the prior package receipts and explicitly document any deliberate replacement of
a stable asset. Changing a package requires matching browsable source at its
release tag; an attached source bundle remains useful additional evidence.
