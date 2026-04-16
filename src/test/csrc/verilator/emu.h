/***************************************************************************************
* Copyright (c) 2020-2023 Institute of Computing Technology, Chinese Academy of Sciences
* Copyright (c) 2020-2021 Peng Cheng Laboratory
*
* DiffTest is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#ifndef __EMU_H
#define __EMU_H

#include "VSimTop.h"
#include "common.h"
#include "dut.h"
#include "lightsss.h"
#include <sys/types.h>
#ifdef ENABLE_FST
#include <verilated_fst_c.h>
#else
#include <verilated_vcd_c.h> // Trace file format header
#endif
#ifdef EMU_THREAD
#include <verilated_threads.h>
#endif
#if VM_TRACE == 1
#include <verilated_vcd_c.h> // Trace file format header
#endif

struct EmuArgs {
  uint32_t reset_cycles = 50;
  uint32_t seed = 0;
  uint64_t max_cycles = -1;
  uint64_t fork_interval = 1000;
  uint64_t max_instr = -1;
  uint64_t warmup_instr = -1;
  uint64_t log_begin = 0, log_end = -1;
  const char *image = nullptr;
  const char *wave_path = nullptr;
  const char *ram_size = nullptr;
  const char *tracertl_file = nullptr;
  const char *tracept_file = nullptr;
  bool enable_waveform = false;
  bool enable_waveform_full = false;
  bool enable_fork = false;
  bool enable_gen_paddr = false;
  bool fast_warmup = false;
  uint64_t fast_warmup_instr = 0;
  uint64_t skip_traceinstr = 0;
};

class Emulator final : public DUT {
private:
  VSimTop *dut_ptr;
#ifdef ENABLE_FST
  VerilatedFstC *tfp;
#else
  VerilatedVcdC *tfp;
#endif
  EmuArgs args;
  LightSSS *lightsss = nullptr;

  // emu control variable
  uint64_t cycles;
  int trapCode;
  uint32_t lasttime_snapshot = 0;
  uint32_t lasttime_poll = 0;
  uint32_t elapsed_time;
  bool warmup_reported = false;

  inline void reset_ncycles(size_t cycles);
  inline void single_cycle();
  void display_trapinfo();
  inline char *timestamp_filename(time_t t, char *buf);
  inline char *waveform_filename(time_t t);
  void fork_child_init();
  inline bool is_fork_child() {
    return lightsss != nullptr && lightsss->is_child();
  }

public:
  Emulator(int argc, const char *argv[]);
  ~Emulator();
  uint64_t get_cycles() const {
    return cycles;
  }
  EmuArgs get_args() const {
    return args;
  }
  bool is_good_trap() {
    return trapCode == STATE_GOODTRAP || trapCode == STATE_LIMIT_EXCEEDED || trapCode == STATE_SIM_EXIT ||
           trapCode == STATE_TRACE_OVER;
  };
  int get_trapcode() {
    return trapCode;
  }
  int tick();
  int is_finished();
  int is_good();
};

#endif
