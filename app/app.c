/* SPDX-License-Identifier: MPL-2.0
 * XX29 Xenolith I dry-core probe, 2026.
 * Platform/UI conventions adapted from Charles Vestal's MPL-2.0 template.
 * Original eight-cell core by default, variable count and compact exhaust. */
#include "fastmath.h"
#include "xenolith_core.h"
#include "xenolith_graph.h"
#include "xenolith_admission.h"
#include "xenolith_analysis.h"
#include "xenolith_exhaust.h"
#include "xenolith_envelope.h"
#include "xenolith_lights.h"
#include "xenolith_load_lamp.h"
#include "xenolith_quality.h"
#include "xenolith_ratio.h"
#include "xenolith_controls.h"
#include "xenolith_control_response.h"
#include "xenolith_rate_response.h"
#include "xenolith_performance.h"
#include "xenolith_paraphony.h"
#include "xenolith_midi.h"
#include "xenolith_midi.c" /* Bounded foreground parser, compiled with the app at -Os. */
#include "xenolith_patches.h"
#include "xenolith_modulation.h"
#include "xenolith_sequence.h"
#include "xenolith_keyboard.h"
#include "xenolith_amplifier.h"
#include "xenolith_filter.h"
#include "xenolith_envelope_bank.h"
#include "xenolith_governor.h"

/* Same single-core ISR publication barrier used by the framework's rings.
 * This is compiler ordering, not a multicore synchronization primitive. */
#define X_PUBLISH_BARRIER() __asm__ volatile("" ::: "memory")

static XenolithCore core;
static XenolithPerformanceConfig performance_edit;
static uint32_t performance_serial,performance_applied,home_detail_page;
static XenolithParaphonyState paraphony_allocator;
static volatile uint32_t paraphony_active_voices,paraphony_effective_voices=1u;
static XenolithGovernor governor;
static XenolithGovernorConfig governor_edit,governor_config[2];
static volatile uint32_t governor_serial;
static uint32_t governor_applied,governor_page;
static uint32_t governor_rec_started,governor_rec_done;
static volatile float governor_load_snapshot,governor_stress_snapshot;
static volatile uint32_t governor_core_holds,governor_output_fallbacks,governor_max_ticks;
static uint32_t governor_post_cost=200u,governor_core_cost=350u;
static XenolithGovernorCost governor_cost={1600u,6400u,0u,0u};
static int32_t governor_last_left,governor_last_right;
static void governor_publish(void)
{
    uint32_t next=governor_serial+1u;
    governor_config[next&1u]=governor_edit;
    X_PUBLISH_BARRIER();governor_serial=next;
}
static XenolithQuality quality PLAT_POOL;
static volatile uint32_t active_quality=1u;
static float core_max_rate=44100.0f;
static uint32_t core_bits,core_quantizer,core_reconstruction;
static uint32_t core_rate_response=XRATE_SWEEP; /* Panel preference, not patch data. */
/* Count and its prepared resource cap are published together. An interrupt
 * between a knob write and publication must never pair64 cells with4x. */
typedef struct {
    XenolithDigitalConfig dsp;
    uint32_t cells,profile,topology_serial;
    uint32_t mod_serial,amplifier_serial,filter_serial,envbank_serial,extended,composite,motion_generation,motion_wet_serial;
    uint32_t exhaust_serial,dispersion,pre_filter,calculation_serial,calculation_workload,coupling_workload,transfer_workload;
    XenolithTopologyConfig topology;
    XenolithCalculationConfig calculation;
    XenolithFlavourConfig flavour;
    XenolithCouplingConfig coupling;
    XenolithPerformanceConfig performance;
    uint32_t performance_serial;
    XenolithGraphConfig graph;
    XenolithGraph prepared_graph;
} AppDigitalConfig;
static AppDigitalConfig digital_config[2];
static volatile uint32_t digital_config_serial;
static uint32_t digital_config_applied;
static volatile float active_core_rate=44100.0f;
/* Bench probes: actual accepted rate gestures and source-gate continuity.
 * The minimum is armed only after a gesture begins on an established note. */
static volatile uint32_t digital_retime_count;
static volatile float digital_rate_gate_min=1.0f;
static uint32_t digital_rate_probe;
static volatile uint32_t count_continuous_changes,count_structural_changes;
static volatile float count_continuous_gate_min=1.0f;
static uint32_t count_continuous_probe;
static volatile uint32_t count_clock_waiting,count_clock_reservations,count_clock_accel_with_suffix;
static volatile float count_clock_entry_excess;
static XenolithEnvelope envelope;
static float envelope_attack=10.0f,envelope_decay=250.0f,envelope_sustain=0.8f,envelope_release=400.0f;
static float envelope_config[2][4];
static volatile uint32_t envelope_config_serial,envelope_event_word;
static uint32_t envelope_config_applied,envelope_event_applied;
static volatile float envelope_level;
static XenolithFilter filter;
static XenolithFilterConfig filter_edit,filter_config[2];
static XenolithFilterCircuitConfig circuit_edit,circuit_config[2];
static XenolithCalculationConfig calculation_edit;
static XenolithFlavourConfig flavour_edit;
static XenolithCouplingConfig coupling_edit;
static uint32_t calculation_serial,calculation_applied;
static volatile uint32_t calculation_active_snapshot;
static volatile uint32_t coupling_active_snapshot;
static volatile uint32_t transfer_active_snapshot;
static uint32_t edit_hold_started,edit_hold_done,glo_hold_started,glo_hold_done;
static volatile uint32_t filter_serial,filter_active_snapshot,filter_pre_snapshot;
static uint32_t filter_applied;
static float filter_route_level=1.0f;
static volatile float filter_cutoff_snapshot=8000.0f,filter_resonance_snapshot;
static volatile float filter_rate_snapshot=44100.0f;
static XenolithEnvelopeBank envbank;
static XenolithEnvelopeBankConfig envbank_edit,envbank_config[2];
static volatile uint32_t envbank_serial,envbank_active_snapshot;
static uint32_t envbank_applied,envbank_edges,envbank_sequence_gate;
static volatile float envbank_level_snapshot[3];
static uint32_t envelope_selection,env_hold_started,env_hold_done;

static XenolithAmplifier amplifier;
static XenolithAmplifierConfig amplifier_edit;
static XenolithAmplifierShapeConfig amplifier_shape_edit;
static XenolithAmplifierPrepared amplifier_config[2];
static XenolithFolderConfig folder_edit;
static XenolithFolderPrepared folder_config[2];
static float amplifier_feedback_edit;
static XenolithFeedbackPrepared feedback_config[2];
static volatile uint32_t amplifier_serial;
static uint32_t amplifier_applied;
static volatile uint32_t amplifier_active_snapshot;
static volatile uint32_t folder_active_snapshot;
static float amplitude_level=1.0f,mod_amplitude;
static uint8_t held_order[27];
static uint32_t held_count,drone_latch,panic_latched;
static XenolithMidiState midi_notes;
static volatile uint32_t midi_held_count;
static uint32_t midi_virtual_keys,midi_usb_link,midi_seen_revision;
static uint8_t midi_recorded[128];
typedef struct { uint32_t status,arrival_ms,generation; } MidiRealtime;
static MidiRealtime midi_realtime[64];
static volatile uint32_t midi_rt_write,midi_rt_read,midi_rt_dropped,midi_rt_reset;
static uint32_t midi_rt_applied,midi_clock_port=XMIDI_EMPTY;
static volatile uint32_t midi_rt_generation;
static int32_t keyboard_octave;
static XenolithExhaust exhaust PLAT_POOL;
static XenolithExhaustConfig exhaust_config[2];
static volatile uint32_t exhaust_config_serial;
static uint32_t exhaust_applied_serial;
static volatile uint32_t exhaust_reset_request,exhaust_reset_ack,exhaust_reset_done;
/* At exact dry bypass the inaudible tank is cleared through the same owned
 * foreground handoff as panic, then sleeps. Raising MIX starts a fresh tail. */
static volatile uint32_t exhaust_sleeping;
static float exhaust_time=0.4f,exhaust_absorb=0.6f,exhaust_decay=5.0f,exhaust_mix=0.25f;
static float exhaust_diffusion=0.65f,exhaust_width=0.70f;
static XenolithDispersionConfig dispersion_edit;
static volatile uint32_t dispersion_active_snapshot;
static uint32_t fx_hold_started,fx_hold_done;
static float exhaust_rate=44100.0f;
static uint32_t exhaust_reconstruction;
static volatile float active_wet_rate=44100.0f;
static float exhaust_damp_hz=800.0f;
static float exhaust_requested_damp_hz=800.0f;
static uint32_t exhaust_damp_model=1u;
static float exhaust_blend=0.25f,exhaust_blend_target=0.25f;
static volatile float pitch_hz=84.0f, fm_index=2.4f, fm_ratio=1.2f, drift_amount=0.2f;
static volatile uint32_t running;
static volatile uint32_t requested_cells=8u,active_cells=8u;
static float gate_level, master_level;
/* Foreground patch load/save takes ownership only after an output fade and
 * audio acknowledgement. No storage operation is performed by the ISR. */
static volatile uint32_t patch_quiet_request,patch_quiet_ack,patch_quiet_done,patch_recalled;
static float patch_level=1.0f;
static uint32_t patch_occupied;
static XenolithPatch patch_pending;
static uint32_t patch_page,patch_slot,patch_save_armed,patch_operation,patch_status;
static volatile uint32_t patch_bank_loaded,patch_store_count;
static volatile int32_t patch_store_result;
static uint32_t patch_request_ms;
static volatile uint32_t output_peak;
static uint32_t previous_keys, previous_buttons, redraw=1u;
static uint32_t output_page,exhaust_page,envelope_page;
static uint32_t modulation_page,lfo_hold_started,lfo_hold_done;
static XenolithModConfig mod_edit,mod_config[2];
static XenolithRandomMemoryConfig random_memory_edit,random_memory_config[2];
static XenolithMod modulator;
static volatile uint32_t mod_config_serial,mod_action_word;
static volatile uint32_t mod_key_event_serial;
static uint32_t mod_key_event_applied;
static uint32_t mod_config_applied,mod_action_applied;
static volatile float mod_source_snapshot[XM_SOURCE_COUNT];
static volatile float mod_offset_snapshot[XM_DESTINATION_COUNT];
static volatile float mod_key;
static uint32_t mod_wet_active;
static float mod_smooth[5];
static volatile uint32_t mod_end_count,mod_key_count;
static uint32_t sequence_page;
static XenolithSequence sequencer;
static XenolithSequenceImage seq_edit,seq_snapshot[2];
static XenolithSequenceFrame seq_frame_snapshot;
static volatile uint32_t seq_snapshot_serial,seq_cursor_snapshot;
static uint32_t seq_snapshot_seen,seq_snapshot_clock;
enum { SQ_CONFIG,SQ_STEP,SQ_LANE,SQ_CURSOR,SQ_PLAY,SQ_RECORD,SQ_KEY,SQ_CAPTURE,SQ_PANIC,SQ_LANE_STEP,SQ_LANE_FILL };
typedef struct {
    uint32_t type,index,a,b;
    union { XenolithSequenceConfig config; XenolithSequenceStep step; XenolithSequenceLane lane; float value; float values[4]; } data;
} SeqCommand;
static SeqCommand seq_queue[64];
static volatile uint32_t seq_write,seq_read,seq_dropped;
static volatile uint32_t seq_panic_serial;
static uint32_t seq_panic_applied,seq_manual_held,seq_wet_active;
static uint32_t seq_record_mode=XSEQ_RECORD_REPLACE,seq_record_notes=1u,seq_record_lanes=0u;
static volatile uint32_t seq_running,seq_recording,seq_playhead;
static volatile uint32_t seq_external_clock;
static volatile uint32_t seq_beat_phase;
static uint32_t seq_beat_increment;
/* Event telemetry for native/physical diagnostics; never used by synthesis. */
static volatile uint32_t seq_note_count,seq_attack_count,seq_release_count;
static uint32_t seq_audio_active;
static float seq_pitch,seq_index,seq_ratio,seq_drift,seq_wet,seq_amplitude=1.0f;
static float seq_filter_cutoff=8000.0f,seq_filter_resonance;
typedef struct { float core,wet;uint32_t mask; } SeqClocks;
static volatile SeqClocks seq_clock_snapshot;
static volatile uint32_t seq_clock_serial;
static SeqClocks seq_clock_edit;
static uint32_t seq_control_tick;
static float seq_note_hz=84.0f;
static uint32_t seq_note_valid,seq_drone_target,seq_drone_position;
static volatile uint32_t seq_owned_destinations;
static float seq_drone_level;
static uint32_t seq_record_request_serial,seq_record_request_mode;
static uint32_t seq_direct_take,seq_direct_key_lanes;
static uint32_t seq_last_control_destination=XSEQ_DEST_NONE;
typedef struct { float tonic,index,ratio,drift; } AppEffectiveControls;
static AppEffectiveControls effective_controls[2],effective_view={84.0f,2.4f,1.2f,0.2f};
static volatile uint32_t effective_serial;
static uint8_t seq_key_note[27];
static uint32_t seq_key_valid;
static XenolithKeyboardConfig keyboard_edit;
typedef struct {
    XenolithKeyboardConfig config;
    float value,frequency[64];
    uint32_t active,mask_lo,mask_hi,cells;
    uint32_t paraphony,voices,performance_serial;
    float reference_pitch;
    float midi_pitch;
    uint32_t midi_active;
} AppKeyboard;
static AppKeyboard keyboard_config[2],keyboard_audio;
static volatile uint32_t keyboard_serial;
static uint32_t keyboard_applied,keyboard_dirty=1u,keyboard_page;
static uint32_t keyboard_latched_keys,keyboard_last_keys,keyboard_last_cells;
static uint32_t keyboard_latch_valid,keyboard_last_key,keyboard_record_signature;
static float keyboard_latch_value;
static volatile float keyboard_value_snapshot;
static volatile uint32_t keyboard_active_snapshot,keyboard_mask_lo_snapshot,keyboard_mask_hi_snapshot;
static uint32_t keyboard_override_active;
static float keyboard_override_value;
static uint32_t keyboard_cells_applied;
static uint32_t seq_ans_keys,seq_ans_active,seq_bits_active,precision_tick;
static float seq_bits_value,mod_bits_offset;
static volatile uint32_t active_precision_bits=24u;
static uint32_t backup_holding;
static XenolithTopologyConfig topology_edit,topology_config[2];
static XenolithGraphConfig graph_edit;
#include "xenolith_graph_editor.h"
static uint32_t graph_global_dirty,graph_select_pending,graph_select_started,graph_select_done;
static volatile uint32_t topology_serial;
static uint32_t topology_applied,topology_page,home_hold_started,home_hold_done;
static uint32_t workload_profile;
static void envelope_event(uint32_t event);
static void digital_publish(void);
static void exhaust_publish(void);
static void seq_capture_control(uint32_t destination,float value);
static void seq_record_keyboard_config(XenolithKeyboardConfig *config);
static uint32_t seq_record_context_active(void);
static void seq_record_range_error(uint32_t destination);
static void seq_record_capture_clear(void);
static int seq_direct_capture_lane(uint32_t destination);
static void seq_keyboard_record_text(char *out);
static void keyboard_ui_config_loaded(void);
static uint32_t keyboard_target_active(uint32_t destination,uint32_t active);
static void keyboard_update(uint32_t keys,uint32_t pressed);
static void midi_input(void);
static float midi_note_pitch(uint32_t note)
{
    return fm_clampf(440.0f*fm_exp2f(((float)note-69.0f)/12.0f),20.0f,800.0f);
}
static float midi_identity_pitch(uint32_t identity)
{
    return identity<27u?xenolith_key_pitch(identity,keyboard_octave):midi_note_pitch(identity-27u);
}
static uint32_t live_note_gate(void)
{
    return midi_held_count||(held_count&&keyboard_edit.gate);
}
static uint32_t graph_growth_for(uint32_t count)
{
    uint32_t i,kind,uniform;
    if(graph_edit.scope==XGRAPH_ALL)return topology_edit.growth;
    if(count<1u)count=1u;if(count>64u)count=64u;
    kind=graph_edit.kind[0];uniform=1u;
    for(i=1u;i<count;i++)if(graph_edit.kind[i]!=kind
        ||(kind==XTOPO_SELF&&graph_edit.anchor[i]<i)){uniform=0u;break;}
    if(uniform&&(kind==XTOPO_PARALLEL||kind==XTOPO_SELF))return kind;
    return XTOPO_MIXED;
}
static uint32_t paraphony_supported(void)
{
    return keyboard_edit.destination==XKEY_TONIC&&keyboard_edit.response==XKEY_CHROMATIC
        &&(graph_growth_for(requested_cells)==XTOPO_PARALLEL||graph_growth_for(requested_cells)==XTOPO_SELF)
        &&!seq_edit.config.enabled;
}
#include "xenolith_mod_motion.inc"
static uint32_t dispersion_workload(void)
{
    return dispersion_edit.amount_a>0.0f||dispersion_edit.amount_b>0.0f||dispersion_active_snapshot;
}
static uint32_t folder_workload(void)
{
    uint32_t i;
    if(folder_active_snapshot)return 1u;
    if(folder_edit.mix<=0.0f)return 0u;
    if(folder_edit.amount>0.0f)return 1u;
    if(mod_edit.enabled)for(i=0;i<XENOLITH_MOD_ROUTES;i++){
        const XenolithModRoute *r=&mod_edit.route[i];
        if(r->enabled&&r->destination==XM_DST_FOLD&&r->depth!=0.0f)return 1u;
    }
    for(i=0;i<XENOLITH_ENVELOPE_BANK_COUNT;i++){
        const XenolithEnvelopeRoute *r=&envbank_edit.envelope[i];
        if(r->enabled&&r->destination==XM_DST_FOLD&&r->depth!=0.0f)return 1u;
    }
    return 0u;
}
static uint32_t folder_audio_active(void)
{
    const XenolithFolder *f=&amplifier.folder;
    return f->configured&&(f->mix>0.0f||f->target.config.mix>0.0f)
        &&(f->amount!=0.0f||f->target.config.amount!=0.0f||f->offset!=0.0f
           ||f->offset_target!=0.0f||f->remaining||f->offset_remaining);
}
static uint32_t seq_lane_has_steps(const XenolithSequencePattern *pattern,uint32_t lane)
{
    return lane<4u&&(pattern->lane[lane].step_mask
        ||(!lane&&(pattern->cv0_extra_mask[0]||pattern->cv0_extra_mask[1])));
}
static uint32_t calculation_workload(void)
{
    return xenolith_calculation_active(&calculation_edit)||calculation_active_snapshot
        ||flavour_edit.flavour[topology_edit.mode]!=XFLAVOUR_NORMAL
        ||xenolith_coupling_active(&coupling_edit);
}
static uint32_t coupling_workload(void)
{
    return coupling_active_snapshot||flavour_edit.flavour[topology_edit.mode]!=XFLAVOUR_NORMAL;
}
static uint32_t transfer_workload(void)
{
    return transfer_active_snapshot||xenolith_coupling_active(&coupling_edit);
}
static uint32_t digital_workload(void)
{
    uint32_t i;
    if(keyboard_edit.destination==XKEY_CELLS||graph_growth_for(motion_budget_cells())||topology_edit.mode
        ||(performance_edit.voices>1u&&paraphony_supported()))return 2u;
    if(seq_edit.config.enabled)for(i=0;i<4u;i++){
        const XenolithSequenceLane *l=&seq_edit.pattern.lane[i];
        if(l->enabled&&seq_lane_has_steps(&seq_edit.pattern,i)&&l->destination==XSEQ_DEST_ANS_CELL)return 2u;
    }
    if(calculation_workload()||dispersion_workload()||folder_workload()||motion_cell_reserve||filter_edit.placement||filter_active_snapshot||envbank_active_snapshot)return 1u;
    for(i=0;i<3u;i++)if(envbank_edit.envelope[i].enabled)return 1u;
    return mod_edit.enabled||seq_edit.config.enabled||amplifier_edit.colour>0.0f||amplifier_active_snapshot?1u:0u;
}
static uint32_t composite_workload(void)
{
    return (mod_edit.enabled||modulator.config.enabled)
        &&(seq_edit.config.enabled||sequencer.image.config.enabled)
        &&(amplifier_edit.colour>0.0f||amplifier_active_snapshot||folder_workload());
}
static uint32_t extended_workload(void)
{
    uint32_t i;if(filter_edit.placement||filter_active_snapshot||envbank_active_snapshot||folder_workload())return 1u;
    for(i=0;i<3u;i++)if(envbank_edit.envelope[i].enabled)return 1u;
    return 0u;
}
static uint32_t filter_structural(const XenolithFilterConfig *a,const XenolithFilterConfig *b)
{
    return a->placement!=b->placement||((a->placement||b->placement)
        &&((a->mode==XFILTER_TWIN_T)!=(b->mode==XFILTER_TWIN_T)));
}
static void filter_publish(void)
{
    uint32_t next=filter_serial+1u;
    if(!xenolith_filter_config_valid(&filter_edit)||!xenolith_filter_circuit_valid(&circuit_edit))return;
    filter_config[next&1u]=filter_edit;circuit_config[next&1u]=circuit_edit;
    X_PUBLISH_BARRIER();filter_serial=next;digital_publish();
}
static void envbank_publish(void)
{
    uint32_t next=envbank_serial+1u;if(!xenolith_envelope_bank_valid(&envbank_edit))return;
    envbank_config[next&1u]=envbank_edit;X_PUBLISH_BARRIER();envbank_serial=next;digital_publish();
}
static void topology_publish(void)
{
    uint32_t next=topology_serial+1u;
    if(!xenolith_topology_valid(&topology_edit)||!xenolith_graph_valid(&graph_edit))return;
    if(topology_edit.mode!=XTOPO_EXPONENTIAL&&fm_index>64.0f)fm_index=64.0f;
    topology_config[next&1u]=topology_edit;X_PUBLISH_BARRIER();topology_serial=next;
}
/* The inactive suffix is the next-growth recipe. It is prepared before a
 * CELLS route can expose it; existing active birth types remain untouched. */
static void graph_next_kind_changed(void)
{
    uint32_t i;
    for(i=requested_cells;i<64u;i++){
        graph_edit.kind[i]=(uint8_t)graph_edit.next_kind;
        graph_edit.anchor[i]=graph_edit.attachment&&graph_edit.attachment<=i
            ?(uint8_t)(graph_edit.attachment-1u):XGRAPH_NONE;
    }
    topology_publish();keyboard_dirty=1u;
}
static void graph_scope_turn(int32_t step)
{
    uint32_t scope=step>0?XGRAPH_ALL:XGRAPH_ADD;
    if(!xenolith_graph_preview_scope(&graph_edit,&topology_edit.growth,scope))return;
    graph_global_dirty=0u;topology_publish();keyboard_dirty=1u;redraw=1u;
}
static void graph_scope_commit(void)
{
    if(!xenolith_graph_commit_global(&graph_edit,topology_edit.growth))return;
    graph_global_dirty=0u;topology_publish();keyboard_dirty=1u;redraw=1u;
}
static void graph_attachment_turn(int32_t step)
{
    graph_edit.attachment=(uint32_t)clamp((int32_t)graph_edit.attachment+clamp(step,-64,64),0,(int32_t)requested_cells);
    graph_next_kind_changed();redraw=1u;
}
static uint32_t graph_prefix_same(const XenolithGraphConfig *a,const XenolithGraphConfig *b,uint32_t count)
{
    uint32_t i;
    if(a->scope!=b->scope)return 0u;
    if(a->scope==XGRAPH_ALL)return 1u;
    for(i=0;i<count;i++)if(a->kind[i]!=b->kind[i]||a->anchor[i]!=b->anchor[i])return 0u;
    return 1u;
}
static void keyboard_publish(void){keyboard_dirty=1u;}
static uint32_t keyboard_sequence_destination(uint32_t destination)
{
    static const uint8_t target[XKEY_DESTINATION_COUNT]={XSEQ_DEST_PITCH,XSEQ_DEST_INDEX,XSEQ_DEST_RATIO,
        XSEQ_DEST_CORE_RATE,XSEQ_DEST_WET_RATE,XSEQ_DEST_DRIFT,XSEQ_DEST_WET_MIX,
        XSEQ_DEST_ANS_CELL,XSEQ_DEST_AMPLITUDE,XSEQ_DEST_BITS,XSEQ_DEST_EXP_DEPTH,XSEQ_DEST_FILTER_CUTOFF,XSEQ_DEST_FILTER_RESONANCE};
    return destination<XKEY_DESTINATION_COUNT?target[destination]:XSEQ_DEST_NONE;
}
static uint32_t keyboard_target_active(uint32_t destination,uint32_t active)
{
    uint32_t d=keyboard_sequence_destination(destination);
    return active&&(held_count||!(seq_owned_destinations&(1u<<d)));
}
static uint32_t seq_record_context_active(void)
{
    if(seq_record_request_serial&&(int32_t)(seq_read-seq_record_request_serial)<0)
        return seq_record_request_mode!=XSEQ_RECORD_OFF;
    return seq_recording!=XSEQ_REC_IDLE;
}
/* Patch recall owns an acknowledged audio mute and has loaded stopped state. */
static void seq_record_recalled(void)
{
    if(seq_direct_take)seq_record_lanes=seq_direct_key_lanes;
    seq_direct_take=0u;seq_direct_key_lanes=0u;seq_owned_destinations=0u;
    seq_recording=XSEQ_REC_IDLE;seq_record_request_serial=0u;seq_record_request_mode=XSEQ_RECORD_OFF;
}
static void keyboard_values(void)
{
    if(keyboard_audio.midi_active)seq_pitch=keyboard_audio.midi_pitch;
    if(!keyboard_target_active(keyboard_audio.config.destination,keyboard_audio.active))return;
    if(keyboard_audio.config.destination==XKEY_TONIC&&!keyboard_audio.midi_active)seq_pitch=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_INDEX)seq_index=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_EXP_DEPTH&&core.topology.mode==XTOPO_EXPONENTIAL)seq_index=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_RATIO)seq_ratio=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_DRIFT)seq_drift=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_WET_MIX)seq_wet=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_AMPLITUDE)seq_amplitude=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_FILTER_CUTOFF)seq_filter_cutoff=keyboard_audio.value;
    else if(keyboard_audio.config.destination==XKEY_FILTER_RESONANCE)seq_filter_resonance=keyboard_audio.value;
}
static void keyboard_update(uint32_t keys,uint32_t pressed)
{
    uint32_t next,mask,last,i;AppKeyboard *c;
    uint32_t old_clock=keyboard_override_active;
    uint32_t signature=((seq_record_context_active()?seq_record_lanes|(seq_record_notes<<8):0u)|(topology_edit.mode<<16))
        ^(performance_serial*2654435761u)^(topology_serial*2246822519u)^(drone_latch<<27)^(panic_latched<<28)^(seq_edit.config.enabled<<29)^((uint32_t)(keyboard_octave+8)<<20);
    XenolithKeyboardConfig assigned;seq_record_keyboard_config(&assigned);
    if(!keyboard_dirty&&keys==keyboard_last_keys&&motion_effective_cells()==keyboard_last_cells&&signature==keyboard_record_signature)return;
    if(keyboard_dirty||signature!=keyboard_record_signature){keyboard_latch_valid=0u;keyboard_latched_keys=0u;}
    keyboard_record_signature=signature;
    keyboard_last_keys=keys;keyboard_last_cells=motion_effective_cells();
    next=keyboard_serial+1u;c=&keyboard_config[next&1u];
    c->config=assigned;c->active=0u;c->mask_lo=0u;c->mask_hi=0u;c->value=0.0f;c->cells=motion_effective_cells();
    c->paraphony=0u;c->voices=0u;c->reference_pitch=84.0f;c->performance_serial=performance_serial;
    c->midi_active=0u;c->midi_pitch=84.0f;
    if(midi_held_count){
        uint32_t identity=xenolith_midi_latest(&midi_notes);
        if(identity>=27u&&identity<XMIDI_IDENTITIES){c->midi_active=1u;c->midi_pitch=midi_identity_pitch(identity);}
        else if(assigned.destination!=XKEY_TONIC){
            uint32_t j=midi_notes.count;
            while(j)if(midi_notes.order[--j]>=27u){c->midi_active=1u;c->midi_pitch=midi_identity_pitch(midi_notes.order[j]);break;}
        }
    }
    for(i=0;i<64u;i++)c->frequency[i]=84.0f;
    last=held_count?held_order[held_count-1u]:keyboard_last_key;
    if(keys)keyboard_last_key=last;
    if(performance_edit.voices>1u&&paraphony_supported()&&assigned.destination==XKEY_TONIC&&!drone_latch&&!panic_latched){
        if(held_count||midi_held_count){
            float pitches[27];XenolithParaphonyOutput result;
            uint8_t tokens[8],identities[8];const uint8_t *order=held_order;uint32_t count=held_count;
            if(midi_virtual_keys){
                count=xenolith_midi_voicing(&midi_notes,tokens,identities);order=tokens;
                for(i=0;i<8u;i++)pitches[i]=identities[i]<XMIDI_IDENTITIES?midi_identity_pitch(identities[i]):84.0f;
                c->reference_pitch=midi_identity_pitch(xenolith_midi_latest(&midi_notes));
            }else{
                for(i=0;i<27u;i++)pitches[i]=xenolith_key_pitch(i,keyboard_octave);
                c->reference_pitch=pitches[last];
            }
            if(xenolith_paraphony_allocate(&paraphony_allocator,order,pitches,count,c->cells,
                performance_edit.voices,c->reference_pitch,c->reference_pitch,performance_edit.spread,&result)){
                c->paraphony=1u;c->voices=result.active_voices;c->mask_lo=result.mask_lo;c->mask_hi=result.mask_hi;
                for(i=0;i<64u;i++)c->frequency[i]=result.frequency[i];
            }
        }else if(keyboard_config[keyboard_serial&1u].paraphony){
            /* Retain the last chord through the shared amplitude release. */
            const AppKeyboard *last_frame=&keyboard_config[keyboard_serial&1u];
            c->paraphony=1u;c->voices=0u;c->mask_lo=last_frame->mask_lo;c->mask_hi=last_frame->mask_hi;c->reference_pitch=last_frame->reference_pitch;
            for(i=0;i<64u;i++)c->frequency[i]=last_frame->frequency[i];
        }
    }else if(assigned.destination==XKEY_CELLS){
        XenolithKeyboardSelection selection={0};
        if(assigned.latch)keyboard_latched_keys^=pressed;
        mask=assigned.latch?keyboard_latched_keys:keys;
        if(mask&&!(mask&(1u<<last)))for(i=0;i<27u;i++)if(mask&(1u<<i))last=i;
        xenolith_keyboard_select_cells(mask,last,motion_effective_cells(),assigned.cell_width,&selection);
        c->mask_lo=selection.mask_lo;c->mask_hi=selection.mask_hi;c->value=selection.normalized;
        c->active=1u;
        if(assigned.cell_layout==XKEY_SPECTRAL)
            xenolith_keyboard_fill_frequencies(c->frequency,motion_effective_cells(),assigned.low,assigned.high);
    }else if(assigned.response!=XKEY_CHROMATIC){
        if(keys&&xenolith_keyboard_map_value(&assigned,last,&keyboard_latch_value))keyboard_latch_valid=1u;
        c->active=(keys||(assigned.latch&&keyboard_latch_valid))?1u:0u;c->value=keyboard_latch_value;
    }
    keyboard_value_snapshot=c->value;keyboard_active_snapshot=c->active;
    keyboard_mask_lo_snapshot=c->mask_lo;keyboard_mask_hi_snapshot=c->mask_hi;
    keyboard_override_active=c->active?(assigned.destination==XKEY_CORE_RATE?1u:assigned.destination==XKEY_WET_RATE?2u:0u):0u;
    keyboard_override_value=c->value;
    X_PUBLISH_BARRIER();keyboard_serial=next;keyboard_dirty=0u;
    if((old_clock|keyboard_override_active)&1u)digital_publish();
    if((old_clock|keyboard_override_active)&2u)exhaust_publish();
}
static float paraphony_pitch=84.0f;
static void paraphony_apply_pitch(float pitch)
{
    float frequencies[64],scale=pitch/keyboard_audio.reference_pitch;uint32_t i;
    for(i=0;i<64u;i++)frequencies[i]=fm_clampf(keyboard_audio.frequency[i]*scale,20.0f,8000.0f);
    xenolith_set_cell_gates(&core,1u,keyboard_audio.mask_lo,keyboard_audio.mask_hi,1u,frequencies);
    paraphony_pitch=pitch;paraphony_active_voices=keyboard_audio.voices;
}
static void core_controls_set(float pitch,float index,float ratio,float drift)
{
    xenolith_set(&core,pitch,index,ratio,drift);
    if(keyboard_audio.paraphony&&pitch!=paraphony_pitch)paraphony_apply_pitch(pitch);
}
/* Audio owner: manual ANS bands and sequence bands form one bounded union.
 * A sequence ANS route can also select the ordinarily tuned array by itself. */
static void cell_selection_apply(void)
{
    if(keyboard_audio.paraphony){
        if(keyboard_cells_applied!=core.cells)paraphony_apply_pitch(paraphony_pitch);
        keyboard_cells_applied=core.cells;
        keyboard_mask_lo_snapshot=keyboard_audio.mask_lo;keyboard_mask_hi_snapshot=keyboard_audio.mask_hi;
        return;
    }
    paraphony_active_voices=0u;
    uint32_t manual=keyboard_audio.config.destination==XKEY_CELLS;
    uint32_t allowed=keyboard_target_active(XKEY_CELLS,keyboard_audio.active);
    uint32_t lo=manual&&allowed?keyboard_audio.mask_lo:0u,hi=manual&&allowed?keyboard_audio.mask_hi:0u;
    uint32_t enabled=manual||seq_ans_active;
    uint32_t absolute=manual&&keyboard_audio.config.cell_layout==XKEY_SPECTRAL;
    if(seq_ans_active){
        XenolithKeyboardSelection selected={0};uint32_t last=0u,i;
        for(i=0;i<27u;i++)if(seq_ans_keys&(1u<<i))last=i;
        xenolith_keyboard_select_cells(seq_ans_keys,last,core.cells,keyboard_audio.config.cell_width,&selected);
        lo|=selected.mask_lo;hi|=selected.mask_hi;
    }
    if(keyboard_cells_applied!=core.cells||enabled!=core.cell_gates_enabled||absolute!=core.cell_gates_absolute||
       lo!=core.cell_gate_mask[0]||hi!=core.cell_gate_mask[1]){
        xenolith_set_cell_gates(&core,enabled,lo,hi,absolute,keyboard_audio.frequency);
        keyboard_cells_applied=core.cells;
    }
    keyboard_mask_lo_snapshot=lo;keyboard_mask_hi_snapshot=hi;
}
/* Live precision is a control-rate quantizer change, never a clock restart. */
static void precision_apply(void)
{
    float value=core_bits?(float)core_bits:24.0f;uint32_t bits,override=0u;
    if(seq_bits_active){value=seq_bits_value;override=1u;}
    if(keyboard_audio.config.destination==XKEY_BITS&&keyboard_target_active(XKEY_BITS,keyboard_audio.active)){value=keyboard_audio.value;override=1u;}
    if((modulator.config.enabled||envbank.enabled_mask)&&mod_bits_offset!=0.0f){value+=mod_bits_offset;override=1u;}
    bits=(uint32_t)(fm_clampf(value,1.0f,24.0f)+0.5f);
    active_precision_bits=bits;
    /* 24 is the normal full-precision endpoint; preserve an old explicitQ24
     * patch when no live override is connected. OFF records map here too. */
    xenolith_quality_set_live_bits(&quality,bits==24u&&(core_bits!=24u||override)?0u:bits);
}
static const uint32_t trace_gain=16u;
static int32_t output_trim_db;
static volatile float output_trim=1.0f;
/* Display-only decimation of actual post-gain output; never feeds the core. */
static volatile int16_t scope_samples[128];
static int16_t scope_snapshot[128];
static volatile uint32_t scope_write;
static uint32_t scope_divider;
static uint32_t scope_capture_start;
static uint32_t analysis_page;
/* Display tap selected at an audio block boundary:0L,1R,2MID. */
static volatile uint32_t analysis_channel;
static volatile uint32_t analysis_capture_enabled,analysis_published;
static uint32_t analysis_write;
#define ANALYSIS_RING_MASK 8191u
static volatile int16_t analysis_ring[8192] PLAT_POOL;
static int16_t analysis_input[XENOLITH_ANALYSIS_HALF_INPUT] PLAT_POOL;
static XenolithAnalysis analyzer PLAT_POOL;
static XenolithLights spectral_lights;
static uint32_t lights_enabled=1u,lights_clock,lights_accumulator,lights_last_capture,lights_capture_start;
static volatile uint32_t lights_reset_pending;
static volatile uint32_t phase_published[8],phase_serial,phase_cells=8u;
/* Display-only snapshots of the first eight active cells; never fed back. */
typedef struct { uint32_t phase[8],cells; int32_t increment[8]; float tonic[8],noise[8],rate,index,drift; } GaugeSnapshot;
static volatile GaugeSnapshot gauge_published;
static volatile uint32_t gauge_serial;
static float gauge_activity[4];
#include "xenolith_analysis_worker.inc"
static void analysis_setup(void);
static void seq_publish_snapshot(void)
{
    uint32_t next=seq_snapshot_serial+1u;
    xenolith_sequence_snapshot(&sequencer,&seq_snapshot[next&1u]);
    seq_frame_snapshot=sequencer.frame;seq_cursor_snapshot=sequencer.edit_step;
    X_PUBLISH_BARRIER();seq_snapshot_serial=next;
}
static void seq_service_snapshot(uint32_t record_before)
{
    if(sequencer.record_state==XSEQ_REC_IDLE){
        seq_snapshot_clock=0u;
        if(record_before!=XSEQ_REC_IDLE)seq_publish_snapshot();
    }else if(++seq_snapshot_clock>=16u){seq_snapshot_clock=0u;seq_publish_snapshot();}
}
static int seq_send(const SeqCommand *command)
{
    uint32_t write=seq_write;
    if(write-seq_read>=64u){seq_dropped++;return 0;}
    seq_queue[write&63u]=*command;
    if(command->type==SQ_RECORD){seq_record_request_mode=command->a;seq_record_request_serial=write+1u;}
    X_PUBLISH_BARRIER();seq_write=write+1u;return 1;
}
static int seq_simple(uint32_t type,uint32_t a,uint32_t b)
{
    SeqCommand c={0};c.type=type;c.a=a;c.b=b;return seq_send(&c);
}
static void seq_configure(void)
{
    SeqCommand c={0};c.type=SQ_CONFIG;c.data.config=seq_edit.config;seq_send(&c);
}
static void seq_foreground_snapshot(void)
{
    uint32_t before=seq_snapshot_serial;
    XenolithSequenceImage image;
    if(before==seq_snapshot_seen||seq_read!=seq_write)return;
    X_PUBLISH_BARRIER();image=seq_snapshot[before&1u];X_PUBLISH_BARRIER();
    if(before!=seq_snapshot_serial)return;
    seq_edit=image;seq_snapshot_seen=before;
}
static void seq_audio_commands(void)
{
    uint32_t read=seq_read,end=seq_write,limit=0;
    X_PUBLISH_BARRIER();
    if(seq_panic_serial!=seq_panic_applied){
        seq_panic_applied=seq_panic_serial;xenolith_sequence_panic(&sequencer);
        seq_read=end;seq_publish_snapshot();return;
    }
    while(read!=end&&limit++<32u){
        const SeqCommand *c=&seq_queue[read&63u];
        if(c->type==SQ_CONFIG)xenolith_sequence_configure(&sequencer,&c->data.config);
        else if(c->type==SQ_STEP)xenolith_sequence_edit_step(&sequencer,c->index,&c->data.step);
        else if(c->type==SQ_LANE)xenolith_sequence_edit_lane(&sequencer,c->index,&c->data.lane);
        else if(c->type==SQ_LANE_STEP)xenolith_sequence_edit_lane_step(&sequencer,c->index,c->a,c->data.value,c->b);
        else if(c->type==SQ_LANE_FILL){
            uint32_t j,count=xenolith_sequence_lane_length(&sequencer.image.config,c->index);
            for(j=0;j<count;j++){float value;uint32_t enabled;
                if(xenolith_sequence_get_lane_step(&sequencer.image.pattern,c->index,j,&value,&enabled))
                    xenolith_sequence_edit_lane_step(&sequencer,c->index,j,c->data.value,enabled);
            }
        }
        else if(c->type==SQ_CURSOR)xenolith_sequence_select_step(&sequencer,c->a);
        else if(c->type==SQ_PLAY)xenolith_sequence_play_toggle(&sequencer);
        else if(c->type==SQ_RECORD)xenolith_sequence_record_arm(&sequencer,c->a,c->b&1u,c->b>>1);
        else if(c->type==SQ_KEY)xenolith_sequence_capture_key_values(&sequencer,c->a,c->b,c->index,c->data.values);
        else if(c->type==SQ_CAPTURE)xenolith_sequence_capture_parameter(&sequencer,c->index,c->data.value);
        else if(c->type==SQ_PANIC)xenolith_sequence_panic(&sequencer);
        read++;
    }
    /* Publish the matching image/cursor before acknowledging UI commands. */
    if(limit)seq_publish_snapshot();
    X_PUBLISH_BARRIER();seq_read=read;
}
static void midi_audio_realtime(void)
{
    uint32_t read=midi_rt_read,end=midi_rt_write,limit=0u;
    X_PUBLISH_BARRIER();
    if(midi_rt_applied!=midi_rt_reset){
        midi_rt_applied=midi_rt_reset;
        xenolith_sequence_midi_realtime(&sequencer,0xfcu,0u);
        midi_rt_read=end;return;
    }
    while(read!=end&&limit++<32u){
        const MidiRealtime *event=&midi_realtime[read&63u];
        if(event->generation==midi_rt_generation)
            xenolith_sequence_midi_realtime(&sequencer,event->status,event->arrival_ms);
        read++;
    }
    X_PUBLISH_BARRIER();midi_rt_read=read;
}
static void seq_capture_control(uint32_t destination,float value)
{
    uint32_t i;
    seq_last_control_destination=destination;
    if(!seq_record_context_active())return;
    if(seq_direct_take&&!seq_direct_capture_lane(destination))return;
    seq_record_capture_clear();
    for(i=0;i<4u;i++)if(seq_record_lanes&(1u<<i)){
        uint32_t d=seq_edit.pattern.lane[i].destination;float normalized;
        if(d!=destination&&!(destination==XSEQ_DEST_EXP_DEPTH&&d==XSEQ_DEST_INDEX))continue;
        if(!xenolith_sequence_value_to_normalized(d,value,&normalized)){seq_record_range_error(d);continue;}
        {SeqCommand c={0};c.type=SQ_CAPTURE;c.index=i;c.data.value=normalized;if(!seq_send(&c))seq_record_range_error(XSEQ_DEST_NONE);}
    }
}
static void seq_clock_service(void)
{
    static uint32_t last;
    uint32_t now=plat_ms();
    uint32_t before=seq_clock_serial;SeqClocks c;
    if(now-last<50u)return;
    last=now;
    X_PUBLISH_BARRIER();c=seq_clock_snapshot;X_PUBLISH_BARRIER();
    if(before!=seq_clock_serial)return;
    if(c.mask!=seq_clock_edit.mask||fm_fabsf(c.core-seq_clock_edit.core)>1.0f||fm_fabsf(c.wet-seq_clock_edit.wet)>1.0f){
        uint32_t changes=c.mask^seq_clock_edit.mask;
        if(fm_fabsf(c.core-seq_clock_edit.core)>1.0f)changes|=1u;
        if(fm_fabsf(c.wet-seq_clock_edit.wet)>1.0f)changes|=2u;
        seq_clock_edit=c;
        if(changes&1u)digital_publish();
        if(changes&2u)exhaust_publish();
    }
}
static void seq_audio_tick(XenolithModInput *input)
{
    const XenolithSequenceFrame *f=xenolith_sequence_sample(&sequencer);
    uint32_t i;
    uint32_t manual=live_note_gate();
    uint32_t drone=f->running&&sequencer.image.config.note_mode==XSEQ_CONTINUOUS;
    if(f->running){if(!seq_audio_active)seq_beat_phase=0u;seq_beat_phase+=seq_beat_increment;}
    if(!f->running||!seq_audio_active)seq_note_valid=0u;
    if(seq_drone_target&&!drone&&!f->running&&!manual&&!drone_latch)xenolith_envelope_clear(&envelope);
    seq_drone_target=drone;envbank_sequence_gate=f->running&&f->gate;
    if(f->note_off&&!manual&&!seq_ans_active){xenolith_envelope_note_off(&envelope);seq_release_count++;}
    if(f->note_on){
        seq_note_count++;envbank_edges|=XENV_SEQUENCE_BIT;
        if(f->envelope_on&&!manual){xenolith_envelope_note_on(&envelope);seq_attack_count++;}
        seq_note_hz=fm_clampf(440.0f*fm_exp2f(((float)f->note-69.0f)/12.0f),20.0f,800.0f);
        seq_note_valid=1u;
        input->events|=XM_EVENT_BIT(XM_EVENT_KEY);
    }
    if(seq_manual_held&&!manual&&!f->gate&&!seq_ans_active)xenolith_envelope_note_off(&envelope);
    seq_manual_held=manual;
    if(f->step_event){input->events|=XM_EVENT_BIT(XM_EVENT_STEP);}
    input->actions|=f->triggers;
    if(f->running&&!manual){input->gate=drone||f->gate;if(seq_note_valid)input->key=fm_clampf(((float)f->note-60.0f)/24.0f,-1.0f,1.0f);}
    if(seq_control_tick==0u||f->step_event||f->note_on){
        SeqClocks clocks={0};
        uint32_t prior_ans=seq_ans_active;
        uint32_t explicit_exp=0u;float exp_depth=0.0f;
        seq_owned_destinations=0u;
        if(f->running)for(i=0;i<4u;i++)if((f->lane_mask&(1u<<i))&&sequencer.image.pattern.lane[i].enabled){
            uint32_t d=sequencer.image.pattern.lane[i].destination;
            if(d==XSEQ_DEST_NONE||(d==XSEQ_DEST_EXP_DEPTH&&core.topology.mode!=XTOPO_EXPONENTIAL))continue;
            seq_owned_destinations|=1u<<d;
            if(core.topology.mode==XTOPO_EXPONENTIAL&&(d==XSEQ_DEST_INDEX||d==XSEQ_DEST_EXP_DEPTH))
                seq_owned_destinations|=(1u<<XSEQ_DEST_INDEX)|(1u<<XSEQ_DEST_EXP_DEPTH);
        }
        seq_ans_keys=0u;seq_ans_active=0u;seq_bits_active=0u;
        seq_pitch=f->running&&seq_note_valid&&!(manual&&(keyboard_audio.config.destination==XKEY_TONIC||midi_held_count))?seq_note_hz:pitch_hz;
        seq_index=fm_index;seq_ratio=fm_ratio;seq_drift=drift_amount;seq_wet=exhaust_blend_target;seq_amplitude=1.0f;seq_filter_cutoff=filter.target.config.cutoff_hz;seq_filter_resonance=filter.target.config.resonance;
        input->sequence_mask=f->running?f->lane_mask:0u;
        for(i=0;i<4u;i++)input->sequence[i]=f->lane[i];
        if(f->running)for(i=0;i<4u;i++)if(f->lane_mask&(1u<<i)){
            uint32_t d=sequencer.image.pattern.lane[i].destination;float v;
            if(!xenolith_sequence_value_from_normalized(d,f->lane[i],&v))continue;
            if(d==XSEQ_DEST_PITCH)seq_pitch=v;
            else if(d==XSEQ_DEST_INDEX)seq_index=v;
            else if(d==XSEQ_DEST_EXP_DEPTH&&core.topology.mode==XTOPO_EXPONENTIAL){explicit_exp=1u;exp_depth=v;}
            else if(d==XSEQ_DEST_RATIO)seq_ratio=v;
            else if(d==XSEQ_DEST_DRIFT)seq_drift=v;
            else if(d==XSEQ_DEST_WET_MIX)seq_wet=v;
            else if(d==XSEQ_DEST_AMPLITUDE)seq_amplitude=v;
            else if(d==XSEQ_DEST_FILTER_CUTOFF)seq_filter_cutoff=v;
            else if(d==XSEQ_DEST_FILTER_RESONANCE)seq_filter_resonance=v;
            else if(d==XSEQ_DEST_CORE_RATE){clocks.mask|=1u;clocks.core=v;}
            else if(d==XSEQ_DEST_WET_RATE){clocks.mask|=2u;clocks.wet=v;}
            else if(d==XSEQ_DEST_ANS_CELL){
                uint32_t key=(uint32_t)(v+0.5f);
                seq_ans_keys|=1u<<key;seq_ans_active=1u;
            }else if(d==XSEQ_DEST_BITS){seq_bits_value=v;seq_bits_active=1u;}
        }
        if(explicit_exp)seq_index=exp_depth;
        if(seq_ans_active&&!prior_ans&&!manual)xenolith_envelope_note_on(&envelope);
        else if(prior_ans&&!seq_ans_active&&!manual&&!f->gate&&!drone_latch)xenolith_envelope_note_off(&envelope);
        cell_selection_apply();
        seq_clock_serial++;X_PUBLISH_BARRIER();seq_clock_snapshot=clocks;X_PUBLISH_BARRIER();seq_clock_serial++;
        keyboard_values();
        if(!modulator.config.enabled&&!envbank.enabled_mask)core_controls_set(seq_pitch,seq_index,seq_ratio,seq_drift);
    }
    seq_control_tick=(seq_control_tick+1u)&31u;
    seq_audio_active=f->running;
}
static float seq_voice_gain(float envelope_gain)
{
    if(seq_drone_target||seq_drone_position){
        if(seq_drone_target){if(seq_drone_position<221u)seq_drone_position++;}
        else seq_drone_position--;
        seq_drone_level=seq_drone_position==221u?1.0f:(float)seq_drone_position*(1.0f/221.0f);
        envelope_gain+=seq_drone_level*(1.0f-envelope_gain);
    }
    return envelope_gain;
}
static void mod_publish(void)
{
    uint32_t next=mod_config_serial+1u;
    if(!xenolith_mod_validate(&mod_edit)||!xenolith_random_memory_validate(&random_memory_edit))return;
    if((motion_config_mask(&mod_edit)&MM_CELLS)&&!motion_cell_reserve){motion_cells=requested_cells;motion_cell_reserve=1u;}
    mod_config[next&1u]=mod_edit;random_memory_config[next&1u]=random_memory_edit;
    X_PUBLISH_BARRIER();mod_config_serial=next;
    digital_publish();
}
static void mod_action(uint32_t bits)
{
    /* Generation and one-shot actions share an aligned word. Foreground is
     * preempted by the sole audio owner; no CPU1 state is involved. */
    mod_action_word=((mod_action_word+32u)&~31u)|(bits&31u);
}
static void mod_audio_config(void)
{
    uint32_t serial=mod_config_serial,i;
    if(serial==mod_config_applied)return;
    X_PUBLISH_BARRIER();xenolith_mod_apply(&modulator,&mod_config[serial&1u]);
    xenolith_mod_random_apply(&modulator,&random_memory_config[serial&1u]);
    mod_config_applied=serial;mod_wet_active=0u;motion_applied_mask=motion_config_mask(&modulator.config);
    motion_audio_publish();
    for(i=0;i<XENOLITH_MOD_ROUTES;i++){
        const XenolithModRoute *r=&modulator.config.route[i];
        if(modulator.config.enabled&&r->enabled&&r->source&&
           r->destination==XM_DST_WET_MIX&&r->depth!=0.0f)mod_wet_active=1u;
    }
    if(!modulator.config.enabled){for(i=0;i<5u;i++)mod_smooth[i]=0.0f;mod_amplitude=0.0f;}
}

/* Audio-owner integration. Generator time is output time, independent of the
 * cell clock. Transcendentals and destination smoothing run once per32samples. */
static void mod_audio_tick(XenolithModInput *input)
{
    uint32_t before=modulator.config.enabled?modulator.quantum_position:precision_tick;
    uint32_t events=xenolith_mod_advance(&modulator,input),i;
    if(events)mod_end_count++;
    if(!before){
        float p,r;motion_audio_publish();
        mod_amplitude=modulator.offset[XM_DST_AMPLITUDE]+envbank.offset[XM_DST_AMPLITUDE];
        mod_bits_offset+=(modulator.offset[XM_DST_BITS]+envbank.offset[XM_DST_BITS]-mod_bits_offset)*0.4f;
        if(fm_fabsf(modulator.offset[XM_DST_BITS]+envbank.offset[XM_DST_BITS]-mod_bits_offset)<0.00001f)mod_bits_offset=modulator.offset[XM_DST_BITS]+envbank.offset[XM_DST_BITS];
        keyboard_values();
        for(i=0;i<5u;i++){
            float value=modulator.offset[XM_DST_PITCH+i]+envbank.offset[XM_DST_PITCH+i];
            mod_smooth[i]+=(value-mod_smooth[i])*0.4f;
            if(fm_fabsf(value-mod_smooth[i])<0.000001f)mod_smooth[i]=value;
        }
        p=seq_pitch*(mod_smooth[0]==0.0f?1.0f:fm_exp2f(mod_smooth[0]/12.0f));
        r=seq_ratio*(mod_smooth[2]==0.0f?1.0f:fm_exp2f(mod_smooth[2]));
        core_controls_set(fm_clampf(p,20.0f,800.0f),seq_index+64.0f*mod_smooth[1],r,seq_drift+mod_smooth[3]);
    }
    input->events=0u;input->actions=0u;
}
static void digital_publish(void)
{
    uint32_t next=digital_config_serial+1u;
    uint32_t target_cells=motion_effective_cells(),budget_cells=motion_budget_cells();
    float requested=motion_effective_core();
    workload_profile=digital_workload();
    {
        uint32_t flags=workload_profile==2u?XADMIT_GRAPH:workload_profile?XADMIT_CONTROL:0u;
        XenolithAdmission admission;
        if(graph_growth_for(budget_cells)==XTOPO_MIXED)flags|=XADMIT_MIXED;
        if(topology_edit.mode==XTOPO_EXPONENTIAL||topology_edit.mode==XTOPO_WARP)flags|=XADMIT_EXP_WARP;
        if(mod_edit.enabled||seq_edit.config.enabled||amplifier_edit.colour>0.0f||folder_workload())flags|=XADMIT_EXP_COMPOSITE;
        if(composite_workload())flags|=XADMIT_COMPOSITE;
        if(extended_workload())flags|=XADMIT_EXTENDED;
        if(dispersion_workload())flags|=XADMIT_DISPERSION;
        if(filter_edit.placement==XFILTER_PRE_DIGITAL||filter_pre_snapshot)flags|=XADMIT_PRE_FILTER;
        if(calculation_workload())flags|=XADMIT_CALCULATION;
        if(coupling_workload())flags|=XADMIT_COUPLING;
        if(transfer_workload())flags|=XADMIT_TRANSFER;
        admission=xenolith_admission_prepare((XenolithAdmissionRequest){requested,budget_cells,flags},1u);
        requested=admission.rate;
    }
    AppDigitalConfig *c=&digital_config[next&1u];
    c->cells=target_cells;c->profile=workload_profile;
    c->mod_serial=mod_config_serial;c->amplifier_serial=amplifier_serial;
    c->filter_serial=filter_serial;c->envbank_serial=envbank_serial;c->extended=extended_workload();c->composite=composite_workload();
    c->motion_generation=motion_owner?motion_generation:0u;c->motion_wet_serial=motion_wet_serial;
    c->exhaust_serial=exhaust_config_serial;c->dispersion=dispersion_workload();
    c->pre_filter=filter_edit.placement==XFILTER_PRE_DIGITAL||filter_pre_snapshot;
    c->topology=topology_edit;c->topology_serial=topology_serial;c->graph=graph_edit;
    xenolith_graph_compile(&c->graph,c->topology.growth,target_cells,&c->prepared_graph);
    c->calculation=calculation_edit;c->calculation_serial=calculation_serial;
    c->flavour=flavour_edit;
    c->coupling=coupling_edit;
    c->performance=performance_edit;c->performance_serial=performance_serial;
    c->calculation_workload=calculation_workload();
    c->coupling_workload=coupling_workload();
    c->transfer_workload=transfer_workload();
    xenolith_quality_prepare_digital(&c->dsp,44100.0f,32u,budget_cells,
        requested,core_bits,core_quantizer,core_reconstruction);
    X_PUBLISH_BARRIER();digital_config_serial=next;
}

static void envelope_publish(void)
{
    uint32_t next=envelope_config_serial+1u;
    envelope_config[next&1u][0]=envelope_attack;envelope_config[next&1u][1]=envelope_decay;
    envelope_config[next&1u][2]=envelope_sustain;envelope_config[next&1u][3]=envelope_release;
    X_PUBLISH_BARRIER();envelope_config_serial=next;
}
static void amplifier_publish(void)
{
    uint32_t next=amplifier_serial+1u;
    if(!xenolith_amplifier_prepare_extended(&amplifier_config[next&1u],&amplifier_edit,&amplifier_shape_edit,44100.0f))return;
    if(!xenolith_folder_prepare(&folder_config[next&1u],&folder_edit,44100.0f))return;
    if(!xenolith_feedback_prepare(&feedback_config[next&1u],amplifier_feedback_edit,amplifier_shape_edit.curve))return;
    X_PUBLISH_BARRIER();amplifier_serial=next;
    digital_publish();
}

static void envelope_event(uint32_t event)
{
    /* Generation and event travel in one aligned word: no torn pair. */
    X_PUBLISH_BARRIER();envelope_event_word=((envelope_event_word+4u)&~3u)|(event&3u);
}

static void lights_accept(uint32_t end)
{
    uint32_t frames=analyzer.sample_rate==22050.0f?XENOLITH_ANALYSIS_HALF_INPUT:analyzer.fft_size;
    if(lights_enabled&&(uint32_t)(end-lights_capture_start)>=frames){
        xenolith_lights_ingest(&spectral_lights,xenolith_analysis_magnitudes(&analyzer),analyzer.fft_size,analyzer.sample_rate);
        lights_last_capture=end;
    }
}

static void exhaust_publish(void)
{
    uint32_t next=exhaust_config_serial+1u;
    /* Prepare an inactive complete config before publishing. Audio interrupts
     * foreground execution, so its selected buffer cannot change mid-apply. */
    xenolith_exhaust_prepare_extended_rate(&exhaust_config[next&1u],44100.0f,motion_effective_wet(),exhaust_reconstruction,
        motion_effective_echo(),exhaust_absorb,exhaust_decay,exhaust_mix,exhaust_diffusion,exhaust_width,exhaust_requested_damp_hz,exhaust_damp_model,&dispersion_edit);
    exhaust_damp_hz=exhaust_config[next&1u].damp_hz;
    X_PUBLISH_BARRIER();
    exhaust_config_serial=next;
    digital_publish();
}

static void exhaust_service(void)
{
    /* A sleeping tank has no wet ticks to retire the optional allpasses.
     * Its inaudible history may use the existing exclusive foreground clear. */
    if(exhaust_sleeping&&dispersion_active_snapshot&&dispersion_edit.amount_a==0.0f&&dispersion_edit.amount_b==0.0f
       &&exhaust_applied_serial==exhaust_config_serial&&exhaust_reset_request==exhaust_reset_done)
        exhaust_reset_request++;
    uint32_t request=exhaust_reset_request;
    if(request!=exhaust_reset_done&&exhaust_reset_ack==request){
        /* Audio has acknowledged exclusive foreground ownership and skips
         * this state until done is published. Never bulk-clear in an ISR. */
        X_PUBLISH_BARRIER();xenolith_exhaust_clear(&exhaust);X_PUBLISH_BARRIER();
        exhaust_reset_done=request;
    }
}

#include "xenolith_storage.inc"
static void patches_init(void)
{
    patch_bank_loaded=xstore_scan(&patch_occupied,&patch_pending)?1u:0u;
}

static void patch_capture(XenolithPatch *p)
{
    xenolith_patch_defaults(p);
    p->pitch_hz=pitch_hz;p->index=fm_index;p->ratio=fm_ratio;p->drift=drift_amount;
    p->attack_ms=envelope_attack;p->decay_ms=envelope_decay;p->sustain=envelope_sustain;p->release_ms=envelope_release;
    p->echo_seconds=exhaust_time;p->absorb=exhaust_absorb;p->tail_seconds=exhaust_decay;p->mix=exhaust_mix;
    p->cells=requested_cells;p->reserved=0u;p->clock_divider=0u;p->trim_db=output_trim_db;
    p->core_rate=core_max_rate;p->fractional_bits=core_bits;p->quantizer=core_quantizer;p->reconstruction=core_reconstruction;
    p->wet_rate=exhaust_rate;p->wet_reconstruction=exhaust_reconstruction;
    p->modulation=mod_edit;xenolith_sequence_snapshot(&sequencer,&p->sequence);
    p->keyboard=keyboard_edit;p->amplifier=amplifier_edit;p->topology=topology_edit;
    p->diffusion=exhaust_diffusion;p->width=exhaust_width;
    p->filter=filter_edit;p->envelopes=envbank_edit;
    p->circuit=circuit_edit;p->calculation=calculation_edit;
    p->flavour=flavour_edit;
    p->coupling=coupling_edit;
    p->random_memory=random_memory_edit;p->dispersion=dispersion_edit;
    p->governor=governor_edit;
    p->performance=performance_edit;p->graph=graph_edit;
    p->damp_hz=exhaust_requested_damp_hz;p->damp_model=exhaust_damp_model;p->amplifier_shape=amplifier_shape_edit;p->folder=folder_edit;p->amplifier_feedback=amplifier_feedback_edit;
}

static void patch_apply(const XenolithPatch *p)
{
    /* Audio has acknowledged a mute and does not read/process its state until
     * done. Held keys, drone, current envelope level and tank memory survive. */
    pitch_hz=p->pitch_hz;fm_index=p->index;fm_ratio=p->ratio;drift_amount=p->drift;
    envelope_attack=p->attack_ms;envelope_decay=p->decay_ms;envelope_sustain=p->sustain;envelope_release=p->release_ms;
    requested_cells=p->cells;
    performance_edit=p->performance;performance_serial++;home_detail_page=0u;
    xenolith_paraphony_reset(&paraphony_allocator);
    governor_edit=p->governor;governor_publish();
    exhaust_rate=p->wet_rate;exhaust_reconstruction=p->wet_reconstruction;exhaust_time=p->echo_seconds;
    exhaust_absorb=p->absorb;exhaust_requested_damp_hz=p->damp_hz;exhaust_damp_model=p->damp_model;
    exhaust_decay=p->tail_seconds;exhaust_mix=p->mix;
    exhaust_diffusion=p->diffusion;exhaust_width=p->width;
    output_trim_db=p->trim_db;output_trim=fm_db2lin((float)output_trim_db);
    core_max_rate=p->core_rate;core_bits=p->fractional_bits;
    core_quantizer=p->quantizer;core_reconstruction=p->reconstruction;
    calculation_edit=p->calculation;flavour_edit=p->flavour;coupling_edit=p->coupling;calculation_serial++;
    filter_edit=p->filter;circuit_edit=p->circuit;envbank_edit=p->envelopes;filter_publish();envbank_publish();
    motion_reset();mod_edit=p->modulation;random_memory_edit=p->random_memory;dispersion_edit=p->dispersion;mod_publish();
    xenolith_sequence_load(&sequencer,&p->sequence,sizeof p->sequence);
    seq_record_recalled();
    seq_clock_edit.mask=0u;seq_clock_edit.core=0.0f;seq_clock_edit.wet=0.0f;
    seq_clock_snapshot=seq_clock_edit;seq_audio_active=0u;seq_running=0u;seq_control_tick=0u;
    seq_ans_keys=seq_ans_active=seq_bits_active=0u;mod_bits_offset=0.0f;
    keyboard_edit=p->keyboard;keyboard_ui_config_loaded();keyboard_dirty=1u;keyboard_update(0u,0u);
    amplifier_edit=p->amplifier;amplifier_shape_edit=p->amplifier_shape;folder_edit=p->folder;amplifier_feedback_edit=p->amplifier_feedback;amplifier_publish();seq_key_valid=0u;
    topology_edit=p->topology;graph_edit=p->graph;
    graph_global_dirty=graph_select_pending=graph_select_done=0u;topology_publish();
    seq_edit=p->sequence;seq_read=seq_write;seq_publish_snapshot();
    envelope_publish();exhaust_publish();digital_publish();patch_recalled=1u;
}

static void patch_begin(uint32_t operation)
{
    if(patch_operation||backup_holding)return;
    if(operation==1u){
        if(patch_slot>=XENOLITH_PATCH_COUNT||!(patch_occupied&(1u<<patch_slot))){patch_status=3u;redraw=1u;return;}
    }
    patch_status=0u;patch_operation=operation;patch_request_ms=plat_ms();
    X_PUBLISH_BARRIER();patch_quiet_request++;redraw=1u;
}

static void patch_service(uint32_t now)
{
    uint32_t request=patch_quiet_request;
    if(!patch_operation||backup_holding)return;
    if(patch_quiet_ack!=request){
        if(now-patch_request_ms>1000u){
            patch_status=4u;patch_store_result=-10;patch_operation=0u;
            X_PUBLISH_BARRIER();patch_quiet_done=request;redraw=1u;
        }
        return;
    }
    X_PUBLISH_BARRIER();
    if(patch_operation==1u){
        if(xstore_load_slot(patch_slot,&patch_pending)){patch_apply(&patch_pending);patch_status=1u;}
        else{patch_status=4u;patch_store_result=-11;}
    }
    else{
        /* Audio is quiet and acknowledged: recording cannot mutate this take
         * while the pending record is validated and committed atomically. */
        patch_capture(&patch_pending);
        patch_store_result=patch_slot<XENOLITH_PATCH_COUNT && xenolith_patch_valid(&patch_pending)?xstore_save_slot(patch_slot,&patch_pending):-11;
        if(patch_store_result==0){patch_occupied|=1u<<patch_slot;patch_store_count++;patch_status=2u;}
        else patch_status=4u;
    }
    patch_operation=0u;patch_save_armed=0u;
    X_PUBLISH_BARRIER();patch_quiet_done=request;redraw=1u;
}

/* USB backups contain explicitly saved objects only. Never silently store
 * the live edit buffer when an archive is requested. Platform owns the wire. */
int app_backup_playing(void)
{
    return running||seq_running||seq_recording||patch_operation||held_count||midi_held_count||envelope.level>0.0001f;
}
void app_backup_flush(void) {}
void app_backup_hold(int on) { backup_holding=on?1u:0u; }
void app_backup_restored(void)
{
    patches_init();patch_save_armed=0u;patch_status=0u;redraw=1u;
}

static void defaults(void)
{
    xenolith_performance_defaults(&performance_edit);performance_serial++;home_detail_page=0u;
    xenolith_paraphony_reset(&paraphony_allocator);
    xenolith_governor_defaults(&governor_edit);governor_publish();governor_page=0u;
    xenolith_calculation_defaults(&calculation_edit);xenolith_flavour_defaults(&flavour_edit);
    xenolith_coupling_defaults(&coupling_edit);calculation_serial++;
    xenolith_filter_circuit_defaults(&circuit_edit);
    xenolith_keyboard_config_defaults(&keyboard_edit);keyboard_ui_config_loaded();keyboard_dirty=1u;
    xenolith_amplifier_defaults(&amplifier_edit);xenolith_amplifier_shape_defaults(&amplifier_shape_edit);xenolith_folder_defaults(&folder_edit);amplifier_feedback_edit=0.0f;amplifier_publish();
    xenolith_topology_defaults(&topology_edit);xenolith_graph_defaults(&graph_edit);graph_edit.scope=XGRAPH_ADD;
    graph_global_dirty=graph_select_pending=graph_select_done=0u;topology_publish();
    pitch_hz=84.0f; fm_index=2.4f; fm_ratio=1.2f; drift_amount=0.2f;
    keyboard_octave=0;
    output_trim_db=0;output_trim=1.0f;output_page=0u;
    requested_cells=8u;motion_reset();core_max_rate=44100.0f;core_bits=0u;core_quantizer=0u;core_reconstruction=0u;digital_publish();
    envelope_page=0u;envelope_selection=0u;
    xenolith_filter_defaults(&filter_edit);xenolith_envelope_bank_defaults(&envbank_edit);
    filter_publish();envbank_publish();
    envelope_attack=10.0f;envelope_decay=250.0f;envelope_sustain=0.8f;envelope_release=400.0f;
    envelope_publish();
    exhaust_page=0u;exhaust_rate=44100.0f;exhaust_reconstruction=0u;exhaust_time=0.4f;exhaust_absorb=0.6f;exhaust_decay=5.0f;exhaust_mix=0.25f;
    exhaust_diffusion=0.65f;exhaust_width=0.70f;
    xenolith_dispersion_defaults(&dispersion_edit);
    exhaust_requested_damp_hz=800.0f;exhaust_damp_model=1u;
    exhaust_publish();
    xenolith_mod_defaults(&mod_edit);xenolith_random_memory_defaults(&random_memory_edit);
    mod_edit.function[0].enabled=1u;mod_edit.function[0].mode=XM_CYCLE;
    mod_edit.function[0].rise_s=0.8f;mod_edit.function[0].fall_s=1.2f;
    mod_edit.function[1].enabled=1u;mod_edit.function[1].mode=XM_CYCLE;
    mod_edit.function[1].rise_s=0.4f;mod_edit.function[1].fall_s=1.6f;
    mod_edit.random.enabled=1u;mod_publish();
    redraw=1;
}

void app_init(void)
{
    xenolith_midi_init(&midi_notes);midi_usb_link=plat_midi_out_link();
    defaults();
    xenolith_governor_init(&governor);
    xenolith_init(&core,44100.0f,0x58584E31u);
    xenolith_quality_init(&quality,&core);
    xenolith_set(&core,pitch_hz,fm_index,fm_ratio,drift_amount);
    xenolith_exhaust_init(&exhaust,44100.0f);
    xenolith_envelope_init(&envelope,44100.0f);
    xenolith_filter_init(&filter);xenolith_envelope_bank_init(&envbank);
    xenolith_amplifier_init(&amplifier);
    xenolith_mod_init(&modulator,0x4d4f4431u);
    xenolith_sequence_init(&sequencer);xenolith_sequence_snapshot(&sequencer,&seq_edit);seq_publish_snapshot();
    keyboard_update(0u,0u);
    xenolith_lights_init(&spectral_lights);lights_clock=plat_ms();
    patches_init();
    running=0u;drone_latch=0u;panic_latched=0u;analysis_capture_enabled=1u;
    analysis_setup();
    analysis_worker_init();
}

void app_panic(void)
{
    xenolith_midi_clear(&midi_notes);midi_held_count=0u;keyboard_dirty=1u;
    {uint32_t i;for(i=0;i<128u;i++)midi_recorded[i]=0u;}
    midi_clock_port=XMIDI_EMPTY;midi_rt_reset++;
    if(seq_direct_take)seq_record_lanes=seq_direct_key_lanes;
    seq_direct_take=0u;
    running=0u;
    drone_latch=0u;panic_latched=1u;envelope_event(2u);
    seq_panic_serial++;
    gate_level=0.0f;
    exhaust_reset_request++;
    lights_reset_pending=1u;
    redraw=1u;
}

/* A committed foreground module waits until its matching budget publication. */
static uint32_t module_admitted(uint32_t serial,uint32_t admitted,uint32_t budget_ready)
{
    return budget_ready&&serial==admitted;
}

/* Low-clock count edits retain the audible prefix. Reserve a lower clock
 * before adding work; retire the suffix before raising its clock. A rapidly
 * reversed gesture may cancel a rising ramp, but cannot accelerate old cells. */
static uint32_t count_clock_transition(const AppDigitalConfig *d)
{
    const XenolithDigitalConfig *c=&d->dsp;
    uint32_t scheduled=quality.continuous||quality.factor==XENOLITH_RATE_CRUSH;
    float nominal=xenolith_quality_nominal_rate(&quality);
    if(d->cells==core.cells&&!core.count_smoothing)return 0u;
    if(!((quality.factor==1u)||(quality.factor>=XENOLITH_RATE_ECO2&&quality.factor<=XENOLITH_RATE_CRUSH))
       ||!((c->factor==1u)||(c->factor>=XENOLITH_RATE_ECO2&&c->factor<=XENOLITH_RATE_CRUSH))
       ||quality.warmup||quality.configured_bits!=c->bits||quality.crush.quantizer!=c->quantizer
       ||(scheduled?quality.crush.reconstruction:0u)!=c->reconstruction)return 0u;
    if((running||seq_running)&&gate_level>0.99f)count_continuous_probe=1u;
    if(quality.ramp_remaining&&quality.target_rate>nominal){
        XenolithDigitalConfig hold=*c;
        hold.effective_rate=nominal;hold.factor=quality.factor;
        if(!xenolith_quality_retime(&quality,&core,&hold))return 0u;
    }
    if(d->cells>core.cells&&nominal>c->effective_rate+0.01f){
        /* An outgoing fade is still charged at its larger extent. Finish it
         * before starting another clock gesture, even if that gesture lowers. */
        if(!core.count_smoothing){
            uint32_t changed=quality.target_rate!=c->effective_rate;
            if(!xenolith_quality_retime(&quality,&core,c))return 0u;
            if(changed){count_clock_reservations++;digital_retime_count++;}
        }
    }else if(d->cells!=core.cells){
        if(d->cells>core.cells&&nominal-c->effective_rate>count_clock_entry_excess)
            count_clock_entry_excess=nominal-c->effective_rate;
        xenolith_transition_cells(&core,d->cells,1u);count_continuous_changes++;
    }
    return 1u;
}

void app_render(int32_t *lr,uint32_t n,uint32_t master_q12)
{
    uint32_t render_start=plat_ticks24(),render_budget=(uint32_t)((uint64_t)n*24000000u/44100u);
    uint32_t chunk_start=render_start,chunk_core=0u,core_held=0u,skipped_core_frames=0u;
    uint32_t held_quality_ticks=0u,held_quality_peak=0u;
    const uint32_t final_reserve=6000u; /*250us: callback epilogue, USB tap and DMA commit. */
    XenolithGovernorFrame sag;
    uint32_t i,peak=0;
    uint32_t amp_needed,budget_ready,topology_request,filter_placement,bank_needed,filter_fading,calculation_request,seq_record_before;
    float filter_gate_target;
    uint32_t patch_request=patch_quiet_request;
    uint32_t capture=analysis_capture_enabled,capture_write=analysis_write,channel=analysis_channel;
    uint32_t wet_ready=exhaust_reset_request==exhaust_reset_done,serial=exhaust_config_serial;
    uint32_t digital_serial=digital_config_serial,wanted_cells,wanted_quality;
    XenolithModInput mod_input={0};
    AppDigitalConfig digital;
    float gain=((float)master_q12/4096.0f)*output_trim;
    float target;
    float patch_target=patch_request==patch_quiet_done?1.0f:0.0f;
    if(patch_request!=patch_quiet_done&&patch_quiet_ack==patch_request){
        /* Foreground owns DSP/control state during the acknowledged load. */
        for(i=0;i<2u*n;i++)lr[i]=0;
        output_peak=0;return;
    }
    if(governor_applied!=governor_serial){
        X_PUBLISH_BARRIER();xenolith_governor_apply(&governor,&governor_config[governor_serial&1u]);
        governor_applied=governor_serial;
    }
    sag=xenolith_governor_frame(&governor);
    xenolith_quality_set_core_hold(&quality,0);
    xenolith_quality_set_load_duty(&quality,sag.duty);
    xenolith_set_interpolation_weight(&core,sag.interpolation);
    xenolith_set_load_erosion(&core,sag.table_loss,sag.dither,sag.cell_ratio,sag.link_gain);
    xenolith_filter_set_load_shape(&filter,governor.config.filter_shape);
    xenolith_filter_set_load_sag(&filter,sag.effects);
    xenolith_amplifier_set_load_bias(&amplifier,governor.config.vca_bias);
    xenolith_amplifier_set_load_sag(&amplifier,sag.effects);
    if(wet_ready){
        xenolith_exhaust_set_load_shape(&exhaust,governor.config.reverb_shape);
        xenolith_exhaust_set_load_sag(&exhaust,sag.effects);
    }
    X_PUBLISH_BARRIER();digital=digital_config[digital_serial&1u];
    topology_request=digital.topology_serial;
    calculation_request=digital.calculation_serial;
    wanted_cells=digital.cells;wanted_quality=digital.dsp.factor;
    /* A new heavy control config waits for its reserved lower calculation
     * rate. The old renderer keeps running during the50ms retime; enabling
     * modulation must not overload the first few ramp blocks. Panic bypasses
     * this gate. Count edits needing a new rate use the structural path below. */
    budget_ready=digital.profile==0u||xenolith_quality_nominal_rate(&quality)<=digital.dsp.effective_rate+0.5f;
    if(patch_recalled){gate_level=0.0f;patch_recalled=0u;}
    if(budget_ready&&performance_applied!=digital.performance_serial){
        xenolith_set_performance(&core,digital.performance.glide_ms,digital.performance.spread);
        performance_applied=digital.performance_serial;
    }
    paraphony_effective_voices=digital.performance.voices<core.cells?digital.performance.voices:core.cells;
    if(budget_ready||seq_panic_serial!=seq_panic_applied)seq_audio_commands();
    midi_audio_realtime();
    seq_external_clock=sequencer.external_clock;
    seq_record_before=sequencer.record_state;
    {uint32_t steps=xenolith_sequence_steps_per_beat(sequencer.image.config.division);
        seq_beat_increment=sequencer.phase_increment/(steps?steps:4u);}
    seq_wet_active=0u;
    if(sequencer.running)for(i=0;i<4u;i++){
        const XenolithSequenceLane *lane=&sequencer.image.pattern.lane[i];
        if(lane->enabled&&seq_lane_has_steps(&sequencer.image.pattern,i)&&lane->destination==XSEQ_DEST_WET_MIX)seq_wet_active=1u;
    }
    if(module_admitted(mod_config_serial,digital.mod_serial,budget_ready))mod_audio_config();
    if(module_admitted(amplifier_serial,digital.amplifier_serial,budget_ready)&&amplifier_serial!=amplifier_applied){
        X_PUBLISH_BARRIER();xenolith_amplifier_apply(&amplifier,&amplifier_config[amplifier_serial&1u]);
        xenolith_amplifier_apply_folder(&amplifier,&folder_config[amplifier_serial&1u]);
        xenolith_amplifier_apply_feedback(&amplifier,&feedback_config[amplifier_serial&1u]);
        amplifier_applied=amplifier_serial;
    }
    if(module_admitted(envbank_serial,digital.envbank_serial,budget_ready)&&envbank_serial!=envbank_applied){
        X_PUBLISH_BARRIER();xenolith_envelope_bank_apply(&envbank,&envbank_config[envbank_serial&1u]);
        envbank_applied=envbank_serial;envbank_active_snapshot=envbank.enabled_mask!=0u;
        if(!envbank.enabled_mask&&!modulator.config.enabled){uint32_t e;mod_amplitude=mod_bits_offset=0.0f;for(e=0;e<5u;e++)mod_smooth[e]=0.0f;}
    }
    if(module_admitted(filter_serial,digital.filter_serial,budget_ready)&&filter_serial!=filter_applied){
        const XenolithFilterConfig *fc=&filter_config[filter_serial&1u];
        if(!filter_structural(fc,&filter.target.config)||filter_route_level==0.0f){
            XenolithFilterPrepared fp;float rate=fc->placement==XFILTER_PRE_DIGITAL?core.sample_rate:44100.0f;
            if(xenolith_filter_prepare(&fp,fc,rate)){
                if(filter_structural(fc,&filter.target.config))xenolith_filter_clear(&filter);
                if(xenolith_filter_apply(&filter,&fp)){
                xenolith_filter_set_circuit(&filter,&circuit_config[filter_serial&1u]);
                filter_applied=filter_serial;filter_active_snapshot=fc->placement!=XFILTER_OFF;
                }
            }
        }
    }
    amp_needed=amplifier.colour!=0.0f||amplifier.remaining!=0u||folder_audio_active()
        ||xenolith_amplifier_load_active(&amplifier);
    mod_input.key=mod_key;mod_input.gate=running?1u:0u;
    if(mod_key_event_serial!=mod_key_event_applied){
        envbank_edges|=XENV_KEY_BIT;
        mod_key_event_applied=mod_key_event_serial;mod_input.events|=XM_EVENT_BIT(XM_EVENT_KEY);mod_key_count++;
    }
    if(mod_action_word!=mod_action_applied){
        mod_action_applied=mod_action_word;mod_input.actions=mod_action_applied&31u;
    }
    {
        uint32_t config=envelope_config_serial,event=envelope_event_word;
        if(config!=envelope_config_applied){
            const float *c;X_PUBLISH_BARRIER();c=envelope_config[config&1u];
            xenolith_envelope_set(&envelope,c[0],c[1],c[2],c[3]);envelope_config_applied=config;
        }
        if(event!=envelope_event_applied){
            X_PUBLISH_BARRIER();
            if((event&3u)==2u){xenolith_envelope_bank_clear(&envbank);xenolith_filter_clear(&filter);envbank_edges=envbank_sequence_gate=0u;xenolith_envelope_clear(&envelope);xenolith_amplifier_clear(&amplifier);seq_drone_level=0.0f;seq_drone_target=seq_drone_position=0u;}
            else if((event&3u)==1u){envbank_edges|=XENV_KEY_BIT;xenolith_envelope_note_on(&envelope);mod_input.events=XM_EVENT_BIT(XM_EVENT_KEY);mod_key_count++;}
            else xenolith_envelope_note_off(&envelope);
            envelope_event_applied=event;
        }
    }
    if(!wet_ready){X_PUBLISH_BARRIER();exhaust_reset_ack=exhaust_reset_request;}
    else if(serial!=exhaust_applied_serial&&module_admitted(serial,digital.exhaust_serial,budget_ready)){
        XenolithExhaustConfig prepared;
        X_PUBLISH_BARRIER();
        prepared=exhaust_config[serial&1u];
        exhaust_blend_target=prepared.mix;prepared.mix=1.0f;
        /* Clock changes may queue a config. Both active and queued module
         * configs must stay fully wet; the app owns the smoothed blend. */
        if(!xenolith_exhaust_retime(&exhaust,&prepared))
            xenolith_exhaust_apply(&exhaust,&prepared);
        exhaust_applied_serial=serial;
    }
    if(wet_ready)xenolith_exhaust_begin_block(&exhaust,n);
    if(wet_ready&&(exhaust_blend_target>0.0f||mod_wet_active||seq_wet_active||
       (keyboard_audio.active&&keyboard_audio.config.destination==XKEY_WET_MIX)))exhaust_sleeping=0u;
    if(topology_request!=topology_applied
       &&digital.topology.growth==core.topology.growth&&digital.topology.mode==core.topology.mode
       &&digital.topology.link==core.topology.link&&digital.topology.feedback==core.topology.feedback
       &&graph_prefix_same(&core.graph_config,&digital.graph,core.render_cells)){
        xenolith_set_topology_graph_prepared(&core,&digital.topology,&digital.graph,&digital.prepared_graph);
        topology_applied=topology_request;
    }
    /* Low-clock count/rate transactions remain audible while reserving work.
     * New topology, precision, reconstruction and FIR stages retain the gate. */
    if(budget_ready&&calculation_request!=calculation_applied&&core.coupling_active&&
       xenolith_coupling_active(&digital.coupling)&&digital.coupling.method==core.coupling.method&&
       digital.calculation.table_bits==core.calculation.table_bits&&
       digital.calculation.interpolation==core.calculation.interpolation&&
       digital.calculation.target==core.calculation.target&&
       digital.calculation.harmonic==core.calculation.harmonic){
        uint32_t i,same=1u;
        for(i=0;i<XFLAVOUR_MODES;i++)if(digital.flavour.flavour[i]!=core.flavour.flavour[i])same=0u;
        if(same){
            /* The same operator ramps its coefficients internally; turning
             * DEPTH/BIAS/TIME need not close the structural audio gate. */
            xenolith_set_coupling(&core,&digital.coupling);calculation_applied=calculation_request;
        }
    }
    count_clock_waiting=0u;
    if(topology_request==topology_applied&&calculation_request==calculation_applied)
        count_clock_waiting=count_clock_transition(&digital);
    /* Same-clock precision/quantizer edits preserve held output/FIR state.
     * The remaining same-clock count path also covers unchanged high stages. */
    if((topology_request==topology_applied&&calculation_request==calculation_applied)&&wanted_cells!=core.cells&&!quality.ramp_remaining&&
       quality.configured_bits==digital.dsp.bits&&quality.crush.quantizer==digital.dsp.quantizer&&
       fm_fabsf(xenolith_quality_nominal_rate(&quality)-digital.dsp.effective_rate)<0.5f&&
       xenolith_quality_update_precision(&quality,&digital.dsp)){
        xenolith_transition_cells(&core,wanted_cells,1u);
        count_continuous_changes++;
        if((running||seq_running)&&gate_level>0.99f)count_continuous_probe=1u;
        digital_config_applied=digital_serial;precision_apply();
    }
    if((topology_request==topology_applied&&calculation_request==calculation_applied)&&digital_serial!=digital_config_applied&&wanted_cells==core.cells&&
       xenolith_quality_update_precision(&quality,&digital.dsp)){
        digital_config_applied=digital_serial;precision_apply();
    }
    if(!core.count_smoothing&&(topology_request==topology_applied&&calculation_request==calculation_applied)&&digital_serial!=digital_config_applied&&wanted_cells==core.cells&&
       xenolith_quality_retime(&quality,&core,&digital.dsp)){
        if(digital.dsp.effective_rate!=xenolith_quality_nominal_rate(&quality)){
            digital_retime_count++;
            if(running&&envelope.level>0.5f&&gate_level>0.99f)digital_rate_probe=1u;
        }
        digital_config_applied=digital_serial;
    }
    /* A topology replacement and its rate allowance are one transaction.
     * Never accelerate the outgoing graph while its structural fade closes. */
    if((topology_request==topology_applied&&calculation_request==calculation_applied)){
        if(core.count_smoothing&&quality.ramp_remaining&&quality.target_rate>xenolith_quality_nominal_rate(&quality)+0.01f)
            count_clock_accel_with_suffix++;
        xenolith_quality_begin_block(&quality,&core,n);
    }
    /* Remaining rate/topology changes share a muted block boundary. Reclocking
     * preserves oscillator/noise state; FIR warmup remains behind this gate. */
    if(!count_clock_waiting&&(wanted_cells!=core.cells||wanted_quality!=quality.factor||digital_serial!=digital_config_applied||topology_request!=topology_applied||calculation_request!=calculation_applied)&&(gate_level<0.0001f||(envelope.level==0.0f&&seq_drone_level==0.0f))){
        if(wanted_cells!=core.cells)count_structural_changes++;
        gate_level=0.0f;xenolith_transition_cells(&core,wanted_cells,0u);
        xenolith_quality_apply_digital(&quality,&core,&digital.dsp);
        digital_config_applied=digital_serial;
        if(topology_request!=topology_applied){
            xenolith_set_topology_graph_prepared(&core,&digital.topology,&digital.graph,&digital.prepared_graph);topology_applied=topology_request;
            keyboard_cells_applied=0u;
        }
        if(calculation_request!=calculation_applied){
            xenolith_set_calculation(&core,&digital.calculation);
            xenolith_set_flavour(&core,&digital.flavour);
            xenolith_set_coupling(&core,&digital.coupling);calculation_applied=calculation_request;
        }
    }
    calculation_active_snapshot=core.calculation_active||core.flavour_active||core.coupling_active;
    coupling_active_snapshot=core.flavour_active!=0u;
    transfer_active_snapshot=core.coupling_active!=0u;
    active_cells=core.cells;active_quality=quality.factor;active_core_rate=core.sample_rate;
    if(budget_ready&&keyboard_serial!=keyboard_applied&&keyboard_config[keyboard_serial&1u].cells==core.cells
        &&(!keyboard_config[keyboard_serial&1u].paraphony
            ||(digital.profile==2u&&digital.performance.voices>1u
                &&performance_applied==keyboard_config[keyboard_serial&1u].performance_serial
                &&topology_request==topology_applied
                &&(core.graph.active_growth==XTOPO_PARALLEL||core.graph.active_growth==XTOPO_SELF)))){
        X_PUBLISH_BARRIER();keyboard_audio=keyboard_config[keyboard_serial&1u];keyboard_applied=keyboard_serial;
        keyboard_cells_applied=0u;
    }
    cell_selection_apply();
    target=(count_clock_waiting||(wanted_cells==core.cells&&wanted_quality==quality.factor&&digital_serial==digital_config_applied&&(topology_request==topology_applied&&calculation_request==calculation_applied)))?1.0f:0.0f;
    if(!target){digital_rate_probe=0u;count_continuous_probe=0u;}
    seq_pitch=pitch_hz;seq_index=fm_index;seq_ratio=fm_ratio;seq_drift=drift_amount;seq_wet=exhaust_blend_target;seq_amplitude=1.0f;seq_filter_cutoff=filter.target.config.cutoff_hz;seq_filter_resonance=filter.target.config.resonance;
    keyboard_values();
    if(!modulator.config.enabled&&!envbank.enabled_mask&&!sequencer.running)core_controls_set(seq_pitch,seq_index,seq_ratio,seq_drift);
    filter_placement=filter.target.config.placement;bank_needed=envbank.enabled_mask;
    if(!bank_needed)envbank_edges=0u;
    filter_gate_target=filter_applied!=filter_serial&&filter_structural(&filter_config[filter_serial&1u],&filter.target.config)?0.0f:1.0f;
    filter_fading=filter_route_level!=1.0f||filter_gate_target==0.0f;
    if(filter_placement==XFILTER_PRE_DIGITAL)xenolith_filter_retime(&filter,xenolith_quality_filter_rate(&quality));
    for(i=0;i<n;i++){
        float mono,amplitude,dry,left,right,frame_master,wet_target=seq_wet,amp_target;
        uint32_t core_start;
        if(!(i&7u)){
            uint32_t now=plat_ticks24();
            if(i){
                uint32_t elapsed=now-chunk_start,post=elapsed>chunk_core?elapsed-chunk_core:0u;
                uint32_t cost=(chunk_core+7u)/8u;
                xenolith_governor_cost_observe(&governor_cost,post);
                if(!core_held&&cost>governor_core_cost)governor_core_cost=cost;
                if(core_held&&cost>held_quality_peak)held_quality_peak=cost;
            }
            chunk_start=now;chunk_core=0u;
            if(!core_held&&!xenolith_governor_admit_core_cost(render_start,now,render_budget,n-i,
                &governor_cost,8u*governor_core_cost,final_reserve)){
                xenolith_quality_set_core_hold(&quality,1);core_held=1u;governor_core_holds++;
                skipped_core_frames=n-i;
            }
            if(!xenolith_governor_admit_post(render_start,now,render_budget,
                governor_cost.post_chunk_peak+8u*(core_held&&held_quality_peak?held_quality_peak:governor_core_cost),final_reserve)){
                uint32_t j;int32_t l=governor_last_left,r=governor_last_right;
                governor_output_fallbacks++;
                /* Bounded emergency release; never replay a full old DMA half.
                 * This is last-resort audible degradation, not normal voice DSP. */
                for(j=i;j<n;j++){
                    l=(int32_t)((int64_t)l*31/32);r=(int32_t)((int64_t)r*31/32);
                    lr[2u*j]=l;lr[2u*j+1u]=r;
                    if(capture)analysis_ring[capture_write++&ANALYSIS_RING_MASK]=(int16_t)(xenolith_analysis_tap(l,r,channel)/256);
                }
                governor_last_left=l;governor_last_right=r;break;
            }
        }
        if(sequencer.image.config.enabled||sequencer.pending_off||seq_audio_active){
            seq_audio_tick(&mod_input);wet_target=seq_wet;
        }
        if(bank_needed){
            uint32_t gates=(held_count||midi_held_count||drone_latch?XENV_KEY_BIT:0u)
                |(envbank_sequence_gate?XENV_SEQUENCE_BIT:0u);
            xenolith_envelope_bank_sample(&envbank,envbank_edges,gates);envbank_edges=0u;
        }
        if(modulator.config.enabled&&!modulator.quantum_position){mod_input.envelope[0]=envelope.level;mod_input.envelope_mask=1u;
            {uint32_t e;for(e=0;e<3u;e++){mod_input.envelope[e+1u]=envbank.level[e];
                if(envbank.enabled_mask&(1u<<e))mod_input.envelope_mask|=2u<<e;}}}
        if(modulator.config.enabled||bank_needed){
            mod_audio_tick(&mod_input);
            wet_target=fm_clampf(wet_target+mod_smooth[4],0.0f,1.0f);
        }
        if(!precision_tick){precision_apply();
            xenolith_amplifier_set_fold_offset(&amplifier,modulator.offset[XM_DST_FOLD]+envbank.offset[XM_DST_FOLD]);
            if(folder_audio_active())amp_needed=1u;}
        precision_tick=(precision_tick+1u)&31u;
        if(!precision_tick&&filter_placement){
            float oct=modulator.offset[XM_DST_FILTER_CUTOFF]+envbank.offset[XM_DST_FILTER_CUTOFF];
            float res=modulator.offset[XM_DST_FILTER_RESONANCE]+envbank.offset[XM_DST_FILTER_RESONANCE];
            float hz=seq_filter_cutoff*(oct==0.0f?1.0f:fm_exp2f(fm_clampf(oct,-8.0f,8.0f)));
            xenolith_filter_set_cutoff(&filter,hz);xenolith_filter_set_resonance(&filter,seq_filter_resonance+res);
        }
        core_start=plat_ticks24();
        mono=filter_placement==XFILTER_PRE_DIGITAL?
            xenolith_quality_process_filtered(&quality,&core,&filter):xenolith_quality_process(&quality,&core);
        {uint32_t quality_ticks=plat_ticks24()-core_start;
            chunk_core+=quality_ticks;
            if(core_held)held_quality_ticks+=quality_ticks;
        }
        if(filter_placement==XFILTER_POST_DIGITAL)mono=xenolith_filter_process_mono(&filter,mono);
        amplitude=seq_voice_gain(xenolith_envelope_sample(&envelope));
        amp_target=fm_clampf(seq_amplitude+mod_amplitude,0.0f,1.0f);
        if(amplitude_level!=amp_target){
            amplitude_level+=(amp_target-amplitude_level)*0.004f;
            if(fm_fabsf(amp_target-amplitude_level)<0.00001f)amplitude_level=amp_target;
        }
        if(amplitude_level!=1.0f)amplitude*=amplitude_level;
        int32_t out_l,out_r,out;
        uint32_t magnitude;
        if(!xenolith_quality_ready(&quality))gate_level=0.0f;
        else gate_level+=(target-gate_level)*0.004f;
        if(count_continuous_probe){
            if(!core.count_smoothing&&!count_clock_waiting)count_continuous_probe=0u;
            else if(gate_level<count_continuous_gate_min)count_continuous_gate_min=gate_level;
        }
        master_level+=(gain-master_level)*0.004f;frame_master=filter_placement==XFILTER_POST_FX?1.0f:master_level;
        /* .8 original dry-bus trim x .2 original amp; added gate/master only. */
        dry=mono*0.16f*gate_level*amplitude;
        if(amp_needed)dry=xenolith_amplifier_process(&amplifier,dry);
        if(filter_placement==XFILTER_PRE_FX)dry=xenolith_filter_process_mono(&filter,dry);
        if(wet_ready&&!exhaust_sleeping){
            xenolith_exhaust_process(&exhaust,dry,&left,&right);
            exhaust_blend+=(wet_target-exhaust_blend)*0.004f;
            if(fm_fabsf(wet_target-exhaust_blend)<0.00001f)exhaust_blend=wet_target;
            if(exhaust_blend==0.0f){
                /* Preserve the original dry expression and rounding. */
                left=dry*frame_master;right=left;
            }else{
                left=(dry+exhaust_blend*(left-dry))*frame_master;
                right=(dry+exhaust_blend*(right-dry))*frame_master;
            }
        }else{
            /* Keep dry audio continuous while the foreground owns/clears the
             * inaudible tank. Panic has independently zeroed envelope/gate. */
            left=dry*frame_master;right=left;
        }
        if(filter_placement==XFILTER_POST_FX){xenolith_filter_process_stereo(&filter,&left,&right);left*=master_level;right*=master_level;}
        if(filter_fading){float ft=filter_gate_target;
            if(filter_route_level<ft){filter_route_level+=1.0f/221.0f;if(filter_route_level>ft)filter_route_level=ft;}
            else if(filter_route_level>ft){filter_route_level-=1.0f/221.0f;if(filter_route_level<ft)filter_route_level=ft;}
            if(filter_route_level!=1.0f){left*=filter_route_level;right*=filter_route_level;}}
        if(patch_level!=patch_target){
            patch_level+=(patch_target-patch_level)*0.01f;
            if(fm_fabsf(patch_target-patch_level)<0.00001f)patch_level=patch_target;
        }
        if(patch_level!=1.0f){left*=patch_level;right*=patch_level;}
        left=fm_clampf(left,-0.95f,0.95f);right=fm_clampf(right,-0.95f,0.95f);
        out_l=(int32_t)(left*8388607.0f);out_r=(int32_t)(right*8388607.0f);
        lr[2u*i]=out_l;lr[2u*i+1u]=out_r;
        governor_last_left=out_l;governor_last_right=out_r;
        out=xenolith_analysis_tap(out_l,out_r,channel); /* Display only. */
        if(capture)analysis_ring[capture_write++&ANALYSIS_RING_MASK]=(int16_t)(out/256);
        if(++scope_divider==8u){
            uint32_t position=scope_write;
            scope_samples[position&127u]=(int16_t)(out/256);
            scope_write=position+1u;scope_divider=0;
        }
        magnitude=(uint32_t)(out_l<0?-out_l:out_l);
        if(magnitude>peak)peak=magnitude;
        magnitude=(uint32_t)(out_r<0?-out_r:out_r);
        if(magnitude>peak)peak=magnitude;
    }
    output_peak=peak;
    {
        uint32_t now=plat_ticks24(),elapsed=now-render_start;
        if(i==n&&n&&!(n&7u)){
            uint32_t chunk=now-chunk_start;
            xenolith_governor_cost_observe(&governor_cost,chunk>chunk_core?chunk-chunk_core:0u);
            if(!core_held&&(chunk_core+7u)/8u>governor_core_cost)governor_core_cost=(chunk_core+7u)/8u;
        }
        if(elapsed>governor_max_ticks)governor_max_ticks=elapsed;
        /* Held frames still execute converter/filter work, already present in
         * elapsed. Add only the unserved remainder of the active estimate. */
        xenolith_governor_update_headroom(&governor,xenolith_governor_demand(elapsed,
            skipped_core_frames,governor_core_cost,held_quality_ticks),render_budget,n,44100.0f,
            xenolith_governor_feedback_headroom(&governor_cost,final_reserve));
        governor_load_snapshot=(float)elapsed/(float)render_budget;
        governor_stress_snapshot=governor.stress;
        /* Retain peaks through gestures; release slowly once work subsides. */
        xenolith_governor_cost_decay(&governor_cost,80u);
        governor_post_cost=(governor_cost.post_quantum_peak+31u)/32u;
        if(governor_core_cost>80u)governor_core_cost-=governor_core_cost/512u+1u;
    }
    if(digital_rate_probe&&gate_level<digital_rate_gate_min)digital_rate_gate_min=gate_level;
    active_wet_rate=exhaust.sample_rate;
    if(patch_target==0.0f&&patch_level==0.0f){X_PUBLISH_BARRIER();patch_quiet_ack=patch_request;}
    if(wet_ready&&!exhaust_sleeping&&!mod_wet_active&&!seq_wet_active&&
       !(keyboard_audio.active&&keyboard_audio.config.destination==XKEY_WET_MIX)&&exhaust_blend_target==0.0f&&exhaust_blend==0.0f){
        /* Tail is now inaudible by construction. Discard it rather than use a
         * brief output-silence heuristic that can miss energy in long delays. */
        exhaust_sleeping=1u;exhaust_reset_request++;
    }
    motion_audio_settled=!core.count_smoothing&&!quality.ramp_remaining&&digital_config_applied==digital_serial
        &&core.cells==digital.cells&&fm_fabsf(xenolith_quality_nominal_rate(&quality)-digital.dsp.effective_rate)<.5f;
    if(digital.motion_generation&&motion_audio_settled&&(!digital.motion_wet_serial||
       (int32_t)(exhaust_applied_serial-digital.motion_wet_serial)>=0))motion_ack_generation=digital.motion_generation;
    amplifier_active_snapshot=amplifier.colour!=0.0f||amplifier.remaining!=0u;
    folder_active_snapshot=folder_audio_active();
    dispersion_active_snapshot=xenolith_dispersion_active(&exhaust.dispersion)?1u:0u;
    filter_pre_snapshot=filter.target.config.placement==XFILTER_PRE_DIGITAL;
    envelope_level=envelope.level;filter_cutoff_snapshot=filter.cutoff_hz;filter_resonance_snapshot=filter.resonance;
    filter_rate_snapshot=filter.sample_rate;
    {uint32_t e;for(e=0;e<3u;e++)envbank_level_snapshot[e]=envbank.level[e];}
    {uint32_t next=effective_serial+1u;AppEffectiveControls *c=&effective_controls[next&1u];
        c->tonic=core.base;c->index=core.index;c->ratio=core.ratio;c->drift=core.drift;
        X_PUBLISH_BARRIER();effective_serial=next;}
    for(i=0;i<XM_SOURCE_COUNT;i++)mod_source_snapshot[i]=modulator.source[i];
    for(i=0;i<XM_DESTINATION_COUNT;i++)mod_offset_snapshot[i]=modulator.offset[i];
    seq_running=sequencer.running;seq_recording=sequencer.record_state;seq_playhead=sequencer.playhead;
    seq_service_snapshot(seq_record_before);
    if(capture){analysis_write=capture_write;analysis_published=capture_write;}
    for(i=0;i<8u;i++)phase_published[i]=i<core.cells?core.car_phase[i]:0u;
    phase_cells=core.cells;
    phase_serial++;
    gauge_serial++;X_PUBLISH_BARRIER();
    gauge_published.cells=core.cells<8u?core.cells:8u;
    gauge_published.rate=core.sample_rate;gauge_published.index=core.index;gauge_published.drift=core.drift;
    for(i=0;i<gauge_published.cells;i++){
        gauge_published.phase[i]=core.mod_phase[i];gauge_published.increment[i]=core.mod_increment[i];
        gauge_published.tonic[i]=core.tonic[i];gauge_published.noise[i]=core.drift_noise[i];
    }
    X_PUBLISH_BARRIER();gauge_serial++;

}

static void number_text(int32_t value,char *text)
{
    char reverse[12];int n=0;
    uint32_t positive=(uint32_t)(value<0?-value:value);
    if(value<0)*text++='-';
    do{reverse[n++]=(char)('0'+positive%10u);positive/=10u;}while(positive&&n<11);
    while(n)*text++=reverse[--n];*text=0;
}

static void fixed_text(float value,char *text)
{
    uint32_t scaled=(uint32_t)(fm_fabsf(value)*100.0f+0.5f);char *p;
    if(value<0.0f)*text++='-';number_text((int32_t)(scaled/100u),text);p=text;while(*p)p++;
    *p++='.';*p++=(char)('0'+(scaled/10u)%10u);*p++=(char)('0'+scaled%10u);*p=0;
}

static void rate_text(float hz,char *text)
{
    uint32_t whole=(uint32_t)(hz+0.5f);char *p=text;
    number_text((int32_t)(whole/1000u),text);while(*p)p++;
    *p++='.';*p++=(char)('0'+(whole/100u)%10u);*p++=(char)('0'+(whole/10u)%10u);*p++=(char)('0'+whole%10u);*p=0;
}

static void ratio_text(float value,char *text)
{
    char *p=text;int32_t scaled;
    if(value>=0.3f){fixed_text(value,text);return;}
    scaled=(int32_t)(value*1000.0f+0.5f);
    *p++='0';*p++='.';*p++=(char)('0'+scaled/100);
    *p++=(char)('0'+(scaled/10)%10);*p++=(char)('0'+scaled%10);*p=0;
}

#include "xenolith_view.inc"
#include "xenolith_boot.inc"
#include "xenolith_mod_ui.inc"
#include "xenolith_seq_ui.inc"
#include "xenolith_keyboard_ui.inc"
static void seq_record_range_error(uint32_t destination)
{
    seq_ui_status=destination==XSEQ_DEST_NONE?SEQ_UI_FULL:destination==XSEQ_DEST_INDEX?SEQ_UI_EXP_RANGE:SEQ_UI_RANGE;redraw=1u;
}
static void seq_record_capture_clear(void){seq_ui_status=SEQ_UI_OK;}
static uint32_t seq_key_destination(uint32_t destination)
{
    static const uint8_t target[XSEQ_DEST_COUNT]={XKEY_TONIC,XKEY_INDEX,XKEY_RATIO,XKEY_DRIFT,XKEY_WET_MIX,
        XKEY_CORE_RATE,XKEY_WET_RATE,XKEY_DESTINATION_COUNT,XKEY_AMPLITUDE,XKEY_CELLS,XKEY_BITS,XKEY_EXP_DEPTH,XKEY_FILTER_CUTOFF,XKEY_FILTER_RESONANCE};
    return destination<XSEQ_DEST_COUNT?target[destination]:XKEY_DESTINATION_COUNT;
}
static int seq_key_config(uint32_t destination,XenolithKeyboardConfig *config)
{
    uint32_t key=seq_key_destination(destination);
    if(key>=XKEY_DESTINATION_COUNT)return 0;
    *config=keyboard_edit;
    if(config->destination!=key){
        config->destination=key;
        xenolith_keyboard_destination_defaults(key,&config->low,&config->high,&config->response);
    }
    return 1;
}
static void seq_record_keyboard_config(XenolithKeyboardConfig *config)
{
    uint32_t i;*config=keyboard_edit;
    if(!seq_record_context_active()||seq_record_notes)return;
    for(i=0;i<4u;i++)if(((seq_direct_take?seq_direct_key_lanes:seq_record_lanes)&(1u<<i))&&seq_edit.pattern.lane[i].enabled
        &&seq_key_config(seq_edit.pattern.lane[i].destination,config))return;
}
static void seq_key_command(SeqCommand *command,uint32_t key,uint32_t note)
{
    uint32_t i;*command=(SeqCommand){0};command->type=SQ_KEY;command->a=note;command->b=1u;
    for(i=0;i<4u;i++)if(((seq_direct_take?seq_direct_key_lanes:seq_record_lanes)&(1u<<i))&&seq_edit.pattern.lane[i].enabled){
        uint32_t d=seq_edit.pattern.lane[i].destination;XenolithKeyboardConfig config;float value;
        if(!seq_key_config(d,&config))continue;
        if(d==XSEQ_DEST_ANS_CELL)value=(float)key;
        else if(config.response==XKEY_CHROMATIC)value=xenolith_key_pitch(key,keyboard_octave);
        else if(!xenolith_keyboard_map_value(&config,key,&value))continue;
        if(xenolith_sequence_value_to_normalized(d,value,&command->data.values[i]))command->index|=1u<<i;
    }
}
static uint32_t physical_recorded_note(uint32_t note)
{
    uint32_t i;for(i=0;i<27u;i++)if((seq_key_valid&(1u<<i))&&seq_key_note[i]==note)return 1u;
    return 0u;
}
static void midi_record_releases(void)
{
    uint32_t note;
    for(note=0;note<128u;note++)if(midi_recorded[note]&&!xenolith_midi_held(&midi_notes,note)){
        midi_recorded[note]=0u;
        if(!physical_recorded_note(note)&&!seq_simple(SQ_KEY,note,0u))seq_panic_serial++;
    }
}
static void midi_queue_realtime(uint32_t status,uint32_t port)
{
    uint32_t write;
    if(status==0xfau||status==0xfbu){
        if(!seq_edit.config.enabled)return;
        midi_clock_port=port;
    }else if(port!=midi_clock_port)return;
    write=midi_rt_write;
    if(write-midi_rt_read>=64u){midi_rt_dropped++;midi_rt_reset++;midi_clock_port=XMIDI_EMPTY;return;}
    midi_realtime[write&63u]=(MidiRealtime){status,plat_ms(),midi_rt_generation};
    X_PUBLISH_BARRIER();midi_rt_write=write+1u;
}
/* The platform assembles USB-MIDI packets for USB and TRS. Process a bounded
 * foreground batch; note ownership and allocations never run in an IRQ. */
static void midi_input(void)
{
    uint32_t packet,limit=0u,before=midi_seen_revision,old=midi_held_count,pressed=0u;
    uint32_t link=plat_midi_out_link();int port;
    if(link!=midi_usb_link){
        midi_usb_link=link;xenolith_midi_clear_port(&midi_notes,XMIDI_USB);
        if(midi_clock_port==XMIDI_USB){midi_clock_port=XMIDI_EMPTY;midi_rt_reset++;}
    }
    while(limit++<32u&&plat_midi_in(&packet,&port)){
        XenolithMidiEvent event;uint32_t input_port;
        if(port!=PORT_USB&&port!=PORT_TRS)continue;
        input_port=port==PORT_USB?XMIDI_USB:XMIDI_TRS;
        event=xenolith_midi_packet(&midi_notes,packet,input_port);
        if(event.type==XMIDI_REALTIME){midi_queue_realtime(event.status,input_port);continue;}
        if(event.type==XMIDI_NOTE_ON){
            pressed=1u;
            if(!midi_virtual_keys){midi_virtual_keys=1u;xenolith_paraphony_reset(&paraphony_allocator);}
            /* Real MIDI pitch reaches the tonic capture path directly. CV
             * keyboard mappings remain the explicitly assigned panel keys. */
            if(seq_edit.config.enabled){
                SeqCommand command={0};command.type=SQ_KEY;command.a=event.note;command.b=1u;
                if(seq_send(&command))midi_recorded[event.note]=1u;else seq_ui_status=SEQ_UI_FULL;
            }
            midi_held_count=midi_notes.midi_count;
            pitch_hz=midi_note_pitch(event.note);
            mod_key=fm_clampf(((float)event.note-66.0f)/24.0f,-1.0f,1.0f);
            drone_latch=0u;panic_latched=0u;running=1u;envelope_event(1u);
        }
    }
    midi_held_count=midi_notes.midi_count;
    midi_record_releases();
    if(before!=midi_notes.revision){
        uint32_t identity=xenolith_midi_latest(&midi_notes);
        if(midi_held_count||old||pressed){
            if(identity>=27u&&identity<XMIDI_IDENTITIES)pitch_hz=midi_identity_pitch(identity);
            else if(identity<27u&&keyboard_edit.destination==XKEY_TONIC&&keyboard_edit.response==XKEY_CHROMATIC)
                pitch_hz=midi_identity_pitch(identity);
        }
        keyboard_dirty=1u;redraw=1u;
    }
    midi_seen_revision=midi_notes.revision;
    if((old||pressed)&&!midi_held_count&&!live_note_gate()&&!drone_latch&&!seq_running){running=0u;envelope_event(0u);}
}
#include "xenolith_topology_ui.inc"

/* ARP's record entrance reuses scalar CV lanes. It never replaces an occupied
 * lane to make space, and retargeting elsewhere never rewrites stored values. */
static uint32_t seq_keyboard_destination(void)
{
    return keyboard_sequence_destination(keyboard_edit.destination);
}
static int seq_lane_for_destination(uint32_t d)
{
    uint32_t i;int empty=-1,matching=-1;
    if(d==XSEQ_DEST_NONE)return -1;
    for(i=0;i<4u;i++){
        const XenolithSequenceLane *l=&seq_edit.pattern.lane[i];
        if(l->destination==d){if(l->enabled)return (int)i;if(matching<0)matching=(int)i;}
        if(empty<0&&!l->enabled&&!seq_lane_has_steps(&seq_edit.pattern,i))empty=(int)i;
    }
    return matching>=0?matching:empty;
}
static int seq_keyboard_lane(void){return seq_lane_for_destination(seq_keyboard_destination());}
static void seq_record_arm_destination(uint32_t destination)
{
    SeqCommand c={0};int lane=seq_lane_for_destination(destination);
    if(lane<0){seq_ui_status=SEQ_UI_BUSY;return;}
    if((uint32_t)(seq_write-seq_read)>62u){seq_ui_status=SEQ_UI_FULL;return;}
    c.type=SQ_LANE;c.index=(uint32_t)lane;c.data.lane=seq_edit.pattern.lane[lane];
    c.data.lane.destination=destination;c.data.lane.enabled=1u;
    if(!seq_ui_propose(&c))return;
    seq_record_notes=0u;seq_record_lanes=1u<<(uint32_t)lane;seq_ui_object=(uint32_t)lane+2u;seq_ui_page=0u;
    seq_simple(SQ_RECORD,seq_record_mode,seq_record_lanes<<1);
}
static void seq_keyboard_record_text(char *out)
{
    int lane;
    if(!seq_edit.config.enabled){mod_ui_copy(out,"REC > FIRST KEY");return;}
    if(seq_recording){
        if(seq_record_notes){mod_ui_copy(out,"REC TONIC / STOP");return;}
        mod_ui_copy(out,"REC CV ");
        for(lane=0;lane<4;lane++)if(seq_record_lanes&(1u<<(uint32_t)lane)){
            char digit[2]={(char)('1'+lane),0};keyboard_ui_append(out,digit);
        }
        keyboard_ui_append(out," / STOP");return;
    }
    if(seq_running&&seq_record_mode==XSEQ_RECORD_STEP){mod_ui_copy(out,"STOP FOR STEP RECORD");return;}
    if(keyboard_edit.response==XKEY_CHROMATIC){mod_ui_copy(out,"REC > TONIC NOTES");return;}
    lane=seq_keyboard_lane();
    if(lane<0){mod_ui_copy(out,"REC: NO FREE CV LANE");return;}
    mod_ui_copy(out,"REC > CV ");out[9]=(char)('1'+lane);out[10]=0;
    keyboard_ui_append(out," / ");keyboard_ui_append(out,KEY_UI_DEST[keyboard_edit.destination]);
}
static void seq_keyboard_record(void)
{
    if(seq_recording){seq_simple(SQ_RECORD,XSEQ_RECORD_OFF,0u);return;}
    if(seq_running&&seq_record_mode==XSEQ_RECORD_STEP){seq_ui_status=SEQ_UI_RECORD_LOCK;return;}
    /* Reserve both lane edit and arm; no half accepted routing on a full queue. */
    if((uint32_t)(seq_write-seq_read)>62u){seq_ui_status=SEQ_UI_FULL;return;}
    if(keyboard_edit.response==XKEY_CHROMATIC){
        seq_record_notes=1u;seq_record_lanes=0u;seq_ui_object=1u;
    }else{seq_record_arm_destination(seq_keyboard_destination());return;}
    seq_ui_page=0u;
    seq_simple(SQ_RECORD,seq_record_mode,seq_record_notes|(seq_record_lanes<<1));
}
static void seq_record_context(void)
{
    if(seq_record_context_active()){
        if(!seq_simple(SQ_RECORD,XSEQ_RECORD_OFF,0u)){seq_ui_status=SEQ_UI_FULL;return;}
        if(seq_direct_take)seq_record_lanes=seq_direct_key_lanes;
        seq_direct_take=0u;seq_ui_status=SEQ_UI_OK;return;
    }
    if(seq_running&&seq_record_mode==XSEQ_RECORD_STEP){seq_ui_status=SEQ_UI_RECORD_LOCK;return;}
    if(keyboard_page){seq_keyboard_record();return;}
    if(sequence_page&&seq_ui_object>=2u){
        uint32_t lane=seq_ui_object-2u;SeqCommand c={0};
        if(seq_edit.pattern.lane[lane].destination==XSEQ_DEST_NONE){seq_ui_status=SEQ_UI_TARGET;return;}
        if((uint32_t)(seq_write-seq_read)>62u){seq_ui_status=SEQ_UI_FULL;return;}
        c.type=SQ_LANE;c.index=lane;c.data.lane=seq_edit.pattern.lane[lane];c.data.lane.enabled=1u;
        if(!seq_ui_propose(&c))return;
        seq_record_notes=0u;seq_record_lanes=1u<<lane;
    }else if(sequence_page&&seq_ui_object==1u){seq_record_notes=1u;seq_record_lanes=0u;}
    else if(!sequence_page&&seq_last_control_destination==XSEQ_DEST_EXP_DEPTH&&topology_edit.mode==XTOPO_EXPONENTIAL){
        seq_record_arm_destination(XSEQ_DEST_EXP_DEPTH);return;
    }
    seq_simple(SQ_RECORD,seq_record_mode,seq_record_notes|(seq_record_lanes<<1));
}
static int seq_direct_capture_lane(uint32_t destination)
{
    int lane=seq_lane_for_destination(destination);SeqCommand c={0};uint32_t bit;
    if(lane<0){seq_ui_status=SEQ_UI_BUSY;return 0;}
    bit=1u<<(uint32_t)lane;
    if(seq_record_lanes&bit)return 1;
    if((uint32_t)(seq_write-seq_read)>61u){seq_ui_status=SEQ_UI_FULL;return 0;}
    c.type=SQ_LANE;c.index=(uint32_t)lane;c.data.lane=seq_edit.pattern.lane[lane];c.data.lane.destination=destination;c.data.lane.enabled=1u;
    if(!seq_ui_propose(&c))return 0;
    seq_record_lanes|=bit;seq_simple(SQ_RECORD,XSEQ_RECORD_STEP,seq_record_notes|(seq_record_lanes<<1));return 1;
}
static void seq_start_direct_take(void)
{
    SeqCommand c={0};uint32_t destination=XSEQ_DEST_NONE,notes=keyboard_edit.response==XKEY_CHROMATIC;
    if(sequence_page&&seq_ui_object>=2u&&seq_edit.pattern.lane[seq_ui_object-2u].destination!=XSEQ_DEST_NONE){
        destination=seq_edit.pattern.lane[seq_ui_object-2u].destination;notes=0u;
    }else if(sequence_page&&seq_ui_object==1u)notes=1u;
    else if(keyboard_page){if(!notes)destination=seq_keyboard_destination();}
    else if(seq_last_control_destination!=XSEQ_DEST_NONE){destination=seq_last_control_destination;notes=0u;}
    else if(!notes)destination=seq_keyboard_destination();
    if(!notes&&seq_lane_for_destination(destination)<0){seq_ui_status=SEQ_UI_BUSY;return;}
    if((uint32_t)(seq_write-seq_read)>61u){seq_ui_status=SEQ_UI_FULL;return;}
    c.type=SQ_CONFIG;c.data.config=seq_edit.config;c.data.config.enabled=1u;
    if(!seq_ui_propose(&c))return;
    if(seq_record_mode==XSEQ_RECORD_OFF)seq_record_mode=XSEQ_RECORD_REPLACE;
    if(notes){seq_record_notes=1u;seq_record_lanes=0u;seq_simple(SQ_RECORD,seq_record_mode,1u);}
    else seq_record_arm_destination(destination);
    seq_direct_take=seq_record_mode==XSEQ_RECORD_STEP;seq_direct_key_lanes=seq_record_lanes;
}

void app_say(const char *a,const char *b)
{
    (void)b;
    if(ann_on())ann_say(a,0,ANN_NOW);
    redraw=1u;
}

/* Keep an off-grid lower endpoint without shifting every later detent.
 * Positive movement takes the next grid point; negative movement the previous.
 * A small tolerance absorbs float representation error at exact grid points. */
static float grid_step(float value,int32_t step,int32_t divisions,float low,float high)
{
    float scaled=value*(float)divisions;
    float rounded=scaled+(step>0?0.0001f:-0.0001f);
    int32_t units=(int32_t)rounded;
    int32_t limit=(int32_t)(high*(float)divisions)+1;
    if(step==0)return value;
    if(step>=limit)return high;
    if(step<=-limit)return low;
    if(step>0&&rounded<(float)units)units--;
    if(step<0&&rounded>(float)units)units++;
    return fm_clampf((float)(units+step)/(float)divisions,low,high);
}

/* SELECT is VCA's own entrance/exit only on HOME. Other section actions keep
 * their local meaning, including the modulation bypass and sequence pages. */
static int vca_select(void)
{
    if(envelope_page==2u||envelope_page==3u)envelope_page=envelope_page==2u?3u:0u;
    else if(!keyboard_page&&!sequence_page&&!modulation_page&&!patch_page
        &&!output_page&&!exhaust_page&&!analysis_page&&!envelope_page&&!topology_page)envelope_page=2u;
    else return 0;
    redraw=1u;return 1;
}
static float amplifier_step(float value,int32_t step,float low,float high)
{
    float scaled=value*20.0f;
    int32_t units=(int32_t)(scaled+(scaled<0.0f?-0.5f:0.5f));
    return fm_clampf((float)(units+clamp(step,-100,100))*0.05f,low,high);
}

static float panel_log_step(float value,int32_t step,float low,float high,uint32_t zero)
{
    if(!step)return value;
    if(zero&&value<=0.0f){if(step<=0)return 0.0f;value=low;step--;}
    if(zero&&value<=low&&step<0)return 0.0f;
    return fm_clampf(fm_clampf(value,low,high)*fm_exp2f((float)clamp(step,-2048,2048)/96.0f),low,high);
}
static int vca_turn(uint32_t knob,int32_t step)
{
    if(envelope_page==2u){
        if(knob==0u)amplifier_edit.drive=amplifier_step(amplifier_edit.drive,step,0.0f,1.0f);
        else if(knob==1u)amplifier_edit.bias=amplifier_step(amplifier_edit.bias,step,-0.75f,0.75f);
        else if(knob==2u)amplifier_edit.colour=amplifier_step(amplifier_edit.colour,step,0.0f,1.0f);
        else if(knob==3u)amplifier_feedback_edit=grid_step(amplifier_feedback_edit,clamp(step,-100,100),100,0.0f,1.0f);
        else return 0;
    }else if(envelope_page==3u){
        if(knob==0u)folder_edit.amount=grid_step(folder_edit.amount,clamp(step,-100,100),100,0.0f,1.0f);
        else if(knob==1u)folder_edit.bias=grid_step(folder_edit.bias,clamp(step,-100,100),100,-1.0f,1.0f);
        else if(knob==2u)folder_edit.stages=mod_ui_enum(folder_edit.stages,step,4u);
        else if(knob==3u)folder_edit.mix=grid_step(folder_edit.mix,clamp(step,-100,100),100,0.0f,1.0f);
        else return 0;
    }else return 0;
    return 1;
}
static void exhaust_damp_turn(int32_t step)
{
    float wet=exhaust_config[exhaust_config_serial&1u].sample_rate;
    float cap=fm_clampf(0.45f*wet,20.0f,16000.0f);
    if(!exhaust_damp_model)exhaust_requested_damp_hz=exhaust_damp_hz;
    exhaust_requested_damp_hz=panel_log_step(exhaust_requested_damp_hz,step,20.0f,cap,0u);
    exhaust_damp_model=1u;
}

static float wet_rate_step(float rate,int32_t step)
{
    int32_t count=clamp(step,-512,512);
    while(count!=0){
        float grid=(rate>10000.0f||(rate==10000.0f&&count>0))?250.0f:125.0f;
        float units=rate/grid;int32_t base=(int32_t)units;
        if(count>0){rate=(float)(base+1)*grid;count--;}
        else{if(units-(float)base<0.00001f)base--;rate=(float)base*grid;count++;}
        rate=fm_clampf(rate,1378.125f,44100.0f);
    }
    return rate;
}

#include "xenolith_filter_env_ui.inc"
#include "xenolith_home_detail.inc"
#include "xenolith_coupling_ui.inc"
#include "xenolith_deep_ui.inc"
#include "xenolith_governor_ui.inc"

/* Bounded foreground observation; fields may span adjacent audio blocks.
 * LC6 has no write access and does not alter the update-loader protocol. */
#define APP_DIAGNOSTIC_SNAPSHOT 1
uint32_t app_diagnostic_snapshot(uint32_t *v,uint32_t capacity)
{
    fm_bits_t b;if(!v||capacity<32u)return 0u;
    v[0]=running;v[1]=drone_latch;
#define SNAP_F(i,x) do{b.f=(x);v[i]=b.u;}while(0)
    SNAP_F(2,envelope.level);SNAP_F(3,gate_level);
    v[4]=requested_cells;v[5]=core.cells;v[6]=core.render_cells;
    SNAP_F(7,quality.nominal_rate);SNAP_F(8,core.sample_rate);
    v[9]=quality.factor;v[10]=quality.ramp_remaining;
    v[11]=digital_config_serial;v[12]=digital_config_applied;
    v[13]=motion_owner;v[14]=motion_generation;v[15]=motion_ack_generation;v[16]=motion_audio_settled;
    v[17]=mod_config_serial;v[18]=mod_config_applied;v[19]=modulator.config.enabled;
    v[20]=governor_core_holds;v[21]=governor_output_fallbacks;
    v[22]=governor_post_cost;v[23]=governor_core_cost;
    SNAP_F(24,quality.raw_held);v[25]=quality.core_hold;
    SNAP_F(26,amplifier.colour);SNAP_F(27,amplifier.folder.amount);SNAP_F(28,amplifier.folder.offset);
    SNAP_F(29,governor_load_snapshot);SNAP_F(30,governor_stress_snapshot);
    v[31]=workload_profile;
#undef SNAP_F
    return 32u;
}

static void input(void)
{
    uint32_t buttons,pressed,k;
    uint32_t previous_topology_page=topology_page;
    uint32_t keys=plat_keys();
    plat_button_snapshot(&buttons,&pressed);
    uint32_t wet_changed=0,env_changed=0,amp_changed=0,digital_changed=0,new_keys=keys&~previous_keys;
    uint32_t old_count=held_count,old_top=held_count?held_order[held_count-1u]:27u,write=0;
    int32_t step;
    XenolithKeyboardConfig live_key;
    uint32_t panic_chord=((pressed&(1u<<B_REC))&&(buttons&(1u<<B_HOME)))||
        ((pressed&(1u<<B_HOME))&&(buttons&(1u<<B_REC)));
    uint32_t governor_chord=!panic_chord&&(pressed&(1u<<B_REC))&&(buttons&(1u<<B_FX));
    if(pressed&((1u<<B_HOME)|(1u<<B_SAVE)|(1u<<B_EDIT)|(1u<<B_LFO)|(1u<<B_FX)|(1u<<B_ENV)|(1u<<B_GLO)|(1u<<B_SEQ)|(1u<<B_ARP)))governor_page=0u;
    if(pressed&((1u<<B_HOME)|(1u<<B_SAVE)|(1u<<B_EDIT)|(1u<<B_LFO)|(1u<<B_FX)|(1u<<B_ENV)|(1u<<B_GLO)|(1u<<B_SEQ)|(1u<<B_ARP))){topology_page=0u;home_detail_page=0u;}
    /* Retain press order across page changes. Simultaneous presses resolve
     * deterministically to the highest key; releases never retrigger a hold. */
    for(k=0;k<held_count;k++)if(keys&(1u<<held_order[k]))held_order[write++]=held_order[k];
    held_count=write;
    for(k=0;k<NKEYS;k++)if(new_keys&(1u<<k))held_order[held_count++]=(uint8_t)k;
    xenolith_midi_physical(&midi_notes,keys);
    if((pressed&(1u<<B_PLAY))&&!((pressed&(1u<<B_REC))&&!seq_edit.config.enabled)){
        midi_clock_port=XMIDI_EMPTY;midi_rt_generation++;
        if(seq_direct_take){seq_record_lanes=seq_direct_key_lanes;seq_direct_take=0u;}
        if(patch_page)patch_begin(1u);
        else if(seq_edit.config.enabled){
            seq_simple(SQ_PLAY,0u,0u);drone_latch=0u;panic_latched=0u;running=!seq_running||live_note_gate();
        }
        else{
            drone_latch=!drone_latch;panic_latched=0u;
            if(drone_latch){running=1u;envelope_event(1u);}
            else if(!live_note_gate()){running=0u;envelope_event(0u);}
        }
        redraw=1u;
    }
    if(pressed&(1u<<B_HOME)){
        home_hold_started=plat_ms();home_hold_done=0u;
        keyboard_page=0u;
        /* Navigation only: never reset controls, notes, envelope or wet tail. */
        output_page=0u;exhaust_page=0u;envelope_page=0u;analysis_page=0u;patch_page=0u;patch_save_armed=0u;modulation_page=0u;sequence_page=0u;redraw=1u;
    }
    if(pressed&(1u<<B_SAVE)){
        keyboard_page=0u;
        modulation_page=0u;sequence_page=0u;
        if(patch_page&&patch_save_armed)patch_begin(2u);
        else{patch_page=1u;patch_save_armed=1u;patch_status=0u;}
        analysis_page=0u;output_page=0u;exhaust_page=0u;envelope_page=0u;redraw=1u;
    }
    if(pressed&(1u<<B_EDIT)){
        edit_hold_started=plat_ms();edit_hold_done=0u;
        keyboard_page=0u;
        modulation_page=0u;sequence_page=0u;
        patch_page=0u;patch_save_armed=0u;
        output_page=analysis_page?(output_page?0u:1u):(output_page==2u?3u:output_page==3u?0u:2u);
        exhaust_page=0u;envelope_page=0u;
        if(previous_topology_page){
            topology_page=previous_topology_page==1u?2u:1u;
            output_page=analysis_page=0u;edit_hold_done=1u;
            graph_select_pending=0u;
        }
        if(!output_page&&analysis_page)xv_reset();
        redraw=1u;
    }
    if(pressed&(1u<<B_LFO)){
        lfo_hold_started=plat_ms();lfo_hold_done=0u;keyboard_page=0u;
        if(modulation_page)mod_ui_next_page();else modulation_page=1u;
        sequence_page=0u;
        output_page=0u;exhaust_page=0u;envelope_page=0u;analysis_page=0u;patch_page=0u;patch_save_armed=0u;
        redraw=1u;
    }
    if(pressed&(1u<<B_FX)){
        fx_hold_started=plat_ms();fx_hold_done=0u;keyboard_page=0u;
        modulation_page=0u;sequence_page=0u;
        patch_page=0u;patch_save_armed=0u;
        exhaust_page=exhaust_page==3u?1u:(exhaust_page+1u)%3u;envelope_page=0;analysis_page=0;output_page=0;redraw=1u;
    }
    if(pressed&(1u<<B_ENV)){
        keyboard_page=0u;
        modulation_page=0u;sequence_page=0u;
        patch_page=0u;patch_save_armed=0u;
        env_hold_started=plat_ms();env_hold_done=0u;
        envelope_page=envelope_page==4u?1u:envelope_page==1u?0u:1u;exhaust_page=0;analysis_page=0;output_page=0;redraw=1u;
    }
    if(pressed&(1u<<B_SEL)){
        if(governor_page){governor_page=governor_page==1u?2u:governor_page==2u?1u:governor_page==3u?4u:3u;redraw=1u;}
        else if(topology_page==1u){graph_select_started=plat_ms();graph_select_pending=1u;graph_select_done=0u;redraw=1u;}
        else if(topology_page==2u){}
        else if(keyboard_page)keyboard_ui_select();
        else if(sequence_page)seq_ui_select();
        else if(modulation_page)mod_ui_select();
        else if(exhaust_page&&exhaust_page!=3u){
            exhaust_reconstruction=xenolith_exhaust_structure_mode(xenolith_exhaust_structure_family(exhaust_reconstruction),
                !xenolith_exhaust_structure_reconstruction(exhaust_reconstruction));wet_changed=1u;redraw=1u;
        }
        else if(output_page==2u){core_rate_response=(core_rate_response+1u)%XRATE_RESPONSE_COUNT;redraw=1u;}
        else if(output_page==3u){performance_edit.filter_response=(performance_edit.filter_response+1u)%XCTRL_FILTER_COUNT;performance_serial++;digital_publish();redraw=1u;}
        else if(home_detail_page){home_detail_page=0u;redraw=1u;}
        else if(envelope_page==1u||envelope_page==4u||output_page>=4u){}
        else if(vca_select()){}
        else{lights_enabled=!lights_enabled;lights_reset_pending=1u;}
    }
    if(pressed&(1u<<B_GLO)){
        glo_hold_started=plat_ms();glo_hold_done=0u;
        keyboard_page=0u;
        modulation_page=0u;sequence_page=0u;
        patch_page=0u;patch_save_armed=0u;
        analysis_page=!analysis_page;output_page=0;
        exhaust_page=0;envelope_page=0;
        analysis_capture_enabled=analysis_page;
        if(analysis_page)xv_reset();
        redraw=1u;
    }
    if(pressed&(1u<<B_SEQ)){
        keyboard_page=0u;
        sequence_page=!sequence_page;modulation_page=0u;
        output_page=0u;exhaust_page=0u;envelope_page=0u;analysis_page=0u;patch_page=0u;patch_save_armed=0u;redraw=1u;
    }
    if(pressed&(1u<<B_ARP)){
        keyboard_page=!keyboard_page;sequence_page=0u;modulation_page=0u;
        output_page=0u;exhaust_page=0u;envelope_page=0u;analysis_page=0u;patch_page=0u;patch_save_armed=0u;redraw=1u;
    }
    if(graph_select_pending){
        if(topology_page!=1u){graph_select_pending=0u;}
        else if(!(buttons&(1u<<B_SEL))){
            if(!graph_select_done){graph_scope_turn(graph_edit.scope==XGRAPH_ADD?1:-1);digital_changed=1u;}
            graph_select_pending=0u;redraw=1u;
        }else if(!graph_select_done&&graph_edit.scope==XGRAPH_ALL&&plat_ms()-graph_select_started>=650u){
            graph_scope_commit();graph_select_done=1u;digital_changed=1u;
        }else if(!graph_select_done&&graph_edit.scope==XGRAPH_ALL)redraw=1u;
    }
    if((buttons&(1u<<B_HOME))&&!(buttons&(1u<<B_REC))&&!home_hold_done&&plat_ms()-home_hold_started>=650u){
        home_hold_done=1u;topology_page=1u;redraw=1u;
    }
    if((buttons&(1u<<B_ENV))&&!env_hold_done&&plat_ms()-env_hold_started>=650u){
        env_hold_done=1u;envelope_page=4u;output_page=exhaust_page=analysis_page=0u;redraw=1u;
    }
    if((buttons&(1u<<B_LFO))&&!lfo_hold_done&&plat_ms()-lfo_hold_started>=650u){
        lfo_hold_done=1u;if(modulation_page)mod_ui_hold();
    }
    if((buttons&(1u<<B_FX))&&!fx_hold_done&&plat_ms()-fx_hold_started>=650u){
        fx_hold_done=1u;exhaust_page=3u;output_page=envelope_page=analysis_page=0u;redraw=1u;
    }
    if((buttons&(1u<<B_EDIT))&&!edit_hold_done&&plat_ms()-edit_hold_started>=650u){
        edit_hold_done=1u;output_page=4u;analysis_page=exhaust_page=envelope_page=0u;redraw=1u;
    }
    if((buttons&(1u<<B_GLO))&&!glo_hold_done&&plat_ms()-glo_hold_started>=650u){
        glo_hold_done=1u;output_page=5u;analysis_page=exhaust_page=envelope_page=0u;redraw=1u;
    }
    if(governor_chord){
        governor_page=1u;fx_hold_done=1u;
        governor_rec_done=1u;
        output_page=exhaust_page=envelope_page=analysis_page=patch_page=modulation_page=sequence_page=keyboard_page=topology_page=0u;
        redraw=1u;
    }
    if(governor_page&&!governor_chord&&(pressed&(1u<<B_REC))){governor_rec_started=plat_ms();governor_rec_done=0u;}
    if(governor_page&&(buttons&(1u<<B_REC))&&!governor_rec_done&&plat_ms()-governor_rec_started>=650u){
        governor_rec_done=1u;governor_page=governor_page<3u?3u:1u;redraw=1u;
    }
    seq_record_keyboard_config(&live_key);
    step=(int32_t)!!(pressed&(1u<<B_OCTUP))-(int32_t)!!(pressed&(1u<<B_OCTDN));
    if(step&&live_key.destination!=XKEY_CELLS){
        int32_t next=xenolith_octave_step(keyboard_octave,step);
        if(next!=keyboard_octave){
            /* Keep physical held-key identities and envelope events intact.
             * Retune the current tonic, including any live K1 adjustment. */
            if(live_key.destination==XKEY_TONIC&&live_key.response==XKEY_CHROMATIC)
                pitch_hz=xenolith_octave_pitch(pitch_hz,next-keyboard_octave);
            keyboard_octave=next;redraw=1u;
        }
    }
    for(k=0;k<NKEYS;k++){
        if(seq_edit.config.enabled){
            uint32_t midi=(uint32_t)clamp(53+(int32_t)k+12*keyboard_octave,0,127);
            if(new_keys&(1u<<k)){SeqCommand c;seq_key_command(&c,k,midi);seq_key_note[k]=(uint8_t)midi;seq_key_valid|=1u<<k;if(!seq_send(&c))seq_ui_status=SEQ_UI_FULL;}
        }
        if((previous_keys&~keys)&seq_key_valid&(1u<<k)){
            uint32_t note=seq_key_note[k];seq_key_valid&=~(1u<<k);
            if(!physical_recorded_note(note)&&!(midi_recorded[note]&&xenolith_midi_held(&midi_notes,note)))seq_simple(SQ_KEY,note,0u);
        }
    }
    if(new_keys&&!(pressed&(1u<<B_REC))){
        k=held_order[held_count-1u];
        if(live_key.destination==XKEY_TONIC&&live_key.response==XKEY_CHROMATIC)pitch_hz=xenolith_key_pitch(k,keyboard_octave);
        mod_key=fm_clampf(((float)k-13.0f+12.0f*(float)keyboard_octave)/24.0f,-1.0f,1.0f);
        if(keyboard_edit.gate){drone_latch=0u;panic_latched=0u;running=1u;envelope_event(1u);}
        else{X_PUBLISH_BARRIER();mod_key_event_serial++;}
        redraw=1u;
    }else if(held_count&&old_top!=held_order[held_count-1u]){
        k=held_order[held_count-1u];
        if(live_key.destination==XKEY_TONIC&&live_key.response==XKEY_CHROMATIC)pitch_hz=xenolith_key_pitch(k,keyboard_octave);
        redraw=1u;
    }
    if(old_count&&held_count==0u&&keyboard_edit.gate&&!midi_held_count&&!drone_latch&&!panic_latched&&!seq_running){
        running=0u;envelope_event(0u);redraw=1u;
    }
    midi_input();
    if(panic_chord)app_panic();
    else if((pressed&(1u<<B_REC))&&!governor_chord&&!governor_page){
        if(seq_edit.config.enabled){
            if(seq_record_mode==XSEQ_RECORD_STEP&&(pressed&(1u<<B_PLAY)))seq_ui_status=SEQ_UI_RECORD_LOCK;
            else seq_record_context();
            redraw=1u;
        }
        else seq_start_direct_take();
    }
    if((step=plat_enc(EN_K1))!=0&&!patch_page){
        if(governor_page)governor_ui_turn(0u,step);
        else if(topology_page==2u)coupling_ui_turn(EN_K1,step);
        else if(topology_page)topology_ui_turn(EN_K1,step);
        else if(home_detail_page)home_detail_turn(0u,step);
        else if(keyboard_page)keyboard_ui_turn(EN_K1,step);
        else if(sequence_page)seq_ui_turn(EN_K1,step);
        else if(modulation_page)mod_ui_turn(EN_K1,step);
        else if(output_page>=4u)deep_ui_turn(0u,step);
        else if(output_page==3u)filter_ui_turn(0u,step);
        else if(output_page==2u){
            core_max_rate=xenolith_rate_response_step(core_max_rate,step,core_rate_response);digital_changed=1u;
            seq_capture_control(XSEQ_DEST_CORE_RATE,core_max_rate);
        }else if(output_page){xv_span_index=(uint32_t)clamp((int32_t)xv_span_index+clamp(step,-3,3),0,3);xv_history_count=0;if(analyzer.valid)xv_history();}
        else if(envelope_page==2u||envelope_page==3u)amp_changed|=(uint32_t)vca_turn(0u,step);
        else if(envelope_page)env_ui_turn(0u,step);
        else if(exhaust_page){
            if(exhaust_page==3u){exhaust_diffusion=grid_step(exhaust_diffusion,step,100,0,.85f);wet_changed=1u;}
            else if(exhaust_page==2u){
                exhaust_rate=wet_rate_step(exhaust_rate,step);
                seq_capture_control(XSEQ_DEST_WET_RATE,exhaust_rate);
            }else exhaust_time=xenolith_exhaust_echo_step(exhaust_time,step,exhaust_rate);
            wet_changed=1;
        }
        else{pitch_hz=xcontrol_home_step(XCTRL_TONIC,pitch_hz,step,performance_edit.curve[0],performance_edit.sensitivity[0],0u);
            seq_capture_control(XSEQ_DEST_PITCH,pitch_hz);}
        redraw=1u;
    }
    if((step=plat_enc(EN_K2))!=0&&!patch_page){
        if(governor_page)governor_ui_turn(1u,step);
        else if(topology_page==2u)coupling_ui_turn(EN_K2,step);
        else if(topology_page)topology_ui_turn(EN_K2,step);
        else if(home_detail_page)home_detail_turn(1u,step);
        else if(keyboard_page)keyboard_ui_turn(EN_K2,step);
        else if(sequence_page)seq_ui_turn(EN_K2,step);
        else if(modulation_page)mod_ui_turn(EN_K2,step);
        else if(output_page>=4u)deep_ui_turn(1u,step);
        else if(output_page==3u)filter_ui_turn(1u,step);
        else if(output_page==2u){
            int32_t bits=core_bits?(int32_t)core_bits:24;
            bits=clamp(bits+clamp(step,-23,23),1,24);
            core_bits=bits==24?0u:(uint32_t)bits;digital_changed=1u;
            seq_capture_control(XSEQ_DEST_BITS,(float)bits);
        }else if(output_page){analysis_channel=(uint32_t)clamp((int32_t)analysis_channel+clamp(step,-2,2),0,2);xv_reset();lights_reset_pending=1u;}
        else if(envelope_page==2u||envelope_page==3u)amp_changed|=(uint32_t)vca_turn(1u,step);
        else if(envelope_page)env_ui_turn(1u,step);
        else if(exhaust_page){
            /* Bounded event before negation also handles INT32_MIN safely. */
            if(exhaust_page==3u)dispersion_edit.amount_a=grid_step(dispersion_edit.amount_a,step,100,0,1);
            else if(exhaust_page==2u)exhaust_reconstruction=xenolith_exhaust_structure_mode(
                mod_ui_enum(xenolith_exhaust_structure_family(exhaust_reconstruction),step,5u),
                xenolith_exhaust_structure_reconstruction(exhaust_reconstruction));
            else exhaust_damp_turn(step);
            wet_changed=1;
        }
        else{fm_index=xcontrol_home_step(XCTRL_INDEX,fm_index,step,performance_edit.curve[1],performance_edit.sensitivity[1],topology_edit.mode==XTOPO_EXPONENTIAL);
            seq_capture_control(topology_edit.mode==XTOPO_EXPONENTIAL?XSEQ_DEST_EXP_DEPTH:XSEQ_DEST_INDEX,fm_index);}
        redraw=1u;
    }
    if((step=plat_enc(EN_K3))!=0&&!patch_page){
        if(governor_page)governor_ui_turn(2u,step);
        else if(topology_page==2u)coupling_ui_turn(EN_K3,step);
        else if(topology_page)topology_ui_turn(EN_K3,step);
        else if(home_detail_page)home_detail_turn(2u,step);
        else if(keyboard_page)keyboard_ui_turn(EN_K3,step);
        else if(sequence_page)seq_ui_turn(EN_K3,step);
        else if(modulation_page)mod_ui_turn(EN_K3,step);
        else if(output_page>=4u)deep_ui_turn(2u,step);
        else if(output_page==3u)filter_ui_turn(2u,step);
        else if(output_page==2u){
            core_quantizer=(uint32_t)clamp((int32_t)core_quantizer+clamp(step,-2,2),0,2);
            digital_changed=1u;
        }else if(output_page)xv_rate_index=(uint32_t)clamp((int32_t)xv_rate_index+step,0,3);
        else if(envelope_page==2u||envelope_page==3u)amp_changed|=(uint32_t)vca_turn(2u,step);
        else if(envelope_page)env_ui_turn(2u,step);
        else if(exhaust_page){
            if(exhaust_page==3u)dispersion_edit.amount_b=grid_step(dispersion_edit.amount_b,step,100,0,1);
            else if(exhaust_page==2u)exhaust_diffusion=grid_step(exhaust_diffusion,step,100,0.0f,0.85f);
            else exhaust_decay=grid_step(exhaust_decay,step,4,0.3f,12.0f);
            wet_changed=1;
        }
        else{fm_ratio=home_ratio_step(fm_ratio,step);seq_capture_control(XSEQ_DEST_RATIO,fm_ratio);}
        redraw=1u;
    }
    if((step=plat_enc(EN_K4))!=0&&!patch_page){
        if(governor_page)governor_ui_turn(3u,step);
        else if(topology_page==2u)coupling_ui_turn(EN_K4,step);
        else if(topology_page)topology_ui_turn(EN_K4,step);
        else if(home_detail_page)home_detail_turn(3u,step);
        else if(keyboard_page)keyboard_ui_turn(EN_K4,step);
        else if(sequence_page)seq_ui_turn(EN_K4,step);
        else if(modulation_page)mod_ui_turn(EN_K4,step);
        else if(output_page>=4u)deep_ui_turn(3u,step);
        else if(output_page==3u)filter_ui_turn(3u,step);
        else if(output_page==2u){core_reconstruction=step>0?1u:0u;digital_changed=1u;}
        else if(output_page){xv_size_index=(uint32_t)clamp((int32_t)xv_size_index+step,0,3);xv_reset();}
        else if(envelope_page==2u||envelope_page==3u)amp_changed|=(uint32_t)vca_turn(3u,step);
        else if(envelope_page)env_ui_turn(3u,step);
        else if(exhaust_page){
            if(exhaust_page==3u)dispersion_edit.scale=mod_ui_enum(dispersion_edit.scale,step,3u);
            else if(exhaust_page==2u)exhaust_width=grid_step(exhaust_width,step,100,0.0f,1.0f);
            else{
                exhaust_mix=fm_clampf(exhaust_mix+(float)step*0.05f,0.0f,1.0f);
                if(exhaust_mix<0.0001f)exhaust_mix=0.0f;
                if(exhaust_mix>0.9999f)exhaust_mix=1.0f;
                seq_capture_control(XSEQ_DEST_WET_MIX,exhaust_mix);
            }
            wet_changed=1;
        }
        else{drift_amount=xcontrol_home_step(XCTRL_DRIFT,drift_amount,step,performance_edit.curve[3],performance_edit.sensitivity[3],0u);seq_capture_control(XSEQ_DEST_DRIFT,drift_amount);}
        redraw=1u;
    }
    if((step=plat_enc(EN_SELECT))!=0){
        if(governor_page){governor_ui_onset(step);}
        else if(topology_page==1u){graph_attachment_turn(step);digital_changed=1u;}
        else if(topology_page==2u){}
        else if(keyboard_page)keyboard_ui_turn(EN_SELECT,step);
        else if(sequence_page)seq_ui_turn(EN_SELECT,step);
        else if(modulation_page)mod_ui_turn(EN_SELECT,step);
        else if(output_page==5u){}
        else if(output_page==3u||output_page==4u){filter_edit.placement=mod_ui_enum(filter_edit.placement,step,XFILTER_PLACEMENT_COUNT);filter_publish();redraw=1u;}
        else if(envelope_page==2u||envelope_page==3u){amplifier_shape_edit.curve=mod_ui_enum(amplifier_shape_edit.curve,step,4u);amp_changed=1u;redraw=1u;}
        else if(envelope_page==1u||envelope_page==4u){envelope_selection=mod_ui_enum(envelope_selection,step,4u);redraw=1u;}
        else if(analysis_page){xv_span_index=(uint32_t)clamp((int32_t)xv_span_index+step,0,3);xv_history_count=0;if(analyzer.valid)xv_history();redraw=1u;}
        else if(!topology_page&&!patch_page&&!output_page&&!exhaust_page&&!envelope_page){home_detail_page=(uint32_t)clamp((int32_t)home_detail_page+clamp(step,-4,4),0,4);redraw=1u;}
    }
    if((step=plat_enc(EN_ALGO))!=0&&!patch_page){
        uint32_t old=requested_cells,new_count=(uint32_t)clamp((int32_t)old+clamp(step,-64,64),1,64);
        if(graph_edit.scope==XGRAPH_ADD&&new_count>old){
            uint32_t j,changed=0u;
            for(j=old;j<new_count;j++){
                uint8_t anchor=graph_edit.attachment&&graph_edit.attachment<=j?(uint8_t)(graph_edit.attachment-1u):XGRAPH_NONE;
                if(graph_edit.kind[j]!=graph_edit.next_kind||graph_edit.anchor[j]!=anchor){
                    graph_edit.kind[j]=(uint8_t)graph_edit.next_kind;graph_edit.anchor[j]=anchor;changed=1u;
                }
            }
            if(changed)topology_publish();
        }
        requested_cells=new_count;
        if(new_count<old&&xenolith_graph_limit_attachment(&graph_edit,new_count))topology_publish();
        keyboard_dirty=1u;keyboard_update(keys,0u);digital_changed=1u;redraw=1u;
    }
    if((step=plat_enc(EN_PRESET))!=0){
        if(analysis_page&&!output_page&&!patch_page){
            uint32_t channel=(uint32_t)clamp((int32_t)analysis_channel+clamp(step,-2,2),0,2);
            if(channel!=analysis_channel){
                analysis_channel=channel;X_PUBLISH_BARRIER();
                xv_reset();lights_reset_pending=1u;
                scope_capture_start=scope_write;
                for(k=0;k<128u;k++)scope_snapshot[k]=0;
                redraw=1u;
            }
        }else if(!patch_operation){
            home_detail_page=0u;governor_page=0u;
            topology_page=0u;
            keyboard_page=0u;
            modulation_page=0u;sequence_page=0u;
            patch_slot=(uint32_t)clamp((int32_t)patch_slot+clamp(step,-16,16),0,15);
            patch_page=1u;patch_status=0u;analysis_page=0u;
            output_page=0u;exhaust_page=0u;envelope_page=0u;redraw=1u;
        }
    }
    if(keyboard_dirty&&keyboard_audio.config.gate&&!keyboard_edit.gate&&held_count&&!midi_held_count&&!drone_latch&&!seq_running){running=0u;envelope_event(0u);}
    keyboard_update(keys,new_keys);
    if(wet_changed)exhaust_publish();
    if(env_changed)envelope_publish();
    if(amp_changed)amplifier_publish();
    if(digital_config[digital_config_serial&1u].composite!=composite_workload()||
       digital_config[digital_config_serial&1u].dispersion!=dispersion_workload()||
       digital_config[digital_config_serial&1u].calculation_workload!=calculation_workload()||
       digital_config[digital_config_serial&1u].coupling_workload!=coupling_workload()||
       digital_config[digital_config_serial&1u].transfer_workload!=transfer_workload()||
       digital_config[digital_config_serial&1u].pre_filter!=(uint32_t)(filter_edit.placement==XFILTER_PRE_DIGITAL||filter_pre_snapshot)||
       digital_config[digital_config_serial&1u].extended!=extended_workload()||
       digital_config[digital_config_serial&1u].profile!=digital_workload()||
       workload_profile!=digital_workload()||
       digital_config[digital_config_serial&1u].topology_serial!=topology_serial)digital_changed=1u;
    if(digital_changed)digital_publish();
    previous_buttons=buttons;previous_keys=keys;
    analysis_capture_enabled=analysis_page||lights_enabled;
}

static uint32_t active_page_leds(void)
{
    if(governor_page)return 1u<<B_FX;
    if(patch_page)return 1u<<B_SAVE;
    if(keyboard_page)return 1u<<B_ARP;
    if(sequence_page)return 1u<<B_SEQ;
    if(modulation_page)return 1u<<B_LFO;
    if(envelope_page)return 1u<<((envelope_page==2u||envelope_page==3u)?B_SEL:B_ENV);
    if(exhaust_page)return 1u<<B_FX;
    if(analysis_page)return (1u<<B_GLO)|(output_page?1u<<B_EDIT:0u);
    if(output_page==5u)return 1u<<B_GLO;
    if(output_page)return 1u<<B_EDIT;
    return 1u<<B_HOME;
}
static uint32_t transport_leds(uint32_t buttons)
{
    static uint32_t observed_interventions,intervention_at;
    uint32_t now=plat_ms(),interventions=governor_core_holds+governor_output_fallbacks;
    float stress=fm_clampf(governor_stress_snapshot,0,1);
    /* Only latch a new intervention timestamp after the previous 180ms burst
     * window finishes, preventing continuous holds from pinning phase at 0. */
    if(interventions!=observed_interventions){
        observed_interventions=interventions;
        if(now-intervention_at>=180u)intervention_at=now;
    }
    uint32_t lit=buttons&((1u<<NB)-1u);lit&=~(1u<<B_PLAY);
    if((buttons&(1u<<B_PLAY))||(seq_running?seq_beat_phase<UINT32_C(0x40000000):drone_latch))lit|=LED_PLAY_GREEN;
    if(seq_recording!=XSEQ_REC_IDLE)lit|=1u<<B_REC;
    else if(governor_page||stress>.025f||(interventions&&now-intervention_at<180u)){
        uint32_t period,on,notch;
        if(stress>.45f){
            period=560u-(uint32_t)((stress-.45f)*800.0f);
            notch=(((now/43u)+interventions)*1103515245u+12345u)>>27;
            on=now%period<period/3u;
            if(notch<(uint32_t)((stress-.45f)*20.0f))on=!on;
        }else if(stress>.025f){
            period=1500u-(uint32_t)(stress*1800.0f);
            on=now%period<80u+(uint32_t)(stress*160.0f);
        }else{
            period=1200u-(uint32_t)(fm_clampf(governor_load_snapshot,0,1)*850.0f);
            on=now%period<period/3u;
        }
        /* Real denied-work events sputter at 45ms without locking solid ON. */
        if(interventions&&now-intervention_at<180u){
            uint32_t arc=(((now/29u)+interventions)*1103515245u+12345u)>>28;
            on=(((now-intervention_at)/45u&1u)==0u)^(arc<5u);
        }
        lit&=~(1u<<B_REC);if(on)lit|=1u<<B_REC;
    }
    return lit;
}
static void lights_frame(uint32_t now)
{
    uint32_t elapsed=now-lights_clock,keys=plat_keys(),buttons=plat_buttons(),lit,dim;
    lights_clock=now;if(elapsed>1000u)elapsed=1000u;
    if(lights_reset_pending){
        xenolith_lights_init(&spectral_lights);
        lights_capture_start=analysis_published;lights_last_capture=lights_capture_start;
        lights_accumulator=0u;lights_reset_pending=0u;
    }
    if(lights_enabled&&!(analysis_page&&!output_page)){
        lights_accumulator+=elapsed*10u;
        if(lights_accumulator>=1000u){
            uint32_t end=analysis_published,i;
            lights_accumulator%=1000u;
            if(end!=lights_last_capture&&(uint32_t)(end-lights_capture_start)>=1024u){
                X_PUBLISH_BARRIER();
                for(i=0;i<1024u;i++)analysis_input[i]=analysis_ring[(end-1024u+i)&ANALYSIS_RING_MASK];
                X_PUBLISH_BARRIER();
                if(analysis_published==end&&analysis_compute(1024u,1024u,0u))lights_accept(end);
            }
        }
    }else lights_accumulator=0u;
    if(panic_latched)keys=0u;
    xenolith_lights_tick(&spectral_lights,elapsed,keys,(int)panic_latched);
    lit=transport_leds(buttons);
    lit|=active_page_leds();
    dim=((1u<<NB)-1u)&~lit;dim&=~(1u<<B_PLAY);
    if(!(lit&LED_PLAY_GREEN))dim|=LED_PLAY_GREEN;
    plat_lights(lit,spectral_lights.bright|keys,dim,spectral_lights.dim&~keys,1);
    plat_lights_mid(0,spectral_lights.mid&~keys);
}

void app_input_only(void){input();}

static void draw_scope(void)
{
    uint32_t i,start=scope_write;int32_t centre=output_page==2u?42:38,extent=output_page==2u?11:17;int32_t old_x=14,old_y=centre;
    int16_t fresh[128];
    for(i=0;i<128u;i++)fresh[i]=scope_samples[(start+i)&127u];
    /* One attempt only: an interrupt may advance the audio writer. In that
     * case keep the previous complete UI snapshot; never wait on audio. */
    if(scope_write==start&&(uint32_t)(start-scope_capture_start)>=128u)
        for(i=0;i<128u;i++)scope_snapshot[i]=fresh[i];
    cv_begin(240,59,C_BLACK);
    if(output_page==2u){
        int32_t rate_width=(int32_t)(fm_clampf(active_core_rate/176400.0f,0.0f,1.0f)*94.0f+0.5f);
        int32_t bits_width=core_bits?(int32_t)(core_bits*94u/24u):94;
        char actual[24];rate_text(active_core_rate,actual);
        cv_text(14,0,&FONT_XS,"ACTUAL KHZ",C_DIM);
        cv_text(222-text_w(&FONT_XS,actual),0,&FONT_XS,actual,C_AMB);
        cv_rect(14,20,94,3,C_LINE);cv_rect(126,20,94,3,C_LINE);
        if(rate_width)cv_rect(14,20,rate_width,3,C_AMB);
        if(bits_width)cv_rect(126,20,bits_width,3,C_HI);
    }else if(exhaust_page==2u){
        char actual[24];rate_text(active_wet_rate,actual);
        cv_text(14,3,&FONT_XS,"ACTUAL KHZ",C_DIM);
        cv_text(222-text_w(&FONT_XS,actual),3,&FONT_XS,actual,C_AMB);
    }else{
        cv_text(14,3,&FONT_XS,"[ OUTPUT ]",C_DIM);
        cv_text(206,3,&FONT_XS,xv_channel_name(),C_DIM);
    }
    cv_line(8,output_page==2u?28:20,231,output_page==2u?28:20,C_LINE);cv_line(8,56,231,56,C_LINE);
    cv_line(8,output_page==2u?28:20,8,56,C_LINE);cv_line(231,output_page==2u?28:20,231,56,C_LINE);
    cv_line(14,centre,225,centre,C_LINE);
    for(i=0;i<128u;i++){
        int32_t x=14+(int32_t)(i*211u/127u);
        int32_t y=centre-clamp((int32_t)scope_snapshot[i]*(int32_t)trace_gain*extent/32768,-extent+1,extent-1);
        if(i)cv_line(old_x,old_y,x,y,C_HI);
        old_x=x;old_y=y;
    }
    cv_blit(0,137);
}

/* Source sidebars follow the envelope. The centre reuses fresh spectral-band
 * peaks (including audible tails); otherwise it follows source activity. */
static void draw_source_eye(void)
{
    const char *error=0;
    if(seq_ui_status==SEQ_UI_EXP_RANGE)error="USE EXP DEPTH TARGET";
    else if(seq_ui_status==SEQ_UI_RANGE)error="OUTSIDE CV RANGE";
    else if(seq_ui_status==SEQ_UI_FULL)error="QUEUE FULL / RETRY";
    else if(seq_ui_status==SEQ_UI_BUSY)error="NO FREE CV TARGET";
    else if(seq_ui_status==SEQ_UI_TARGET)error="SELECT CV TARGET";
    if(error){cv_begin(240,28,C_BLACK);cv_text(16,6,&FONT_XS,error,C_HI);cv_blit(0,212);return;}
    static float activity;
    static uint32_t category;
    uint32_t i,active=running||envelope_level>0.0f;
    uint32_t segments=active?(uint32_t)clamp((int32_t)(envelope_level*8.0f+0.5f),0,8):0u;
    float visual_peak=fm_sqrtf(fm_clampf((float)output_peak*(32.0f/8388607.0f),0.0f,1.0f));
    float target=active?fm_clampf(envelope_level*0.25f+visual_peak*0.75f,0.0f,1.0f):0.0f;
    uint32_t peak,shape,spectral=0u;
    float bands[4]={0,0,0,0};
    if(lights_enabled&&spectral_lights.age_ms<500u){
        uint32_t best=0u;
        for(i=0;i<27u;i++){
            uint32_t group=i/7u;
            float level=spectral_lights.envelope[i];
            bands[group]+=level*level;
        }
        for(i=1;i<4u;i++)if(bands[i]>bands[best])best=i;
        if(bands[best]>0.00000001f){
            spectral=1u;
            if(bands[category]<0.00000001f||bands[best]>bands[category]*1.2f)category=best;
        }
    }
    if(active||spectral){
        if(!active)target=visual_peak*0.75f;
        activity+=(target-activity)*0.25f;
    }else activity=0.0f;
    peak=(uint32_t)(activity*255.0f);
    shape=active?(uint32_t)clamp((int32_t)(activity*5.99f),0,5):0u;
    if(spectral){static const uint8_t shapes[4]={1u,2u,4u,5u};shape=shapes[category];}
    uint16_t resting=RGB(40,0,0),eye=RGB(40u+peak*200u/255u,peak*(48u+category*20u)/255u,peak*(24u+category*12u)/255u);
    cv_begin(240,28,C_BLACK);
    cv_line(8,7,8,23,C_LINE);cv_line(8,7,14,7,C_LINE);cv_line(8,23,14,23,C_LINE);
    cv_line(231,7,231,23,C_LINE);cv_line(225,7,231,7,C_LINE);cv_line(225,23,231,23,C_LINE);
    for(i=0;i<8u;i++){
        uint16_t colour=i<segments?C_AMB:resting;
        cv_rect(24+(int32_t)i*9,13,5,5,colour);
        cv_rect(211-(int32_t)i*9,13,5,5,colour);
    }
    /* Small prismatic shape follows smoothed published source activity.
     * Flat/dim at idle; no clock-driven animation or invented measurement. */
    if(shape==0u)cv_rect(110,15,21,1,active?eye:resting);
    else if(shape==1u){
        cv_rect(110,14,21,3,eye);cv_rect(110,11,2,9,eye);cv_rect(129,11,2,9,eye);
    }else if(shape<4u){
        cv_line(109,15,115,9,eye);cv_line(115,9,125,9,eye);cv_line(125,9,131,15,eye);
        cv_line(109,15,115,21,eye);cv_line(115,21,125,21,eye);cv_line(125,21,131,15,eye);
        if(shape==3u){cv_rect(119,10,3,11,eye);cv_rect(113,14,15,3,eye);}
        else cv_rect(119,14,3,3,eye);
    }else if(shape==4u){
        cv_line(110,9,130,9,eye);cv_line(110,21,130,21,eye);
        cv_line(110,9,130,21,eye);cv_line(130,9,110,21,eye);
        cv_rect(119,13,3,5,eye);
    }else{
        for(i=0;i<9u;i++){
            int32_t height=1+(int32_t)i;
            cv_rect(111+(int32_t)i,15-height/2,1,height,eye);
            cv_rect(129-(int32_t)i,15-height/2,1,height,eye);
        }
        cv_rect(119,9,3,13,C_HI);
    }
    if(shape&&peak)cv_rect(120,15,1,1,RGB(144u+peak*104u/255u,peak*176u/255u,peak*104u/255u));
    cv_blit(0,212);
}

/* Foreground colour only. Snapshot sampling is stroboscopic, not an
 * oscilloscope or CPU-load measurement; no FFT or transcendental work in ISR. */
static void gauge_update(uint32_t now)
{
    static uint32_t previous_serial,previous_ms,previous_cells;
    static float previous_noise[8];
    GaugeSnapshot sample;uint32_t serial=gauge_serial,i,elapsed=now-previous_ms;
    float target[4]={0,0,0,0},weight=0,weighted=0,tonic=0,deviation=0,rate=0,movement=0;
    float active=fm_clampf(envelope_level,0.0f,1.0f);
    if((serial&1u)||serial==previous_serial||elapsed<40u)return;
    X_PUBLISH_BARRIER();sample=gauge_published;X_PUBLISH_BARRIER();
    if(serial!=gauge_serial)return;
    if(lights_enabled&&spectral_lights.age_ms<500u){
        for(i=0;i<27u;i++){float energy=spectral_lights.envelope[i]*spectral_lights.envelope[i];weight+=energy;weighted+=energy*(float)i;}
        if(weight>0.00000001f)target[0]=0.15f+0.85f*weighted/(26.0f*weight);
    }
    for(i=0;i<sample.cells;i++){
        float sine=fm_sin_turns((float)sample.phase[i]*(1.0f/536870912.0f));
        tonic+=fm_fabsf(sample.tonic[i]);
        deviation+=fm_fabsf(sine*sample.tonic[i]*sample.index);
        rate+=fm_fabsf((float)sample.increment[i])*(sample.rate/536870912.0f);
        if(previous_cells==sample.cells)movement+=fm_fabsf(sample.noise[i]-previous_noise[i]);
        previous_noise[i]=sample.noise[i];
    }
    if(sample.cells){
        float normalized=tonic>0.0f?deviation/tonic:0.0f;
        rate/=(float)sample.cells;
        target[1]=active*normalized/(normalized+2.0f);
        /* Actual wrapped modulator phase plus its quantized advance rate. */
        target[2]=active*(0.55f*rate/(rate+1000.0f)+0.45f*(float)sample.phase[0]*(1.0f/536870912.0f));
        if(previous_ms&&previous_cells==sample.cells)
            target[3]=active*fm_clampf(movement*sample.drift*(12000.0f/((float)sample.cells*(float)elapsed)),0.0f,1.0f);
    }
    for(i=0;i<4u;i++)gauge_activity[i]+=(target[i]-gauge_activity[i])*0.35f;
    previous_cells=sample.cells;previous_serial=serial;previous_ms=now;
}

/* Parameter scales, not signal meters: each cursor is value / its stated max. */
static void draw_home_gauge(uint32_t kind,float value,int32_t top)
{
    static const float maximum[4]={800.0f,64.0f,32.0f,1.0f};
    static const char *ends[4]={"800","64","32","1.00"};
    /* Sixteen familiar terminal blocks: solid, shaded, quantized, thermal.
     * These show parameter position, never waveform or signal energy. */
    float scale=kind==1u?xenolith_depth_limit(topology_edit.mode):maximum[kind];
    int32_t filled=(int32_t)(fm_clampf(value/scale,0.0f,1.0f)*176.0f+0.5f);
    uint16_t brick=RGB(48,20,20);uint32_t column;
    /* Linear horizontal clipping keeps a low value partial rather than
     * lighting a whole decorative unit. Resolution is1/176 of full scale. */
    cv_line(8,top,12,top,C_AMB);cv_line(8,top+12,12,top+12,C_AMB);
    for(column=0;column<176u;column++){
        uint32_t mask,row=0u,local=column%11u;
        uint32_t zone=column<59u?0u:(column<118u?1u:2u);
        uint32_t level=(uint32_t)(fm_clampf(gauge_activity[kind],0.0f,1.0f)*255.0f);
        uint16_t active=RGB(96u+zone*24u+level*100u/255u,14u+zone*18u+level*120u/255u,10u+zone*10u+level*75u/255u);
        uint16_t colour=(int32_t)column<filled?active:brick;
        if(local>=9u)mask=0u; /* two black columns separate each unit */
        else if(kind==0u)mask=0x03feu;
        else if(kind==1u){
            if(column<59u)mask=(local&1u)?0x02aau:0x0154u;
            else if(column<118u)mask=(local&1u)?0x03bau:0x036eu;
            else mask=0x03feu;
        }else if(kind==2u)mask=0x0666u; /* three short equal rails */
        else{
            uint32_t shift=local<3u?2u:(local<6u?1u:0u);
            mask=0x01ffu<<shift;
            if((int32_t)column>=filled&&local>0u&&local<8u)mask=(0x0101u<<shift);
        }
        /* Batch vertical runs; no per-pixel signal or DSP work. */
        while(row<13u){
            uint32_t start;
            while(row<13u&&!(mask&(1u<<row)))row++;
            start=row;
            while(row<13u&&(mask&(1u<<row)))row++;
            if(row>start)cv_rect(18+(int32_t)column,top+(int32_t)start,1,(int32_t)(row-start),colour);
        }
    }
    for(column=44u;column<176u;column+=44u)cv_line(18+(int32_t)column,top+14,18+(int32_t)column,top+15,C_DIM);
    cv_text(206,top,&FONT_XS,kind==1u&&topology_edit.mode==XTOPO_WARP?"100":kind==1u&&topology_edit.mode==XTOPO_EXPONENTIAL?"120":ends[kind],C_GRAY);
}

static void draw_patches(void)
{
    char slot[3];uint32_t i,occupied=patch_occupied&(1u<<patch_slot);
    const char *state=patch_save_armed?(occupied?"REPLACE SLOT":"STORE SLOT"):(occupied?"SAVED SLOT":"EMPTY SLOT");
    if(patch_operation)state=patch_operation==2u?"SAVING SLOT":"LOADING SLOT";
    else if(patch_status==1u)state="LOADED SLOT";
    else if(patch_status==2u)state="SAVED SLOT";
    else if(patch_status==3u)state="EMPTY SLOT";
    else if(patch_status==4u)state="WRITE / LOAD FAILED";
    cv_begin(240,36,C_BLACK);
    draw_panel_header(patch_save_armed?"[ SAVE PATCH ]":"[ PATCHES ]");
    cv_line(8,28,231,28,C_LINE);cv_blit(0,0);
    cv_begin(240,132,C_BLACK);
    for(i=0;i<XENOLITH_PATCH_COUNT;i++){
        int32_t x=18+(int32_t)(i%4u)*52,y=8+(int32_t)(i/4u)*30;
        uint32_t present=patch_occupied&(1u<<i);
        uint16_t edge=i==patch_slot?C_HI:C_LINE;
        cv_rect(x,y,44,24,RGB(24,4,4));
        cv_line(x,y,x+43,y,edge);cv_line(x,y+23,x+43,y+23,edge);
        cv_line(x,y,x,y+23,edge);cv_line(x+43,y,x+43,y+23,edge);
        slot[0]=(char)('0'+(i+1u)/10u);slot[1]=(char)('0'+(i+1u)%10u);slot[2]=0;
        cv_text(x+14,y+4,&FONT_XS,slot,present?RGB(200,155,121):C_DIM);
        if(present)cv_rect(x+4,y+10,3,4,C_AMB);
    }
    cv_blit(0,36);
    cv_begin(240,72,C_BLACK);
    slot[0]=(char)('0'+(patch_slot+1u)/10u);slot[1]=(char)('0'+(patch_slot+1u)%10u);slot[2]=0;
    cv_text(16,3,&FONT_XS,state,patch_status==4u?C_HI:C_GRAY);
    if(patch_status!=4u)cv_text(212,1,&FONT_B,slot,RGB(200,155,121));
    cv_line(8,26,231,26,C_LINE);
    if(patch_operation)cv_text(16,34,&FONT_XS,"PLEASE WAIT",C_DIM);
    else if(patch_save_armed)cv_text(16,34,&FONT_XS,"SAVE CONFIRM / PRESET SLOT",C_GRAY);
    else cv_text(16,34,&FONT_XS,"PLAY LOAD / PRESET SLOT",C_GRAY);
    cv_text(16,53,&FONT_XS,patch_save_armed?"HOME CANCEL":"SAVE STORE / HOME EXIT",C_DIM);
    cv_blit(0,168);
}

static void draw(void)
{
    char value[24];uint32_t i;
    const char *labels[4]={"K1 TONIC","K2 FM INDEX","K3 MOD RATIO","K4 DRIFT"};
    float values[4]={pitch_hz,fm_index,fm_ratio,drift_amount};
    if(!patch_page&&!output_page&&!exhaust_page&&!envelope_page){
        uint32_t before=effective_serial;
        if(before){AppEffectiveControls c;X_PUBLISH_BARRIER();c=effective_controls[before&1u];X_PUBLISH_BARRIER();
            if(before==effective_serial)effective_view=c;
            values[0]=effective_view.tonic;values[1]=effective_view.index;values[2]=effective_view.ratio;values[3]=effective_view.drift;}
    }
    if(topology_edit.mode==XTOPO_PHASE)labels[1]="K2 PHASE INDEX";
    else if(topology_edit.mode==XTOPO_EXPONENTIAL)labels[1]="K2 EXP DEPTH ST";
    else if(topology_edit.mode==XTOPO_PULSE)labels[1]="K2 PULSE DEPTH";
    else if(topology_edit.mode==XTOPO_WARP)labels[1]="K2 PHASE WARP %";
    if(home_detail_page){home_detail_draw();return;}
    if(patch_page){draw_patches();return;}
    if(output_page>=4u){deep_ui_draw();return;}
    if(output_page==3u){filter_ui_draw();return;}
    if(envelope_page==1u||envelope_page==4u){env_ui_draw();return;}
    if(exhaust_page){
        labels[0]=exhaust_page==2u?"K1 WET KHZ":"K1 ECHO MS";labels[1]=exhaust_page==2u?"K2 STRUCTURE":"K2 DAMP HZ";
        labels[2]=exhaust_page==2u?"K3 DIFFUSION":"K3 TAIL S";labels[3]=exhaust_page==2u?"K4 WIDTH %":"K4 MIX %";
        values[0]=exhaust_page==2u?exhaust_rate/1000.0f:fm_clampf(exhaust_time,0.002f,xenolith_exhaust_echo_limit(exhaust_rate))*1000.0f;
        values[1]=exhaust_damp_hz;values[2]=exhaust_page==2u?exhaust_diffusion:exhaust_decay;values[3]=(exhaust_page==2u?exhaust_width:exhaust_mix)*100.0f;
    }
    if(exhaust_page==3u){
        labels[0]="K1 INPUT %";labels[1]="K2 TANK A %";labels[2]="K3 TANK B %";labels[3]="K4 SCALE";
        values[0]=exhaust_diffusion*100;values[1]=dispersion_edit.amount_a*100;values[2]=dispersion_edit.amount_b*100;values[3]=(float)dispersion_edit.scale;
    }
    if(envelope_page){
        labels[0]="K1 ATTACK MS";labels[1]="K2 DECAY MS";labels[2]="K3 SUSTAIN %";labels[3]="K4 RELEASE MS";
        values[0]=envelope_attack;values[1]=envelope_decay;values[2]=envelope_sustain*100.0f;values[3]=envelope_release;
    }
    if(envelope_page==2u){
        labels[0]="K1 DRIVE %";labels[1]="K2 BIAS";labels[2]="K3 COLOUR %";labels[3]="K4 DEGEN %";
        values[0]=amplifier_edit.drive*100.0f;values[1]=amplifier_edit.bias;values[2]=amplifier_edit.colour*100.0f;values[3]=amplifier_feedback_edit*100.0f;
    }else if(envelope_page==3u){
        static const uint32_t stages[4]={1u,2u,4u,6u};
        labels[0]="K1 FOLD %";labels[1]="K2 BIAS";labels[2]="K3 STAGES";labels[3]="K4 MIX %";
        values[0]=folder_edit.amount*100.0f;values[1]=folder_edit.bias;
        values[2]=(float)stages[folder_edit.stages];values[3]=folder_edit.mix*100.0f;
    }
    cv_begin(240,36,C_BLACK);
    draw_panel_header("[ X E N O L I T H ]");
    cv_line(8,28,231,28,C_LINE);
    cv_line(8,28,8,35,C_LINE);cv_line(231,28,231,35,C_LINE);
    cv_rect(13,20,output_page?114:(exhaust_page?84:108),16,C_BLACK);
    cv_text(16,21,&FONT_XS,output_page==2u?"[ DIGITAL CORE ]":output_page?"[ ANALYZER ]":(envelope_page==3u?"[ WAVEFOLDER ]":envelope_page==2u?"[ VCA ]":envelope_page?"[ ENVELOPE ]":(exhaust_page==3u?"[ DIFFUSION ]":exhaust_page==1u?"[ EXHAUST ]":(exhaust_page==2u?"[ TANK ]":"[ CORE ARRAY ]"))),C_AMB);
    if(output_page==2u){
        static const char *const response[]={"FINE","SWEEP","FAST","LINEAR"};
        const char *name=response[core_rate_response];
        cv_rect(162,19,65,17,C_BLACK);
        cv_line(162,19,226,19,C_LINE);cv_line(162,35,226,35,C_LINE);
        cv_line(162,19,162,35,C_LINE);cv_line(226,19,226,35,C_LINE);
        cv_text(194-text_w(&FONT_XS,name)/2,20,&FONT_XS,name,C_HI);
    }
    if(exhaust_page&&exhaust_page!=3u){const char *name=xenolith_exhaust_structure_reconstruction(exhaust_reconstruction)?"LINEAR":"ZOH";
        cv_rect(180,19,47,17,C_BLACK);
        cv_line(180,19,226,19,C_LINE);cv_line(180,35,226,35,C_LINE);
        cv_line(180,19,180,35,C_LINE);cv_line(226,19,226,35,C_LINE);
        cv_text(203-text_w(&FONT_XS,name)/2,21,&FONT_XS,name,C_HI);}
    if(envelope_page==2u||envelope_page==3u){
        static const char *const curves[]={"CUBIC","SPACE3/2","SOFTSIGN","TRI FOLD"};
        const char *curve=curves[amplifier_shape_edit.curve<4u?amplifier_shape_edit.curve:0u];
        int32_t width=text_w(&FONT_XS,curve)+12,left=226-width;
        cv_rect(left,19,width+1,17,C_BLACK);
        cv_line(left,19,226,19,C_LINE);cv_line(left,35,226,35,C_LINE);
        cv_line(left,19,left,35,C_LINE);cv_line(226,19,226,35,C_LINE);
        cv_text(left+6,21,&FONT_XS,curve,C_HI);
        if(envelope_page==2u&&amplifier_edit.colour==0.0f)cv_text(76,21,&FONT_XS,"DRY",C_DIM);
    }
    if(!output_page&&!exhaust_page&&!envelope_page){
        topology_ui_home_glyph();topology_ui_home_count();
    }
    cv_blit(0,0);
    if(!output_page&&!exhaust_page&&!envelope_page){
        for(i=0;i<4u;i++){
            cv_begin(240,44,C_BLACK);
            cv_line(8,0,8,42,C_LINE);cv_line(231,0,231,42,C_LINE);
            cv_line(8,43,231,43,C_LINE);
            cv_text(16,3,&FONT_XS,labels[i],C_GRAY);
            if(i==2u)ratio_text(values[i],value);
            else fixed_text(i==1u&&topology_edit.mode==XTOPO_WARP?values[i]*(100.0f/64.0f):values[i],value);
            cv_text(222-text_w(&FONT_S,value),1,&FONT_S,value,RGB(200,155,121));
            draw_home_gauge(i,values[i],25);
            cv_blit(0,36u+i*44u);
        }
        draw_source_eye();return;
    }
    cv_begin(240,101,C_BLACK);
    cv_line(8,0,8,99,C_LINE);cv_line(231,0,231,99,C_LINE);cv_line(8,99,231,99,C_LINE);
    if(!output_page){
        for(i=0;i<4u;i++){
            int32_t y=(int32_t)i*24;
            cv_text(16,y+6,&FONT_XS,labels[i],C_GRAY);
            if(exhaust_page==3u&&i==3u){
                static const char *const scales[]={"1/4","1/2","1"};
                const char *scale=scales[dispersion_edit.scale<3u?dispersion_edit.scale:2u];
                cv_text(222-text_w(&FONT_B,scale),y+3,&FONT_B,scale,C_HI);continue;
            }
            if(exhaust_page==2u&&i==1u){
                static const char *const structures[5]={"TANK","CLOUD4","CLOUD8","RING","PAIRS"};
                const char *recon=structures[xenolith_exhaust_structure_family(exhaust_reconstruction)];
                cv_text(222-text_w(&FONT_B,recon),y+3,&FONT_B,recon,C_HI);continue;
            }
            if(envelope_page==3u&&i==2u)number_text((int32_t)values[i],value);
            else if(exhaust_page==2u&&i==0u)rate_text(exhaust_rate,value);else fixed_text(values[i],value);
            cv_text(222-text_w(&FONT_B,value),y+3,&FONT_B,value,C_HI);
            if(i<3u)cv_line(8,y+24,12,y+24,C_LINE);
        }
    }else if(output_page==2u){
        static const char *quantizers[3]={"TRUNC","ROUND","ERROR FB"};
        const char *recon=active_quality==2u||active_quality==4u?"FIR":(core_reconstruction?"LINEAR":"ZOH");
        cv_text(16,5,&FONT_XS,"K1 MAX KHZ",C_GRAY);rate_text(core_max_rate,value);cv_text(222-text_w(&FONT_B,value),2,&FONT_B,value,C_HI);
        cv_text(16,30,&FONT_XS,"K2 FRAC BITS",C_GRAY);
        number_text((int32_t)(core_bits?core_bits:24u),value);
        cv_text(222-text_w(&FONT_B,value),27,&FONT_B,value,C_HI);
        cv_text(16,55,&FONT_XS,"K3 QUANTIZER",C_GRAY);cv_text(222-text_w(&FONT_B,quantizers[core_quantizer]),52,&FONT_B,quantizers[core_quantizer],core_bits?C_HI:C_DIM);
        cv_text(16,80,&FONT_XS,"K4 RECON",C_GRAY);cv_text(222-text_w(&FONT_B,recon),77,&FONT_B,recon,C_HI);
    }else{
        cv_text(16,5,&FONT_XS,"K1 SPAN KHZ",C_GRAY);
        rate_text((float)XV_SPANS[xv_span_index],value);cv_text(222-text_w(&FONT_B,value),2,&FONT_B,value,C_HI);
        cv_text(16,30,&FONT_XS,"K2 TAP",C_GRAY);
        cv_text(222-text_w(&FONT_B,xv_channel_name()),27,&FONT_B,xv_channel_name(),C_HI);
        cv_text(16,55,&FONT_XS,"K3 FFT / SEC",C_GRAY);
        number_text((int32_t)XV_RATES[xv_rate_index],value);cv_text(222-text_w(&FONT_B,value),52,&FONT_B,value,C_HI);
        cv_text(16,80,&FONT_XS,"K4 FFT SIZE",C_GRAY);
        cv_text(222-text_w(&FONT_B,XV_MODE_NAMES[xv_size_index]),77,&FONT_B,XV_MODE_NAMES[xv_size_index],C_HI);
    }
    cv_blit(0,36);
    draw_scope();
    cv_begin(240,44,C_BLACK);
    if(output_page==2u){
        char *end;cv_text(8,1,&FONT_XS,"ISR",C_DIM);number_text((int32_t)plat_cpu_pct(),value);end=value;while(*end)end++;*end++='%';*end=0;cv_text(32,1,&FONT_XS,value,C_GRAY);
        cv_text(76,1,&FONT_XS,"STATE",C_DIM);number_text((int32_t)sizeof quality,value);end=value;while(*end)end++;*end++='B';*end=0;cv_text(112,1,&FONT_XS,value,C_GRAY);
    }else cv_text(8,1,&FONT_XS,running?"[>] RUN":(output_peak>256u?"[~] TAIL":"[ ] STOP"),running?C_HI:C_GRAY);
    cv_text(output_page==2u?164:140,1,&FONT_XS,output_page==2u?"EDIT HOME":(output_page==1u?"EDIT > FFT":(exhaust_page==1u?"FX > CLOCK":(exhaust_page==2u?"FX > HOME":"FX / GLO / EDIT"))),C_DIM);
    cv_line(8,18,231,18,C_LINE);
    cv_blit(0,196);
}

void app_ui_init(void)
{
    /* Operator's #980000 accent maps to RGB565 0x9800 (5-bit red=19).
     * Values and active trace stay brighter than the structural crimson. */
    palette_set(3);
    pal[0]=RGB(152,0,0);pal[1]=RGB(112,56,56);pal[2]=RGB(176,112,112);
    pal[3]=RGB(152,0,0);pal[4]=RGB(240,112,96);
    /* Continue through ordinary foreground frames; no blocking delay or audio
     * work. Any key/button dismisses the title and is still handled normally. */
    app_splash();boot_started=plat_ms();boot_last_frame=0u;boot_active=1u;redraw=1;
}

void app_frame(void)
{
    static uint32_t last_scope,last_info,last_voice,last_quality,last_cells,last_gauges;
    uint32_t now=plat_ms();
    uint32_t voice;
    seq_foreground_snapshot();seq_clock_service();motion_service();
    input();patch_service(now);exhaust_service();lights_frame(now);ann_tick(now);gauge_update(now);
    if(boot_active){
        uint32_t elapsed=now-boot_started;
        if(elapsed<XENOLITH_BOOT_MS&&!plat_buttons()&&!plat_keys()){
            if(elapsed-boot_last_frame>=33u){boot_title(elapsed);boot_last_frame=elapsed;}
            return;
        }
        boot_active=0u;redraw=1u;
    }
    voice=drone_latch?2u:((running||envelope_level>0.0f)?1u:0u);
    if(voice!=last_voice){last_voice=voice;redraw=1u;}
    if(active_quality!=last_quality){last_quality=active_quality;redraw=1u;}
    if(active_cells!=last_cells){last_cells=active_cells;redraw=1u;}
    if(home_detail_page){
        if(redraw||now-last_scope>=100u){redraw=0u;last_scope=now;home_detail_draw();}
        return;
    }
    if(governor_page){
        if(redraw||now-last_scope>=100u){redraw=0u;last_scope=now;governor_ui_draw();}
        return;
    }
    if(topology_page){
        if(redraw||now-last_scope>=100u){redraw=0u;last_scope=now;if(topology_page==2u)coupling_ui_draw();else topology_ui_draw();}
        return;
    }
    if(keyboard_page){
        if(redraw||now-last_scope>=100u){redraw=0u;last_scope=now;keyboard_ui_draw();}
        return;
    }
    if(sequence_page){
        if(redraw||now-last_scope>=100u){redraw=0u;last_scope=now;seq_ui_draw();}
        return;
    }
    if(modulation_page){
        if(redraw||now-last_scope>=100u){redraw=0u;last_scope=now;mod_ui_draw();}
        return;
    }
    if(output_page>=3u||envelope_page==1u||envelope_page==4u){
        if(redraw||now-last_scope>=100u){redraw=0u;last_scope=now;
            if(output_page>=4u)deep_ui_draw();else if(output_page==3u)filter_ui_draw();else env_ui_draw();}
        return;
    }
    if(analysis_page&&!output_page){
        analysis_tick(now);
        if(redraw||xv_draw_due){redraw=0;xv_draw_due=0;draw_analysis();}
        return;
    }
    if(now-last_info>=500u){redraw=1;last_info=now;}
    if(redraw){redraw=0;draw();last_scope=now;}
    else if(now-last_scope>=50u){
        last_scope=now;
        if(patch_page)draw_patches();
        else if(output_page||exhaust_page||envelope_page)draw_scope();
        else{
            draw_source_eye();
            if(now-last_gauges>=100u){
                float values[4]={effective_view.tonic,effective_view.index,effective_view.ratio,effective_view.drift};uint32_t j;last_gauges=now;
                for(j=0;j<4u;j++){
                    cv_begin(240,18,C_BLACK);cv_line(8,0,8,17,C_LINE);cv_line(231,0,231,17,C_LINE);
                    draw_home_gauge(j,values[j],0);cv_blit(0,61u+j*44u);
                }
            }
        }
    }
}

