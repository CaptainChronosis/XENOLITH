/* SPDX-License-Identifier: MPL-2.0 */
#include "xenolith_midi.h"

static void remove_identity(XenolithMidiState *s,uint32_t id)
{
    uint32_t i,w=0u;
    for(i=0;i<s->count;i++)if(s->order[i]!=id)s->order[w++]=s->order[i];
    s->count=w;
}
static void press_identity(XenolithMidiState *s,uint32_t id)
{
    remove_identity(s,id);
    if(s->count<XMIDI_IDENTITIES)s->order[s->count++]=(uint8_t)id;
    s->revision++;
}
uint32_t xenolith_midi_held(const XenolithMidiState *s,uint32_t note)
{
    return s&&note<128u&&(s->channels[0][note]||s->channels[1][note]);
}
void xenolith_midi_init(XenolithMidiState *s)
{
    uint32_t i;uint8_t *bytes=(uint8_t*)s;
    for(i=0;i<sizeof(*s);i++)bytes[i]=0u;
    for(i=0;i<8u;i++)s->virtual_identity[i]=XMIDI_EMPTY;
}
void xenolith_midi_physical(XenolithMidiState *s,uint32_t keys)
{
    uint32_t i,old=s->physical;keys&=0x07ffffffu;
    for(i=0;i<27u;i++)if((old&~keys)&(1u<<i)){remove_identity(s,i);s->revision++;}
    for(i=0;i<27u;i++)if((keys&~old)&(1u<<i))press_identity(s,i);
    s->physical=keys;
}
static void release_channel(XenolithMidiState *s,uint32_t port,uint32_t channel)
{
    uint32_t n;uint16_t mask=(uint16_t)~(1u<<channel);
    for(n=0;n<128u;n++)if(s->channels[port][n]&(1u<<channel)){
        s->channels[port][n]&=mask;
        if(!xenolith_midi_held(s,n)){remove_identity(s,n+27u);s->midi_count--;}
        s->revision++;
    }
}
void xenolith_midi_clear_port(XenolithMidiState *s,uint32_t port)
{
    uint32_t ch;if(port>=XMIDI_PORTS)return;
    for(ch=0;ch<16u;ch++)release_channel(s,port,ch);
}
void xenolith_midi_clear(XenolithMidiState *s)
{
    xenolith_midi_clear_port(s,XMIDI_USB);xenolith_midi_clear_port(s,XMIDI_TRS);
}
XenolithMidiEvent xenolith_midi_packet(XenolithMidiState *s,uint32_t packet,uint32_t port)
{
    XenolithMidiEvent event={XMIDI_IGNORE,0u,0u};
    uint32_t status=(packet>>8)&255u,note=(packet>>16)&255u,value=(packet>>24)&255u;
    uint32_t kind=status&0xf0u,ch=status&15u;
    if(port>=XMIDI_PORTS)return event;
    event.status=status;event.note=note;
    if(status==0xf8u||status==0xfau||status==0xfbu||status==0xfcu){event.type=XMIDI_REALTIME;return event;}
    if(note>=128u||value>=128u)return event;
    if(kind==0x90u&&value){
        if(!xenolith_midi_held(s,note))s->midi_count++;
        s->channels[port][note]|=(uint16_t)(1u<<ch);press_identity(s,note+27u);
        event.type=XMIDI_NOTE_ON;
    }else if(kind==0x80u||(kind==0x90u&&!value)){
        if(s->channels[port][note]&(1u<<ch)){
            s->channels[port][note]&=(uint16_t)~(1u<<ch);
            if(!xenolith_midi_held(s,note)){remove_identity(s,note+27u);s->midi_count--;}
            s->revision++;
        }
        event.type=XMIDI_RELEASE;
    }else if(kind==0xb0u&&(note==120u||note==123u)){
        release_channel(s,port,ch);event.type=XMIDI_RELEASE;
    }
    return event;
}
uint32_t xenolith_midi_latest(const XenolithMidiState *s)
{
    return s->count?s->order[s->count-1u]:XMIDI_EMPTY;
}
uint32_t xenolith_midi_voicing(XenolithMidiState *s,uint8_t order[8],uint8_t identities[8])
{
    uint32_t n=s->count<8u?s->count:8u,start=s->count-n,i,j;
    for(i=0;i<8u;i++){
        uint32_t retain=0u;
        for(j=start;j<s->count;j++)if(s->virtual_identity[i]==s->order[j])retain=1u;
        if(!retain)s->virtual_identity[i]=XMIDI_EMPTY;
    }
    for(j=0;j<n;j++){
        uint32_t id=s->order[start+j],slot=8u;
        for(i=0;i<8u;i++)if(s->virtual_identity[i]==id)slot=i;
        if(slot==8u)for(i=0;i<8u;i++)if(s->virtual_identity[i]==XMIDI_EMPTY){slot=i;s->virtual_identity[i]=(uint8_t)id;break;}
        order[j]=(uint8_t)slot;
    }
    for(i=0;i<8u;i++)identities[i]=s->virtual_identity[i];
    return n;
}
