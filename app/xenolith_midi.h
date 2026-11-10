/* SPDX-License-Identifier: MPL-2.0 */
#ifndef XENOLITH_MIDI_H
#define XENOLITH_MIDI_H
#include <stdint.h>

enum { XMIDI_USB, XMIDI_TRS, XMIDI_PORTS, XMIDI_KEYS=27, XMIDI_NOTES=128,
       XMIDI_IDENTITIES=155, XMIDI_EMPTY=255 };
enum { XMIDI_IGNORE, XMIDI_NOTE_ON, XMIDI_RELEASE, XMIDI_REALTIME };
typedef struct {
    uint16_t channels[XMIDI_PORTS][XMIDI_NOTES];
    uint8_t order[XMIDI_IDENTITIES],virtual_identity[8];
    uint32_t count,midi_count,physical,revision;
} XenolithMidiState;
typedef struct { uint32_t type,status,note; } XenolithMidiEvent;

void xenolith_midi_init(XenolithMidiState *state);
void xenolith_midi_physical(XenolithMidiState *state,uint32_t keys);
XenolithMidiEvent xenolith_midi_packet(XenolithMidiState *state,uint32_t packet,uint32_t port);
void xenolith_midi_clear_port(XenolithMidiState *state,uint32_t port);
/* Clears external ownership only; the physical key matrix remains independent. */
void xenolith_midi_clear(XenolithMidiState *state);
uint32_t xenolith_midi_held(const XenolithMidiState *state,uint32_t note);
uint32_t xenolith_midi_latest(const XenolithMidiState *state);
/* Stable eight-key tokens adapt full-range MIDI pitches to the existing cell
 * allocator. Newest identities win; surviving identities keep their tokens. */
uint32_t xenolith_midi_voicing(XenolithMidiState *state,uint8_t order[8],uint8_t identities[8]);
#endif
