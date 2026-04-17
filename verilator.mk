SIM_CONFIG_DIR := $(abspath ./config)
SIM_CSRC_DIR := $(abspath ./src/test/csrc/common)
ROOT_GEN_CSRC_DIR := $(ROOT_BUILD_DIR)/generated-src
LOCAL_GEN_CSRC_DIR := $(abspath ./build/generated-src)
PLUGIN_CSRC_DIR := $(abspath ./src/test/csrc/plugin)
PLUGIN_SPIKE_DIR := $(abspath $(PLUGIN_CSRC_DIR)/spikedasm)
VSRC_DIR := $(abspath ./src/test/vsrc/common)
TRACERTL_CSRC_DIR := $(abspath ./src/test/csrc/tracertl)

EMU_TOP := $(SIM_TOP)
EMU_CSRC_DIR := $(abspath ./src/test/csrc/verilator)

COMMON_CXXFILES := \
  $(SIM_CSRC_DIR)/main.cpp \
  $(SIM_CSRC_DIR)/common.cpp \
  $(SIM_CSRC_DIR)/compress.cpp \
  $(SIM_CSRC_DIR)/dut.cpp \
  $(SIM_CSRC_DIR)/device.cpp \
  $(SIM_CSRC_DIR)/lightsss.cpp \
  $(SIM_CSRC_DIR)/ram.cpp \
  $(SIM_CSRC_DIR)/tracertl_dpi.cpp \
  $(SIM_CSRC_DIR)/sdcard.cpp \
  $(SIM_CSRC_DIR)/uart.cpp

SIM_CXXFILES := $(COMMON_CXXFILES)
SIM_CXXFILES += $(shell find $(ROOT_GEN_CSRC_DIR) -name '*.cpp' 2> /dev/null)
SIM_CXXFILES += $(shell find $(TRACERTL_CSRC_DIR) -name '*.cpp')
SIM_CXXFILES += $(shell find $(PLUGIN_SPIKE_DIR) -name '*.cpp')

SIM_CXXFLAGS := -I$(SIM_CSRC_DIR) -I$(SIM_CONFIG_DIR)
SIM_CXXFLAGS += -I$(ROOT_GEN_CSRC_DIR) -I$(LOCAL_GEN_CSRC_DIR)
SIM_CXXFLAGS += -I$(TRACERTL_CSRC_DIR)
SIM_CXXFLAGS += -I$(PLUGIN_SPIKE_DIR)
SIM_CXXFLAGS += -DNOOP_HOME=\\\"$(NOOP_HOME)\\\" -DTRACERTL_MODE
SIM_CXXFLAGS += $(EXTRA_SIM_CXXFLAGS)

SIM_VSRC := $(shell find $(VSRC_DIR) -name '*.v' -o -name '*.sv')
SIM_LDFLAGS += -lz -lzstd

EMU_CXXFILES := $(SIM_CXXFILES) $(EMU_CSRC_DIR)/emu.cpp
EMU_CXXFLAGS := $(SIM_CXXFLAGS) -I$(EMU_CSRC_DIR) -DVERILATOR -DNUM_CORES=$(NUM_CORES) --std=c++17
EMU_LDFLAGS := $(SIM_LDFLAGS)

VERILATOR_VER_CMD = verilator --version 2> /dev/null | cut -f2 -d' ' | tr -d '.'
VERILATOR_5_000 := $(shell expr `$(VERILATOR_VER_CMD)` \>= 5000 2> /dev/null)
ifeq ($(VERILATOR_5_000),1)
VEXTRA_FLAGS += --no-timing +define+VERILATOR_5
else
VEXTRA_FLAGS += +define+VERILATOR_LEGACY
endif

EMU_TRACE ?= $(WAVE_FORMAT)
ifneq (,$(filter $(EMU_TRACE),1 vcd VCD))
VEXTRA_FLAGS += --trace
endif
ifneq (,$(filter $(EMU_TRACE),fst FST))
VEXTRA_FLAGS += --trace-fst
EMU_CXXFLAGS += -DENABLE_FST
endif

EMU_OPTIMIZE ?= -O3
OPT_FAST ?= -O3

VERILATOR_FLAGS = \
  --exe $(EMU_OPTIMIZE) \
  --cc --top-module $(EMU_TOP) \
  +define+VERILATOR=1 \
  +define+PRINTF_COND=1 \
  +define+RANDOMIZE_REG_INIT \
  +define+RANDOMIZE_MEM_INIT \
  +define+RANDOMIZE_GARBAGE_ASSIGN \
  +define+RANDOMIZE_DELAY=0 \
  -Wno-STMTDLY -Wno-WIDTH \
  --max-num-width 150000 \
  --assert --x-assign unique \
  --output-split 30000 \
  --output-split-cfuncs 30000 \
  -I$(RTL_DIR) \
  -I$(ROOT_GEN_CSRC_DIR) \
  -I$(LOCAL_GEN_CSRC_DIR) \
  -CFLAGS "$(EMU_CXXFLAGS)" \
  -LDFLAGS "$(EMU_LDFLAGS)" \
  -o $(abspath $(EMU)) \
  $(VEXTRA_FLAGS)

EMU_DIR := $(BUILD_DIR)/emu-compile
EMU_MK := $(EMU_DIR)/V$(EMU_TOP).mk
EMU_DEPS := $(SIM_VSRC) $(EMU_CXXFILES)
EMU_HEADERS := $(shell find $(EMU_CSRC_DIR) -name '*.h') \
               $(shell find $(SIM_CSRC_DIR) -name '*.h') \
               $(shell find $(TRACERTL_CSRC_DIR) -name '*.h') \
               $(shell find $(PLUGIN_SPIKE_DIR) -name '*.h') \
               $(LOCAL_GEN_CSRC_DIR)/tracertl_dut_info.h

VERILATOR_INPUTS := $(SIM_TOP_V) $(SIM_VSRC) $(EMU_CXXFILES)

$(EMU_MK): $(VERILATOR_INPUTS) $(abspath ./Makefile) $(abspath ./verilator.mk)
	@mkdir -p $(@D)
	verilator $(VERILATOR_FLAGS) -Mdir $(@D) $(VERILATOR_INPUTS)
ifneq ($(VERILATOR_5_000),1)
	@sed -i 's/private/public/g' $(EMU_DIR)/V$(EMU_TOP).h
	@sed -i 's/const vlSymsp/vlSymsp/g' $(EMU_DIR)/V$(EMU_TOP).h
	@sed -i 's/VlThreadPool\* const/VlThreadPool*/g' $(EMU_DIR)/V$(EMU_TOP)__Syms.h
endif

build_emu:
	$(MAKE) -s VM_PARALLEL_BUILDS=$(VM_PARALLEL_BUILDS) OPT_SLOW="-O0" OPT_FAST=$(OPT_FAST) -C $(EMU_DIR) -f $(EMU_MK)

$(EMU): $(EMU_MK) $(EMU_DEPS) $(EMU_HEADERS)
	$(MAKE) build_emu OPT_FAST=$(OPT_FAST)

clean_obj:
	rm -f $(EMU_DIR)/*.o $(EMU_DIR)/*.gch $(EMU_DIR)/*.a $(EMU)

.PHONY: build_emu clean_obj
