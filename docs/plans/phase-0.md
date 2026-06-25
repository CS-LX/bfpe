# Phase 0 — 最小构建闭环

> 状态：**已完成**  
> 目标：`bfpe build one.bf -o out.dll`，构建后自动 `verify_pe.ps1` 验收

---

## 范围

**做：**

- 从 reference 提取 `runtime/`（vm、io、stub、dllmain）
- 移植 `tools/codegen/bf2asm.py`（暂沿用 `; bfdll: export=output`）
- 泛化 `tools/verify_pe.ps1`（参数化导出表与 BF 片段）
- 实现 `tools/bfpe.py build <file.bf> -o <out.dll>`
- 添加 `examples/hello_world.bf` 作为验收样例

**不做（留给 Phase 1+）：**

- `; bfpe:` 完整签名 DSL
- `bfpe run` / `bfpe exec`
- EXE 产物
- 多 `.bf` 合并单 DLL

---

## 任务清单

| # | 任务 | 文件 | 状态 |
|---|------|------|------|
| 0.1 | 提取 runtime 源码 | `runtime/**` | ✅ |
| 0.2 | 精简 Stub（去掉硬编码 Ping/Hello/Add） | `runtime/bf_stub.c` | ✅ |
| 0.3 | 移植 bf2asm | `tools/codegen/bf2asm.py` | ✅ |
| 0.4 | 泛化 verify 脚本 | `tools/verify_pe.ps1` | ✅ |
| 0.5 | MSVC 工具链探测 + link 编排 | `tools/bfpe.py` | ✅ |
| 0.6 | 示例与 .gitignore | `examples/`, `.gitignore` | ✅ |
| 0.7 | 端到端验收 | 手动 + verify | ✅ |

---

## 技术要点

### 构建流水线

```text
.bf
  → bf2asm.py → bf_programs.asm + bf_exports.gen.{h,c}
  → ml64 /c   → bf_programs.obj
  → cl /c     → runtime/*.c + bf_exports.gen.c
  → link /DLL /MERGE:bf_text=.text /INCLUDE:BF_Prog_* /DEF:generated.def
  → out.dll
  → verify_pe.ps1 -Manifest build_manifest.json
```

### Phase 0 导出约定

输入 `.bf` 须含：

```bf
; bfdll: export=output
```

生成 `BF_<Stem>` 导出（与 reference 一致），并始终链接：

- `BF_GetLastOutput`
- `BF_SetOutputCallback`

### Stub 差异

reference 的 `bf_stub.c` 硬编码 `BF_Ping` / `BF_Hello` / `BF_Add`。Phase 0 改为**仅**提供 `bfdll_run_output_program` 与 `BF_GetLastOutput`；业务导出全部由 `bf_exports.gen.c` 生成。

---

## 验收标准

1. 命令成功（exit 0）：

   ```powershell
   python tools/bfpe.py build examples/hello_world.bf -o build/hello_world.dll
   ```

2. `verify_pe.ps1` 全部 `[OK]`（BF 明文在 `.text`、不在 `.rdata`）

3. 宿主可加载并调用（任选其一）：

   ```powershell
   # PowerShell P/Invoke 或小型 test 脚本
   # BF_HelloWorld() 返回 "Hello World!"
   ```

4. runtime / tools 源码中**无** BF 指令字面量（合规 C5 静态检查，verify 可选）

---

## 风险

| 风险 | 对策 |
|------|------|
| VS 非默认安装路径 | `vswhere` 探测 + vcvars64 |
| `/INCLUDE` 遗漏导致 BF 符号被链接器剔除 | manifest 记录 `program_symbol`，link 时显式 `/INCLUDE` |
| 单文件 .def 导出名冲突 | Phase 0 仅支持单 `.bf` |

---

## 完成后

更新 [项目计划.md](../项目计划.md) 进度，进入 [phase-1.md](phase-1.md)。
