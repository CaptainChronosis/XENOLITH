/* SPDX-License-Identifier: MPL-2.0
 * Slow modulation destinations: audio snapshots, foreground transactions.
 * No preparation, allocation or new clock/count setup runs per audio sample. */
enum { MM_CORE=1u,MM_WET=2u,MM_ECHO=4u,MM_CELLS=8u };
enum { MM_IDLE,MM_CLOCK,MM_COUNT };
typedef struct { float core,wet,echo,cells;uint32_t mask,source_generation; } AppMotionSnapshot;
typedef struct { float core,wet,echo;uint32_t cells,mask; } AppMotionRequest;
static AppMotionSnapshot motion_snapshot[2];
static volatile uint32_t motion_snapshot_serial,motion_applied_mask;
static volatile uint32_t motion_ack_generation,motion_audio_settled=1u;
static volatile float motion_nominal_core=44100.0f;
static uint32_t motion_owner,motion_generation,motion_wet_serial,motion_prefer_count;
static uint32_t motion_cell_reserve,motion_cells=8u,motion_value_mask;
static float motion_core=44100.0f,motion_wet=44100.0f,motion_echo=.4f;
static AppMotionRequest motion_active,motion_pending;
static uint32_t motion_config_mask(const XenolithModConfig *config)
{
    uint32_t i,mask=0u;if(!config->enabled)return 0u;
    for(i=0;i<XENOLITH_MOD_ROUTES;i++){
        const XenolithModRoute *r=&config->route[i];
        if(r->enabled&&r->source&&r->depth!=0.0f&&r->destination>=XM_DST_CORE_RATE&&r->destination<=XM_DST_CELLS)
            mask|=1u<<(r->destination-XM_DST_CORE_RATE);
    }
    return mask;
}
static uint32_t motion_route_mask(void){return motion_config_mask(&mod_edit)|motion_applied_mask;}
static uint32_t motion_effective_cells(void){return motion_cell_reserve?motion_cells:requested_cells;}
static uint32_t motion_budget_cells(void){return motion_cell_reserve?64u:motion_effective_cells();}
static float motion_base_core(void)
{
    return keyboard_target_active(XKEY_CORE_RATE,keyboard_override_active&1u)?keyboard_override_value:
        (seq_clock_edit.mask&1u)?seq_clock_edit.core:core_max_rate;
}
static float motion_base_wet(void)
{
    return keyboard_target_active(XKEY_WET_RATE,keyboard_override_active&2u)?keyboard_override_value:
        (seq_clock_edit.mask&2u)?seq_clock_edit.wet:exhaust_rate;
}
static float motion_effective_core(void)
{
    float base=motion_base_core();uint32_t mask=motion_route_mask();
    if(motion_owner==MM_COUNT)return motion_active.core;
    if(motion_owner==MM_CLOCK){base=motion_active.core;mask|=motion_active.mask;}
    else if((mask&MM_CORE)&&(motion_value_mask&MM_CORE))base=motion_core;
    if(mask&MM_CORE)base=fm_clampf(base,1000.0f,44100.0f);
    return base;
}
static float motion_effective_wet(void)
{
    if(motion_owner)return motion_active.wet;
    return (motion_route_mask()&MM_WET)&&(motion_value_mask&MM_WET)?motion_wet:motion_base_wet();
}
static float motion_effective_echo(void)
{
    if(motion_owner)return motion_active.echo;
    return (motion_route_mask()&MM_ECHO)&&(motion_value_mask&MM_ECHO)?motion_echo:exhaust_time;
}
static void motion_audio_publish(void)
{
    uint32_t next=motion_snapshot_serial+1u;AppMotionSnapshot *s=&motion_snapshot[next&1u];
    motion_nominal_core=xenolith_quality_nominal_rate(&quality);
    s->core=modulator.offset[XM_DST_CORE_RATE];s->wet=modulator.offset[XM_DST_WET_RATE];
    s->echo=modulator.offset[XM_DST_ECHO_TIME];s->cells=modulator.offset[XM_DST_CELLS];
    s->mask=motion_applied_mask;s->source_generation=mod_config_applied;
    X_PUBLISH_BARRIER();motion_snapshot_serial=next;
}
static void motion_reset(void)
{
    motion_owner=MM_IDLE;motion_prefer_count=motion_value_mask=motion_wet_serial=0u;
    motion_cells=requested_cells;motion_cell_reserve=(core.cells&&core.cells!=requested_cells)||(motion_applied_mask&MM_CELLS);
    motion_core=core_max_rate;motion_wet=exhaust_rate;motion_echo=exhaust_time;
    if(++motion_generation==0u)++motion_generation;
}
static uint32_t motion_cell_target(float value,uint32_t mask)
{
    uint32_t target;if(!(mask&MM_CELLS))target=requested_cells;
    else{
        value=fm_clampf((float)requested_cells+value,1.0f,64.0f);
        if(value<(float)motion_cells+.75f&&value>(float)motion_cells-.75f)return motion_cells;
        target=(uint32_t)(value+.5f);
    }
    if(target>motion_cells+4u)target=motion_cells+4u;
    if(target+4u<motion_cells)target=motion_cells-4u;
    return target;
}
static void motion_begin(uint32_t owner,const AppMotionRequest *request)
{
    float held_wet=motion_effective_wet(),held_echo=motion_effective_echo();
    motion_active=*request;motion_owner=owner;motion_wet_serial=0u;
    if(++motion_generation==0u)++motion_generation;
    if(owner==MM_COUNT){
        /* Freeze the admitted clock, not temporary governor duty: copying the
         * actual clock here compounds its reduction on every count gesture. */
        motion_active.core=motion_nominal_core;motion_active.wet=held_wet;motion_active.echo=held_echo;
        motion_cells=request->cells;
        keyboard_update(plat_keys(),0u);
    }else{
        uint32_t old_mask=motion_value_mask;
        float old_wet=motion_wet,old_echo=motion_echo;
        motion_core=request->core;motion_wet=request->wet;motion_echo=request->echo;motion_value_mask=request->mask;
        if(((old_mask^request->mask)&(MM_WET|MM_ECHO))||fm_fabsf(old_wet-motion_wet)>1.0f||fm_fabsf(old_echo-motion_echo)>.00001f){
            exhaust_publish();motion_wet_serial=exhaust_config_serial;
        }
    }
    digital_publish();
}
static void motion_service(void)
{
    static uint32_t last;uint32_t now=plat_ms(),before=motion_snapshot_serial,clock_changed,count_changed;
    AppMotionSnapshot s;AppMotionRequest next;
    if(now-last<50u)return;last=now;
    X_PUBLISH_BARRIER();s=motion_snapshot[before&1u];X_PUBLISH_BARRIER();
    if(before!=motion_snapshot_serial||s.source_generation!=mod_config_serial)return;
    if(!(s.mask|motion_value_mask)&&!motion_cell_reserve&&motion_owner==MM_IDLE){
        motion_core=motion_base_core();motion_wet=motion_base_wet();motion_echo=exhaust_time;return;
    }
    if(s.mask&MM_CELLS)motion_cell_reserve=1u;
    next.mask=s.mask;next.core=motion_base_core();next.wet=motion_base_wet();next.echo=exhaust_time;
    if(s.mask&MM_CORE)next.core=fm_clampf(fm_clampf(next.core,1000.0f,44100.0f)*fm_exp2f(fm_clampf(s.core,-16,16)),1000,44100);
    if(s.mask&MM_WET)next.wet=fm_clampf(next.wet*fm_exp2f(fm_clampf(s.wet,-16,16)),1378.125f,44100);
    if(s.mask&MM_ECHO)next.echo=fm_clampf(next.echo*fm_exp2f(fm_clampf(s.echo,-16,16)),.002f,32);
    next.cells=motion_cell_target(s.cells,s.mask);motion_pending=next;
    if(motion_owner!=MM_IDLE){
        if(motion_ack_generation!=motion_generation)return;
        motion_prefer_count=motion_owner==MM_CLOCK;motion_owner=MM_IDLE;motion_wet_serial=0u;
    }
    if(motion_cell_reserve&&!(motion_route_mask()&MM_CELLS)&&motion_cells==requested_cells
       &&active_cells==requested_cells&&motion_audio_settled){motion_cell_reserve=0u;digital_publish();}
    count_changed=motion_cell_reserve&&next.cells!=motion_cells;
    clock_changed=((next.mask^motion_value_mask)&(MM_CORE|MM_WET|MM_ECHO))
        ||fm_fabsf(next.core-motion_core)>1.0f||fm_fabsf(next.wet-motion_wet)>1.0f||fm_fabsf(next.echo-motion_echo)>.00001f;
    if(!motion_audio_settled)return;
    if(count_changed&&(motion_prefer_count||!clock_changed))motion_begin(MM_COUNT,&next);
    else if(clock_changed)motion_begin(MM_CLOCK,&next);
    else if(count_changed)motion_begin(MM_COUNT,&next);
}
