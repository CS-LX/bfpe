# Phase 3 — 工程化收尾

> 状态：**已完成**  
> 前置：Phase 2 完成

---

## 目标

多 `.bf` 合并单 PE、`bfpe exec` 调试模式、示例集与文档完善；可选 CI。

---

## 任务清单

| # | 任务 | 产出 | 状态 |
|---|------|------|------|
| 3.1 | `bfpe build a.bf b.bf -o lib.dll` | 多程序单 DLL | ✅ |
| 3.2 | 导出名唯一性检查 | 构建期错误提示 | ✅ |
| 3.3 | `bfpe exec <file.bf> [args...]` | 内存 VM，不写 PE | ✅ |
| 3.4 | `examples/` 完整样例集 | add / hello / multi-export | ✅ |
| 3.5 | 更新 README + 项目计划状态 | 文档 | ✅ |
| 3.6 | GitHub Actions（可选） | `windows-latest` 构建 + verify | ✅ |

---

## 验收标准

- 单 DLL 含 ≥2 个 BF 导出，verify 对每个 `.bf` 片段命中 `.text` ✅
- `bfpe exec add.bf 3 5` 输出 `8`（与 DLL 行为一致） ✅
- 项目计划 Phase 0–3 标记完成 ✅

---

## 验收命令

```powershell
python tools/bfpe.py build examples/add.bf examples/hello_world.bf -o build/bfpe_lib.dll
python tools/bfpe.py exec examples/add.bf 3 5          # 8
python tools/bfpe.py run build/bfpe_lib.dll Add 3 5    # 8
python tools/bfpe.py run build/bfpe_lib.dll HelloWorld # Hello World!
```

---

## 已知不做（MVP 外）

- x86 32-bit
- Authenticode 签名
- 向第三方 PE 注入 BF
