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

#ifndef __TRACE_WRONG_PATH_H__
#define __TRACE_WRONG_PATH_H__

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "trace_format.h"

/*
 * Real wrong-path emulation oracle.
 *
 * The trace only records the right path. When the predicted fetch blocks
 * leave the trace, the hardware asks this oracle which instructions sit at
 * the predicted fetch PCs. Each PC is mapped to a dynamic instance of the
 * same static instruction taken from the trace (the latest instance before
 * the right-path frontier; optionally the first future one), so a wrong-path
 * instruction carries a plausible encoding, branch outcome and memory address.
 *
 * The query is a pure function of the request, which keeps it safe for the
 * activity-gated combinational evaluation of GSIM external modules.
 */

#define TraceWPMaxSlots 32

// meta word layout of one placed instruction
#define TRACE_WP_META_MEM_TYPE_SHIFT   0
#define TRACE_WP_META_MEM_SIZE_SHIFT   4
#define TRACE_WP_META_BR_TYPE_SHIFT    8
#define TRACE_WP_META_BR_TAKEN_SHIFT   16
#define TRACE_WP_META_EXCEPTION_SHIFT  24
#define TRACE_WP_META_END_OFFSET_SHIFT 32
#define TRACE_WP_META_SELECT_BLOCK_BIT 40
#define TRACE_WP_META_CROSS_BLOCK_BIT  41
#define TRACE_WP_META_VALID_BIT        42

// cfg word
#define TRACE_WP_CFG_IFU_CHECK_BIT     0

// trace_wp_query field selectors (SV path)
enum TraceWPField {
  TRACE_WP_FIELD_PC_VA = 0,
  TRACE_WP_FIELD_PC_PA,
  TRACE_WP_FIELD_MEM_VA,
  TRACE_WP_FIELD_MEM_PA,
  TRACE_WP_FIELD_TARGET,
  TRACE_WP_FIELD_INST,
  TRACE_WP_FIELD_META,
  TRACE_WP_FIELD_COUNT,
  TRACE_WP_FIELD_RANGE,
  TRACE_WP_FIELD_CFG,
};

struct TraceWPRequest {
  uint64_t b0_start;
  uint64_t b1_start;
  uint64_t anchor_id;
  uint8_t  b0_valid;
  uint8_t  b0_size;
  uint8_t  b0_taken_valid;
  uint8_t  b1_valid;
  uint8_t  b1_size;
  uint8_t  vaddr_bits;

  bool operator==(const TraceWPRequest &o) const {
    return b0_start == o.b0_start && b1_start == o.b1_start && anchor_id == o.anchor_id &&
      b0_valid == o.b0_valid && b0_size == o.b0_size && b0_taken_valid == o.b0_taken_valid &&
      b1_valid == o.b1_valid && b1_size == o.b1_size && vaddr_bits == o.vaddr_bits;
  }
};

struct TraceWPSlot {
  uint64_t pc_va;
  uint64_t pc_pa;
  uint64_t mem_va;
  uint64_t mem_pa;
  uint64_t target;
  uint32_t inst;
  uint64_t meta;
};

struct TraceWPPacket {
  uint8_t  count;
  uint32_t range; // halfword occupancy over the two-fetch logical window
  TraceWPSlot slots[TraceWPMaxSlots];
};

struct TraceWPStats {
  uint64_t queries = 0;        // distinct consecutive requests
  uint64_t empty_packets = 0;
  uint64_t placed = 0;
  uint64_t placed_past = 0;
  uint64_t placed_future = 0;
  uint64_t cross_block = 0;
  uint64_t skip_rvi_tail = 0;  // block started at the second halfword of an RVI
  uint64_t stop_unknown = 0;   // pc never executed in the trace
  uint64_t stop_future = 0;    // only future instances and TRACERTL_WP_PAST_ONLY=1
  uint64_t stop_exception = 0; // selected instance raises an exception
  uint64_t drop_incomplete = 0;
};

class TraceWrongPathOracle {
public:
  explicit TraceWrongPathOracle(const std::vector<Instruction> &list, uint8_t vaddr_bits);
  const TraceWPPacket &query(const TraceWPRequest &req);
  uint8_t cfg() const { return cfgWord; }
  void dumpStats() const;

private:
  const std::vector<Instruction> &list;
  uint64_t vmask;
  bool pastOnly;
  bool disabled;
  uint8_t cfgWord;

  // static instruction table in CSR form: pc -> sid -> instance indices
  std::unordered_map<uint64_t, uint32_t> pcToSid;
  std::vector<uint64_t> sidBegin; // size nSid + 1
  std::vector<uint32_t> instIdx;  // indices into list, ascending per sid
  std::vector<uint8_t> sidIsRVI;

  bool hasLast = false;
  TraceWPRequest lastReq;
  TraceWPPacket lastPkt;
  TraceWPStats stats;

  void build();
  bool known(uint64_t pc, bool *isRVI = nullptr) const;
  const Instruction *lookup(uint64_t pc, uint64_t anchor, bool &future, bool &stopFuture) const;
  void place(const TraceWPRequest &req, TraceWPPacket &pkt);
};

const TraceWPPacket &trace_wp_packet(const TraceWPRequest &req);
uint8_t trace_wp_cfg(uint8_t vaddr_bits);

extern "C" uint64_t trace_wp_query(
  uint8_t b0_valid, uint64_t b0_start, uint8_t b0_size, uint8_t b0_taken_valid,
  uint8_t b1_valid, uint64_t b1_start, uint8_t b1_size,
  uint64_t anchor_id, uint8_t vaddr_bits, uint8_t slot, uint8_t field);

#endif
