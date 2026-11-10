/* SPDX-License-Identifier: MPL-2.0
 * XX29 host mocks compile the actual platform function bodies, not a copied model.
 * Generated *_implementation.inc files are extracted by button_snapshot_test.py. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"

static unsigned assertions;
#define CHECK(x) do { ++assertions; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static struct { volatile uint32_t buttons,pressed,released; } fm1_in;
static struct { uint8_t btn[NB]; } panel;
static struct { volatile uint32_t btn; } remote;
static uint32_t mirror_buttons;
static uint32_t mirror_in_buttons(void) { return mirror_buttons; }
static unsigned irq_enabled=1,lock_calls,unlock_calls;
static uint32_t inject_press_on_unlock;
static uint32_t fm1__lock(void) {
    CHECK(irq_enabled);irq_enabled=0;lock_calls++;return 0;
}
static void fm1__unlock(uint32_t ignored) {
    (void)ignored;CHECK(!irq_enabled);irq_enabled=1;unlock_calls++;
    if(inject_press_on_unlock){
        fm1_in.buttons|=inject_press_on_unlock;
        fm1_in.pressed|=inject_press_on_unlock;inject_press_on_unlock=0;
    }
}
#define plat_button_snapshot device_snapshot
#define plat_buttons device_buttons
#include "device_implementation.inc"
#undef plat_button_snapshot
#undef plat_buttons

static uint32_t held_btn,pressed_btn,host_mirror_buttons;
static uint32_t host_mirror_in_buttons(void) { return host_mirror_buttons; }
#define button_snapshot_previous host_snapshot_previous
#define plat_button_snapshot host_snapshot
#define plat_buttons host_buttons
#define plat_host_button_level host_level
#define mirror_in_buttons host_mirror_in_buttons
#include "host_implementation.inc"
#undef button_snapshot_previous
#undef plat_button_snapshot
#undef plat_buttons
#undef plat_host_button_level
#undef mirror_in_buttons

static void reset(void) {
    unsigned i;
    memset((void*)&fm1_in,0,sizeof fm1_in);remote.btn=mirror_buttons=0;
    for(i=0;i<NB;i++)panel.btn[i]=(uint8_t)((i+2u)%NB);
    button_snapshot_previous=0;inject_press_on_unlock=0;irq_enabled=1;
    lock_calls=unlock_calls=0;
    held_btn=pressed_btn=host_mirror_buttons=host_snapshot_previous=0;
}
static void physical(unsigned label,int down) {
    uint32_t bit=1u<<panel.btn[label];
    if(down){fm1_in.pressed|=bit&~fm1_in.buttons;fm1_in.buttons|=bit;}
    else{fm1_in.released|=bit&fm1_in.buttons;fm1_in.buttons&=~bit;}
}

static void device_tests(void) {
    uint32_t held,pressed;unsigned i;
    reset();physical(B_HOME,1);physical(B_HOME,0);
    CHECK(device_buttons()==0 && fm1_in.pressed==(1u<<panel.btn[B_HOME]));
    device_snapshot(&held,&pressed);
    CHECK(held==0 && pressed==(1u<<B_HOME));
    CHECK(fm1_in.pressed==0 && fm1_in.released==(1u<<panel.btn[B_HOME]));
    device_snapshot(&held,&pressed);CHECK(held==0 && pressed==0);
    physical(B_HOME,1);device_snapshot(&held,&pressed);
    CHECK(held==(1u<<B_HOME) && pressed==(1u<<B_HOME));
    for(i=0;i<100;i++){device_snapshot(&held,&pressed);CHECK(held==(1u<<B_HOME) && pressed==0);}
    physical(B_HOME,0);device_snapshot(&held,&pressed);CHECK(held==0 && pressed==0);
    /* Two quick taps coalesce to one bit, not a fictitious queued press count. */
    physical(B_EDIT,1);physical(B_EDIT,0);physical(B_EDIT,1);physical(B_EDIT,0);
    device_snapshot(&held,&pressed);CHECK(held==0 && pressed==(1u<<B_EDIT));
    device_snapshot(&held,&pressed);CHECK(pressed==0);
    /* Each matrix id is mapped through both default and calibrated maps. */
    for(i=0;i<NB;i++){
        reset();physical(i,1);device_snapshot(&held,&pressed);
        CHECK(held==(1u<<i) && pressed==(1u<<i));
        panel.btn[i]=(uint8_t)(NB-1u-i);fm1_in.buttons=fm1_in.pressed=1u<<panel.btn[i];
        device_snapshot(&held,&pressed);CHECK((held&(1u<<i)) && (pressed&(1u<<i)));
    }
    reset();remote.btn=1u<<B_FX;mirror_buttons=1u<<B_FX;physical(B_FX,1);
    device_snapshot(&held,&pressed);CHECK(held==(1u<<B_FX) && pressed==(1u<<B_FX));
    device_snapshot(&held,&pressed);CHECK(pressed==0);
    physical(B_FX,0);device_snapshot(&held,&pressed);CHECK(held==(1u<<B_FX) && pressed==0);
    remote.btn=0;device_snapshot(&held,&pressed);CHECK(held==(1u<<B_FX) && pressed==0);
    mirror_buttons=0;device_snapshot(&held,&pressed);CHECK(held==0 && pressed==0);
    remote.btn=1u<<B_FX;device_snapshot(&held,&pressed);CHECK(pressed==(1u<<B_FX));
    reset();physical(B_ENV,1);device_snapshot(&held,&pressed);CHECK(pressed==(1u<<B_ENV));
    remote.btn=mirror_buttons=1u<<B_ENV;
    device_snapshot(&held,&pressed);CHECK(pressed==0); /* no second virtual edge while held */
    remote.btn=1u<<B_REC;mirror_buttons=1u<<B_PLAY;
    device_snapshot(&held,&pressed);CHECK(held==((1u<<B_ENV)|(1u<<B_REC)|(1u<<B_PLAY)));
    CHECK(pressed==((1u<<B_REC)|(1u<<B_PLAY)));
    /* Press arrives immediately after IRQs are restored: neither the held bit
     * nor the edge leaks into this snapshot; both appear once in the next. */
    reset();inject_press_on_unlock=1u<<panel.btn[B_HOME];
    device_snapshot(&held,&pressed);CHECK(held==0 && pressed==0);
    CHECK(fm1_in.pressed==(1u<<panel.btn[B_HOME]));
    device_snapshot(&held,&pressed);CHECK(held==(1u<<B_HOME) && pressed==(1u<<B_HOME));
    device_snapshot(&held,&pressed);CHECK(held==(1u<<B_HOME) && pressed==0);
    CHECK(lock_calls==3 && unlock_calls==3 && irq_enabled);
    reset();physical(B_SAVE,1);physical(B_SAVE,0);device_snapshot(0,0);
    device_snapshot(&held,&pressed);CHECK(held==0 && pressed==0);
    remote.btn=1u<<B_GLO;device_snapshot(0,&pressed);CHECK(pressed==(1u<<B_GLO));
    device_snapshot(&held,0);CHECK(held==(1u<<B_GLO));
}
static void host_tests(void) {
    uint32_t held,pressed;unsigned i;
    reset();host_level(1u<<B_HOME,1);host_level(1u<<B_HOME,0);
    CHECK(host_buttons()==0 && pressed_btn==(1u<<B_HOME));
    host_snapshot(&held,&pressed);CHECK(held==0 && pressed==(1u<<B_HOME));
    host_snapshot(&held,&pressed);CHECK(pressed==0);
    host_level(1u<<B_HOME,1);host_level(1u<<B_HOME,1);
    host_snapshot(&held,&pressed);CHECK(held==(1u<<B_HOME) && pressed==(1u<<B_HOME));
    for(i=0;i<100;i++){host_snapshot(&held,&pressed);CHECK(held==(1u<<B_HOME) && pressed==0);}
    host_mirror_buttons=1u<<B_HOME;host_snapshot(&held,&pressed);CHECK(pressed==0);
    host_level(1u<<B_HOME,0);host_snapshot(&held,&pressed);CHECK(held==(1u<<B_HOME) && pressed==0);
    host_mirror_buttons=0;host_snapshot(&held,&pressed);CHECK(held==0 && pressed==0);
    host_mirror_buttons=1u<<B_EDIT;host_snapshot(&held,&pressed);CHECK(pressed==(1u<<B_EDIT));
    host_snapshot(&held,&pressed);CHECK(pressed==0);
    /* Existing browser/rfm1 inputs may set held levels directly. */
    held_btn=1u<<B_REC;host_snapshot(&held,&pressed);CHECK(pressed==(1u<<B_REC));
    host_snapshot(&held,&pressed);CHECK(pressed==0);
    reset();host_level(1u<<B_SAVE,1);host_level(1u<<B_SAVE,0);host_snapshot(0,0);
    host_snapshot(&held,&pressed);CHECK(held==0 && pressed==0);
}
int main(void) {
    device_tests();host_tests();
    printf("{\"passed\":true,\"assertions\":%u}\n",assertions);return 0;
}
