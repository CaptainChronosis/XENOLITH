/* SPDX-License-Identifier: MPL-2.0 */
#pragma once
#include <stdint.h>

/* Known FM-1 1 MiB parts. Capacity alone does not establish compatibility. */
static inline int fm1_flash_known(uint32_t jedec)
{
    return jedec == 0x856014u || jedec == 0xB36014u;
}
