/* SPDX-License-Identifier: MPL-2.0
 * Three assignable control ADSRs beside the unchanged amplitude ADSR. */
#ifndef XX29_XENOLITH_ENVELOPE_BANK_H
#define XX29_XENOLITH_ENVELOPE_BANK_H
#include "xenolith_envelope.h"
#include "xenolith_modulation.h"
#define XENOLITH_ENVELOPE_BANK_COUNT 3u
enum { XENV_KEY=0, XENV_SEQUENCE=1, XENV_BOTH=2, XENV_TRIGGER_COUNT=3 };
#define XENV_KEY_BIT 1u
#define XENV_SEQUENCE_BIT 2u
typedef struct {
    float attack_ms,decay_ms,sustain,release_ms,depth;
    uint32_t destination,trigger,enabled;
} XenolithEnvelopeRoute;
typedef struct { XenolithEnvelopeRoute envelope[XENOLITH_ENVELOPE_BANK_COUNT]; } XenolithEnvelopeBankConfig;
typedef struct {
    XenolithEnvelopeBankConfig config;
    XenolithEnvelope envelope[XENOLITH_ENVELOPE_BANK_COUNT];
    float level[XENOLITH_ENVELOPE_BANK_COUNT],offset[XM_DESTINATION_COUNT];
    uint32_t enabled_mask,gate_mask,position;
} XenolithEnvelopeBank;
_Static_assert(sizeof(XenolithEnvelopeRoute)==32,"Envelope route ABI");
_Static_assert(sizeof(XenolithEnvelopeBankConfig)==96,"Envelope bank ABI");
void xenolith_envelope_bank_defaults(XenolithEnvelopeBankConfig *config);
int xenolith_envelope_bank_valid(const XenolithEnvelopeBankConfig *config);
int xenolith_envelope_route_destination(uint32_t destination);
void xenolith_envelope_bank_init(XenolithEnvelopeBank *state);
int xenolith_envelope_bank_apply(XenolithEnvelopeBank *state,const XenolithEnvelopeBankConfig *config);
void xenolith_envelope_bank_clear(XenolithEnvelopeBank *state);
/* Fixed44100Hz, audio owner. Edges retrigger from current level; matching gate
 * release starts release. BOTH holds while either gate remains. Off freezes no
 * hidden envelope and publishes exact zero. Route aggregation every32samples;
 * summed physical offsets bounded by the shared destination descriptor. */
void xenolith_envelope_bank_sample(XenolithEnvelopeBank *state,uint32_t edges,uint32_t gates);
#endif
