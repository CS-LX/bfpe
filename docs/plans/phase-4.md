# Phase 4 — 原生 CLI（C++ `bfpe.exe`）

> 状态：**未开始**  
> 前置：**Phase 3 完成**（多 `.bf`、`bfpe exec`、DSL 与 manifest 形态稳定后再移植）

---

## 目标

将当前 **Python 驱动 + codegen** 工具链替换/封装为可独立分发的 **`bfpe.exe`（C++）**，使用户**无需安装 Python** 即可完成 `build` / `run`；可选进一步去掉对 `tools/codegen/*.py` 的运行时依赖。

**不改变：**

- `runtime/` 中 VM + Stub + I/O 的 C 实现（仍由 `cl` / `link` 链入产物）
- PE 合规路径：BF 仅经 MASM `db` 进 `.text`；构建后强制验收
- 产物 DLL/EXE 的运行时语义（BFC-0、I/O 模式、签名 DSL）

**可行性报告 §4.3.3 已预留：**「bfpe.exe（Python 或 C++ 驱动，MVP 推荐 Python + 调用 MSVC 工具链）」。

---

## 分两档实施

| 档位 | 名称 | 去掉 Python？ | 说明 |
|------|------|---------------|------|
| **4a** | 原生驱动 | ❌ 仍调 `bf2asm.py` | 快速得到 `bfpe.exe`；codegen 逻辑暂不重写 |
| **4b** | 全原生工具链 | ✅ | `parse` + `codegen` + `run` +（可选）verify 全部 C++ |

**推荐顺序：** 先 **4a** 验证 MSVC 编排与分发形态，再 **4b** 去掉 Python 依赖。4a 可作为 4b 前的里程碑，也可在 4b 完成后弃用。

---

## 范围

### 做

- C++ 实现的 CLI：`build` / `run` / 简写（与 Phase 2 行为一致）
- MSVC 工具链探测（`vswhere` + `vcvars64` 等价逻辑）
- 子进程编排：`ml64`、`cl`、`link`；构建目录 `.bfpe-build/` 布局与现版兼容
- manifest JSON 格式与 Phase 3 终态对齐（含多 program、exec 元数据若已有）
- 单元测试 / 集成测试：至少覆盖 `add.dll`、`hello.exe` 与 verify 通过
- 文档：README 安装节改为「仅需 VS + bfpe.exe」

### 不做（本 Phase 外）

- Rust 版 CLI（除非后续单独立项）
- 去掉 Visual Studio 依赖（`ml64` / `cl` / `link` 仍为硬需求）
- 将 BF 业务逻辑编译进 `bfpe.exe`（CLI 不得构成「欺骗 DLL」路径）
- x86、Authenticode、第三方 PE patch

---

## 任务清单

### Phase 4a — 原生驱动（仍用 Python codegen）

| # | 任务 | 产出 |
|---|------|------|
| 4a.1 | CMake 工程 `src/cli/` 或 `tools/native/` | 可构建的 `bfpe.exe` |
| 4a.2 | 参数解析：`build` / `run` / 简写 | 与 `bfpe.py` 行为一致 |
| 4a.3 | MSVC 环境初始化 | 等价于现 `msvc_env()` + `resolve_tool()` |
| 4a.4 | 调用 `python tools/codegen/bf2asm.py` | subprocess，manifest 路径不变 |
| 4a.5 | 链接编排（DLL/EXE 分支） | 复用现 `.def` / `/MERGE:bf_text=.text` 逻辑 |
| 4a.6 | 构建后调用 `verify_pe.ps1` | 或 4b 再原生化 |
| 4a.7 | `run`：`LoadLibrary` + 导出调用；EXE `CreateProcess` | 等价 `run_pe.py` |
| 4a.8 | 安装/分发说明 | 仓库内 `build/bin/bfpe.exe` 或 Release 产物 |

### Phase 4b — 全原生 codegen（去掉 Python）

| # | 任务 | 产出 |
|---|------|------|
| 4b.1 | C++ 移植 `parse_sig` | `src/codegen/parse_sig.{h,cpp}` + 测试 |
| 4b.2 | C++ 移植 `bf2asm` | 生成 asm / stub.c / def / exe_main / manifest |
| 4b.3 | 移除 4a 对 Python 的 subprocess 依赖 | `bfpe.exe` 单二进制 + `runtime/` |
| 4b.4 | （可选）C++ 移植 `verify_pe` 核心检查 | 替代 PowerShell，便于 CI 无 PS 依赖 |
| 4b.5 | 静态检查：runtime 无 BF 字面量 | 并入 verify 或独立 `bfpe verify` |
| 4b.6 | 弃用或归档 `tools/bfpe.py` | README 以 `bfpe.exe` 为准；Python 可留 `scripts/` 对照 |

---

## 目标目录（Phase 4b 完成后）

```text
bfpe/
├── src/
│   ├── cli/                 bfpe.exe 入口（build/run/dispatch）
│   └── codegen/             parse_sig + bf2asm（C++）
├── runtime/                 不变
├── tools/
│   ├── verify_pe.ps1        4a 保留；4b 可选 C++ 替代
│   └── archive/             可选：迁出 bfpe.py / codegen/*.py
├── examples/
└── CMakeLists.txt           构建 bfpe.exe + 可选 test
```

4a 可仅在 `tools/native/` 下起 CMake 工程，不必立刻大挪目录。

---

## 技术要点

### CLI 与 Python 版对齐

| 能力 | 4a | 4b |
|------|----|----|
| `bfpe build one.bf -o out.{dll,exe}` | ✅ | ✅ |
| `bfpe run pe Export [args...]` | ✅ | ✅ |
| 简写 `bfpe a.bf -o out.pe` | ✅ | ✅ |
| 简写 `bfpe a.bf args... out.pe` | ✅ | ✅ |
| 多 `.bf` 单 PE（Phase 3） | 调 Python codegen | C++ codegen |
| `bfpe exec`（Phase 3） | 可仍用 Python 或 4b 一并移植 | C++ |

### MSVC 编排

- 通过 `vswhere.exe` 定位 VS 2022，启动 `vcvars64.bat` 或等价方式注入 `PATH`
- 编译参数与现版一致：`/DBFDLL_EXPORTS`、`/MERGE:bf_text=.text`、`/INCLUDE:BF_Prog_*`
- **不得**在 C++ 源码中嵌入 BF 程序字符串（合规 C5）

### verify

- **4a：** 继续 `powershell -File tools/verify_pe.ps1 -ManifestPath ...`（零移植风险）
- **4b：** 可选将 C1–C5 检查写入 C++，manifest 格式保持不变

### 与 Phase 3 的衔接

Phase 4 **依赖** Phase 3 冻结的：

- manifest JSON 字段（`export_name`、`param_count`、`pe_kind`、`programs[]`）
- 多文件 build 的 `.def` / 导出名唯一性规则
- `bfpe exec` 语义（若 4b 要覆盖 exec，需在 Phase 3 验收用例稳定后移植）

---

## 验收标准

### 4a 验收

```powershell
# 未安装 Python 的机器上仍可 build 失败；开发机验收：
bfpe.exe build examples/add.bf -o build/add.dll
bfpe.exe run build/add.dll Add 3 5
# 期望：8

bfpe.exe build examples/hello.bf -o build/hello.exe
bfpe.exe run build/hello.exe Hello
# 期望 stdout：Hi

verify_pe.ps1 对 DLL/EXE 全部 [OK]
```

- `bfpe.exe` 行为与 `python tools/bfpe.py` 一致（同一 manifest / 同一 build 目录布局）
- 构建产物仍通过五项合规检查

### 4b 验收

- **无需 Python** 即可完成 4a 全部用例 + Phase 3 多 `.bf` / `exec` 用例（若 Phase 3 已交付）
- `tools/codegen/*.py` 标记 deprecated 或移入 `tools/archive/`
- CI（若 Phase 3 已加）仅依赖 VS + `bfpe.exe`

---

## 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| Phase 3 变更 manifest / DSL | 4b 重复劳动 | **Phase 4 在 Phase 3 之后**；4a 可并行但仅作驱动验证 |
| C++ 字符串 codegen 易出错 | asm/stub 生成 bug | 4b 与 Python 输出 diff 测试；保留 Python 为 golden |
| verify 仍依赖 PowerShell | 无 PS 环境 CI 失败 | 4a 接受；4b 可选 C++ verify |
| 双轨维护（py + exe） | 文档/行为漂移 | 4a 短期；4b 完成后 Python 归档 |
| 路径含空格（VS 安装） | vcvars 失败 | 复现现 `call "path"` 修复经验 |

---

## 工作量粗估

| 档位 | 人天（单人） | 说明 |
|------|--------------|------|
| 4a | 5–10 | 驱动 + run + CMake；codegen 仍 Python |
| 4b | 15–25 | parse + bf2asm + 测试 + verify 可选移植 |
| 合计 | 20–35 | 视 Phase 3 复杂度与 verify 是否原生化 |

---

## 备选：PyInstaller（非本 Phase）

若仅需「有个 exe」、可接受内嵌 Python，可用 PyInstaller 打包现 `bfpe.py`（约 1 天）。**不替代 Phase 4**，但可作为 4a 之前的临时分发方案。

---

## 完成后

- 更新 [项目计划.md](../项目计划.md) 进度
- README：默认命令改为 `bfpe.exe`；Python 路径移入「开发者/对照」小节
- 可行性报告 §4.3.3 状态改为「C++ 驱动已落地」

---

## 参考

- [可行性报告.md](../可行性报告.md) §4.3.3
- [reference/Brainfuck-in-PE/host/test_host.c](../../reference/Brainfuck-in-PE/host/test_host.c) — C 宿主加载 DLL
- 现 Python 实现：`tools/bfpe.py`、`tools/codegen/`、`tools/run_pe.py`
