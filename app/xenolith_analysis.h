/* SPDX-License-Identifier: MPL-2.0
 * XX29 display analysis only. Never call the transform from the audio callback.
 */
#ifndef XX29_XENOLITH_ANALYSIS_H
#define XX29_XENOLITH_ANALYSIS_H
#include <stdint.h>

#define XENOLITH_ANALYSIS_MAX 4096u
#define XENOLITH_ANALYSIS_MAX_BINS ((XENOLITH_ANALYSIS_MAX/2u)+1u)
#define XENOLITH_ANALYSIS_HALF_INPUT 4160u

/* Display tap only, from the platform's bounded signed24 output words.
 * 0=L,1=R,2=MID. L/R avoid cross-channel cancellation but do not measure
 * total stereo energy. A common mono value is preserved in all three modes.
 * The bounded24bit input contract makes left+right safe in signed32. */
static inline int32_t xenolith_analysis_tap(int32_t left, int32_t right,
                                           uint32_t channel) {
    return channel == 1u ? right : channel == 2u ? (left + right) / 2 : left;
}

/* Approximately40 KiB: put the whole struct in PLAT_POOL on the device.
 * The caller owns full-rate capture/snapshot buffers and snapshot coherence.
 * After process(), real[0..bins) is peak amplitude and imag[0..bins) is
 * positive linear-amplitude spectral flux. The remaining entries are scratch.
 */
typedef struct {
    float real[XENOLITH_ANALYSIS_MAX];
    float imag[XENOLITH_ANALYSIS_MAX];
    float previous[XENOLITH_ANALYSIS_MAX_BINS];
    float sample_rate, frame_flux, window_sum;
    uint32_t fft_size, bins, processed_frames, valid;
} XenolithAnalysis;

void xenolith_analysis_init(XenolithAnalysis *state);
/* n=1024,2048 or4096. Samples are contiguous full-rate mono signed16 PCM.
 * Returns1 on success,0 on invalid input without changing prior results.
 * The periodic Hann has coherent gain0.5. Amplitudes are single-sided and
 * peak-calibrated: a bin-centred full-scale sine reads1.0 (0dBFS), not RMS/PSD.
 * First frame, size change or sample-rate change establishes a flux baseline.
 */
int xenolith_analysis_process(XenolithAnalysis *state, const int16_t *mono,
                              uint32_t n, float sample_rate);
/* Foreground-only optional 2:1 analysis decimation: exactly4160 full-rate
 * signed16 samples =64 preceding history +4096-frame observation. A63tap
 * symmetric Kaiser halfband produces2048 samples at input_rate/2 directly
 * into the existing window/FFT scratch. All FIR history comes from the real
 * snapshot, so there is no artificial zero-state start at a frame boundary.
 * Effective group delay31 input samples; the filter rolls off near the new
 * Nyquist, not a flat passband all the way to it. No extra sample-loop work.
 * Fixed4096 FFT arrays are retained: this option does not release28KiB RAM.
 */
int xenolith_analysis_process_half(XenolithAnalysis *state, const int16_t *mono,
                                   uint32_t count, float input_rate);
const float *xenolith_analysis_magnitudes(const XenolithAnalysis *state);
const float *xenolith_analysis_flux(const XenolithAnalysis *state);
/* Display conversion only: -120dB floor. No impact on returned linear data. */
float xenolith_analysis_dbfs(float peak_amplitude);
/* Optional ideal-model coefficients J0..J8. beta=index/ratio, clamped0..512.
 * About9k points through96,18k above: cache on beta change, never in audio.
 * These nine orders are a limited guide, not total FM energy or the eight-cell
 * measured spectrum. Signed coefficients are returned; draw absolute heights.
 */
void xenolith_analysis_bessel8(float beta, float coefficients[9]);

#endif
