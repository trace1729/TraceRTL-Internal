# TraceRTL

This workspace is a TraceRTL simulation environment that supports two build modes:

- standalone mode with a local Chisel DUT named Hollow
- XiangShan mode for future subrepo-style integration back into a parent XiangShan workspace

It keeps only the pieces needed to:

- generate TraceRTL-enabled Hollow RTL locally with Chisel
- build a Verilator emulator
- load TraceRTL instruction streams and optional trace page-table side files
- run end-to-end correctness checking without XiangShan or the full difftest runtime

The current standalone build generates the final emulator at the repository root:

- `build/emu`
- `build/emu-compile/`

while keeping local generated TraceRTL headers in:

- `build/generated-src/`

The Hollow design is documented in [docs/hollow-design.zh-CN.md](docs/hollow-design.zh-CN.md).

## What this workspace contains

- `src/main/scala/hollow`: Hollow Chisel sources, parameter definitions, BlackBox wrappers, assertions, and the local `HollowTop` generator
- `src/test/csrc/tracertl`: TraceRTL runtime, trace reader, fake icache/TLB/page-table helpers, fast-warmup logic, and commit/redirect checking
- `src/test/csrc/verilator`: emulator entry, command-line parsing, reset/run loop, and TraceRTL integration
- `src/test/csrc/common`: minimal simulation runtime kept by the  flow, including RAM/device helpers, LightSSS fork support, and DPI shims such as `tracertl_dpi.cpp`
- `src/test/vsrc/common`: Verilog/SystemVerilog helper modules used by the Hollow/local build
- `config`: runtime configuration headers
- `Makefile`, `verilator.mk`, `build.sc`, and `.mill-version`: local Hollow elaboration and Verilator build entry points

## Hollow design

Hollow is intentionally minimal:

- it refills from TraceRTL in 8-instruction bursts
- it drives at most 4 instructions per cycle and commits at most 4 instructions per cycle
- it uses local queues to decouple 8-wide refill from 4-wide issue/commit
- it injects low-probability redirect events on head branch instructions to simulate branch misprediction recovery
- it does not execute instructions architecturally; it only preserves the TraceRTL handshake expected by the checker
- it keeps the existing runtime arguments used by `build/emu`, while the local Hollow generator itself only needs `--target-dir`

The runtime still keeps the DPI helpers `pte_helper` and `amo_helper` in `src/test/csrc/common/tracertl_dpi.cpp`, so the memory-side helper behavior remains compatible with the existing local TraceRTL environment.

Implementation notes:

- the standalone top module is now `HollowTop`
- Hollow Scala sources are split into multiple files instead of one monolithic source
- `HollowTraceFetch` and `HollowTraceReporter` use per-slot ports to make waveform debug more direct
- continuity assertions now check adjacent PC and `instId` relationships for fetched and queued instructions

## Build flow

The standalone build is split into two stages:

1. `make emu` runs the local Hollow Chisel elaboration with Mill and emits `HollowTop` RTL.
2. Verilator then compiles the standalone emulator and places the binary in `build/emu`.

Typical standalone command:

```bash
make emu
```

Important defaults:

- `CONFIG ?= MinimalConfig`
- `NUM_CORES ?= 1`
- `ISSUE ?= E.b`
- output binary goes to the repository root `build/emu`
- Hollow no longer consumes those compatibility variables during local generation; the generated DUT is intentionally fixed-function

Mode-switching commands:

```bash
make standalone-emu
make standalone-emu-run TRACE_FILE=/abs/path/to/trace.zst
make xiangshan-emu NOOP_HOME=/abs/path/to/XiangShan
make xiangshan-emu-run NOOP_HOME=/abs/path/to/XiangShan TRACE_FILE=/abs/path/to/trace.zst
```

Equivalent variable-based switching:

```bash
make MODE=standalone emu
make MODE=xiangshan emu NOOP_HOME=/abs/path/to/XiangShan
```

## Run flow

Run through the wrapper target:

```bash
make emu-run TRACE_FILE=/abs/path/to/trace.zst
```

Equivalent direct invocation:

```bash
./build/emu --tracertl-file /abs/path/to/trace.zst
```

Common options:

```bash
make emu-run TRACE_FILE=/abs/path/to/trace.zst MAX_CYCLES=2000000
make emu-run TRACE_FILE=/abs/path/to/trace.zst TRACE_PT_FILE=/abs/path/to/trace.pt
make emu-run TRACE_FILE=/abs/path/to/trace.zst GEN_PADDR=1
make emu-run TRACE_FILE=/abs/path/to/trace.zst FAST_WARMUP=300000 WARMUP_INSTR=300000
```

Wave dump options:

```bash
make emu WAVE_FORMAT=vcd
make emu-run TRACE_FILE=/abs/path/to/trace.zst WAVE=1 WAVE_PATH=build/hollow.vcd

make emu WAVE_FORMAT=fst
make emu-run TRACE_FILE=/abs/path/to/trace.zst WAVE=1 WAVE_PATH=build/hollow.fst
```

Notes:

- `WAVE_FORMAT` controls the Verilator trace backend at build time and defaults to `vcd`
- `WAVE=1` forwards `--dump-wave` to the runtime
- `WAVE_FULL=1` forwards `--dump-wave-full`
- `WAVE_PATH=/abs/path/to/file.{vcd,fst}` overrides the output path

Key runtime arguments accepted by `build/emu`:

- `--tracertl-file`
- `--tracept-file`
- `--gen-paddr`
- `--fast-warmup`
- `--skip-traceinstr`

## XiangShan mode

The XiangShan mode keeps the original `xiangshan.test.runMain top.SimTop` elaboration flow and is meant for using this repository as a subrepo inside a larger XiangShan workspace.

Current status:

- the mode switch and command expansion are implemented in `Makefile`
- the dry-run command expands to the expected parent-workspace elaboration flow
- full runtime validation has not been completed yet because this repository is not currently mounted as a XiangShan subrepo in the target workspace
- `--max-cycles`
- `--max-instr`

## Directory layout

- `src/main/scala/hollow/`: Hollow Chisel elaboration sources
- `docs/hollow-design.zh-CN.md`: detailed Hollow design document
- `config/`: standalone runtime configuration
- `src/test/csrc/common/`: common C/C++ simulation support
- `src/test/csrc/plugin/`: optional helper plugins such as `spike-dasm`
- `src/test/csrc/tracertl/`: TraceRTL runtime core
- `src/test/csrc/verilator/`: emulator
- `src/test/vsrc/common/`: helper Verilog/SystemVerilog modules
- `build/generated-src/`: local generated TraceRTL headers for Hollow/local integration