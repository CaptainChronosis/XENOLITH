/* SPDX-License-Identifier: MPL-2.0
 * XX29 foreground-only per-slot performance storage. Include after app globals
 * and patch types, before patches_init(). No I/O is allowed from audio render.
 * Legacy OBJ_SETTINGS is read-only here; explicit SAVE changes one new object.
 */
#include "xenolith_patches.h"
#include <stddef.h>

#define XSTORE_MAGIC UINT32_C(0x58535031)
#define XSTORE_VERSION 21u
typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV5 patch;
} XStoreRecordV1;
typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV6 patch;
} XStoreRecordV2;
typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV7 patch;
} XStoreRecordV3;
typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV8 patch;
} XStoreRecordV4;
typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV9 patch;
} XStoreRecordV5;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV10 patch;
} XStoreRecordV6;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV11 patch;
} XStoreRecordV7;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV12 patch;
} XStoreRecordV8;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV13 patch;
} XStoreRecordV9;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV14 patch;
} XStoreRecordV10;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV15 patch;
} XStoreRecordV11;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV16 patch;
} XStoreRecordV12;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV17 patch;
} XStoreRecordV13;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV18 patch;
} XStoreRecordV14;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV19 patch;
} XStoreRecordV15;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV20 patch;
} XStoreRecordV16;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV21 patch;
} XStoreRecordV17;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV22 patch;
} XStoreRecordV18;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV23 patch;
} XStoreRecordV19;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatchV24 patch;
} XStoreRecordV20;

typedef struct {
    uint32_t magic,version,bytes,slot,patch_version,reserved,crc;
    XenolithPatch patch;
} XStoreRecord;
static union {
    XenolithPatchBankV4 legacy;
    XStoreRecord current;
    XStoreRecordV1 old;
    XStoreRecordV2 v2;
    XStoreRecordV3 v3;
    XStoreRecordV4 v4;
    XStoreRecordV5 v5;
    XStoreRecordV6 v6;
    XStoreRecordV7 v7;
    XStoreRecordV8 v8;
    XStoreRecordV9 v9;
    XStoreRecordV10 v10;
    XStoreRecordV11 v11;
    XStoreRecordV12 v12;
    XStoreRecordV13 v13;
    XStoreRecordV14 v14;
    XStoreRecordV15 v15;
    XStoreRecordV16 v16;
    XStoreRecordV17 v17;
    XStoreRecordV18 v18;
    XStoreRecordV19 v19;
    XStoreRecordV20 v20;
} xstore_scratch; /* foreground-owned scratch, not callback stack */

_Static_assert(OBJ_SETTINGS==0 && OBJ_PANEL==1 && OBJ_BLE==2, "Legacy storage IDs changed");
_Static_assert(OBJ_PATCH0==3 && OBJ_PATCH15-OBJ_PATCH0+1==XENOLITH_PATCH_COUNT && OBJ_NOBJ==19,
               "Per-slot object numbering changed");
_Static_assert(offsetof(XStoreRecord,crc)==24 && offsetof(XStoreRecord,patch)==28,
               "Slot header ABI changed");
_Static_assert(sizeof(XStoreRecordV1)==860 && offsetof(XStoreRecordV1,patch)==28,
               "Old slot layout changed");
_Static_assert(sizeof(XStoreRecordV2)==892 && offsetof(XStoreRecordV2,patch)==28,
               "Version2 slot layout changed");
_Static_assert(sizeof(XStoreRecordV3)==904 && offsetof(XStoreRecordV3,patch)==28,
               "Version3 slot layout changed");
_Static_assert(sizeof(XStoreRecordV4)==920 && offsetof(XStoreRecordV4,patch)==28,
               "Version4 slot layout changed");
_Static_assert(sizeof(XStoreRecordV5)==928 && offsetof(XStoreRecordV5,patch)==28,
               "Version5 slot layout changed");
_Static_assert(sizeof(XStoreRecordV6)==928 && offsetof(XStoreRecordV6,patch)==28,
               "Version6 slot layout changed");
_Static_assert(sizeof(XStoreRecordV7)==928 && offsetof(XStoreRecordV7,patch)==28,
               "Version7 slot layout changed");
_Static_assert(sizeof(XStoreRecordV8)==928 && offsetof(XStoreRecordV8,patch)==28,
               "Version8 slot layout changed");
_Static_assert(sizeof(XStoreRecordV10)==1068,"Version10 slot layout changed");
_Static_assert(sizeof(XStoreRecordV11)==1088,"Version11 slot layout changed");
_Static_assert(sizeof(XStoreRecordV12)==1120,"Version12 slot layout changed");
_Static_assert(sizeof(XStoreRecordV13)==1136,"Version13 slot layout changed");
_Static_assert(sizeof(XStoreRecordV14)==1140,"Version14 slot layout changed");
_Static_assert(sizeof(XStoreRecordV15)==1472,"Version15 slot layout changed");
_Static_assert(sizeof(XStoreRecordV16)==1488,"Version16 slot layout changed");
_Static_assert(sizeof(XStoreRecordV17)==1524,"Version17 slot layout changed");
_Static_assert(sizeof(XStoreRecordV18)==1576,"Version18 slot layout changed");
_Static_assert(sizeof(XStoreRecordV19)==1716,"Version19 slot layout changed");
_Static_assert(sizeof(XStoreRecordV20)==1724,"Version20 slot layout changed");
_Static_assert(sizeof(XStoreRecord)==1740 && sizeof(XStoreRecord)<=PLAT_STORE_MAX,
               "Slot record exceeds atomic platform payload");

/* Standard reflected CRC-32 polynomial, independently written byte/bit loop.
 * Covers the six header words preceding crc, then the entire patch payload. */
static uint32_t xstore_crc_bytes(uint32_t crc,const void *memory,uint32_t bytes) {
    const uint8_t *p=(const uint8_t*)memory;uint32_t bit;
    while(bytes--){
        crc^=*p++;
        for(bit=0;bit<8u;bit++)crc=(crc>>1)^(UINT32_C(0xedb88320)&(0u-(crc&1u)));
    }
    return crc;
}
static uint32_t xstore_checksum(const XStoreRecord *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecord,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static uint32_t xstore_checksum_v1(const XStoreRecordV1 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV1,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static uint32_t xstore_checksum_v2(const XStoreRecordV2 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV2,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static uint32_t xstore_checksum_v3(const XStoreRecordV3 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV3,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static uint32_t xstore_checksum_v4(const XStoreRecordV4 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV4,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static uint32_t xstore_checksum_v5(const XStoreRecordV5 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV5,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static uint32_t xstore_checksum_v6(const XStoreRecordV6 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV6,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static uint32_t xstore_checksum_v7(const XStoreRecordV7 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV7,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static int xstore_record_v7_valid(const XStoreRecordV7 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==7u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==11u && record->reserved==0u
        && record->crc==xstore_checksum_v7(record) && xenolith_patch_v11_valid(&record->patch);
}
static uint32_t xstore_checksum_v8(const XStoreRecordV8 *record) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,record,offsetof(XStoreRecordV8,crc));
    return ~xstore_crc_bytes(crc,&record->patch,sizeof record->patch);
}
static int xstore_record_v8_valid(const XStoreRecordV8 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==8u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==12u && record->reserved==0u
        && record->crc==xstore_checksum_v8(record) && xenolith_patch_v12_valid(&record->patch);
}
static uint32_t xstore_checksum_v9(const XStoreRecordV9 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV9,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v9_valid(const XStoreRecordV9 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==9u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==13u&&r->reserved==0u
        &&r->crc==xstore_checksum_v9(r)&&xenolith_patch_v13_valid(&r->patch);
}
static uint32_t xstore_checksum_v10(const XStoreRecordV10 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV10,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v10_valid(const XStoreRecordV10 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==10u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==14u&&r->reserved==0u
        &&r->crc==xstore_checksum_v10(r)&&xenolith_patch_v14_valid(&r->patch);
}
static uint32_t xstore_checksum_v11(const XStoreRecordV11 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV11,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v11_valid(const XStoreRecordV11 *r,uint32_t slot) {
    return r && slot<XENOLITH_PATCH_COUNT && r->magic==XSTORE_MAGIC && r->version==11u
        && r->bytes==sizeof *r && r->slot==slot && r->patch_version==15u && r->reserved==0u
        && r->crc==xstore_checksum_v11(r) && xenolith_patch_v15_valid(&r->patch);
}
static uint32_t xstore_checksum_v12(const XStoreRecordV12 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV12,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v12_valid(const XStoreRecordV12 *r,uint32_t slot) {
    return r && slot<XENOLITH_PATCH_COUNT && r->magic==XSTORE_MAGIC && r->version==12u
        && r->bytes==sizeof *r && r->slot==slot && r->patch_version==16u && r->reserved==0u
        && r->crc==xstore_checksum_v12(r) && xenolith_patch_v16_valid(&r->patch);
}
static uint32_t xstore_checksum_v13(const XStoreRecordV13 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV13,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v13_valid(const XStoreRecordV13 *r,uint32_t slot) {
    return r && slot<XENOLITH_PATCH_COUNT && r->magic==XSTORE_MAGIC && r->version==13u
        && r->bytes==sizeof *r && r->slot==slot && r->patch_version==17u && r->reserved==0u
        && r->crc==xstore_checksum_v13(r) && xenolith_patch_v17_valid(&r->patch);
}
static uint32_t xstore_checksum_v14(const XStoreRecordV14 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV14,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v14_valid(const XStoreRecordV14 *r,uint32_t slot) {
    return r && slot<XENOLITH_PATCH_COUNT && r->magic==XSTORE_MAGIC && r->version==14u
        && r->bytes==sizeof *r && r->slot==slot && r->patch_version==18u && r->reserved==0u
        && r->crc==xstore_checksum_v14(r) && xenolith_patch_v18_valid(&r->patch);
}
static int xstore_record_valid(const XStoreRecord *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==XSTORE_VERSION && record->bytes==sizeof *record
        && record->slot==slot && record->patch_version==XENOLITH_PATCH_VERSION
        && record->reserved==0u && record->crc==xstore_checksum(record)
        && xenolith_patch_valid(&record->patch);
}
static int xstore_record_v1_valid(const XStoreRecordV1 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==1u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==5u && record->reserved==0u
        && record->crc==xstore_checksum_v1(record) && xenolith_patch_v5_valid(&record->patch);
}
static int xstore_record_v2_valid(const XStoreRecordV2 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==2u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==6u && record->reserved==0u
        && record->crc==xstore_checksum_v2(record) && xenolith_patch_v6_valid(&record->patch);
}
static int xstore_record_v3_valid(const XStoreRecordV3 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==3u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==7u && record->reserved==0u
        && record->crc==xstore_checksum_v3(record) && xenolith_patch_v7_valid(&record->patch);
}
static int xstore_record_v4_valid(const XStoreRecordV4 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==4u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==8u && record->reserved==0u
        && record->crc==xstore_checksum_v4(record) && xenolith_patch_v8_valid(&record->patch);
}
static int xstore_record_v5_valid(const XStoreRecordV5 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==5u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==9u && record->reserved==0u
        && record->crc==xstore_checksum_v5(record) && xenolith_patch_v9_valid(&record->patch);
}
static int xstore_record_v6_valid(const XStoreRecordV6 *record,uint32_t slot) {
    return record && slot<XENOLITH_PATCH_COUNT && record->magic==XSTORE_MAGIC
        && record->version==6u && record->bytes==sizeof *record && record->slot==slot
        && record->patch_version==10u && record->reserved==0u
        && record->crc==xstore_checksum_v6(record) && xenolith_patch_v10_valid(&record->patch);
}
static uint32_t xstore_checksum_v15(const XStoreRecordV15 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV15,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v15_valid(const XStoreRecordV15 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==15u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==19u&&!r->reserved
        &&r->crc==xstore_checksum_v15(r)&&xenolith_patch_v19_valid(&r->patch);
}
static uint32_t xstore_checksum_v16(const XStoreRecordV16 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV16,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v16_valid(const XStoreRecordV16 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==16u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==20u&&!r->reserved
        &&r->crc==xstore_checksum_v16(r)&&xenolith_patch_v20_valid(&r->patch);
}
static uint32_t xstore_checksum_v17(const XStoreRecordV17 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV17,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v17_valid(const XStoreRecordV17 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==17u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==21u&&!r->reserved
        &&r->crc==xstore_checksum_v17(r)&&xenolith_patch_v21_valid(&r->patch);
}
static uint32_t xstore_checksum_v18(const XStoreRecordV18 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV18,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v18_valid(const XStoreRecordV18 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==18u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==22u&&!r->reserved
        &&r->crc==xstore_checksum_v18(r)&&xenolith_patch_v22_valid(&r->patch);
}
static uint32_t xstore_checksum_v19(const XStoreRecordV19 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV19,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v19_valid(const XStoreRecordV19 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==19u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==23u&&!r->reserved
        &&r->crc==xstore_checksum_v19(r)&&xenolith_patch_v23_valid(&r->patch);
}
static uint32_t xstore_checksum_v20(const XStoreRecordV20 *r) {
    uint32_t crc=xstore_crc_bytes(UINT32_MAX,r,offsetof(XStoreRecordV20,crc));
    return ~xstore_crc_bytes(crc,&r->patch,sizeof r->patch);
}
static int xstore_record_v20_valid(const XStoreRecordV20 *r,uint32_t slot) {
    return r&&slot<XENOLITH_PATCH_COUNT&&r->magic==XSTORE_MAGIC&&r->version==20u
        &&r->bytes==sizeof *r&&r->slot==slot&&r->patch_version==24u&&!r->reserved
        &&r->crc==xstore_checksum_v20(r)&&xenolith_patch_v24_valid(&r->patch);
}
static int xstore_map_valid(void) {
    static const uint32_t map[OBJ_NOBJ]=APP_STORE_MAP;
    uint32_t i,j;
    if(map[OBJ_SETTINGS]!=0xfc000u || map[OBJ_PANEL]!=0xdc000u || map[OBJ_BLE]!=0xde000u)return 0;
    for(i=0;i<OBJ_NOBJ;i++){
        if(map[i]&0xfffu)return 0;
        if(i>=OBJ_PATCH0 && map[i]!=0xb0000u+(i-OBJ_PATCH0)*0x2000u)return 0;
        /* All19 pairs are in the ordinary store or its existing global pair.
         * Nothing enters app/XDATA/loader staging or crosses flash end. */
        if(i!=OBJ_SETTINGS && !(map[i]>=0x97000u && map[i]<=0xde000u))return 0;
        for(j=0;j<i;j++)if(map[i]<map[j]+0x2000u && map[j]<map[i]+0x2000u)return 0;
    }
    return 1;
}
/* Slot objects overlay the original small V1..V4 settings bank. The platform
 * object ceiling is3840B; later full banks never fitted and use slot objects.
 * All reads here are foreground-owned. No automatic migration write occurs. */
static int xstore_load_object(uint32_t slot,XenolithPatch *out) {
    uint32_t object=OBJ_PATCH0+slot;int bytes=plat_store_size(object);
        if(bytes==(int)sizeof xstore_scratch.current
           && plat_store_load(object,&xstore_scratch.current,sizeof xstore_scratch.current)==bytes
           && xstore_record_valid(&xstore_scratch.current,slot)){
            *out=xstore_scratch.current.patch;return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV20)
           && plat_store_load(object,&xstore_scratch.v20,sizeof xstore_scratch.v20)==bytes
           && xstore_record_v20_valid(&xstore_scratch.v20,slot)
           && xenolith_patch_from_v24(out,&xstore_scratch.v20.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV19)
           && plat_store_load(object,&xstore_scratch.v19,sizeof xstore_scratch.v19)==bytes
           && xstore_record_v19_valid(&xstore_scratch.v19,slot)
           && xenolith_patch_from_v23(out,&xstore_scratch.v19.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV18)
           && plat_store_load(object,&xstore_scratch.v18,sizeof xstore_scratch.v18)==bytes
           && xstore_record_v18_valid(&xstore_scratch.v18,slot)
           && xenolith_patch_from_v22(out,&xstore_scratch.v18.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV17)
           && plat_store_load(object,&xstore_scratch.v17,sizeof xstore_scratch.v17)==bytes
           && xstore_record_v17_valid(&xstore_scratch.v17,slot)
           && xenolith_patch_from_v21(out,&xstore_scratch.v17.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV16)
           && plat_store_load(object,&xstore_scratch.v16,sizeof xstore_scratch.v16)==bytes
           && xstore_record_v16_valid(&xstore_scratch.v16,slot)
           && xenolith_patch_from_v20(out,&xstore_scratch.v16.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV15)
           && plat_store_load(object,&xstore_scratch.v15,sizeof xstore_scratch.v15)==bytes
           && xstore_record_v15_valid(&xstore_scratch.v15,slot)
           && xenolith_patch_from_v19(out,&xstore_scratch.v15.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV14)
           && plat_store_load(object,&xstore_scratch.v14,sizeof xstore_scratch.v14)==bytes
           && xstore_record_v14_valid(&xstore_scratch.v14,slot)
           && xenolith_patch_from_v18(out,&xstore_scratch.v14.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV13)
           && plat_store_load(object,&xstore_scratch.v13,sizeof xstore_scratch.v13)==bytes
           && xstore_record_v13_valid(&xstore_scratch.v13,slot)
           && xenolith_patch_from_v17(out,&xstore_scratch.v13.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV12)
           && plat_store_load(object,&xstore_scratch.v12,sizeof xstore_scratch.v12)==bytes
           && xstore_record_v12_valid(&xstore_scratch.v12,slot)
           && xenolith_patch_from_v16(out,&xstore_scratch.v12.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV11)
           && plat_store_load(object,&xstore_scratch.v11,sizeof xstore_scratch.v11)==bytes
           && xstore_record_v11_valid(&xstore_scratch.v11,slot)
           && xenolith_patch_from_v15(out,&xstore_scratch.v11.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV10)
           && plat_store_load(object,&xstore_scratch.v10,sizeof xstore_scratch.v10)==bytes
           && xstore_record_v10_valid(&xstore_scratch.v10,slot)
           && xenolith_patch_from_v14(out,&xstore_scratch.v10.patch)){
            return 1;
        }else if(bytes==(int)sizeof(XStoreRecordV9)
           && plat_store_load(object,&xstore_scratch.v9,sizeof xstore_scratch.v9)==bytes
           && xstore_record_v9_valid(&xstore_scratch.v9,slot)
           && xenolith_patch_from_v13(out,&xstore_scratch.v9.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.v8
           && plat_store_load(object,&xstore_scratch.v8,sizeof xstore_scratch.v8)==bytes
           && xstore_record_v8_valid(&xstore_scratch.v8,slot)
           && xenolith_patch_from_v12(out,&xstore_scratch.v8.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.v7
           && plat_store_load(object,&xstore_scratch.v7,sizeof xstore_scratch.v7)==bytes
           && xstore_record_v7_valid(&xstore_scratch.v7,slot)
           && xenolith_patch_from_v11(out,&xstore_scratch.v7.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.v6
           && plat_store_load(object,&xstore_scratch.v6,sizeof xstore_scratch.v6)==bytes
           && xstore_record_v6_valid(&xstore_scratch.v6,slot)
           && xenolith_patch_from_v10(out,&xstore_scratch.v6.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.v5
           && plat_store_load(object,&xstore_scratch.v5,sizeof xstore_scratch.v5)==bytes
           && xstore_record_v5_valid(&xstore_scratch.v5,slot)
           && xenolith_patch_from_v9(out,&xstore_scratch.v5.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.old
           && plat_store_load(object,&xstore_scratch.old,sizeof xstore_scratch.old)==bytes
           && xstore_record_v1_valid(&xstore_scratch.old,slot)
           && xenolith_patch_from_v5(out,&xstore_scratch.old.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.v2
           && plat_store_load(object,&xstore_scratch.v2,sizeof xstore_scratch.v2)==bytes
           && xstore_record_v2_valid(&xstore_scratch.v2,slot)
           && xenolith_patch_from_v6(out,&xstore_scratch.v2.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.v3
           && plat_store_load(object,&xstore_scratch.v3,sizeof xstore_scratch.v3)==bytes
           && xstore_record_v3_valid(&xstore_scratch.v3,slot)
           && xenolith_patch_from_v7(out,&xstore_scratch.v3.patch)){
            return 1;
        }else if(bytes==(int)sizeof xstore_scratch.v4
           && plat_store_load(object,&xstore_scratch.v4,sizeof xstore_scratch.v4)==bytes
           && xstore_record_v4_valid(&xstore_scratch.v4,slot)
           && xenolith_patch_from_v8(out,&xstore_scratch.v4.patch)){
            return 1;
        }
    return 0;
}
static int xstore_legacy_bytes(void) {
    int bytes=plat_store_size(OBJ_SETTINGS);
    if(bytes<16||bytes>(int)sizeof xstore_scratch.legacy)return 0;
    return plat_store_load(OBJ_SETTINGS,&xstore_scratch.legacy,(uint32_t)bytes)==bytes?bytes:0;
}
static int xstore_scan(uint32_t *occupied,XenolithPatch *scratch) {
    uint32_t mask=0u,slot;int loaded=0,bytes;
    if(!occupied||!scratch)return 0;
    *occupied=0u;if(!xstore_map_valid())return 0;
    bytes=xstore_legacy_bytes();
    if(bytes&&xenolith_patches_bank_info(&xstore_scratch.legacy,(uint32_t)bytes,&mask))loaded=1;
    for(slot=0;slot<XENOLITH_PATCH_COUNT;slot++)if(xstore_load_object(slot,scratch)){
        mask|=1u<<slot;loaded=1;
    }
    *occupied=mask;return loaded;
}
static int xstore_load_slot(uint32_t slot,XenolithPatch *out) {
    int bytes;
    if(!out||slot>=XENOLITH_PATCH_COUNT||!xstore_map_valid())return 0;
    if(xstore_load_object(slot,out))return 1;
    bytes=xstore_legacy_bytes();
    return bytes&&xenolith_patches_decode_slot(out,&xstore_scratch.legacy,(uint32_t)bytes,slot);
}
/* Foreground acknowledged quiet ownership only. Caller updates occupancy
 * only after return0. Platform A/B payload+header CRC/commit supplies atomicity. */
static int xstore_save_slot(uint32_t slot,const XenolithPatch *patch) {
    XStoreRecord *record=&xstore_scratch.current;
    if(slot>=XENOLITH_PATCH_COUNT || !xenolith_patch_valid(patch))return -11;
    if(!xstore_map_valid())return -12;
    record->magic=XSTORE_MAGIC;record->version=XSTORE_VERSION;
    record->bytes=sizeof *record;record->slot=slot;
    record->patch_version=XENOLITH_PATCH_VERSION;record->reserved=0u;
    record->patch=*patch;record->crc=xstore_checksum(record);
    return plat_store_save(OBJ_PATCH0+slot,record,sizeof *record);
}
