# Phase 2 — build/run 闭环

> 状态：**已完成**

---

## 目标

`bfpe run` 带参调用 DLL；CLI 简写；`verify_pe.ps1` 支持 EXE；可选生成 `.exe`。

---

## 任务清单

| # | 任务 | 产出 |
|---|------|------|
| 2.1 | `bfpe run <pe> <Export> [args...]` | `tools/run_pe.py` ✅ |
| 2.2 | CLI 简写规则 | `tools/bfpe.py` ✅ |
| 2.3 | EXE 链接 + 生成 main | `bf2asm.py` `exe_main.gen.c` ✅ |
| 2.4 | `verify_pe.ps1` EXE 模式 | EXE 头检查 ✅ |
| 2.5 | `; bfpe: entry` EXE 入口 | `examples/hello.bf` ✅ |
| 2.6 | 文档更新 | README / 项目计划 ✅ |

---

## 验收标准

```powershell
bfpe build examples/add.bf -o build/add.dll
bfpe run build/add.dll Add 3 5
# 输出：8

bfpe build examples/hello.bf -o build/hello.exe
bfpe run build/hello.exe Hello
```

- verify 对 DLL 与 EXE 均通过
- README 命令行章节与实现一致

---

## 参考

- 可行性报告 §4.3.2 DLL vs EXE
- README §命令行（目标接口）
