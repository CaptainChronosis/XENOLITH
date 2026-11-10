/* SPDX-License-Identifier: MPL-2.0 */
#include <stdio.h>
#include "fm1_flash_id.h"

int main(void)
{
    unsigned accepted = 0;
    for (uint32_t id = 0; id < 0x1000000u; ++id) {
        if (fm1_flash_known(id)) {
            if (id != 0x856014u && id != 0xB36014u) return 1;
            ++accepted;
        }
    }
    if (accepted != 2u || !fm1_flash_known(0x856014u) ||
        !fm1_flash_known(0xB36014u) || fm1_flash_known(0xFF856014u) ||
        fm1_flash_known(0xFFB36014u) || fm1_flash_known(0xFFFFFFFFu)) return 1;
    puts("flash_id: all 16777216 JEDEC values checked; exactly two accepted");
    return 0;
}
