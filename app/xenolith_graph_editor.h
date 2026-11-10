/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XX29_XENOLITH_GRAPH_EDITOR_H
#define XX29_XENOLITH_GRAPH_EDITOR_H
#include "xenolith_graph.h"

/* Scope preview preserves the CURRENT recipe, including dormant cells. */
static inline int xenolith_graph_preview_scope(XenolithGraphConfig *c,
    uint32_t *legacy_growth,uint32_t scope)
{
    if(!c||!legacy_growth||scope>=XGRAPH_SCOPE_COUNT||scope==c->scope)return 0;
    if(scope==XGRAPH_ALL)*legacy_growth=c->next_kind;
    c->scope=scope;return 1;
}

/* Only the explicit GLOBAL hold gesture calls this destructive operation. */
static inline int xenolith_graph_commit_global(XenolithGraphConfig *c,uint32_t growth)
{
    if(!c||c->scope!=XGRAPH_ALL||growth>=XTOPO_GROWTH_COUNT)return 0;
    xenolith_graph_from_legacy(c,growth);return 1;
}

/* A removed attachment is clamped to the surviving requested prefix. Existing
 * active anchors remain untouched; future growth follows the corrected cursor. */
static inline int xenolith_graph_limit_attachment(XenolithGraphConfig *c,uint32_t count)
{
    uint32_t i;
    if(!c||!count||count>XGRAPH_CELLS||c->attachment<=count)return 0;
    c->attachment=count;
    for(i=count;i<XGRAPH_CELLS;i++){
        c->kind[i]=(uint8_t)c->next_kind;
        c->anchor[i]=(uint8_t)(count-1u);
    }
    return 1;
}
#endif
