/* SPDX-License-Identifier: MPL-2.0 */
#include "xenolith_patches.h"

static int within(float x, float lo, float hi) {
    /* Ordered comparisons also reject NaN and either infinity. */
    return x>=lo && x<=hi;
}
static void copy_bytes(void *out,const void *in,uint32_t count) {
    uint32_t i;uint8_t *dst=(uint8_t *)out;const uint8_t *src=(const uint8_t *)in;
    for(i=0;i<count;i++)dst[i]=src[i];
}
/* Pre-V23 records have no saved CURRENT recipe. Build its equivalent legacy
 * graph only after the validated topology has been copied/defaulted. */
static int legacy_graph_finish(XenolithPatch *out,int result) {
    if(result){
        xenolith_graph_from_legacy(&out->graph,out->topology.growth);
        out->graph.scope=XGRAPH_ALL;
        xenolith_flavour_defaults(&out->flavour);xenolith_coupling_defaults(&out->coupling);
    }
    return result;
}
static void legacy_patch_copy(XenolithPatch *out,const void *input,uint32_t bytes) {
    const uint8_t *in=(const uint8_t*)input;
    xenolith_governor_defaults(&out->governor);
    xenolith_performance_defaults(&out->performance);xenolith_graph_defaults(&out->graph);
    copy_bytes(out,in,offsetof(XenolithPatchV18,sequence));
    if(bytes>offsetof(XenolithPatchV18,keyboard))
        copy_bytes(&out->keyboard,in+offsetof(XenolithPatchV18,keyboard),bytes-(uint32_t)offsetof(XenolithPatchV18,keyboard));
}
static int legacy_sequence_valid(const XenolithSequenceImageV4 *s) {
    return xenolith_sequence_image_v4_valid(s,sizeof *s)||xenolith_sequence_image_v3_valid(s,sizeof *s);
}
static void circuit_extensions(XenolithPatch *p) {
    xenolith_flavour_defaults(&p->flavour);xenolith_coupling_defaults(&p->coupling);
    xenolith_governor_defaults(&p->governor);
    xenolith_performance_defaults(&p->performance);xenolith_graph_defaults(&p->graph);
    xenolith_folder_defaults(&p->folder);p->amplifier_feedback=0.0f;
    xenolith_filter_circuit_defaults(&p->circuit);xenolith_calculation_defaults(&p->calculation);
}
static void motion_extensions(XenolithPatch *p) {
    circuit_extensions(p);
    xenolith_random_memory_defaults(&p->random_memory);xenolith_dispersion_defaults(&p->dispersion);
}
static void new_extensions(XenolithPatch *p) {
    motion_extensions(p);
    xenolith_filter_defaults(&p->filter);xenolith_envelope_bank_defaults(&p->envelopes);
}
static void legacy_extensions(XenolithPatch *p) {
    new_extensions(p);
    p->damp_hz=800.0f;p->damp_model=0u;xenolith_amplifier_shape_defaults(&p->amplifier_shape);
}
void xenolith_patch_defaults(XenolithPatch *p) {
    uint32_t i;uint8_t *bytes=(uint8_t *)p;
    if(!p)return;
    for(i=0;i<sizeof *p;i++)bytes[i]=0;
    p->pitch_hz=84.0f;p->index=2.4f;p->ratio=1.2f;p->drift=0.2f;
    p->attack_ms=10.0f;p->decay_ms=250.0f;p->sustain=0.8f;p->release_ms=400.0f;
    p->echo_seconds=0.4f;p->absorb=0.6f;p->tail_seconds=5.0f;p->mix=0.25f;
    p->cells=8u;p->core_rate=44100.0f;p->wet_rate=44100.0f;
    xenolith_mod_defaults(&p->modulation);
    xenolith_sequence_image_defaults(&p->sequence);
    xenolith_keyboard_config_defaults(&p->keyboard);
    xenolith_amplifier_defaults(&p->amplifier);
    xenolith_topology_defaults(&p->topology);
    p->diffusion=0.65f;p->width=0.70f;
    p->damp_hz=800.0f;p->damp_model=1u;xenolith_amplifier_shape_defaults(&p->amplifier_shape);new_extensions(p);
    p->graph.scope=XGRAPH_ADD;
}
void xenolith_patches_init(XenolithPatchBank *bank) {
    uint32_t i;uint8_t *bytes=(uint8_t *)bank;
    for(i=0;i<sizeof *bank;i++)bytes[i]=0;
    bank->magic=XENOLITH_PATCH_MAGIC;
    bank->version=XENOLITH_PATCH_VERSION;
    bank->bytes=sizeof *bank;
    for(i=0;i<XENOLITH_PATCH_COUNT;i++)xenolith_patch_defaults(&bank->slots[i]);
}
static int common_valid(const XenolithPatchV4 *p) {
    if(!p)return 0;
    return within(p->pitch_hz,20.0f,800.0f) && within(p->index,0.0f,64.0f)
        && within(p->ratio,0.001f,32.0f) && within(p->drift,0.0f,1.0f)
        && within(p->attack_ms,0.0f,10000.0f) && within(p->decay_ms,0.0f,10000.0f)
        && within(p->sustain,0.0f,1.0f) && within(p->release_ms,0.0f,10000.0f)
        && within(p->echo_seconds,0.02f,1.0f) && within(p->absorb,0.0f,1.0f)
        && within(p->tail_seconds,0.3f,12.0f) && within(p->mix,0.0f,1.0f)
        && p->cells>=1u && p->cells<=64u
        && p->trim_db>=-24 && p->trim_db<=0;
}
static int sound_valid(const XenolithPatchV4 *p,uint32_t minimum_bits) {
    return common_valid(p) && p->reserved==0u && p->clock_divider==0u
        && (within(p->core_rate,1000.0f,44100.0f)
            || p->core_rate==88200.0f || p->core_rate==176400.0f)
        && (p->fractional_bits==0u || (p->fractional_bits>=minimum_bits && p->fractional_bits<=24u))
        && p->quantizer<=2u && p->reconstruction<=1u
        && within(p->wet_rate,1378.125f,44100.0f) && p->wet_reconstruction<=1u;
}
static int v4_valid(const XenolithPatchV4 *p) {return sound_valid(p,2u);}
static int prefix_valid(const XenolithPatchV12 *p,uint32_t max_mode) {
    XenolithPatchV4 sound;
    if(!p)return 0;
    copy_bytes(&sound,p,sizeof sound);
    if(!within(p->index,0.0f,xenolith_depth_limit(p->topology.mode)))return 0;
    if(!within(p->echo_seconds,0.002f,32.0f))return 0;
    if(p->wet_reconstruction>max_mode)return 0;
    sound.wet_reconstruction=0u;
    sound.index=0.0f; /* Current mode-aware depth checked above; legacy helper stays frozen. */
    sound.echo_seconds=0.4f; /* V11 request checked above; old formats retain20ms..1s. */
    return sound_valid(&sound,1u) && xenolith_mod_validate(&p->modulation)
        && legacy_sequence_valid(&p->sequence)
        && xenolith_keyboard_config_valid(&p->keyboard) && xenolith_amplifier_config_valid(&p->amplifier)
        && xenolith_topology_valid(&p->topology)
        && within(p->diffusion,0.0f,0.85f) && within(p->width,0.0f,1.0f);
}
static int prefix_ids_valid(const XenolithPatchV12 *p,uint32_t max_mode) {
    uint32_t i;
    if(!p || p->topology.growth>4u || p->topology.mode>4u
       || p->keyboard.destination>10u || p->sequence.version!=3u
       || !prefix_valid(p,max_mode))return 0;
    for(i=0;i<2u;i++)if(p->modulation.function[i].input>14u
        || p->modulation.mix[i].input_a>14u || p->modulation.mix[i].input_b>14u)return 0;
    for(i=0;i<XENOLITH_MOD_ROUTES;i++)if(p->modulation.route[i].source>14u
        || p->modulation.route[i].destination>26u)return 0;
    for(i=0;i<XENOLITH_SEQUENCE_LANES;i++)if(p->sequence.pattern.lane[i].destination>11u)return 0;
    return 1;
}
int xenolith_patch_v13_valid(const XenolithPatchV13 *p) {
    XenolithPatchV12 prefix;if(!p)return 0;copy_bytes(&prefix,p,sizeof prefix);
    return prefix_ids_valid(&prefix,9u) && within(p->damp_hz,20.0f,16000.0f)
        && p->damp_model<=1u && xenolith_amplifier_shape_valid(&p->amplifier_shape);
}
int xenolith_patch_v16_valid(const XenolithPatchV16 *p) {
    XenolithPatchV12 prefix;if(!p)return 0;copy_bytes(&prefix,p,sizeof prefix);
    return prefix_valid(&prefix,9u) && within(p->damp_hz,20.0f,16000.0f)
        && p->damp_model<=1u && xenolith_amplifier_shape_valid(&p->amplifier_shape)
        && xenolith_sequence_image_v4_valid(&p->sequence,sizeof p->sequence)
        && xenolith_filter_config_valid(&p->filter) && xenolith_envelope_bank_valid(&p->envelopes)
        && xenolith_random_memory_validate(&p->random_memory) && xenolith_dispersion_config_valid(&p->dispersion)
        && xenolith_filter_circuit_valid(&p->circuit) && xenolith_calculation_valid(&p->calculation);
}
int xenolith_patch_v17_valid(const XenolithPatchV17 *p) {
    XenolithPatchV16 prefix;if(!p)return 0;copy_bytes(&prefix,p,sizeof prefix);
    return xenolith_patch_v16_valid(&prefix) && xenolith_folder_config_valid(&p->folder);
}
int xenolith_patch_v18_valid(const XenolithPatchV18 *p) {
    XenolithPatchV17 prefix;if(!p)return 0;copy_bytes(&prefix,p,sizeof prefix);
    return xenolith_patch_v17_valid(&prefix) && within(p->amplifier_feedback,0.0f,1.0f);
}
int xenolith_patch_from_v18(XenolithPatch *out,const XenolithPatchV18 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v18_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);
    return legacy_graph_finish(out,xenolith_sequence_image_from_v4(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_v19_valid(const XenolithPatchV19 *p) {
    XenolithPatchV4 sound;
    if(!p || !within(p->index,0.0f,xenolith_depth_limit(p->topology.mode))
       || !within(p->echo_seconds,.002f,32.0f) || p->wet_reconstruction>9u)return 0;
    copy_bytes(&sound,p,sizeof sound);sound.index=0.0f;sound.echo_seconds=.4f;sound.wet_reconstruction=0u;
    return sound_valid(&sound,1u) && xenolith_mod_validate(&p->modulation)
        && xenolith_sequence_image_valid(&p->sequence,sizeof p->sequence)
        && xenolith_keyboard_config_valid(&p->keyboard) && xenolith_amplifier_config_valid(&p->amplifier)
        && xenolith_topology_valid(&p->topology) && within(p->diffusion,0,.85f) && within(p->width,0,1)
        && within(p->damp_hz,20,16000) && p->damp_model<=1u && xenolith_amplifier_shape_valid(&p->amplifier_shape)
        && xenolith_filter_config_valid(&p->filter) && xenolith_envelope_bank_valid(&p->envelopes)
        && xenolith_random_memory_validate(&p->random_memory) && xenolith_dispersion_config_valid(&p->dispersion)
        && xenolith_filter_circuit_valid(&p->circuit) && xenolith_calculation_valid(&p->calculation)
        && xenolith_folder_config_valid(&p->folder) && within(p->amplifier_feedback,0,1);
}
int xenolith_patch_from_v19(XenolithPatch *out,const XenolithPatchV19 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v19_valid(p))return 0;
    copy_bytes(out,p,sizeof *p);xenolith_governor_defaults(&out->governor);
    xenolith_performance_defaults(&out->performance);xenolith_graph_defaults(&out->graph);return legacy_graph_finish(out,1);
}
int xenolith_patch_v20_valid(const XenolithPatchV20 *p) {
    XenolithPatchV19 prefix;
    if(!p)return 0;
    copy_bytes(&prefix,p,sizeof prefix);
    return xenolith_patch_v19_valid(&prefix)
        && within(p->governor.onset,.10f,.90f) && within(p->governor.span,.05f,.50f)
        && p->governor.onset+p->governor.span<=.980001f
        && within(p->governor.recovery_ms,20.0f,2000.0f) && within(p->governor.depth,0.0f,1.0f);
}
int xenolith_patch_from_v20(XenolithPatch *out,const XenolithPatchV20 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v20_valid(p))return 0;
    copy_bytes(out,p,sizeof(XenolithPatchV19));
    xenolith_performance_defaults(&out->performance);xenolith_graph_defaults(&out->graph);
    xenolith_governor_defaults(&out->governor);
    out->governor.onset=p->governor.onset;out->governor.span=p->governor.span;
    out->governor.recovery_ms=p->governor.recovery_ms;
    out->governor.filter_shape=p->governor.depth;out->governor.reverb_shape=p->governor.depth;
    out->governor.table_depth=p->governor.depth;out->governor.clock_floor=1.0f-.5f*p->governor.depth;
    return legacy_graph_finish(out,1);
}
int xenolith_patch_v21_valid(const XenolithPatchV21 *p) {
    XenolithPatchV4 sound;
    if(!p || !within(p->index,0.0f,xenolith_depth_limit(p->topology.mode))
       || !within(p->echo_seconds,.002f,32.0f) || p->wet_reconstruction>9u)return 0;
    copy_bytes(&sound,p,sizeof sound);sound.index=0.0f;sound.echo_seconds=.4f;sound.wet_reconstruction=0u;
    return sound_valid(&sound,1u) && xenolith_mod_validate(&p->modulation)
        && xenolith_sequence_image_valid(&p->sequence,sizeof p->sequence)
        && xenolith_keyboard_config_valid(&p->keyboard) && xenolith_amplifier_config_valid(&p->amplifier)
        && xenolith_topology_valid(&p->topology) && within(p->diffusion,0,.85f) && within(p->width,0,1)
        && within(p->damp_hz,20,16000) && p->damp_model<=1u && xenolith_amplifier_shape_valid(&p->amplifier_shape)
        && xenolith_filter_config_valid(&p->filter) && xenolith_envelope_bank_valid(&p->envelopes)
        && xenolith_random_memory_validate(&p->random_memory) && xenolith_dispersion_config_valid(&p->dispersion)
        && xenolith_filter_circuit_valid(&p->circuit) && xenolith_calculation_valid(&p->calculation)
        && xenolith_folder_config_valid(&p->folder) && within(p->amplifier_feedback,0,1) && xenolith_governor_config_valid(&p->governor);
}
int xenolith_patch_from_v21(XenolithPatch *out,const XenolithPatchV21 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v21_valid(p))return 0;
    copy_bytes(out,p,sizeof *p);xenolith_performance_defaults(&out->performance);xenolith_graph_defaults(&out->graph);return legacy_graph_finish(out,1);
}
int xenolith_patch_v22_valid(const XenolithPatchV22 *p) {
    XenolithPatchV21 prefix;
    if(!p || !xenolith_performance_valid(&p->performance))return 0;
    copy_bytes(&prefix,p,sizeof prefix);
    return xenolith_patch_v21_valid(&prefix);
}
int xenolith_patch_from_v22(XenolithPatch *out,const XenolithPatchV22 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v22_valid(p))return 0;
    copy_bytes(out,p,sizeof *p);xenolith_graph_defaults(&out->graph);return legacy_graph_finish(out,1);
}
int xenolith_patch_v23_valid(const XenolithPatchV23 *p) {
    XenolithPatchV22 prefix;
    if(!p||!xenolith_graph_valid(&p->graph))return 0;
    copy_bytes(&prefix,p,sizeof prefix);return xenolith_patch_v22_valid(&prefix);
}
int xenolith_patch_from_v23(XenolithPatch *out,const XenolithPatchV23 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v23_valid(p))return 0;
    copy_bytes(out,p,sizeof *p);xenolith_flavour_defaults(&out->flavour);xenolith_coupling_defaults(&out->coupling);return 1;
}
int xenolith_patch_v24_valid(const XenolithPatchV24 *p) {
    XenolithPatchV23 prefix;
    if(!p||!xenolith_flavour_valid(&p->flavour))return 0;
    copy_bytes(&prefix,p,sizeof prefix);return xenolith_patch_v23_valid(&prefix);
}
int xenolith_patch_from_v24(XenolithPatch *out,const XenolithPatchV24 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v24_valid(p))return 0;
    copy_bytes(out,p,sizeof *p);xenolith_coupling_defaults(&out->coupling);return 1;
}
int xenolith_patch_valid(const XenolithPatch *p) {
    XenolithPatchV24 prefix;
    if(!p||!xenolith_coupling_valid(&p->coupling))return 0;
    copy_bytes(&prefix,p,sizeof prefix);return xenolith_patch_v24_valid(&prefix);
}
int xenolith_patch_from_v17(XenolithPatch *out,const XenolithPatchV17 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v17_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);out->amplifier_feedback=0.0f;return legacy_graph_finish(out,xenolith_sequence_image_from_v4(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_from_v16(XenolithPatch *out,const XenolithPatchV16 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v16_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);xenolith_folder_defaults(&out->folder);out->amplifier_feedback=0.0f;return legacy_graph_finish(out,xenolith_sequence_image_from_v4(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_v15_valid(const XenolithPatchV15 *p) {
    XenolithPatchV12 prefix;uint32_t i;if(!p)return 0;copy_bytes(&prefix,p,sizeof prefix);
    if(!prefix_valid(&prefix,9u) || p->topology.growth>4u || p->topology.mode>4u
       || p->keyboard.destination>12u || p->sequence.version!=4u
       || p->filter.placement>3u || p->filter.mode>5u || p->filter.slope>2u || p->amplifier_shape.curve>3u
       || !within(p->damp_hz,20.0f,16000.0f) || p->damp_model>1u
       || !xenolith_amplifier_shape_valid(&p->amplifier_shape)
       || !xenolith_sequence_image_v4_valid(&p->sequence,sizeof p->sequence)
       || !xenolith_filter_config_valid(&p->filter) || !xenolith_envelope_bank_valid(&p->envelopes)
       || !xenolith_random_memory_validate(&p->random_memory) || !xenolith_dispersion_config_valid(&p->dispersion))return 0;
    for(i=0;i<2u;i++)if(p->modulation.function[i].input>18u
        || p->modulation.mix[i].input_a>18u || p->modulation.mix[i].input_b>18u)return 0;
    for(i=0;i<XENOLITH_MOD_ROUTES;i++)if(p->modulation.route[i].source>18u
        || p->modulation.route[i].destination>32u)return 0;
    for(i=0;i<XENOLITH_ENVELOPE_BANK_COUNT;i++)if(p->envelopes.envelope[i].destination>28u)return 0;
    for(i=0;i<XENOLITH_SEQUENCE_LANES;i++)if(p->sequence.pattern.lane[i].destination>13u)return 0;
    return 1;
}
int xenolith_patch_from_v15(XenolithPatch *out,const XenolithPatchV15 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v15_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);circuit_extensions(out);return legacy_graph_finish(out,xenolith_sequence_image_from_v4(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_v14_valid(const XenolithPatchV14 *p) {
    XenolithPatchV12 prefix;uint32_t i;if(!p)return 0;copy_bytes(&prefix,p,sizeof prefix);
    if(!prefix_valid(&prefix,9u) || p->topology.growth>4u || p->topology.mode>4u
       || p->keyboard.destination>12u || p->sequence.version!=4u
       || p->filter.placement>3u || p->filter.mode>5u || p->filter.slope>2u || p->amplifier_shape.curve>3u
       || !within(p->damp_hz,20.0f,16000.0f) || p->damp_model>1u
       || !xenolith_amplifier_shape_valid(&p->amplifier_shape)
       || !xenolith_sequence_image_v4_valid(&p->sequence,sizeof p->sequence)
       || !xenolith_filter_config_valid(&p->filter) || !xenolith_envelope_bank_valid(&p->envelopes))return 0;
    for(i=0;i<2u;i++)if(p->modulation.function[i].input>18u
        || p->modulation.mix[i].input_a>18u || p->modulation.mix[i].input_b>18u)return 0;
    for(i=0;i<XENOLITH_MOD_ROUTES;i++)if(p->modulation.route[i].source>18u
        || p->modulation.route[i].destination>28u)return 0;
    for(i=0;i<XENOLITH_ENVELOPE_BANK_COUNT;i++)if(p->envelopes.envelope[i].destination>28u)return 0;
    for(i=0;i<XENOLITH_SEQUENCE_LANES;i++)if(p->sequence.pattern.lane[i].destination>13u)return 0;
    return 1;
}
int xenolith_patch_from_v14(XenolithPatch *out,const XenolithPatchV14 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v14_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);motion_extensions(out);return legacy_graph_finish(out,xenolith_sequence_image_from_v4(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_from_v13(XenolithPatch *out,const XenolithPatchV13 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out||!p||(a&3u)||(b&3u))return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<sizeof *p))return 0;
    if(!xenolith_patch_v13_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);new_extensions(out);return legacy_graph_finish(out,xenolith_sequence_image_from_v3(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_v12_valid(const XenolithPatchV12 *p) {return prefix_ids_valid(p,3u);}
int xenolith_patch_v11_valid(const XenolithPatchV11 *p) {return prefix_ids_valid(p,1u);}
int xenolith_patch_from_v12(XenolithPatch *out,const XenolithPatchV12 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v12_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);return legacy_graph_finish(out,xenolith_sequence_image_from_v3(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_from_v11(XenolithPatch *out,const XenolithPatchV11 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v11_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);return legacy_graph_finish(out,xenolith_sequence_image_from_v3(&out->sequence,&p->sequence,sizeof p->sequence));
}
/* Keep historical persisted semantics fixed when independent modules append IDs. */
static int legacy_performance_valid(const XenolithModConfig *m,const XenolithSequenceImageV4 *s) {
    uint32_t i;
    if(!xenolith_mod_validate(m) || !xenolith_sequence_image_v1_valid(s,sizeof *s))return 0;
    for(i=0;i<2u;i++)if(m->function[i].input>10u || m->mix[i].input_a>10u || m->mix[i].input_b>10u)return 0;
    for(i=0;i<XENOLITH_MOD_ROUTES;i++)if(m->route[i].source>10u || m->route[i].destination>24u)return 0;
    for(i=0;i<XENOLITH_SEQUENCE_LANES;i++)if(s->pattern.lane[i].destination>7u)return 0;
    return 1;
}
int xenolith_patch_v5_valid(const XenolithPatchV5 *p) {
    XenolithPatchV4 sound;
    if(!p)return 0;
    copy_bytes(&sound,p,sizeof sound);
    return v4_valid(&sound) && legacy_performance_valid(&p->modulation,&p->sequence);
}
int xenolith_patch_from_v5(XenolithPatch *out,const XenolithPatchV5 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v5_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);xenolith_keyboard_config_defaults(&out->keyboard);
    xenolith_amplifier_defaults(&out->amplifier);xenolith_topology_defaults(&out->topology);out->diffusion=0.65f;out->width=1.0f;
    return legacy_graph_finish(out,xenolith_sequence_image_from_v1(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_v6_valid(const XenolithPatchV6 *p) {
    XenolithPatchV4 sound;
    if(!p)return 0;
    copy_bytes(&sound,p,sizeof sound);
    return v4_valid(&sound) && legacy_performance_valid(&p->modulation,&p->sequence)
        && p->keyboard.destination<=7u && xenolith_keyboard_config_valid(&p->keyboard);
}
int xenolith_patch_from_v6(XenolithPatch *out,const XenolithPatchV6 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v6_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);xenolith_amplifier_defaults(&out->amplifier);
    xenolith_topology_defaults(&out->topology);out->diffusion=0.65f;out->width=1.0f;
    return legacy_graph_finish(out,xenolith_sequence_image_from_v1(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_v7_valid(const XenolithPatchV7 *p) {
    XenolithPatchV4 sound;uint32_t i;
    if(!p)return 0;
    copy_bytes(&sound,p,sizeof sound);
    if(!v4_valid(&sound) || !xenolith_mod_validate(&p->modulation)
       || !xenolith_sequence_image_v1_valid(&p->sequence,sizeof p->sequence)
       || !xenolith_keyboard_config_valid(&p->keyboard) || !xenolith_amplifier_config_valid(&p->amplifier)
       || p->keyboard.destination>8u)return 0;
    for(i=0;i<2u;i++)if(p->modulation.function[i].input>14u
        || p->modulation.mix[i].input_a>14u || p->modulation.mix[i].input_b>14u)return 0;
    for(i=0;i<XENOLITH_MOD_ROUTES;i++)if(p->modulation.route[i].source>14u
        || p->modulation.route[i].destination>25u)return 0;
    for(i=0;i<XENOLITH_SEQUENCE_LANES;i++)if(p->sequence.pattern.lane[i].destination>8u)return 0;
    return 1;
}
int xenolith_patch_from_v7(XenolithPatch *out,const XenolithPatchV7 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v7_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);xenolith_topology_defaults(&out->topology);out->diffusion=0.65f;out->width=1.0f;
    return legacy_graph_finish(out,xenolith_sequence_image_from_v1(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_v8_valid(const XenolithPatchV8 *p) {
    XenolithPatchV7 performance;
    if(!p)return 0;
    copy_bytes(&performance,p,sizeof performance);
    return xenolith_patch_v7_valid(&performance)
        && p->topology.growth<=3u && p->topology.mode<=1u
        && within(p->topology.link,0.0f,4.0f) && within(p->topology.feedback,0.0f,0.95f);
}
int xenolith_patch_from_v8(XenolithPatch *out,const XenolithPatchV8 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v8_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);out->diffusion=0.65f;out->width=1.0f;
    return legacy_graph_finish(out,xenolith_sequence_image_from_v1(&out->sequence,&p->sequence,sizeof p->sequence));
}
static int recent_legacy_valid(const XenolithPatchV9 *p,uint32_t image_version) {
    XenolithPatchV4 sound;uint32_t i;
    if(!p)return 0;
    copy_bytes(&sound,p,sizeof sound);
    if(!sound_valid(&sound,1u) || !xenolith_mod_validate(&p->modulation)
       || !(image_version==1u?xenolith_sequence_image_v1_valid(&p->sequence,sizeof p->sequence)
            :xenolith_sequence_image_v2_valid(&p->sequence,sizeof p->sequence))
       || !xenolith_keyboard_config_valid(&p->keyboard) || p->keyboard.destination>9u
       || !xenolith_amplifier_config_valid(&p->amplifier)
       || !xenolith_topology_valid(&p->topology) || p->topology.growth>4u || p->topology.mode>4u
       || !within(p->diffusion,0.0f,0.85f) || !within(p->width,0.0f,1.0f))return 0;
    for(i=0;i<2u;i++)if(p->modulation.function[i].input>14u
        || p->modulation.mix[i].input_a>14u || p->modulation.mix[i].input_b>14u)return 0;
    for(i=0;i<XENOLITH_MOD_ROUTES;i++)if(p->modulation.route[i].source>14u
        || p->modulation.route[i].destination>26u)return 0;
    for(i=0;i<XENOLITH_SEQUENCE_LANES;i++)if(p->sequence.pattern.lane[i].destination>10u)return 0;
    return 1;
}
int xenolith_patch_v9_valid(const XenolithPatchV9 *p) {return recent_legacy_valid(p,1u);}
int xenolith_patch_v10_valid(const XenolithPatchV10 *p) {return recent_legacy_valid(p,2u);}
int xenolith_patch_from_v9(XenolithPatch *out,const XenolithPatchV9 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v9_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);
    return legacy_graph_finish(out,xenolith_sequence_image_from_v1(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patch_from_v10(XenolithPatch *out,const XenolithPatchV10 *p) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)p;
    if(!out || !p || (a&3u) || (b&3u))return 0;
    if((a<=b && b-a<sizeof *out) || (b<a && a-b<sizeof *p))return 0;
    if(!xenolith_patch_v10_valid(p))return 0;
    legacy_patch_copy(out,p,sizeof *p);legacy_extensions(out);
    return legacy_graph_finish(out,xenolith_sequence_image_from_v2(&out->sequence,&p->sequence,sizeof p->sequence));
}
int xenolith_patches_valid(const XenolithPatchBank *b,uint32_t bytes) {
    uint32_t i;
    if(!b || bytes!=sizeof *b || b->magic!=XENOLITH_PATCH_MAGIC
        || b->version!=XENOLITH_PATCH_VERSION || b->bytes!=sizeof *b
        || (b->occupied&0xFFFF0000u))return 0;
    for(i=0;i<XENOLITH_PATCH_COUNT;i++)
        if((b->occupied&(1u<<i)) && !xenolith_patch_valid(&b->slots[i]))return 0;
    return 1;
}
int xenolith_patches_get(const XenolithPatchBank *b,uint32_t slot,XenolithPatch *out) {
    if(!out || slot>=XENOLITH_PATCH_COUNT || !xenolith_patches_valid(b,sizeof *b)
        || !(b->occupied&(1u<<slot)))return 0;
    *out=b->slots[slot];return 1;
}
int xenolith_patches_put(XenolithPatchBank *b,uint32_t slot,const XenolithPatch *p) {
    if(slot>=XENOLITH_PATCH_COUNT || !xenolith_patch_valid(p)
        || !xenolith_patches_valid(b,sizeof *b))return 0;
    b->slots[slot]=*p;b->occupied|=1u<<slot;return 1;
}

static void from_v1(XenolithPatchV2 *out,const XenolithPatchV1 *in) {
    uint32_t i;uint8_t *dst=(uint8_t *)out;const uint8_t *src=(const uint8_t *)in;
    for(i=0;i<sizeof *in;i++)dst[i]=src[i];
    out->crush_rate=11025.0f;out->crush_bits=7u;
    out->crush_quantizer=0u;out->crush_reconstruction=0u;
}
static void from_v2(XenolithPatch *out,const XenolithPatchV2 *in) {
    uint32_t i;uint8_t *dst=(uint8_t *)out;const uint8_t *src=(const uint8_t *)in;
    xenolith_patch_defaults(out);legacy_extensions(out);
    out->width=1.0f; /* Historical default, independent of new-patch defaults. */
    for(i=0;i<sizeof *in;i++)dst[i]=src[i];
    out->reserved=0u;
    if(in->quality!=6u){
        out->core_rate=in->quality==1u||in->quality==3u?176400.0f
            :in->quality==2u?88200.0f:in->quality==4u?22050.0f
            :in->quality==5u?11025.0f:44100.0f;
        out->fractional_bits=0u;out->quantizer=0u;out->reconstruction=0u;
    }
    out->wet_rate=44100.0f/(float)in->clock_divider;
    out->wet_reconstruction=0u;out->clock_divider=0u;
}
static int v2_valid(const XenolithPatchV2 *p) {
    XenolithPatchV4 common={0};uint32_t i;uint8_t *dst=(uint8_t *)&common;
    const uint8_t *src=(const uint8_t *)p;
    for(i=0;i<sizeof *p;i++)dst[i]=src[i];
    return common_valid(&common) && p->quality<=6u
        && p->clock_divider>=1u && p->clock_divider<=32u
        && within(p->crush_rate,1000.0f,44100.0f)
        && p->crush_bits>=2u && p->crush_bits<=15u
        && p->crush_quantizer<=2u && p->crush_reconstruction<=1u;
}
static void from_v3(XenolithPatch *out,const XenolithPatchV3 *in) {
    uint32_t i;uint8_t *dst=(uint8_t *)out;const uint8_t *src=(const uint8_t *)in;
    xenolith_patch_defaults(out);legacy_extensions(out);
    out->width=1.0f;
    for(i=0;i<sizeof *in;i++)dst[i]=src[i];
    out->wet_rate=44100.0f/(float)in->clock_divider;
    out->wet_reconstruction=0u;out->clock_divider=0u;
}
static int v3_valid(const XenolithPatchV3 *p) {
    XenolithPatchV4 converted={0};
    if(p->clock_divider<1u || p->clock_divider>32u || p->fractional_bits>15u)return 0;
    copy_bytes(&converted,p,sizeof *p);
    converted.wet_rate=44100.0f/(float)p->clock_divider;
    converted.wet_reconstruction=0u;converted.clock_divider=0u;
    return v4_valid(&converted);
}
static void from_v4(XenolithPatch *out,const XenolithPatchV4 *in) {
    xenolith_patch_defaults(out);legacy_extensions(out);out->width=1.0f;copy_bytes(out,in,sizeof *in);
}
static uint32_t record_bytes(uint32_t version) {
    switch(version){
    case 1u:return sizeof(XenolithPatchV1);
    case 2u:return sizeof(XenolithPatchV2);
    case 3u:return sizeof(XenolithPatchV3);
    case 4u:return sizeof(XenolithPatchV4);
    case 5u:return sizeof(XenolithPatchV5);
    case 6u:return sizeof(XenolithPatchV6);
    case 7u:return sizeof(XenolithPatchV7);
    case 8u:return sizeof(XenolithPatchV8);
    case 9u:return sizeof(XenolithPatchV9);
    case 10u:return sizeof(XenolithPatchV10);
    case 11u:return sizeof(XenolithPatchV11);
    case 12u:return sizeof(XenolithPatchV12);
    case 13u:return sizeof(XenolithPatchV13);
    case 14u:return sizeof(XenolithPatchV14);
    case 15u:return sizeof(XenolithPatchV15);
    case 16u:return sizeof(XenolithPatchV16);
    case 17u:return sizeof(XenolithPatchV17);
    case 18u:return sizeof(XenolithPatchV18);
    case 19u:return sizeof(XenolithPatchV19);
    case 20u:return sizeof(XenolithPatchV20);
    case 21u:return sizeof(XenolithPatchV21);
    case 22u:return sizeof(XenolithPatchV22);
    case 23u:return sizeof(XenolithPatchV23);
    case 24u:return sizeof(XenolithPatchV24);
    case XENOLITH_PATCH_VERSION:return sizeof(XenolithPatch);
    default:return 0u;
    }
}
static int record_valid(uint32_t version,const void *record) {
    XenolithPatchV2 converted;
    switch(version){
    case 1u:from_v1(&converted,(const XenolithPatchV1*)record);return v2_valid(&converted);
    case 2u:return v2_valid((const XenolithPatchV2*)record);
    case 3u:return v3_valid((const XenolithPatchV3*)record);
    case 4u:return v4_valid((const XenolithPatchV4*)record);
    case 5u:return xenolith_patch_v5_valid((const XenolithPatchV5*)record);
    case 6u:return xenolith_patch_v6_valid((const XenolithPatchV6*)record);
    case 7u:return xenolith_patch_v7_valid((const XenolithPatchV7*)record);
    case 8u:return xenolith_patch_v8_valid((const XenolithPatchV8*)record);
    case 9u:return xenolith_patch_v9_valid((const XenolithPatchV9*)record);
    case 10u:return xenolith_patch_v10_valid((const XenolithPatchV10*)record);
    case 11u:return xenolith_patch_v11_valid((const XenolithPatchV11*)record);
    case 12u:return xenolith_patch_v12_valid((const XenolithPatchV12*)record);
    case 13u:return xenolith_patch_v13_valid((const XenolithPatchV13*)record);
    case 14u:return xenolith_patch_v14_valid((const XenolithPatchV14*)record);
    case 15u:return xenolith_patch_v15_valid((const XenolithPatchV15*)record);
    case 16u:return xenolith_patch_v16_valid((const XenolithPatchV16*)record);
    case 17u:return xenolith_patch_v17_valid((const XenolithPatchV17*)record);
    case 18u:return xenolith_patch_v18_valid((const XenolithPatchV18*)record);
    case 19u:return xenolith_patch_v19_valid((const XenolithPatchV19*)record);
    case 20u:return xenolith_patch_v20_valid((const XenolithPatchV20*)record);
    case 21u:return xenolith_patch_v21_valid((const XenolithPatchV21*)record);
    case 22u:return xenolith_patch_v22_valid((const XenolithPatchV22*)record);
    case 23u:return xenolith_patch_v23_valid((const XenolithPatchV23*)record);
    case 24u:return xenolith_patch_v24_valid((const XenolithPatchV24*)record);
    case XENOLITH_PATCH_VERSION:return xenolith_patch_valid((const XenolithPatch*)record);
    default:return 0;
    }
}
static void record_convert(XenolithPatch *out,uint32_t version,const void *record) {
    XenolithPatchV2 converted;
    switch(version){
    case 1u:from_v1(&converted,(const XenolithPatchV1*)record);from_v2(out,&converted);break;
    case 2u:from_v2(out,(const XenolithPatchV2*)record);break;
    case 3u:from_v3(out,(const XenolithPatchV3*)record);break;
    case 4u:from_v4(out,(const XenolithPatchV4*)record);break;
    case 5u:xenolith_patch_from_v5(out,(const XenolithPatchV5*)record);break;
    case 6u:xenolith_patch_from_v6(out,(const XenolithPatchV6*)record);break;
    case 7u:xenolith_patch_from_v7(out,(const XenolithPatchV7*)record);break;
    case 8u:xenolith_patch_from_v8(out,(const XenolithPatchV8*)record);break;
    case 9u:xenolith_patch_from_v9(out,(const XenolithPatchV9*)record);break;
    case 10u:xenolith_patch_from_v10(out,(const XenolithPatchV10*)record);break;
    case 11u:xenolith_patch_from_v11(out,(const XenolithPatchV11*)record);break;
    case 12u:xenolith_patch_from_v12(out,(const XenolithPatchV12*)record);break;
    case 13u:xenolith_patch_from_v13(out,(const XenolithPatchV13*)record);break;
    case 14u:xenolith_patch_from_v14(out,(const XenolithPatchV14*)record);break;
    case 15u:xenolith_patch_from_v15(out,(const XenolithPatchV15*)record);break;
    case 16u:xenolith_patch_from_v16(out,(const XenolithPatchV16*)record);break;
    case 17u:xenolith_patch_from_v17(out,(const XenolithPatchV17*)record);break;
    case 18u:xenolith_patch_from_v18(out,(const XenolithPatchV18*)record);break;
    case 19u:xenolith_patch_from_v19(out,(const XenolithPatchV19*)record);break;
    case 20u:xenolith_patch_from_v20(out,(const XenolithPatchV20*)record);break;
    case 21u:xenolith_patch_from_v21(out,(const XenolithPatchV21*)record);break;
    case 22u:xenolith_patch_from_v22(out,(const XenolithPatchV22*)record);break;
    case 23u:xenolith_patch_from_v23(out,(const XenolithPatchV23*)record);break;
    case 24u:xenolith_patch_from_v24(out,(const XenolithPatchV24*)record);break;
    default:*out=*(const XenolithPatch*)record;break;
    }
    if(version<=4u)legacy_graph_finish(out,1);
}
static uint32_t validated_bank(const void *data,uint32_t bytes) {
    const uint32_t *header=(const uint32_t*)data;const uint8_t *records;
    uint32_t stride,i;
    if(!data||((uintptr_t)data&3u)||bytes<16u)return 0u;
    stride=record_bytes(header[1]);
    if(header[0]!=XENOLITH_PATCH_MAGIC||!stride||bytes!=16u+16u*stride
       ||header[2]!=bytes||(header[3]&0xffff0000u))return 0u;
    records=(const uint8_t*)data+16u;
    for(i=0;i<XENOLITH_PATCH_COUNT;i++)
        if((header[3]&(1u<<i))&&!record_valid(header[1],records+i*stride))return 0u;
    return stride;
}
int xenolith_patches_bank_info(const void *data,uint32_t bytes,uint32_t *occupied) {
    if(!occupied||!validated_bank(data,bytes))return 0;
    *occupied=((const uint32_t*)data)[3];return 1;
}
int xenolith_patches_decode_slot(XenolithPatch *out,const void *data,uint32_t bytes,uint32_t slot) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)data;uint32_t stride;
    const uint32_t *header=(const uint32_t*)data;
    if(!out||!data||(a&3u)||slot>=XENOLITH_PATCH_COUNT)return 0;
    if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<bytes))return 0;
    stride=validated_bank(data,bytes);
    if(!stride||!(header[3]&(1u<<slot)))return 0;
    record_convert(out,header[1],(const uint8_t*)data+16u+slot*stride);return 1;
}
static int decode_bank(XenolithPatchBank *out,const void *data,uint32_t bytes,XenolithPatch *scratch) {
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)data,c=(uintptr_t)scratch;
    const uint32_t *header=(const uint32_t*)data;const uint8_t *records;
    uint32_t version,occupied,stride,i;
    if(!out||!data||(a&3u)||(b&3u)||bytes<16u)return 0;
    if(a==b){
        if(!scratch||(c&3u)||(c<=a&&a-c<sizeof *scratch)||(a<c&&c-a<sizeof *out))return 0;
    }else if((a<=b&&b-a<sizeof *out)||(b<a&&a-b<bytes))return 0;
    records=(const uint8_t*)data+16u;
    stride=validated_bank(data,bytes);if(!stride)return 0;
    version=header[1];occupied=header[3];
    if(version==XENOLITH_PATCH_VERSION){if(a!=b)*out=*(const XenolithPatchBank*)data;return 1;}
    /* New records never shrink. Descending writes cannot destroy an unread
     * old slot. The complete validation above precedes the first mutation. */
    for(i=XENOLITH_PATCH_COUNT;i-->0u;){
        if(occupied&(1u<<i)){
            if(a==b){record_convert(scratch,version,records+i*stride);out->slots[i]=*scratch;}
            else record_convert(&out->slots[i],version,records+i*stride);
        }else xenolith_patch_defaults(&out->slots[i]);
    }
    out->magic=XENOLITH_PATCH_MAGIC;out->version=XENOLITH_PATCH_VERSION;
    out->bytes=sizeof *out;out->occupied=occupied;return 1;
}
int xenolith_patches_decode(XenolithPatchBank *out,const void *data,uint32_t bytes) {
    return decode_bank(out,data,bytes,0);
}
int xenolith_patches_decode_inplace(XenolithPatchBank *bank,uint32_t bytes,XenolithPatch *scratch) {
    return decode_bank(bank,bank,bytes,scratch);
}
