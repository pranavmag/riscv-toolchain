# riscv-toolchain

A complete, self-hosted RISC-V toolchain built from scratch in C++ — no external dependencies beyond the standard library and a standard RISC-V assembler for the final assemble step. The project consists of a cycle-accurate CPU emulator and a compiler for a small C subset ("Mini C") that targets it directly, producing flat binaries that run end-to-end on the emulator.

Built alongside study of Patterson & Hennessy's *Computer Organization and Design: RISC-V Edition* and documented on [YouTube](https://www.youtube.com/@NootNavDev).

---

## Repository Structure

```
riscv-toolchain/
├── emulator/          # RISC-V CPU emulator (RV32I + M + F)
│   └── src/
│       ├── decoder.h / decoder.cpp        # Instruction decode logic
│       ├── interpreter.h / interpreter.cpp # Execution, pipeline, memory
│       └── main.cpp                        # Entry point, mode selection
├── compiler/          # Mini C compiler targeting the emulator
│   └── src/
│       ├── lexer.h / lexer.cpp            # Tokenizer
│       ├── ast.h                          # AST node definitions
│       ├── parser.h / parser.cpp          # Recursive-descent parser
│       ├── visitor.h / visitor.cpp        # Visitor interfaces + accept()
│       ├── astprinter.h                   # AST debug printer
│       ├── irgenerator.h / irgenerator.cpp # Three-address IR generation
│       ├── regalloc.h / regalloc.cpp      # Naive stack-based register allocator
│       ├── codegen.h / codegen.cpp        # RISC-V assembly code generator
│       └── main.cpp                       # REPL + file-compilation entry point
├── shared/            # Shared types (IR structs, etc.) — header-only
│   └── include/shared/
├── run.sh             # Compiles + assembles + runs a .c file in one command
└── CMakeLists.txt     # Root CMake — C++20, out-of-source builds enforced
```

---

## Emulator

A cycle-accurate RISC-V emulator supporting the full **RV32I + RV32M + RV32F** ISA. Two execution modes are implemented: single-cycle and a 5-stage pipelined core with hazard handling.

### ISA Coverage

| Extension | Instructions |
|-----------|-------------|
| RV32I | All base integer instructions — arithmetic, logic, shifts, branches, loads, stores, jumps, environment |
| RV32M | MUL, MULH, MULHU, MULHSU, DIV, DIVU, REM, REMU |
| RV32F | FLW, FSW, FADD.S, FSUB.S, FMUL.S, FDIV.S, FSQRT.S, FMIN, FMAX, FMADD.S, FMSUB.S, FNMADD.S, FNMSUB.S, FSGNJ.S, FSGNJN.S, FSGNJX.S, FCVT.W.S, FCVT.WU.S, FCVT.S.W, FCVT.S.WU, FMV.X.W, FMV.W.X, FEQ.S, FLT.S, FLE.S, FCLASS.S |

### Pipeline (5-Stage)

The pipelined core implements the classic **IF → ID → EX → MEM → WB** stages with:

- Double-buffered pipeline registers simulating hardware clock edges
- **Data hazard resolution** via full forwarding (EX/MEM and MEM/WB paths)
- **Load-use hazard** detection with 1-cycle stall insertion
- **Control hazard** resolution via predict-not-taken with 2-cycle flush on misprediction

### Memory & ABI

- 64KB byte-addressed, little-endian flat memory
- Stack pointer initialized to top of memory (`0x10000`)
- Loads raw flat `.bin` files — no ELF parsing
- Custom ECALL ABI: `a7=1` prints integer in `a0`; `a7=93` exits

### Modes

| Mode | Description |
|------|-------------|
| Disassembler | Decode 32-bit binary strings and print assembly mnemonics |
| Manual interpreter | Enter binary instructions, execute, dump register state |
| Binary interpreter | Load a `.bin` file, run, dump register state |

### Verification

Verified against a Fibonacci sequence program (compiled with xPack RISC-V Embedded GCC) producing correct results across register file, branches, and loop control flow.

---

## Compiler — Mini C

A complete compiler for a small C subset, "Mini C," targeting the emulator directly. Source goes in through a lexer, parser, IR generator, register allocator, and code generator entirely written in-house — the only external tool in the loop is a standard RISC-V assembler (`as`/`objcopy`) for the final text-assembly-to-binary step.

### Architecture

```
Source (.c)
    └── Lexer           → token stream
    └── Parser          → Abstract Syntax Tree (AST)
    └── IR Generator    → three-address IR (Quads) over a control-flow graph,
                           one CFG per function, explicit branches/jumps only
                           (no implicit fall-through between blocks)
    └── Register Allocator (Naive / Stack-Based)
                         → every virtual register gets its own permanent
                           stack slot for its function's lifetime
    └── Code Generator  → RV32IM assembly (.s)
    └── RISC-V as       → object file        [external]
    └── objcopy         → flat binary (.bin) [external]
    └── Emulator        → execution
```

### Calling Convention

Standard RISC-V Base Integer Calling Convention (as per Patterson & Hennessy):
- Arguments in `a0`–`a7` (up to 8; no stack-passed arguments yet), return value in `a0`, return address in `ra`
- Stack frame managed with callee-saved prologue/epilogue (`ra` and the caller's `fp` saved at fixed offsets `-4(fp)`/`-8(fp)` in every frame)
- The top-level statements in a file act as an implicit entry point, called first by a fixed `_start` stub; if the file also defines its own `main()`, that's called next, and *its* return value becomes the actual program exit code

### Mini C Language Reference

**Types**: `int` is the only type that's actually meaningful — every value is a 32-bit word under the hood. `float`, `char`, and `void` are accepted as syntax (for declarations, parameters, and return types) but aren't type-checked or given distinct behavior.

**Supported:**
- Variable declarations, with or without an initializer: `int x;`, `int x = 5;`
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Unary: `-x`, `!x`
- Comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Assignment, including chained: `x = 5;`, `x = y = 5;`
- `if` / `else`, arbitrarily nested
- `while` loops, arbitrarily nested
- Function declarations with up to 8 parameters and a return value: `int add(int a, int b) { return a + b; }`
- Function calls, including recursion and calls between sibling functions
- `return`, with or without a value — a function that falls off the end without one gets an implicit `return 0;`
- `print(x)` — prints a single integer expression, followed by a newline
- Block scoping — a `{ }` introduces a new scope; a variable declared inside doesn't leak out, and a function's own parameters/locals are fully isolated from every other function's, including the caller's

**Not supported:**
- Function prototypes / forward declarations — a function must be fully defined (with a body) before anything else in the file calls it; compilation is single-pass
- `for` loops
- Arrays, pointers, strings, structs — `int` is the only real type
- More than 8 function arguments
- Any standard library beyond `print(x)`
- Multiple source files / `#include`

**Example — a Mini C source file, `factorial.c`:**
```c
int fact(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

int main() {
    print(fact(5));
    return 0;
}
```

### Running a Mini C Program

Compile and run a `.c` file all the way through the toolchain in one command:

```bash
./run.sh factorial.c
```

This compiles the file, assembles and links the resulting `.s` with a real RISC-V assembler, extracts a flat binary with `objcopy`, and runs it on the emulator — printing just the program's actual output (`120` for the example above).

`run.sh` needs a RISC-V assembler on your `PATH`. If you don't have one:

```bash
# Debian/Ubuntu/WSL
sudo apt-get install binutils-riscv64-unknown-elf

# Fedora
sudo dnf install binutils-riscv64-linux-gnu

# macOS (Homebrew)
brew install riscv64-elf-binutils
```

Adjust `AS_PREFIX` at the top of `run.sh` to match whichever one you install — the binary name prefix differs by distribution (`riscv64-unknown-elf`, `riscv64-linux-gnu`, etc.).

The compiler binary itself (`build/compiler/compiler`) can also be run directly:
- `./build/compiler/compiler file.c` — compiles the file and writes `file.c.s`
- `./build/compiler/compiler` (no arguments) — starts an interactive REPL that prints the generated IR and register allocation after each line, useful for inspecting the compiler's own behavior rather than running a program

---

## Build

**Requirements:** CMake 3.20+, a C++20-capable compiler (MSVC, GCC, or Clang), and a RISC-V assembler on `PATH` if you want to run compiled programs (see above)

```bash
git clone https://github.com/pranavmag/riscv-toolchain
cd riscv-toolchain

mkdir build
cd build
cmake ..
cmake --build .
```

In-source builds are explicitly rejected by the root CMakeLists.

**Visual Studio:** Open the `riscv-toolchain/` folder directly via **File → Open → Folder**. Visual Studio detects the CMakeLists automatically — no `.sln` or `.vcxproj` needed.

---

## Roadmap

### Emulator

- [x] Instruction decoder — all RV32I formats (R, I, S, B, J, U)
- [x] Single-cycle execution engine
- [x] Byte-addressed memory with little-endian read/write
- [x] Flat `.bin` file loading
- [x] Disassembler mode
- [x] Program counter and fetch-decode-execute loop
- [x] Branches, loads, stores, jumps
- [x] RV32M extension (multiply/divide)
- [x] RV32F extension (single-precision float)
- [x] 5-stage pipeline with forwarding and hazard detection
- [x] Fibonacci verification

### Compiler

- [x] Lexer — tokenize Mini C source
- [x] Parser — recursive descent, build AST
- [x] IR Generator — three-address IR (Quads) over a per-function CFG, with correct scoping and control flow
- [x] Naive register allocator — all variables on stack
- [x] Code generator — emit RV32IM assembly, including branches and function calls
- [x] End-to-end: compile and run a Mini C program on the emulator
- [ ] Upgrade to linear scan register allocator (currently every vreg gets a permanent slot; no liveness-based reuse)
- [ ] Stack-passed arguments (currently capped at 8, `a0`–`a7` only)
- [ ] Function prototypes / forward declarations (currently single-pass, define-before-use only)

### Toolchain

- [ ] Binary emitter — emit `.bin` directly (remove external assembler dependency)
- [ ] CMake integration — single command builds and runs a `.c` file (currently `run.sh` handles this outside CMake)

---

## Learning Resources

- Patterson & Hennessy — *Computer Organization and Design: RISC-V Edition*
- Nystrom — *Crafting Interpreters*

Documented on YouTube: [NootNavDev](https://www.youtube.com/@NootNavDev)