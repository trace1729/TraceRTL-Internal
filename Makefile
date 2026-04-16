#***************************************************************************************
# TraceRTL standalone simulation makefile
#***************************************************************************************

TRACERTL_HOME := $(abspath .)
NOOP_HOME ?= $(abspath ..)

ROOT_BUILD_DIR := $(NOOP_HOME)/build
BUILD_DIR ?= $(ROOT_BUILD_DIR)
RTL_DIR := $(ROOT_BUILD_DIR)/rtl

SIM_TOP ?= SimTop
RTL_SUFFIX ?= sv
NUM_CORES ?= 1
CONFIG ?= MinimalConfig
ISSUE ?= E.b

EMU := $(BUILD_DIR)/emu
SIMTOP := top.SimTop
SIM_TOP_V := $(RTL_DIR)/$(SIM_TOP).$(RTL_SUFFIX)

SIM_MEM_ARGS = --firtool-opt "--repl-seq-mem --repl-seq-mem-file=$(SIM_TOP).$(RTL_SUFFIX).conf"
MFC_ARGS = --target systemverilog --split-verilog \
	--firtool-opt "-O=release --disable-annotation-unknown --lowering-options=explicitBitcast,disallowLocalVariables,disallowPortDeclSharing,locationInfoStyle=none"

SCALA_FILE := $(shell find $(NOOP_HOME)/src/main/scala -name '*.scala')
TEST_FILE := $(shell find $(NOOP_HOME)/src/test/scala -name '*.scala')

MILL_SIM_ARGS += --trace-rtl --disable-always-basic-diff $(SIM_MEM_ARGS) $(MFC_ARGS)
ifneq ($(WITH_CHISELDB),)
MILL_SIM_ARGS += --with-chiseldb
endif
ifneq ($(WITH_CHISELMAP),)
MILL_SIM_ARGS += --with-chiselmap
endif
ifneq ($(WITH_CONSTANTIN),)
MILL_SIM_ARGS += --with-constantin
endif
ifneq ($(WITH_RESETGEN),)
MILL_SIM_ARGS += --reset-gen
endif
ifneq ($(DISABLE_PERF),)
MILL_SIM_ARGS += --disable-perf
endif
ifneq ($(DISABLE_ALWAYSDB),)
MILL_SIM_ARGS += --disable-alwaysdb
endif

MEM_GEN := $(NOOP_HOME)/scripts/vlsi_mem_gen
MEM_GEN_SEP := $(NOOP_HOME)/scripts/gen_sep_mem.sh

.DEFAULT_GOAL := emu

help:
	@echo "Targets: sim-verilog emu emu-run clean"
	@echo "Variables: TRACE_FILE=... TRACE_PT_FILE=... CONFIG=... NUM_CORES=... EMU_EXTRA_ARGS=..."

$(SIM_TOP_V): $(SCALA_FILE) $(TEST_FILE)
	@mkdir -p $(RTL_DIR) $(TRACERTL_HOME)/build/generated-src
	cd $(NOOP_HOME) && NOOP_HOME=$(TRACERTL_HOME) mill -i xiangshan.test.runMain $(SIMTOP) \
		--target-dir $(RTL_DIR) --config $(CONFIG) --issue $(ISSUE) \
		--num-cores $(NUM_CORES) $(MILL_SIM_ARGS) --full-stacktrace
	$(MEM_GEN_SEP) "$(MEM_GEN)" "$(SIM_TOP_V).conf" "$(RTL_DIR)"

sim-verilog: $(SIM_TOP_V)

include verilator.mk

emu: sim-verilog $(EMU)

emu-run: emu
ifndef TRACE_FILE
	$(error TRACE_FILE is required, e.g. make emu-run TRACE_FILE=/path/to/trace.zst)
endif
	$(EMU) --tracertl-file $(TRACE_FILE) \
		$(if $(TRACE_PT_FILE),--tracept-file $(TRACE_PT_FILE),) \
		$(if $(MAX_CYCLES),--max-cycles=$(MAX_CYCLES),) \
		$(if $(MAX_INSTR),--max-instr=$(MAX_INSTR),) \
		$(if $(WARMUP_INSTR),--warmup-instr=$(WARMUP_INSTR),) \
		$(if $(FAST_WARMUP),--fast-warmup=$(FAST_WARMUP),) \
		$(if $(SKIP_TRACEINSTR),--skip-traceinstr=$(SKIP_TRACEINSTR),) \
		$(if $(GEN_PADDR),--gen-paddr,) \
		$(EMU_EXTRA_ARGS)

clean:
	rm -rf $(TRACERTL_HOME)/build

.PHONY: help sim-verilog emu emu-run clean
