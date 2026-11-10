/* SPDX-License-Identifier: MPL-2.0
 * Foreground-only display analysis. Included after the application text helpers.
 * Full-rate PCM capture and published phase words are written only by audio.
 * No transform, window drawing, allocation or waiting occurs in that callback. */
#define XV_COLS 112u
#define XV_ROWS 16u
static const uint32_t XV_RATES[4]={4u,10u,20u,30u};
static const uint32_t XV_SIZES[4]={1024u,2048u,4096u,2048u};
static const char *XV_MODE_NAMES[4]={"1K","2K","4K","2K/2"};
static const uint32_t XV_SPANS[4]={1200u,3000u,6000u,22050u};
static uint32_t xv_rate_index=3u,xv_size_index=2u,xv_span_index;
static uint32_t xv_capture_start,xv_last_accepted,xv_draw_due;
/* Retained for offline RAM profiling; deliberately absent from the play UI. */
static volatile uint32_t xv_fft_us,xv_snapshot_age_us;
static uint32_t xv_capture_ticks,xv_capture_end;
static uint32_t xv_fft_count,xv_draw_count;
static volatile uint32_t xv_measured_fft_hz,xv_measured_view_hz;
static uint32_t xv_clock,xv_accumulator,xv_phase_accumulator,xv_stats_clock;
static uint32_t xv_history_head,xv_history_count,xv_phase[8];
static uint32_t xv_phase_cells=8u;
static uint8_t xv_magnitude[XV_ROWS][XV_COLS] PLAT_POOL;
static uint8_t xv_flux[XV_ROWS][XV_COLS] PLAT_POOL;
static float xv_model_beta=-1.0f,xv_model_weights[9];
static uint32_t xv_model_clock,xv_model_valid;

static const char *xv_channel_name(void)
{
    return analysis_channel==1u?"R":analysis_channel==2u?"MID":"L";
}

static uint32_t xv_display_span(void)
{
    uint32_t maximum=xv_size_index==3u?11025u:22050u;
    return XV_SPANS[xv_span_index]>maximum?maximum:XV_SPANS[xv_span_index];
}

static void xv_reset(void)
{
    xenolith_analysis_init(&analyzer);
    xv_history_count=0;xv_history_head=0;xv_last_accepted=0;
    xv_capture_start=analysis_published;xv_draw_due=1;
}

static void analysis_setup(void)
{
    xv_clock=plat_ms();xv_stats_clock=xv_clock;
    xv_reset();
}

static uint16_t xv_colour(uint32_t magnitude,uint32_t flux)
{
    uint32_t lift=magnitude*flux/255u;
    return RGB((152u*magnitude+103u*lift)/255u,48u*lift/255u,36u*lift/255u);
}

static void xv_history(void)
{
    uint32_t column,maxbin=(uint32_t)((float)xv_display_span()*(float)analyzer.fft_size/analyzer.sample_rate);
    const float *mag=xenolith_analysis_magnitudes(&analyzer);
    const float *flux=xenolith_analysis_flux(&analyzer);
    if(maxbin>=analyzer.bins)maxbin=analyzer.bins-1u;
    xv_history_head=(xv_history_head+1u)%XV_ROWS;
    for(column=0;column<XV_COLS;column++){
        uint32_t first=column*maxbin/XV_COLS,last=(column+1u)*maxbin/XV_COLS,bin;
        float amplitude=0.0f,change=0.0f,db;
        if(last<first)last=first;if(last>=analyzer.bins)last=analyzer.bins-1u;
        for(bin=first;bin<=last;bin++){
            if(mag[bin]>amplitude)amplitude=mag[bin];
            if(bin&&flux[bin]>change)change=flux[bin];
        }
        db=xenolith_analysis_dbfs(amplitude);
        xv_magnitude[xv_history_head][column]=(uint8_t)clamp((int32_t)((db+90.0f)*(255.0f/90.0f)),0,255);
        xv_flux[xv_history_head][column]=(uint8_t)clamp((int32_t)(255.0f*change/(amplitude+0.0000001f)),0,255);
    }
    if(xv_history_count<XV_ROWS)xv_history_count++;
}

static void analysis_tick(uint32_t now)
{
    uint32_t elapsed=now-xv_clock,n=XV_SIZES[xv_size_index],i;
    uint32_t frames=xv_size_index==3u?XENOLITH_ANALYSIS_HALF_INPUT:n;
    uint32_t signal_live=running||output_peak>256u;
    xv_clock=now;
    if(elapsed>1000u)elapsed=1000u;
    xv_accumulator+=elapsed*XV_RATES[xv_rate_index];
    xv_phase_accumulator+=elapsed*30u;
    if(xv_phase_accumulator>=1000u){
        uint32_t serial=phase_serial,phase[8],cells=phase_cells;
        xv_phase_accumulator%=1000u;
        if(running||envelope_level>0.0f){
            for(i=0;i<8u;i++)phase[i]=phase_published[i];
            if(phase_serial==serial){
                for(i=0;i<8u;i++)xv_phase[i]=phase[i];
                xv_phase_cells=cells;
            }
        }
        xv_draw_due=1;
    }
    if(xv_accumulator>=1000u){
        uint32_t end=analysis_published,ticks=plat_ticks24();
        xv_accumulator%=1000u; /* skip missed deadlines; never catch up in bursts */
        if(signal_live&&end!=xv_last_accepted&&(uint32_t)(end-xv_capture_start)>=frames){
            X_PUBLISH_BARRIER();
            for(i=0;i<frames;i++)analysis_input[i]=analysis_ring[(end-frames+i)&ANALYSIS_RING_MASK];
            X_PUBLISH_BARRIER();
            if(analysis_published==end){
                uint32_t before=plat_ticks24();
                uint32_t accepted=(uint32_t)analysis_compute(n,frames,xv_size_index==3u);
                if(accepted){
                    xv_fft_us=(plat_ticks24()-before)/PLAT_TICKS_PER_US;
                    xv_capture_ticks=ticks;xv_capture_end=end;xv_last_accepted=end;
                    lights_accept(end);
                    xv_fft_count++;xv_history();xv_draw_due=1;
                }
            }
        }
    }
    if(now-xv_stats_clock>=1000u){
        uint32_t span=now-xv_stats_clock;
        xv_measured_fft_hz=(xv_fft_count*1000u+span/2u)/span;
        xv_measured_view_hz=(xv_draw_count*1000u+span/2u)/span;
        xv_fft_count=xv_draw_count=0;xv_stats_clock=now;
    }
    {
        float beta=topology_edit.mode==XTOPO_PHASE?fm_index:fm_index/fm_ratio;
        if(topology_edit.growth!=XTOPO_PARALLEL||topology_edit.mode>XTOPO_PHASE){xv_model_valid=0u;xv_model_beta=-1.0f;}
        else if(beta!=xv_model_beta&&now-xv_model_clock>=100u){
            xv_model_valid=beta<=512.0f;
            if(xv_model_valid)xenolith_analysis_bessel8(beta,xv_model_weights);
            xv_model_beta=beta;xv_model_clock=now;
        }
    }
}

static void draw_analysis(void)
{
    char text[24];uint32_t i,row;
    uint32_t signal_live=running||output_peak>256u;
    cv_begin(240,96,C_BLACK);
    cv_line(6,8,233,8,C_LINE);cv_line(6,8,6,82,C_LINE);cv_line(233,8,233,82,C_LINE);
    cv_rect(12,0,126,15,C_BLACK);cv_rect(195,0,37,15,C_BLACK);
    cv_text(14,0,&FONT_XS,analysis_channel==1u?"[ SPECTRUM / R ]":analysis_channel==2u?"[ SPECTRUM / MID ]":"[ SPECTRUM / L ]",C_AMB);
    cv_text(202,0,&FONT_XS,running?"RUN":(signal_live?"TAIL":"HOLD"),signal_live?C_HI:C_GRAY);
    cv_text(14,17,&FONT_XS,"0 dBFS",C_DIM);
    cv_line(6,82,233,82,C_LINE);
    if(analyzer.valid){
        for(i=0;i<XV_COLS;i++){
            uint32_t m=xv_magnitude[xv_history_head][i],f=xv_flux[xv_history_head][i];
            int32_t height=(int32_t)(m*52u/255u);
            if(height)cv_rect(8+(int32_t)i*2,82-height,2,height,xv_colour(m,f));
        }
    }else cv_text(74,48,&FONT_XS,signal_live?"ACQUIRING":"[ PLAY ]",C_GRAY);
    cv_text(14,68,&FONT_XS,"-90",C_DIM);
    cv_text(8,83,&FONT_XS,"0",C_DIM);
    number_text((int32_t)xv_display_span(),text);cv_text(196,83,&FONT_XS,text,C_DIM);cv_text(226,83,&FONT_XS,"Hz",C_DIM);
    cv_blit(0,0);
    cv_begin(240,35,C_BLACK);
    cv_line(6,0,6,33,C_LINE);cv_line(233,0,233,33,C_LINE);
    for(row=0;row<xv_history_count;row++){
        uint32_t source=(xv_history_head+XV_ROWS-row)%XV_ROWS;
        for(i=0;i<XV_COLS;i++)cv_rect(8+(int32_t)i*2,(int32_t)row*2,2,2,xv_colour(xv_magnitude[source][i],xv_flux[source][i]));
    }
    cv_blit(0,96);
    cv_begin(240,47,C_BLACK);
    cv_text(8,0,&FONT_XS,"[ PHASE / C1 ]",C_DIM);
    number_text((int32_t)(xv_phase_cells<8u?xv_phase_cells:8u),text);cv_text(176,0,&FONT_XS,text,C_GRAY);
    cv_text(188,0,&FONT_XS,"/",C_DIM);number_text((int32_t)xv_phase_cells,text);cv_text(202,0,&FONT_XS,text,C_GRAY);
    for(i=0;i<8u;i++){
        uint32_t relative=(xv_phase[i]-xv_phase[0])&0x1fffffffu;
        float angle=(float)relative*(6.28318530718f/536870912.0f);
        int32_t x=20+(int32_t)i*28,y=26;
        cv_rect(x-8,y-8,17,1,C_LINE);cv_rect(x-8,y+8,17,1,C_LINE);
        cv_rect(x-8,y-8,1,17,C_LINE);cv_rect(x+8,y-8,1,17,C_LINE);
        if(i<xv_phase_cells)cv_line(x,y,x+(int32_t)(7.0f*fm_sinf(angle)),y-(int32_t)(7.0f*fm_cosf(angle)),C_HI);
        number_text((int32_t)i+1,text);cv_text(x-3,35,&FONT_XS,text,i?C_GRAY:C_HI);
    }
    cv_blit(0,131);
    cv_begin(240,62,C_BLACK);
    cv_text(8,1,&FONT_XS,"[ SIDEBANDS ]",C_DIM);
    cv_text(200,1,&FONT_XS,"MODEL",C_DIM);
    if(xv_model_valid){
        for(i=0;i<9u;i++){
            int32_t height=clamp((int32_t)(fm_fabsf(xv_model_weights[i])*14.0f),0,14);
            cv_rect(8+(int32_t)i*24,27-height,16,height,C_AMB);
        }
    }else cv_text(8,18,&FONT_XS,"UNAVAILABLE",C_DIM);
    cv_line(8,39,231,39,C_LINE);
    cv_text(8,47,&FONT_XS,"PRESET L / R / MID",C_GRAY);
    cv_text(206,47,&FONT_XS,"EDIT",C_DIM);
    cv_blit(0,178);
    xv_draw_count++;
    if(analyzer.valid)xv_snapshot_age_us=(plat_ticks24()-xv_capture_ticks)/PLAT_TICKS_PER_US;
}
