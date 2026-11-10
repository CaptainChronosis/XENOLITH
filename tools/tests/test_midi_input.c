/* SPDX-License-Identifier: MPL-2.0 */
#include "xenolith_midi.h"
#include "xenolith_paraphony.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t packet(unsigned status,unsigned a,unsigned b)
{return 9u|(status<<8)|(a<<16)|(b<<24);}
static XenolithMidiEvent send(XenolithMidiState *s,unsigned port,unsigned status,unsigned a,unsigned b)
{return xenolith_midi_packet(s,packet(status,a,b),port);}
static void invariant(const XenolithMidiState *s)
{
    unsigned i,j,notes=0,physical=0;
    for(i=0;i<128;i++)notes+=xenolith_midi_held(s,i);
    for(i=0;i<27;i++)physical+=(s->physical>>i)&1u;
    CHECK(s->midi_count==notes);CHECK(s->count==notes+physical);CHECK(s->count<=155);
    for(i=0;i<s->count;i++){
        unsigned id=s->order[i];CHECK(id<155);
        CHECK(id<27?((s->physical>>id)&1u):xenolith_midi_held(s,id-27));
        for(j=i+1;j<s->count;j++)CHECK(id!=s->order[j]);
    }
}
int main(void)
{
    XenolithMidiState s,before;XenolithMidiEvent e;uint8_t order[8],identities[8],saved[8];
    unsigned i,j,p,ch,n;
    xenolith_midi_init(&s);invariant(&s);CHECK(xenolith_midi_latest(&s)==255);
    /* A note is held independently by every input/channel; zero velocity
     * removes only its own ownership. All128pitches remain representable. */
    for(n=0;n<128;n++){
        for(p=0;p<2;p++)for(ch=0;ch<16;ch++){
            e=send(&s,p,0x90+ch,n,1+(ch*7)%127);CHECK(e.type==XMIDI_NOTE_ON);
        }
        CHECK(s.midi_count==1&&s.count==1);CHECK(xenolith_midi_latest(&s)==27+n);
        for(p=0;p<2;p++)for(ch=0;ch<16;ch++){
            e=send(&s,p,0x90+ch,n,0);CHECK(e.type==XMIDI_RELEASE);
            CHECK(xenolith_midi_held(&s,n)==!(p==1&&ch==15));
        }
        invariant(&s);
    }
    xenolith_midi_physical(&s,5);send(&s,0,0x90,53,64);send(&s,1,0x91,53,127);
    CHECK(s.count==3);send(&s,0,0xb0,123,0);CHECK(s.count==3&&s.midi_count==1);
    send(&s,1,0xb1,120,0);CHECK(s.count==2&&s.physical==5);invariant(&s);
    /* Port reconnect and panic cannot clear panel keys or another port. */
    send(&s,0,0x90,64,100);send(&s,1,0x90,67,100);xenolith_midi_clear_port(&s,0);
    CHECK(!xenolith_midi_held(&s,64)&&xenolith_midi_held(&s,67)&&s.physical==5);
    xenolith_midi_clear(&s);CHECK(s.count==2&&s.midi_count==0);invariant(&s);
    /* Bad packets and unsupported bend/CC do not mutate any owner. */
    before=s;send(&s,2,0x90,60,100);send(&s,0,0x90,128,100);send(&s,0,0x90,60,128);
    send(&s,0,0xe0,127,127);send(&s,0,0xb0,64,127);CHECK(!memcmp(&before,&s,sizeof(s)));
    for(i=0;i<4;i++){
        unsigned statuses[]={0xf8,0xfa,0xfb,0xfc};e=send(&s,0,statuses[i],0,0);
        CHECK(e.type==XMIDI_REALTIME&&e.status==statuses[i]);CHECK(!memcmp(&before,&s,sizeof(s)));
    }
    /* Simultaneously hold every physical and MIDI key; bounded newest eight
     * adapters retain surviving tokens as old and new notes are released. */
    xenolith_midi_init(&s);xenolith_midi_physical(&s,0x07ffffffu);
    for(i=0;i<128;i++)send(&s,0,0x90,i,100);
    invariant(&s);CHECK(s.count==155);CHECK(xenolith_midi_voicing(&s,order,identities)==8);
    memcpy(saved,identities,8);
    for(i=0;i<8;i++)CHECK(identities[order[i]]==147+i);
    send(&s,0,0x80,127,64);CHECK(xenolith_midi_voicing(&s,order,identities)==8);
    for(i=0;i<8;i++)if(saved[i]!=154)CHECK(saved[i]==identities[i]);
    invariant(&s);
    /* Integration with the unchanged allocator: all voices/cell capacities. */
    for(n=1;n<=8;n++)for(i=1;i<=64;i++){
        XenolithParaphonyState allocator={0};XenolithParaphonyOutput out;float pitches[8];
        for(j=0;j<8;j++)pitches[j]=100.0f+10.0f*j;
        CHECK(xenolith_paraphony_allocate(&allocator,order,pitches,8,i,n,100,100,1,&out));
        CHECK(out.active_voices==(i<n?i:n));
    }
    /* Deterministic mixed traffic catches removal/order underflow. */
    for(i=0;i<3000;i++){
        p=(i/3)%2;ch=(i/7)%16;n=(i*37)%128;
        send(&s,p,(i%5?0x90:0x80)+ch,n,i%3?80:0);
        if(i%19==0)send(&s,p,0xb0+ch,123,0);
        if(i%23==0)xenolith_midi_physical(&s,(i*17391u)&0x07ffffffu);
        if(i%101==0)xenolith_midi_clear_port(&s,p);
        invariant(&s);
    }
    printf("MIDI ownership: %u assertions passed\n",checks);return 0;
}
