/* SPDX-License-Identifier: MPL-2.0
 * XX29 independent keyboard assignment and held-key cell-band selection. */
#ifndef XX29_XENOLITH_KEYBOARD_H
#define XX29_XENOLITH_KEYBOARD_H
#include <stdint.h>

#define XENOLITH_KEYBOARD_KEYS 27u
#define XENOLITH_KEYBOARD_CELLS 64u
#define XENOLITH_KEYBOARD_KEY_MASK UINT32_C(0x07ffffff)

enum { XKEY_TONIC, XKEY_INDEX, XKEY_RATIO, XKEY_CORE_RATE, XKEY_WET_RATE,
       XKEY_DRIFT, XKEY_WET_MIX, XKEY_CELLS, XKEY_AMPLITUDE, XKEY_BITS, XKEY_EXP_DEPTH, XKEY_FILTER_CUTOFF, XKEY_FILTER_RESONANCE, XKEY_DESTINATION_COUNT };
enum { XKEY_CHROMATIC, XKEY_LINEAR, XKEY_LOG, XKEY_RESPONSE_COUNT };
enum { XKEY_RELATIVE, XKEY_SPECTRAL, XKEY_LAYOUT_COUNT };

/* Fixed little-endian32-bit configuration; all fields are validated, including
 * currently unused cell fields. No embedded runtime or transport state. */
typedef struct {
    uint32_t destination, response, latch, gate;
    float low, high;
    uint32_t cell_width, cell_layout;
} XenolithKeyboardConfig;
typedef struct {
    uint32_t mask_lo, mask_hi;
    float normalized; /* last held physical key /26; zero for empty selection */
} XenolithKeyboardSelection;
_Static_assert(sizeof(float)==4, "Keyboard schema requires32-bit floats");
_Static_assert(sizeof(XenolithKeyboardConfig)==32, "Keyboard config ABI changed");
_Static_assert(sizeof(XenolithKeyboardSelection)==12, "Keyboard selection ABI changed");

/* Invalid arguments return0 without modifying any output. Defaults are TONIC,
 * CHROMATIC, latch0, gate1,20..800Hz, width1, RELATIVE. */
void xenolith_keyboard_config_defaults(XenolithKeyboardConfig *config);
int xenolith_keyboard_config_valid(const XenolithKeyboardConfig *config);
int xenolith_keyboard_destination_bounds(uint32_t destination,float *low,float *high);
int xenolith_keyboard_destination_defaults(uint32_t destination,float *low,float *high,
                                         uint32_t *response);
/* Foreground-only physical-value calculation. key0..26; endpoints exact.
 * CHROMATIC returns0: parent retains its normal MIDI/octave/last-note handling.
 * LINEAR permits zero bounds; LOG requires positive low and high. */
int xenolith_keyboard_map_value(const XenolithKeyboardConfig *config,uint32_t key,float *value);
/* Pure bounded union of held-key bands. last_key must belong to a nonempty
 * mask; ignored when keys==0. count1..64,width1..16. At count>=27 keyk owns
 * [floor(k*count/27),floor((k+1)*count/27)); all27keys therefore cover the
 * complete bank without gaps, even at64cells. At count<27 keys duplicate the
 * nearest cell, with exact first/last endpoints. Width is a MINIMUM width,
 * clamped to count; wider base partitions are never narrowed. Extra cells
 * expand the owned band symmetrically (odd extra goes above), with edge shift
 * to stay wholly in bank. No octave/pitch interpretation is performed here.
 * Parent supplies held/latch/order state and owns the shared master envelope. */
int xenolith_keyboard_select_cells(uint32_t keys,uint32_t last_key,uint32_t count,
                                  uint32_t width,XenolithKeyboardSelection *selection);
/* Foreground-only log-spaced absolute frequency table,20..8000Hz. count1..64;
 * low<=high. count1 yields low. Writes only count entries; no heap or I/O.
 * SPECTRAL uses this table; RELATIVE retains existing core cell tuning. */
int xenolith_keyboard_fill_frequencies(float *frequencies,uint32_t count,float low,float high);
#endif
