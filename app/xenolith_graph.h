/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XX29_XENOLITH_GRAPH_H
#define XX29_XENOLITH_GRAPH_H
#include <stdint.h>

enum { XTOPO_PARALLEL, XTOPO_CHAIN4, XTOPO_BRANCH, XTOPO_LOOP4,
       XTOPO_SELF, XTOPO_GROWTH_COUNT, XTOPO_MIXED=XTOPO_GROWTH_COUNT };
enum { XGRAPH_ADD, XGRAPH_ALL, XGRAPH_SCOPE_COUNT };
#define XGRAPH_KIND_COUNT 5u
#define XGRAPH_NONE 255u
#define XGRAPH_CELLS 64u
typedef struct {
    uint32_t scope,next_kind,attachment;
    uint8_t kind[XGRAPH_CELLS],anchor[XGRAPH_CELLS];
} XenolithGraphConfig;
_Static_assert(sizeof(XenolithGraphConfig)==140,"Graph config ABI changed");
typedef struct {
    uint8_t parent[XGRAPH_CELLS],feedback[XGRAPH_CELLS];
    uint32_t active_growth,edge_count,feedback_count;
} XenolithGraph;

static inline void xenolith_graph_defaults(XenolithGraphConfig *c) {
    uint32_t i;if(!c)return;
    c->scope=XGRAPH_ALL;c->next_kind=XTOPO_PARALLEL;c->attachment=0u;
    for(i=0;i<XGRAPH_CELLS;i++) {
        c->kind[i]=XTOPO_PARALLEL;c->anchor[i]=XGRAPH_NONE;
    }
}
static inline int xenolith_graph_valid(const XenolithGraphConfig *c) {
    uint32_t i;
    if(!c||c->scope>=XGRAPH_SCOPE_COUNT||c->next_kind>=XGRAPH_KIND_COUNT||
       c->attachment>XGRAPH_CELLS)return 0;
    for(i=0;i<XGRAPH_CELLS;i++)
        if(c->kind[i]>=XGRAPH_KIND_COUNT||
           (c->anchor[i]!=XGRAPH_NONE&&c->anchor[i]>=i))return 0;
    return 1;
}
/* Reclassify only the audible/fading prefix. Dormant future links must not
 * move a parallel bank into the more expensive general renderer. */
static inline void xenolith_graph_classify(const XenolithGraphConfig *c,
    uint32_t legacy_growth,uint32_t count,XenolithGraph *g) {
    uint32_t i,all_self=1u;
    g->edge_count=g->feedback_count=0u;
    if(count<1u)count=1u;
    if(count>XGRAPH_CELLS)count=XGRAPH_CELLS;
    for(i=0;i<count;i++) {
        if(g->parent[i]<count)g->edge_count++;
        if(g->feedback[i]<count)g->feedback_count++;
        if(g->parent[i]!=XGRAPH_NONE||g->feedback[i]!=i)all_self=0u;
    }
    if(c->scope==XGRAPH_ALL)g->active_growth=legacy_growth;
    else if(!g->edge_count&&!g->feedback_count)g->active_growth=XTOPO_PARALLEL;
    else if(all_self)g->active_growth=XTOPO_SELF;
    else g->active_growth=XTOPO_MIXED;
}
/* Foreground/snapshot preparation: bounded linear construction, no heap or
 * sort. parent<i reads the current tick; feedback>=i reads the prior tick.
 * Each later loop can claim one previously unused delayed receiver. */
static inline int xenolith_graph_compile(const XenolithGraphConfig *c,
    uint32_t legacy_growth,uint32_t count,XenolithGraph *g) {
    uint8_t children[XGRAPH_CELLS]={0};
    uint32_t i,branch_cursor=0u;
    if(!g||!xenolith_graph_valid(c)||legacy_growth>=XTOPO_GROWTH_COUNT)return 0;
    for(i=0;i<XGRAPH_CELLS;i++)g->parent[i]=g->feedback[i]=XGRAPH_NONE;
    for(i=0;i<XGRAPH_CELLS;i++) {
        uint32_t kind=c->scope==XGRAPH_ALL?legacy_growth:c->kind[i];
        if(c->scope==XGRAPH_ALL) {
            if(kind==XTOPO_BRANCH&&i)g->parent[i]=(uint8_t)((i-1u)>>1);
            else if(kind==XTOPO_SELF)g->feedback[i]=(uint8_t)i;
            else if((kind==XTOPO_CHAIN4||kind==XTOPO_LOOP4)&&(i&3u))g->parent[i]=(uint8_t)(i-1u);
            else if(kind==XTOPO_LOOP4&&i+3u<XGRAPH_CELLS)g->feedback[i]=(uint8_t)(i+3u);
            continue;
        }
        if(!i||kind==XTOPO_PARALLEL||
           (kind==XTOPO_SELF&&c->anchor[i]==XGRAPH_NONE)) {
            branch_cursor=i;
            if(kind==XTOPO_SELF)g->feedback[i]=(uint8_t)i;
            continue;
        }
        if(c->anchor[i]!=XGRAPH_NONE)g->parent[i]=c->anchor[i];
        else if(kind==XTOPO_BRANCH) {
            while(branch_cursor<i&&children[branch_cursor]>=2u)branch_cursor++;
            g->parent[i]=(uint8_t)(branch_cursor<i?branch_cursor:i-1u);
        } else g->parent[i]=(uint8_t)(i-1u);
        children[g->parent[i]]++;
        if(kind==XTOPO_SELF)g->feedback[i]=(uint8_t)i;
        if(kind==XTOPO_LOOP4) {
            uint8_t ancestors[3];uint32_t n=0u,node=g->parent[i],target=i;
            do {
                ancestors[n++]=(uint8_t)node;
                node=g->parent[node];
            } while(n<3u&&node!=XGRAPH_NONE);
            while(n) {
                node=ancestors[--n];
                if(g->feedback[node]==XGRAPH_NONE){target=node;break;}
            }
            g->feedback[target]=(uint8_t)i;
        }
    }
    xenolith_graph_classify(c,legacy_growth,count,g);
    return 1;
}
/* Preserve a legacy array when switching ALL to ADD. */
static inline void xenolith_graph_from_legacy(XenolithGraphConfig *c,uint32_t growth) {
    uint32_t i;xenolith_graph_defaults(c);c->scope=XGRAPH_ADD;
    if(growth>=XTOPO_GROWTH_COUNT)growth=XTOPO_PARALLEL;
    c->next_kind=growth;
    for(i=0;i<XGRAPH_CELLS;i++) {
        if(growth==XTOPO_CHAIN4)c->kind[i]=(i&3u)?XTOPO_CHAIN4:XTOPO_PARALLEL;
        else if(growth==XTOPO_LOOP4)c->kind[i]=!(i&3u)?XTOPO_PARALLEL:(i&3u)==3u?XTOPO_LOOP4:XTOPO_CHAIN4;
        else c->kind[i]=(uint8_t)growth;
    }
}
/* Prefix ownership check for an append/pop transaction. New delayed returns
 * are compatible when their source lies in the entering/leaving suffix. */
static inline int xenolith_graph_prefix_compatible(const XenolithGraph *a,uint32_t acount,
    const XenolithGraph *b,uint32_t bcount) {
    uint32_t i,n=acount<bcount?acount:bcount;
    if(!a||!b||acount>XGRAPH_CELLS||bcount>XGRAPH_CELLS)return 0;
    for(i=0;i<n;i++) {
        uint32_t af=a->feedback[i]<n?a->feedback[i]:XGRAPH_NONE;
        uint32_t bf=b->feedback[i]<n?b->feedback[i]:XGRAPH_NONE;
        if(a->parent[i]!=b->parent[i]||af!=bf)return 0;
    }
    return 1;
}
#endif
