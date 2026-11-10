/* Actual app functions are inserted verbatim by verify_midi_app_host.py. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fastmath.h"
#include "plat.h"
#include "xenolith_midi.h"
#include "xenolith_paraphony.h"
#include "xenolith_sequence.h"
#include "xenolith_keyboard.h"
#include "xenolith_controls.h"
#define X_PUBLISH_BARRIER() ((void)0)
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
/* COMMAND */
static unsigned checks,scenarios;
static XenolithMidiState midi_notes;
static uint32_t midi_held_count,midi_virtual_keys,midi_usb_link,midi_seen_revision;
static uint8_t midi_recorded[128];
typedef struct {uint32_t status,arrival_ms,generation;} MidiRealtime;
static MidiRealtime midi_realtime[64];
static uint32_t midi_rt_write,midi_rt_read,midi_rt_dropped,midi_rt_reset,midi_rt_applied,midi_rt_generation,midi_clock_port;
static XenolithSequence sequencer;
static XenolithSequenceImage seq_edit;
static SeqCommand seq_queue[64];
static uint32_t seq_write,seq_read,seq_dropped,seq_panic_serial,seq_panic_applied,seq_record_request_mode,seq_record_request_serial;
static uint32_t seq_ui_status,seq_key_valid;
static uint8_t seq_key_note[27];
enum {SEQ_UI_FULL=7};
static uint32_t held_count,drone_latch,panic_latched,running,seq_running,keyboard_dirty,redraw;
static XenolithKeyboardConfig keyboard_edit;
static int32_t keyboard_octave;
static float pitch_hz,mod_key;
static XenolithParaphonyState paraphony_allocator;
static uint32_t event_count,last_event,ms,link_counter;
static struct {uint32_t packet;int port;} packets[256];
static uint32_t packet_write,packet_read;
uint32_t plat_ms(void){return ms;}
uint32_t plat_midi_out_link(void){return link_counter;}
int plat_midi_in(uint32_t *packet,int *port){if(packet_read==packet_write)return 0;*packet=packets[packet_read&255u].packet;*port=packets[packet_read++&255u].port;return 1;}
static void envelope_event(uint32_t e){event_count++;last_event=e;}
static void seq_publish_snapshot(void){seq_edit=sequencer.image;}
/* FUNCTIONS */
static void inject(unsigned port,unsigned status,unsigned note,unsigned velocity)
{packets[packet_write&255u].packet=9u|(status<<8)|(note<<16)|(velocity<<24);packets[packet_write++&255u].port=(int)port;}
static void reset(void)
{
    xenolith_midi_init(&midi_notes);xenolith_sequence_init(&sequencer);seq_edit=sequencer.image;
    xenolith_keyboard_config_defaults(&keyboard_edit);xenolith_paraphony_reset(&paraphony_allocator);
    midi_held_count=midi_virtual_keys=midi_usb_link=midi_seen_revision=0;memset(midi_recorded,0,sizeof midi_recorded);
    midi_rt_write=midi_rt_read=midi_rt_dropped=midi_rt_reset=midi_rt_applied=midi_rt_generation=0;midi_clock_port=255;
    seq_write=seq_read=seq_dropped=seq_panic_serial=seq_panic_applied=seq_record_request_mode=seq_record_request_serial=0;
    seq_ui_status=seq_key_valid=held_count=drone_latch=panic_latched=running=seq_running=keyboard_dirty=redraw=0;
    event_count=last_event=ms=link_counter=packet_write=packet_read=0;keyboard_octave=0;pitch_hz=84;
}
static void physical(unsigned mask)
{unsigned i;held_count=0;for(i=0;i<27;i++)held_count+=(mask>>i)&1u;xenolith_midi_physical(&midi_notes,mask);}
int main(void)
{
    unsigned i,before;reset();
    inject(PORT_USB,0x90,60,100);midi_input();CHECK(running&&midi_held_count==1&&last_event==1&&fabsf(pitch_hz-261.6256f)<.01f);
    inject(PORT_USB,0x80,60,0);midi_input();CHECK(!running&&!midi_held_count&&last_event==0);scenarios++;
    reset();inject(PORT_USB,0x90,60,100);inject(PORT_USB,0x90,60,0);midi_input();CHECK(!running&&!midi_held_count&&last_event==0);scenarios++;
    reset();physical(1u);running=1;inject(PORT_USB,0x90,60,1);midi_input();before=event_count;
    inject(PORT_USB,0x90,60,0);midi_input();CHECK(running&&held_count==1&&last_event==1&&event_count==before);scenarios++;
    reset();drone_latch=running=1;inject(PORT_TRS,0x92,40,127);midi_input();CHECK(!drone_latch&&running);
    inject(PORT_TRS,0x82,40,0);midi_input();CHECK(!running&&last_event==0);scenarios++;
    reset();inject(PORT_USB,0x90,60,100);inject(PORT_TRS,0x91,60,100);midi_input();
    inject(PORT_USB,0xb0,123,0);midi_input();CHECK(midi_held_count==1&&running);
    inject(PORT_TRS,0xb1,120,0);midi_input();CHECK(!midi_held_count&&!running);scenarios++;
    reset();inject(PORT_USB,0x90,0,100);midi_input();CHECK(pitch_hz==20);inject(PORT_USB,0x90,127,100);midi_input();CHECK(pitch_hz==800);scenarios++;
    reset();inject(PORT_USB,0x90,60,100);inject(PORT_TRS,0x90,67,100);midi_input();link_counter++;
    midi_input();CHECK(midi_held_count==1&&running&&xenolith_midi_held(&midi_notes,67));scenarios++;
    /* Tonic step capture receives full MIDI pitches, even outside the panel. */
    reset();sequencer.image.config.enabled=1;seq_edit=sequencer.image;
    CHECK(xenolith_sequence_record_arm(&sequencer,XSEQ_RECORD_STEP,1,0));
    inject(PORT_USB,0x90,0,100);midi_input();seq_audio_commands();CHECK(sequencer.edit_step==1&&sequencer.image.pattern.step[0].note==0);
    inject(PORT_USB,0x80,0,0);midi_input();seq_audio_commands();
    inject(PORT_USB,0x90,127,100);midi_input();seq_audio_commands();CHECK(sequencer.edit_step==2&&sequencer.image.pattern.step[1].note==127);scenarios++;
    /* A panel key and MIDI note sharing a pitch do not release one another's capture. */
    seq_key_valid=1;seq_key_note[0]=127;before=seq_write;
    inject(PORT_USB,0x80,127,0);midi_input();CHECK(seq_write==before&&midi_recorded[127]==0);scenarios++;
    reset();sequencer.image.config.enabled=1;seq_edit=sequencer.image;
    inject(PORT_USB,0xf8,0,0);midi_input();midi_audio_realtime();CHECK(!sequencer.running&&midi_rt_write==0);
    inject(PORT_TRS,0xfa,0,0);inject(PORT_USB,0xf8,0,0);inject(PORT_TRS,0xf8,0,0);midi_input();
    CHECK(midi_rt_write==2);midi_audio_realtime();CHECK(sequencer.running&&sequencer.external_clock);scenarios++;
    /* Local PLAY cancels pending external events without stopping local transport. */
    inject(PORT_TRS,0xfc,0,0);midi_input();midi_clock_port=255;midi_rt_generation++;
    CHECK(xenolith_sequence_play_toggle(&sequencer));midi_audio_realtime();CHECK(!sequencer.external_clock);scenarios++;
    reset();sequencer.image.config.enabled=1;seq_edit=sequencer.image;
    inject(PORT_USB,0xfa,0,0);for(i=0;i<75;i++)inject(PORT_USB,0xf8,0,0);
    midi_input();CHECK(packet_read==32);midi_input();midi_input();CHECK(midi_rt_dropped==1&&midi_clock_port==255);
    midi_audio_realtime();CHECK(!sequencer.running&&midi_rt_read==midi_rt_write);scenarios++;
    printf("{\"passed\":%u,\"scenarios\":%u}\n",checks,scenarios);return 0;
}
