# JieLi AC79 packaging resources

This is a minimal resource snapshot from JieLi's official Gitee mirror at `d179b4484759423312073f5fbb232501aa491047` (AC79NN SDK V1.2.1). `SOURCE_MANIFEST.json` records URLs and SHA256 hashes. All three binary resources match the hashes expected by the pinned fm1-base build script. The root Apache-2.0 licence is retained.

Only `uboot.boot`, `cfg_tool.bin` and `cfg/eq_cfg_hw.bin` are present. This is not a complete SDK. The open template builds without BLE and does not link the proprietary Bluetooth archive. The Windows adapter stops on a resource hash mismatch. No loader protocol code is edited.
