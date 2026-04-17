#***************************************************************************************
# TraceRTL standalone simulation makefile
#***************************************************************************************

TRACERTL_HOME := $(abspath .)
NOOP_HOME ?= $(abspath ..)
MODE ?= standalone
LOCAL_GEN_CSRC_DIR := $(TRACERTL_HOME)/build/generated-src

STANDALONE_MILL_MODULE := standalone
STANDALONE_TOP_MODULE := HollowTop
STANDALONE_TOP_MAIN := hollow.HollowTop
STANDALONE_DUT_INFO_SRC := $(TRACERTL_HOME)/src/test/csrc/tracertl/tracertl_dut_info_standalone.h

ifeq ($(MODE),standalone)
NOOP_HOME := $(TRACERTL_HOME)
ROOT_BUILD_DIR := $(TRACERTL_HOME)/build
BUILD_DIR ?= $(ROOT_BUILD_DIR)
RTL_DIR := $(ROOT_BUILD_DIR)/rtl
SIMTOP := $(STANDALONE_TOP_MAIN)
SIM_TOP ?= $(STANDALONE_TOP_MODULE)
EXTRA_SIM_CXXFLAGS += -DTRACERTL_STANDALONE_HOLLOW_TOP
SCALA_FILE := $(shell find $(TRACERTL_HOME)/src/main/scala -name '*.scala')
SIM_VERILOG_DEPS := $(SCALA_FILE) $(abspath ./build.sc) $(abspath ./.mill-version) $(LOCAL_GEN_CSRC_DIR)/tracertl_dut_info.h
MILL_GENERATE_CMD = cd $(TRACERTL_HOME) && NOOP_HOME=$(TRACERTL_HOME) mill -i $(STANDALONE_MILL_MODULE).runMain $(SIMTOP) --target-dir $(RTL_DIR)
else ifeq ($(MODE),xiangshan)
ROOT_BUILD_DIR := $(NOOP_HOME)/build
BUILD_DIR ?= $(ROOT_BUILD_DIR)
RTL_DIR := $(ROOT_BUILD_DIR)/rtl
SIMTOP := top.SimTop
SIM_TOP ?= SimTop
SCALA_FILE := $(shell find $(NOOP_HOME)/src/main/scala -name '*.scala')
TEST_FILE := $(shell find $(NOOP_HOME)/src/test/scala -name '*.scala')
SIM_VERILOG_DEPS := $(SCALA_FILE) $(TEST_FILE)

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
MILL_GENERATE_CMD = cd $(NOOP_HOME) && NOOP_HOME=$(TRACERTL_HOME) mill -i xiangshan.test.runMain $(SIMTOP) --target-dir $(RTL_DIR) --config $(CONFIG) --issue $(ISSUE) --num-cores $(NUM_CORES) $(MILL_SIM_ARGS) --full-stacktrace && $(MEM_GEN_SEP) "$(MEM_GEN)" "$(SIM_TOP_V).conf" "$(RTL_DIR)"
else
$(error Unsupported MODE=$(MODE), expected standalone or xiangshan)
endif

RTL_SUFFIX ?= sv
NUM_CORES ?= 1
CONFIG ?= MinimalConfig
ISSUE ?= E.b

EMU := $(BUILD_DIR)/emu
SIM_TOP_V := $(RTL_DIR)/$(SIM_TOP).$(RTL_SUFFIX)

SIM_MEM_ARGS = --firtool-opt "--repl-seq-mem --repl-seq-mem-file=$(SIM_TOP).$(RTL_SUFFIX).conf"
MFC_ARGS = --target systemverilog --split-verilog \
	--firtool-opt "-O=release --disable-annotation-unknown --lowering-options=explicitBitcast,disallowLocalVariables,disallowPortDeclSharing,locationInfoStyle=none"

.DEFAULT_GOAL := emu

WAVE_FORMAT ?= vcd
VM_PARALLEL_BUILDS ?= 1

help:
	@echo "Targets: sim-verilog emu emu-run clean standalone-emu standalone-emu-run xiangshan-emu xiangshan-emu-run"
	@echo "Variables: MODE=standalone|xiangshan TRACE_FILE=... TRACE_PT_FILE=... CONFIG=... NUM_CORES=... NOOP_HOME=... WAVE=1 WAVE_FULL=1 WAVE_PATH=... WAVE_FORMAT=vcd|fst EMU_EXTRA_ARGS=... VM_PARALLEL_BUILDS=1|2|4"


$(LOCAL_GEN_CSRC_DIR)/tracertl_dut_info.h: $(STANDALONE_DUT_INFO_SRC)
	@mkdir -p $(@D)
	cp $< $@


$(SIM_TOP_V): $(SIM_VERILOG_DEPS)
	@mkdir -p $(RTL_DIR) $(TRACERTL_HOME)/build/generated-src
	$(MILL_GENERATE_CMD)

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
		$(if $(WAVE),--dump-wave,) \
		$(if $(WAVE_FULL),--dump-wave-full,) \
		$(if $(WAVE_PATH),--wave-path=$(WAVE_PATH),) \
		$(EMU_EXTRA_ARGS)

standalone-emu:
	$(MAKE) MODE=standalone emu

standalone-emu-run:
	$(MAKE) MODE=standalone emu-run TRACE_FILE="$(TRACE_FILE)" TRACE_PT_FILE="$(TRACE_PT_FILE)" MAX_CYCLES="$(MAX_CYCLES)" MAX_INSTR="$(MAX_INSTR)" WARMUP_INSTR="$(WARMUP_INSTR)" FAST_WARMUP="$(FAST_WARMUP)" SKIP_TRACEINSTR="$(SKIP_TRACEINSTR)" GEN_PADDR="$(GEN_PADDR)" EMU_EXTRA_ARGS="$(EMU_EXTRA_ARGS)"

xiangshan-emu:
	$(MAKE) MODE=xiangshan emu NOOP_HOME="$(NOOP_HOME)"

xiangshan-emu-run:
	$(MAKE) MODE=xiangshan emu-run NOOP_HOME="$(NOOP_HOME)" TRACE_FILE="$(TRACE_FILE)" TRACE_PT_FILE="$(TRACE_PT_FILE)" MAX_CYCLES="$(MAX_CYCLES)" MAX_INSTR="$(MAX_INSTR)" WARMUP_INSTR="$(WARMUP_INSTR)" FAST_WARMUP="$(FAST_WARMUP)" SKIP_TRACEINSTR="$(SKIP_TRACEINSTR)" GEN_PADDR="$(GEN_PADDR)" EMU_EXTRA_ARGS="$(EMU_EXTRA_ARGS)"

clean:
	rm -rf $(TRACERTL_HOME)/build $(TRACERTL_HOME)/out

.PHONY: help sim-verilog emu emu-run clean standalone-emu standalone-emu-run xiangshan-emu xiangshan-emu-run
