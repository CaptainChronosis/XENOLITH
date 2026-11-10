/* SPDX-License-Identifier: MPL-2.0 */
#include "xenolith_paraphony.h"

static int bounded(float value, float low, float high)
{
    return value >= low && value <= high;
}

void xenolith_paraphony_reset(XenolithParaphonyState *state)
{
    uint32_t i;
    if (!state) return;
    state->initialized = 1u;
    for (i = 0u; i < XENOLITH_PARAPHONY_VOICES; ++i)
        state->slot_key[i] = XENOLITH_PARAPHONY_EMPTY;
}

/* Bit-reversed positions fill the compact interval without retuning earlier
 * cells when their group grows. The first cell remains exactly on the note. */
static const int8_t cluster_offset[64] = {
     0,-16, 16,-24,  8, -8, 24,-28,  4,-12, 20,-20, 12, -4, 28,-30,
     2,-14, 18,-22, 10, -6, 26,-26,  6,-10, 22,-18, 14, -2, 30,-31,
     1,-15, 17,-23,  9, -7, 25,-27,  5,-11, 21,-19, 13, -3, 29,-29,
     3,-13, 19,-21, 11, -5, 27,-25,  7, -9, 23,-17, 15, -1, 31,-32
};

int xenolith_paraphony_allocate(XenolithParaphonyState *state,
    const uint8_t *held_order, const float *pitches, uint32_t held_count,
    uint32_t cells, uint32_t requested_voices, float reference_pitch,
    float effective_pitch, float spread, XenolithParaphonyOutput *out)
{
    XenolithParaphonyState next;
    float slot_pitch[8], ratio;
    uint32_t seen = 0u, selected = 0u, occupied = 0u;
    uint32_t capacity, active, first, i, slot;

    if (!state || !out || held_count > XENOLITH_PARAPHONY_KEYS ||
        cells < 1u || cells > XENOLITH_PARAPHONY_CELLS ||
        requested_voices < 1u || requested_voices > XENOLITH_PARAPHONY_VOICES ||
        !bounded(reference_pitch, 20.0f, 8000.0f) ||
        !bounded(effective_pitch, 20.0f, 8000.0f) || !bounded(spread, 0.0f, 2.0f) ||
        state->initialized > 1u || (held_count && (!held_order || !pitches))) return 0;

    for (i = 0u; i < held_count; ++i) {
        uint32_t key = held_order[i];
        if (key >= XENOLITH_PARAPHONY_KEYS || (seen & (1u << key)) ||
            !bounded(pitches[key], 20.0f, 8000.0f)) return 0;
        seen |= 1u << key;
    }
    if (state->initialized) {
        seen = 0u;
        for (i = 0u; i < XENOLITH_PARAPHONY_VOICES; ++i) {
            uint32_t key = state->slot_key[i];
            if (key == XENOLITH_PARAPHONY_EMPTY) continue;
            if (key >= XENOLITH_PARAPHONY_KEYS || (seen & (1u << key))) return 0;
            seen |= 1u << key;
        }
        next = *state;
    } else xenolith_paraphony_reset(&next);

    capacity = requested_voices < cells ? requested_voices : cells;
    active = held_count < capacity ? held_count : capacity;
    first = held_count - active;
    for (i = first; i < held_count; ++i) selected |= 1u << held_order[i];
    for (slot = 0u; slot < XENOLITH_PARAPHONY_VOICES; ++slot) {
        uint32_t key = next.slot_key[slot];
        if (slot >= capacity || key == XENOLITH_PARAPHONY_EMPTY ||
            !(selected & (1u << key))) next.slot_key[slot] = XENOLITH_PARAPHONY_EMPTY;
        else occupied |= 1u << key;
    }
    for (i = first; i < held_count; ++i) {
        uint32_t key = held_order[i];
        if (occupied & (1u << key)) continue;
        for (slot = 0u; slot < capacity; ++slot)
            if (next.slot_key[slot] == XENOLITH_PARAPHONY_EMPTY) break;
        next.slot_key[slot] = (uint8_t)key;
        occupied |= 1u << key;
    }

    ratio = effective_pitch / reference_pitch;
    for (slot = 0u; slot < capacity; ++slot) {
        uint32_t key = next.slot_key[slot];
        slot_pitch[slot] = key == XENOLITH_PARAPHONY_EMPTY ? effective_pitch : pitches[key] * ratio;
    }
    out->mask_lo = 0u;
    out->mask_hi = 0u;
    out->requested_voices = requested_voices;
    out->effective_voices = capacity;
    out->active_voices = active;
    for (i = 0u; i < XENOLITH_PARAPHONY_CELLS; ++i) {
        float frequency = effective_pitch;
        uint8_t key = XENOLITH_PARAPHONY_EMPTY;
        if (i < cells) {
            slot = i % requested_voices;
            key = next.slot_key[slot];
            if (key != XENOLITH_PARAPHONY_EMPTY) {
                frequency = slot_pitch[slot] *
                    (1.0f + (float)cluster_offset[i / requested_voices] * (0.002f / 32.0f) * spread);
                if (frequency < 20.0f) frequency = 20.0f;
                if (frequency > 8000.0f) frequency = 8000.0f;
                if (i < 32u) out->mask_lo |= 1u << i;
                else out->mask_hi |= 1u << (i - 32u);
            }
        }
        out->frequency[i] = frequency;
        out->owner[i] = key;
    }
    *state = next;
    return 1;
}
