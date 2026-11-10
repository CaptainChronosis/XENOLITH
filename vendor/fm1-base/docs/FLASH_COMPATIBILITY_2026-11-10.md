# FM-1 1 MiB flash compatibility

The application now recognizes JEDEC `0x856014` and `0xB36014`. The rescue
tool accepts either ID only with the existing chip key `0x980F` and flash type
`3`. Other IDs remain ineligible; a matching capacity byte alone is insufficient.

This is a recognition change. The SPI driver, loader, USB update handshake,
erase/program timeouts, flash layout and write policies are unchanged.

## Evidence

The manufacturer's [UC25HQ80 Version 1.2 datasheet](https://www.ucuntech.com/uploads/20220324/d8602ea203b2303c2162d067ce715c4c.pdf)
identifies the 8 Mbit / 1 MiB device as `B3 60 14` after command `9F`
(Table 8.0, PDF page 14). Its documented operations match the subset used by
`bsp/chip/fm1_flash.h`:

| Operation | Opcode | Existing driver contract |
|---|---:|---|
| Write enable | `06` | WEL checked before writing |
| Read status | `05`, `35` | BUSY bit 0 and WEL bit 1 of status register 1 |
| Fast read | `0B` | Three address bytes and one dummy byte |
| Page program | `02` | At most 256 bytes; no page crossing |
| Sector erase | `20` | Aligned 4 KiB sector |
| JEDEC identification | `9F` | Three identification bytes |

Read, erase and program descriptions are on PDF pages 18, 26 and 28. The
program/erase AC table is on PDF page 53. Its 3 ms maximum page-program time
fits the existing 10 ms program timeout; the documented sector-erase timing
also fits the existing 500 ms timeout. No new status-register writes, quad-mode
transitions or chip erase are introduced. The optional unique-ID/radio path is
not exercised by Xenolith, whose Wi-Fi/BLE options are disabled.

This supports source-level compatibility of the command subset. It does not
establish operation on an actual UC25HQ80-equipped FM-1, electrical margins,
or the behaviour of its factory bootloader and protection settings. Those still
require a device of that revision. No such device was used for these checks.

## Boundaries retained

- The normal update loader programs the application only in `[0x4000, 0x93000)`.
  Its authenticated update-record cleanup can erase recognized record sectors
  above that range, excluding the store `[0x97000, 0xE0000)` and globals at
  `0xFC000` and above. Package length, CRC and chip-key checks are unchanged.
- The loader never writes the protected flash head `[0, 0x4000)`.
- Rescue still accepts the exact hash-pinned stock V15 package, reads and saves
  the entire 1 MiB flash first, rejects a protected-head mismatch, writes only
  differing application sectors, and verifies each write plus the final image.
  Its tail `[0x93000, 0x100000)` remains untouched.
- Runtime storage retains its existing range checks. Recognizing a second
  1 MiB part provides no additional address space.
- The normal loader has no JEDEC whitelist of its own and was not changed.
  Its existing command and range checks remain authoritative once launched.

## Offline verification

- `bsp/tests/flash_id_test.c`: exhaustively checked all 16,777,216 possible
  three-byte IDs; exactly the two listed IDs accepted. Upper-byte contamination
  is rejected as well.
- `bsp/tests/test_rescue_flash_ids.py`: four test groups pass. Both supported
  identities reach the backup/check flow; unknown IDs, wrong keys and wrong
  types fail before a flash read/write; protected-head changes are refused;
  an application-sector write preserves the entire tail for both IDs.
- Existing `tests/ldr_test.c`, using immutable packages `03ce656b` → `601c8d1b`:
  eight checks pass, including changed-image installation, protected-head
  preservation, allowed write ranges, idempotent reinstall and rejection of a
  damaged/foreign-key application before erase. The model performed 84 sector
  erases on the changed-image pass. This exercises the unchanged portable
  loader logic, not physical flash command timing.

No firmware build or physical flash was performed as part of this isolated
compatibility edit. Subsequent release verification records those separately.
