/* SPDX-License-Identifier: MPL-2.0
 * CPU1 owns only the existing analyzer and frozen capture during this call.
 * Foreground dispatches and joins before reading results, drawing or returning
 * to storage/OTA service. Audio interrupts CPU0 normally and never waits here.
 * No additional FFT arrays, oscillator changes or per-sample worker messages.
 */
typedef struct {
    int32_t start_result;
    uint32_t dispatches, completed, fallbacks, local_frames, invalid_results;
    uint32_t join_us, max_join_us, worker_us, max_worker_us;
    uint32_t on, gave_up, bad_jobs;
} XenolithWorkerStats;
static volatile XenolithWorkerStats analysis_worker_stats;
static volatile uint32_t analysis_worker_token, analysis_worker_done;
static volatile uint32_t analysis_worker_done_inverse, analysis_worker_elapsed;

static void analysis_worker_init(void)
{
    /* app_init only, before platform audio/timers. An unavailable or previously
     * barred worker leaves the instrument on the original CPU0 route. */
    analysis_worker_stats.start_result=plat_cpu2_set(1);
    analysis_worker_stats.on=(uint32_t)plat_cpu2_on();
    analysis_worker_stats.gave_up=(uint32_t)plat_cpu2_gave_up();
}

static void analysis_worker_job(uint32_t token)
{
    uint32_t before=plat_ticks24();
    int accepted=xenolith_analysis_process(&analyzer,analysis_input,1024u,44100.0f);
    analysis_worker_elapsed=(plat_ticks24()-before)/PLAT_TICKS_PER_US;
    plat_cpu2_sync();
    analysis_worker_done=accepted?token:0u;
    analysis_worker_done_inverse=~analysis_worker_done;
    plat_cpu2_sync();
}

static int analysis_worker_result_valid(uint32_t token)
{
    uint32_t i;
    if(analysis_worker_done!=token||analysis_worker_done_inverse!=~token||
       analyzer.valid!=1u||analyzer.fft_size!=1024u||analyzer.bins!=513u||
       analyzer.sample_rate!=44100.0f||analyzer.window_sum!=512.0f||
       !(analyzer.frame_flux>=0.0f&&analyzer.frame_flux<=1.0f))return 0;
    /* Normalized signed16 input and coherent-gain compensated Hann imply a
     * peak magnitude <=2. This also rejects nonfinite display results. It is
     * a sanity bound, not proof against arbitrary shared-RAM corruption. */
    for(i=0;i<513u;i++){
        if(!(analyzer.real[i]>=0.0f&&analyzer.real[i]<=2.001f&&
             analyzer.imag[i]>=0.0f&&analyzer.imag[i]<=2.001f))return 0;
    }
    return 1;
}

static int analysis_compute(uint32_t n,uint32_t frames,uint32_t half_rate)
{
    /* The bounded first deployment offloads only1K. Larger transforms retain
     * their existing CPU0 implementation; no global timeout/rate cap change. */
    if(n==1024u&&!half_rate&&frames==1024u&&plat_cpu2_on()){
        uint32_t token=++analysis_worker_token,before=plat_ticks24();
        if(!token)token=++analysis_worker_token;
        analysis_worker_done=0u;analysis_worker_done_inverse=0u;
        plat_cpu2_sync();
        if(plat_cpu2_run(analysis_worker_job,token)){
            int result;
            analysis_worker_stats.dispatches++;
            /* Always wait: busy()==0 can also mean a fault/rejected job. The
             * BSP either joins successfully or holds/retires CPU1 before -1. */
            result=plat_cpu2_wait();
            plat_cpu2_sync();
            analysis_worker_stats.join_us=(plat_ticks24()-before)/PLAT_TICKS_PER_US;
            if(analysis_worker_stats.join_us>analysis_worker_stats.max_join_us)
                analysis_worker_stats.max_join_us=analysis_worker_stats.join_us;
            if(result==0&&analysis_worker_result_valid(token)){
                analysis_worker_stats.worker_us=analysis_worker_elapsed;
                if(analysis_worker_elapsed>analysis_worker_stats.max_worker_us)
                    analysis_worker_stats.max_worker_us=analysis_worker_elapsed;
                analysis_worker_stats.completed++;
                analysis_worker_stats.on=1u;
                return 1;
            }
            if(result==0){
                /* Joined but malformed result: explicitly hold the worker.
                 * This application never restarts it after initialization. */
                plat_cpu2_set(0);analysis_worker_stats.invalid_results++;
            }
            analysis_worker_stats.fallbacks++;
            /* A failed worker may have updated any part of the FFT or its
             * previous-frame flux. Reestablish the baseline before retrying. */
            xenolith_analysis_init(&analyzer);
        }
    }
    analysis_worker_stats.on=(uint32_t)plat_cpu2_on();
    analysis_worker_stats.gave_up=(uint32_t)plat_cpu2_gave_up();
    analysis_worker_stats.bad_jobs=plat_cpu2_bad();
    analysis_worker_stats.local_frames++;
    return half_rate?xenolith_analysis_process_half(&analyzer,analysis_input,frames,44100.0f)
                    :xenolith_analysis_process(&analyzer,analysis_input,n,44100.0f);
}
