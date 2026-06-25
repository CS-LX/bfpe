# BFPE

**Brainfuck-in-PE 工程化工具链** — 将 Brainfuck 源码作为「指令集」嵌入 Windows PE 的 `.text` 节，生成可被正常加载的 **第三种 DLL / EXE**。

> 基于实验项目 [Brainfuck-in-PE](reference/Brainfuck-in-PE/)（reference 子模块）的产品化演进。实验结论与约束见 [docs/可行性报告.md](docs/可行性报告.md)。

---

## 这是什么？

常见 PE 产物的代码节内容：

| 类型 | `.text` 里是什么 | 怎么执行 |
|------|------------------|----------|
| 原生 DLL/EXE | x64 机器码 | CPU 直接跑 |
| 托管 DLL | .NET IL | CLR |
| **BFPE 产物** | **明文 Brainfuck**（`+-<>[],.`） | **Stub + VM 解释** |

BFPE 不是 Brainfuck 解释器的普通打包，而是：

1. 每个 `.bf` 文件 = **一个函数体**
2. 函数签名写在 `.bf` **头部注释**里
3. `bfpe.exe` 把 BF **写进 PE 代码节**，并可用命令行 **构建 / 调用**
4. 构建流程 **强制验收**，排除「欺骗 DLL」和「伪装 DLL」

---

## 项目状态

🚧 **规划 / 早期开发** — runtime 与验收逻辑继承自 reference；独立 CLI 与签名 DSL 待实现。

| 能力 | reference | bfpe 目标 |
|------|-----------|-----------|
| BF 明文进 `.text` | ✅ | ✅ 继承 |
| VM + Stub | ✅ | ✅ 继承 |
| 输出缓冲 / 回调 | ✅ | ➜ 统一 I/O 流 |
| 输入 `,` | ❌（恒 0） | ➜ stdin / 回调 |
| 注释声明签名 | 部分（`export=output`） | ➜ 完整 DSL |
| 命令行 `bfpe.exe` | ❌（CMake 工程） | ➜ 目标 |
| 生成 EXE | ❌ | ➜ 目标 |

---

## 快速概念

### 示例：带参整数函数

`add.bf`：

```bf
; bfpe: export=Add
; bfpe: int add(int a, int b)
>[-<+>]
```

构建：

```powershell
bfpe build add.bf -o add.dll
# 简写
bfpe add.bf -o add.dll
```

调用：

```powershell
bfpe run add.dll Add 3 5
# 简写：末参数为已存在的 PE，前面为函数参数
bfpe add.bf 3 5 add.dll
# 期望输出：8
```

### 示例：标准输出型函数

`hello.bf`：

```bf
; bfpe: export=Hello
; bfpe: const char* hello(void)
; bfpe: io=stdio
++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.
```

```powershell
bfpe build hello.bf -o hello.dll
bfpe run hello.dll Hello
```

---

## 签名注释（DSL 草案）

在 `.bf` 顶部用 `;` 写元数据，不影响 BF 语义：

| 指令 | 含义 |
|------|------|
| `; bfpe: export=<Name>` | 导出名 → 链接符号 `BF_<Name>` |
| `; bfpe: <ret> <fn>(<params>)` | C 风格签名，驱动 Stub 生成 |
| `; bfpe: io=stdio` | `,` / `.` 绑定标准输入输出 |
| `; bfpe: io=buffer` | `.` 写入内部缓冲（默认，适合 `const char*` 返回） |
| `; bfpe: entry` | 标记 EXE 入口（单文件 EXE 时使用） |

**MVP 支持的类型：** `void`、`int`、`const char*`

**调用约定 BFC-0（x64）：**

- 第 *i* 个整型参数 → `tape[i]`（8-bit 截断）
- 整型返回值 → 执行后 `tape[0]`
- 字符串返回值 → `.` 输出经 trim 后返回

与 reference 中 `; bfdll: export=output` 兼容，视为 `const char* name(void)` + `io=buffer`。

---

## 命令行（目标接口）

```text
bfpe build <file.bf> [file2.bf ...] -o <out.dll|out.exe>   构建 PE
bfpe run   <pe> <ExportName> [args...]                     调用 DLL 导出
bfpe exec  <file.bf> [args...]                             内存解释执行（调试用，不写 PE）

简写：
bfpe <file.bf> -o <out.pe>                                 等同 build
bfpe <file.bf> <args...> <existing.pe>                     等同 run（按 .bf 签名匹配导出）
```

### 构建产物

- **`xxx.dll`** — 导出表指向 Native Stub；宿主 `LoadLibrary` / P/Invoke 调用
- **`xxx.exe`** — CONSOLE 子系统；`main` 入口调 Stub；BF 仍在 `.text` 明文

每次 `build` 结束后自动运行 **`verify_pe.ps1`**，未通过则构建失败。

---

## 合规保证（非伪 DLL）

BFPE 继承 reference 五项硬性约束，构建时强制检查：

| 约束 | 含义 |
|------|------|
| 合法 PE | 标准 DLL/EXE，可被系统加载器识别 |
| 非欺骗 | 禁止 BF→机器码 AOT/JIT；业务逻辑仅 VM 解释 |
| 非伪装 | 禁止 BF 进 `.rdata`、`.rsrc` 或外部文件 |
| BF 在代码节 | 明文必须在 `.text`；记事本可搜到 `+-<>[],.` |
| 唯一嵌入路径 | **仅** MASM `db` 写入 `.text`；C 源禁止 BF 字符串字面量 |

详见 [docs/可行性报告.md §4.4](docs/可行性报告.md#44-r4禁止欺骗-dll-与伪装-dll)。

---

## 目录结构（规划）

```text
bfpe/
├── docs/
│   └── 可行性报告.md          项目可行性分析
├── reference/
│   └── Brainfuck-in-PE/       Git 子模块：实验 reference 实现
├── runtime/                   VM、I/O、Stub（自 reference 提取/演进）
├── tools/
│   ├── bfpe.py                CLI 入口（规划）
│   ├── codegen/               签名解析、asm/stub 生成
│   └── verify_pe.ps1          PE 合规验收（由 reference 泛化）
├── templates/                 链接用 runtime 源码模板
├── examples/                  示例 .bf 与用法
└── README.md
```

---

## 构建要求（规划）

- Windows 10/11 x64
- Visual Studio 2022（MSVC v143、`ml64`、`link`）
- Python 3.10+（CLI / codegen）
- CMake 3.20+（可选，用于 runtime 静态库）

reference 子模块可先独立验证：

```powershell
git submodule update --init --recursive

cd reference/Brainfuck-in-PE
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cd build/bin/Release
.\test_host.exe
```

---

## 架构一览

```
.bf 源文件（函数体 + 头部签名注释）
        │
        ▼
   bfpe.exe（解析 / 代码生成）
        │
        ├── MASM: BF 明文 → .text$bf（db 嵌入）
        ├── C:    Stub + VM + I/O 流
        └── link → xxx.dll / xxx.exe
        │
        ▼
   verify_pe.ps1（五项约束）
        │
        ▼
   宿主 / bfpe run 调用
```

---

## 文档

| 文档 | 说明 |
|------|------|
| [docs/可行性报告.md](docs/可行性报告.md) | BFPE 四项需求的可行性分析与实施路线 |
| [reference/Brainfuck-in-PE/docs/实验报告.md](reference/Brainfuck-in-PE/docs/实验报告.md) | reference 实验结论与验收记录 |
| [reference/Brainfuck-in-PE/docs/实现方案.md](reference/Brainfuck-in-PE/docs/实现方案.md) | PE 布局与模块设计（bfpe 继承） |

---

## 许可证

待定。
