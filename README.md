# TraceRTL Standalone

`tracertl/` is a standalone TraceRTL-oriented simulation workspace extracted around the XiangShan TraceRTL flow.

It keeps only the pieces needed to:

- generate TraceRTL-enabled XiangShan RTL
- build a standalone Verilator emulator
- load TraceRTL instruction streams and optional trace page-table side files
- run end-to-end correctness checking without the full difftest runtime

The current standalone build generates the final emulator at the repository root:

- `build/emu`
- `build/emu-compile/`

while keeping local generated TraceRTL headers in:

- `tracertl/build/generated-src/`

## What this workspace contains

- `src/test/csrc/tracertl`: TraceRTL runtime, trace reader, fake icache/TLB/page-table helpers, fast-warmup logic, and commit/redirect checking
- `src/test/csrc/verilator`: standalone emulator entry, command-line parsing, reset/run loop, and TraceRTL integration
- `src/test/csrc/common`: minimal simulation runtime kept by the standalone flow, including RAM/device helpers, LightSSS fork support, and DPI shims such as `tracertl_dpi.cpp`
- `src/test/vsrc/common`: Verilog/SystemVerilog helper modules used by the standalone build
- `config`: standalone runtime configuration headers
- `Makefile` and `verilator.mk`: standalone RTL generation and Verilator build entry points

## What was intentionally removed

- full difftest runtime and reference model loading
- standalone coverage runtime and coverage-specific emulator driving code
- golden-memory-specific standalone naming and config residue

Note that the RTL still requires the DPI interfaces `pte_helper` and `amo_helper`, so standalone keeps those semantics through `src/test/csrc/common/tracertl_dpi.cpp` instead of the old golden-memory implementation.

## Build flow

The standalone build is split into two stages:

1. `make emu` runs XiangShan elaboration with `--trace-rtl` and emits `SimTop` RTL plus generated headers.
2. Verilator then compiles the standalone emulator and places the binary in `../build/emu`.

Typical command:

```bash
cd tracertl
make emu NOOP_HOME=.. NUM_CORES=1 CONFIG=MinimalConfig
```

Important defaults:

- `CONFIG ?= MinimalConfig`
- `NUM_CORES ?= 1`
- output binary goes to the repository root `build/emu`

## Run flow

Run through the wrapper target:

```bash
cd tracertl
make emu-run NOOP_HOME=.. TRACE_FILE=/abs/path/to/trace.zst
```

Equivalent direct invocation:

```bash
./build/emu --tracertl-file /abs/path/to/trace.zst
```

Common options:

```bash
make emu-run NOOP_HOME=.. TRACE_FILE=/abs/path/to/trace.zst MAX_CYCLES=2000000
make emu-run NOOP_HOME=.. TRACE_FILE=/abs/path/to/trace.zst TRACE_PT_FILE=/abs/path/to/trace.pt
make emu-run NOOP_HOME=.. TRACE_FILE=/abs/path/to/trace.zst GEN_PADDR=1
make emu-run NOOP_HOME=.. TRACE_FILE=/abs/path/to/trace.zst FAST_WARMUP=300000 WARMUP_INSTR=300000
```

Key runtime arguments accepted by `build/emu`:

- `--tracertl-file`
- `--tracept-file`
- `--gen-paddr`
- `--fast-warmup`
- `--skip-traceinstr`
- `--max-cycles`
- `--max-instr`

## Directory layout

- `config/`: standalone runtime configuration
- `src/test/csrc/common/`: common C/C++ simulation support
- `src/test/csrc/plugin/`: optional helper plugins such as `spike-dasm`
- `src/test/csrc/tracertl/`: TraceRTL runtime core
- `src/test/csrc/verilator/`: standalone emulator
- `src/test/vsrc/common/`: helper Verilog/SystemVerilog modules
- `build/generated-src/`: local generated TraceRTL headers for standalone integration

