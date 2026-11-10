/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XX29_XENOLITH_FLAVOUR_H
#define XX29_XENOLITH_FLAVOUR_H
#include <stdint.h>

enum { XFLAVOUR_NORMAL, XFLAVOUR_RETURN_NEGATIVE,
       XFLAVOUR_RETURN_ALTERNATING, XFLAVOUR_CASCADE, XFLAVOUR_COUNT };
#define XFLAVOUR_MODES 5u
typedef struct {
    uint8_t flavour[XFLAVOUR_MODES],reserved[3];
} XenolithFlavourConfig;
_Static_assert(sizeof(XenolithFlavourConfig)==8,"Flavour config ABI changed");
static inline void xenolith_flavour_defaults(XenolithFlavourConfig *c) {
    uint32_t i;if(!c)return;
    for(i=0;i<XFLAVOUR_MODES;i++)c->flavour[i]=XFLAVOUR_NORMAL;
    for(i=0;i<3u;i++)c->reserved[i]=0u;
}
static inline int xenolith_flavour_valid(const XenolithFlavourConfig *c) {
    uint32_t i;if(!c)return 0;
    for(i=0;i<XFLAVOUR_MODES;i++)if(c->flavour[i]>=XFLAVOUR_COUNT)return 0;
    for(i=0;i<3u;i++)if(c->reserved[i])return 0;
    return 1;
}
#endif
