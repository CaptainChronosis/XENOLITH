/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef XENOLITH_RANDOM_MEMORY_H
#define XENOLITH_RANDOM_MEMORY_H
#include <stdint.h>

enum { XRM_CURRENT, XRM_BELL, XRM_EDGE, XRM_BINARY, XRM_LAW_COUNT };
typedef struct {
    float renew;
    uint8_t law, length, reserved[2];
} XenolithRandomMemoryConfig;
typedef struct {
    XenolithRandomMemoryConfig config;
    float value[32];
    uint32_t rng;
    uint8_t position, filled, reserved[2];
} XenolithRandomMemory;

void xenolith_random_memory_defaults(XenolithRandomMemoryConfig *config);
int xenolith_random_memory_validate(const XenolithRandomMemoryConfig *config);
void xenolith_random_memory_init(XenolithRandomMemory *state, uint32_t seed);
/* A length change starts a fresh capture. Other edits retain remembered values;
 * their distribution changes as slots renew. Invalid edits change nothing. */
int xenolith_random_memory_apply(XenolithRandomMemory *state,
                                const XenolithRandomMemoryConfig *config);
/* Once per RANDOM event, before bias/centre/spread/correlation/quantization.
 * Input/output are normalized0..1. CURRENT + length0 returns the input exactly
 * and leaves this helper's RNG untouched. No allocation or variable iteration. */
/* bias changes BINARY's upper-state probability; other laws use the caller's
 * existing continuous bias mapping after this helper. */
float xenolith_random_memory_next(XenolithRandomMemory *state, float current, float bias);
#endif
