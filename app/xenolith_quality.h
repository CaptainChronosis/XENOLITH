/* SPDX-License-Identifier: MPL-2.0
 * Optional FM oversampling; the default1x source remains the SC-style path.
 */
#ifndef XX29_XENOLITH_QUALITY_H
#define XX29_XENOLITH_QUALITY_H
#include "xenolith_core.h"
#include "xenolith_filter.h"
#define XENOLITH_QUALITY_NATIVE 0u
#define XENOLITH_QUALITY_AUTO 1u
#define XENOLITH_QUALITY_HI2 2u
#define XENOLITH_QUALITY_HI4 3u
#define XENOLITH_QUALITY_ECO2 4u
#define XENOLITH_QUALITY_ECO4 5u
#define XENOLITH_QUALITY_CRUSH 6u
#define XENOLITH_RATE_ECO2 16u
#define XENOLITH_RATE_ECO4 17u
#define XENOLITH_RATE_CRUSH 18u

typedef struct {
    float history[64];
    uint32_t write;
} XenolithHalfband;

typedef struct {
    float rate, scale, inverse_scale;
    uint32_t bits, quantizer, reconstruction, control_frames;
} XenolithCrushConfig;

typedef struct {
    float requested_rate, effective_rate;
    uint32_t factor, bits, quantizer, reconstruction;
    XenolithCrushConfig crush;
} XenolithDigitalConfig;

typedef struct {
    XenolithHalfband stage[2];
    float output_rate;
    uint32_t control_frames, factor, warmup;
    float held;
    uint32_t hold_remaining;
    XenolithCrushConfig crush;
    float crush_phase, previous_held, quantization_error, crush_step;
    uint32_t digital_bits;
    uint32_t continuous, ramp_remaining;
    float target_rate;
    uint32_t configured_bits; /* structural base; live modulation owns the grid */
    float nominal_rate, load_duty;
    float raw_held;
    uint32_t core_hold;
    float high_actual_rate;
} XenolithQuality;

/* Initialize beside a fresh1x core; no heap or additional core. */
void xenolith_quality_init(XenolithQuality *q, const XenolithCore *core);
/* OFF=1x. AUTO:4x for1..8cells,2x for9..16,1x above16; additionally honor
 * the core's192k internal-rate ceiling and1024-frame control-block limit.
 * HI2 requests2x but caps at1x above16cells; HI4 follows the AUTO ceiling.
 * ECO2/ECO4 clock the core at1/2 or1/4 with output hold; CRUSH uses its prepared
 * clock/quantizer/reconstruction config. They preserve pitch/control-time nominally,
 * intentionally alias and add hold images. Reject rates below the core8k
 * floor or indivisible control blocks by falling back to1x.
 * Return values1/2/4 are multipliers;16/17/18 are opaque low-rate codes.
 * This is a fixed resource policy, not measured CPU feedback or a guarantee.
 */
uint32_t xenolith_quality_factor_for(const XenolithQuality *q, uint32_t mode,
                                    uint32_t cells);
/* Call only from the audio owner with the application's transition gate at0.
 * Does not reset phases/drift. Clears FIRs and starts32/48 output-frame warmup
 * for2x/4x. Same-factor calls are no-ops. Accepts1/2/4 or low-rate codes16..18.
 */
void xenolith_quality_set_factor(XenolithQuality *q, XenolithCore *core, uint32_t factor);
/* FM core only; envelope, tank and output stay at the original output rate.
 *1x directly returns xenolith_sample: bit-identical, no added delay or gain.
 *2x/4x use63-tap halfband FIR stages (delay15.5/23.25 output samples).
 * LPF can have transient overshoot; no clipping or gain renormalization here.
 */
float xenolith_quality_process(XenolithQuality *q, XenolithCore *core);
/* Optional PRE DIGITAL insertion, at every internal tick before quantization
 * and halfband/hold reconstruction. OFF callers retain the original process. */
float xenolith_quality_process_filtered(XenolithQuality *q,XenolithCore *core,XenolithFilter *filter);
uint32_t xenolith_quality_ready(const XenolithQuality *q);
float xenolith_quality_rate(const XenolithQuality *q);
/* Foreground preparation: rate1000..min(44100,output_rate), fractional bits2..15,
 * quantizer0=truncate,1=signed nearest,2=first-order error feedback with nearest;
 * reconstruction0=hold,1=linear (one internal-tick interpolation latency).
 * Scale coefficients and the nearest control-block size are prepared here.
 */
void xenolith_quality_prepare_crush(XenolithCrushConfig *c, float output_rate,
    uint32_t base_control_frames, float rate, uint32_t bits,
    uint32_t quantizer, uint32_t reconstruction);
/* Audio owner, external gate muted. Active CRUSH is reclocked, residual and
 * hold history reset, and ready immediately. Other modes only cache config.
 * Default11025/Q7/TRUNC/HOLD matches the preceding fixed CRUSH path exactly.
 */
void xenolith_quality_apply_crush(XenolithQuality *q, XenolithCore *core,
                                const XenolithCrushConfig *c);
/* Unified static digital core. Request1000..44100 or exact88200/176400;
 * intermediate high requests round down to a supported maximum stage.
 * bits0 bypasses quantization exactly; bits1 selects a polarity quantizer;
 * otherwise2..24 remain the existing fractional-bit grids.
 * High stages retain mandatory halfband FIR; reconstruction selects only the
 * low-rate converter. Requested rate survives the fixed cell-count cap.
 * Foreground preparation uses explicit immutable host-rate/control limits.
 */
void xenolith_quality_prepare_digital(XenolithDigitalConfig *c,
    float output_rate, uint32_t base_control_frames, uint32_t cells, float requested_rate,
    uint32_t bits, uint32_t quantizer, uint32_t reconstruction);
/* Audio owner, external gate muted. Preserves oscillator/noise state. A
 * configuration identical to the active one is a no-op, including histories.
 * This is structural configuration, not a per-control-tick modulation API.
 */
void xenolith_quality_apply_digital(XenolithQuality *q, XenolithCore *core,
                                  const XenolithDigitalConfig *c);
/* Audio owner, control tick. Changes only the live grid/scales and clears its
 * residual when precision changes. No phase, FIR, rate, held-sample, scheduler
 * or readiness reset. Structural configured_bits remains the rate API's base.
 * bits0 bypasses exactly; bits2..24 retain their historical fractional grids.
 * bits1 quantizes the SUM to +/-0.5 (TRUNC/ROUND both use polarity). Exact zero
 * returns0 and clears residual, preventing idle pulses/DC at silence. ERROR FB
 * clips input to+/-0.5, adds bounded first-order residual and emits+/-0.5;
 * a zero accumulator chooses positive. It is density/error shaping, not extra
 * amplitude resolution. Residual stays within+/-0.5. Reconstruction/FIR can
 * produce intermediate values/overshoot after this internal quantization.
 * Values above24 clamp24; NULL is a no-op. UI24 may map to bypass0; the DSP
 * value24 itself remains the old true24-fractional-bit setting.
 */
void xenolith_quality_set_live_bits(XenolithQuality *q,uint32_t bits);
/* Audio owner, callback boundary. Admit precision/quantizer-only configuration
 * changes at the SAME effective clock, factor and reconstruction. Caller must
 * also require unchanged cell count/topology. Preserves phases, FIR history,
 * held samples, scheduler/rate ramp and readiness; only quantizer grid/residual
 * can change. Return0 without mutation when a structural transition is needed.
 * Reapply live KEY/SEQ/MOD precision immediately after success if connected.
 */
uint32_t xenolith_quality_update_precision(XenolithQuality *q,
                                         const XenolithDigitalConfig *c);
/* Audio owner, live block boundary. Accepts only unchanged precision,
 * quantizer/reconstruction and low-rate paths (native/ECO/CRUSH), never FIR.
 * Caller must separately ensure the cell count is unchanged. Returns1 on
 * acceptance, preserving oscillator/noise/held/error/scheduler state. The
 * target path code becomes q.factor immediately; actual clock remains given
 * by quality_rate/core.sample_rate. Once retimed, low scheduling stays phase
 * continuous even at native/ECO anchors until a muted structural apply.
 */
uint32_t xenolith_quality_retime(XenolithQuality *q, XenolithCore *core,
                                const XenolithDigitalConfig *c);
/* Once per callback, before its samples. Advances the accepted rate target
 * over50ms of host frames and updates core increments/control timing once.
 * No exponentials, allocations, history clearing, readiness delay or mute.
 */
void xenolith_quality_begin_block(XenolithQuality *q, XenolithCore *core,
                                  uint32_t output_frames);
/* Runtime load relief, independent of the stored digital MAX. Set before
 * begin_block: low/native clocks become nominal*duty, bounded at1000Hz.
 * Duty0.5..1; NaN becomes1. High2x/4x paths retain their FIR input clock but
 * schedule raw oscillator updates at nominal*duty, holding between updates.
 * The core is retimed to that actual update rate; FIR history is retained.
 * This deliberate sample/hold colour is not arbitrary-rate ideal resampling.
 * At1 with no prior relief, the original renderer is unchanged.
 * The governor supplies the smooth duty trajectory. Retiming preserves phase,
 * RNG, drift and converter histories; nonlinear FM at a different clock is
 * not mathematically identical to rendering every original internal sample.
 */
void xenolith_quality_set_load_duty(XenolithQuality *q,float duty);
/* Current nominal ramp clock, before load relief; use for structural matching.
 * quality_rate/core.sample_rate continue to report the actual calculation rate.
 */
float xenolith_quality_nominal_rate(const XenolithQuality *q);
/* PRE DIGITAL filters run at the converter input clock, which remains the
 * full2x/4x FIR clock during high-rate raw-core relief. */
float xenolith_quality_filter_rate(const XenolithQuality *q);
/* Emergency audio-owner gate. Reuse the last RAW core sample at internal fetch
 * points while PRE filtering, quantization, reconstruction and FIR keep running.
 * Oscillator/noise time pauses; this is deadline containment, not pitch-correct
 * skipping. Clear at the next admitted block. No history is erased on either edge.
 */
void xenolith_quality_set_core_hold(XenolithQuality *q,int hold);
#endif
