# Phase 1 — 签名 DSL 与 I/O 流

> 状态：**已完成**

---

## 目标

注释驱动函数签名与 Stub 生成；统一 BF I/O 流（stdio / buffer / callback）；补齐 `,` 输入。

---

## 任务清单

| # | 任务 | 产出 |
|---|------|------|
| 1.1 | 签名 DSL 解析器 | `tools/codegen/parse_sig.py` ✅ |
| 1.2 | 支持 `; bfpe: export=` / C 风格签名 / `io=` | `tools/codegen/test_parse_sig.py` ✅ |
| 1.3 | 按签名生成 Stub + `.def` | `bf2asm.py` 生成导出 ✅ |
| 1.4 | I/O 模式抽象 | `runtime/bf_io.c` `bf_io_bind_mode` ✅ |
| 1.5 | `io=stdio` / `io=buffer` | VM `,` / `.` 绑定 ✅ |
| 1.6 | 整型参数/返回值（BFC-0） | `examples/add.bf` ✅ |
| 1.7 | 兼容 `; bfdll: export=output` | 映射为 `const char*` + buffer ✅ |

---

## 验收标准

```powershell
bfpe build examples/add.bf -o build/add.dll
# add.bf 头部：
# ; bfpe: export=Add
# ; bfpe: int add(int a, int b)
```

- 导出 `BF_Add`，`LoadLibrary` 调用 `(3,5) → 8`
- `hello.bf` + `io=stdio` 时 `.` 写入 stdout（CLI 文档说明使用场景）

---

## 参考

- README §签名注释（DSL 草案）
- 可行性报告 §4.1、§4.2
