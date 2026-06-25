# Archived Python toolchain (Phase 4b 之前)

自 Phase 4b 起，**`bfpe.exe`（C++）** 已包含 codegen、exec 与 build 编排。本目录仅作对照与历史参考，**不再用于构建**。

| 原路径 | 说明 |
|--------|------|
| `bfpe.py` | 旧 Python CLI |
| `exec_bf.py` | 旧内存 VM |
| `run_pe.py` | 旧 PE 运行器 |
| `codegen/` | 旧 `parse_sig` / `bf2asm` |

当前入口：`build-native/bin/bfpe.exe`（或 CMake 安装产物）。
