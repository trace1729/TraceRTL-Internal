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

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "trace_wrong_path.h"
#include "trace_reader.h"

extern TraceReader *trace_reader;

static bool envFlag(const char *name, bool dflt) {
  const char *v = getenv(name);
  if (v == nullptr || *v == '\0') {
    return dflt;
  }
  return strcmp(v, "0") != 0;
}

static inline bool isRVIInst(uint32_t instr) { return (instr & 0x3) == 0x3; }

TraceWrongPathOracle::TraceWrongPathOracle(const std::vector<Instruction> &list, uint8_t vaddr_bits)
  : list(list) {
  vmask = vaddr_bits >= 64 ? ~0ULL : ((1ULL << vaddr_bits) - 1);
  pastOnly = envFlag("TRACERTL_WP_PAST_ONLY", false);
  disabled = envFlag("TRACERTL_WP_DISABLE", false);
  cfgWord = envFlag("TRACERTL_WP_IFU_CHECK", true) ? (1 << TRACE_WP_CFG_IFU_CHECK_BIT) : 0;
  memset(&lastReq, 0, sizeof(lastReq));
  memset(&lastPkt, 0, sizeof(lastPkt));
  build();
  printf("[TraceRTL-WP] real wrong-path oracle: %zu static pcs, %zu instances, vaddr_bits %u, "
         "past_only %d, disable %d, ifu_check %d\n",
    pcToSid.size(), instIdx.size(), vaddr_bits, pastOnly, disabled, cfgWord & 1);
  fflush(stdout);
}

void TraceWrongPathOracle::build() {
  std::vector<uint32_t> count;
  uint64_t lastId = 0;
  for (size_t i = 0; i < list.size(); i++) {
    const Instruction &inst = list[i];
    if (inst.inst_id <= lastId) {
      printf("[TraceRTL-WP] inst_id is not monotonic at index %zu (%lu after %lu)\n", i, inst.inst_id, lastId);
      fflush(stdout);
      exit(1);
    }
    lastId = inst.inst_id;
    if (inst.is_squashed) {
      continue;
    }
    uint64_t pc = inst.instr_pc_va & vmask;
    auto it = pcToSid.find(pc);
    if (it == pcToSid.end()) {
      it = pcToSid.emplace(pc, (uint32_t)count.size()).first;
      count.push_back(0);
      sidIsRVI.push_back(isRVIInst(inst.instr));
    }
    count[it->second]++;
  }
  sidBegin.assign(count.size() + 1, 0);
  for (size_t s = 0; s < count.size(); s++) {
    sidBegin[s + 1] = sidBegin[s] + count[s];
  }
  instIdx.resize(sidBegin.back());
  std::vector<uint64_t> fill(sidBegin.begin(), sidBegin.end() - 1);
  for (size_t i = 0; i < list.size(); i++) {
    const Instruction &inst = list[i];
    if (inst.is_squashed) {
      continue;
    }
    uint32_t sid = pcToSid[inst.instr_pc_va & vmask];
    instIdx[fill[sid]++] = (uint32_t)i;
  }
}

bool TraceWrongPathOracle::known(uint64_t pc, bool *isRVI) const {
  auto it = pcToSid.find(pc & vmask);
  if (it == pcToSid.end()) {
    return false;
  }
  if (isRVI) {
    *isRVI = sidIsRVI[it->second];
  }
  return true;
}

// Latest instance older than the anchor; otherwise the oldest future one.
const Instruction *TraceWrongPathOracle::lookup(uint64_t pc, uint64_t anchor, bool &future, bool &stopFuture) const {
  future = false;
  stopFuture = false;
  auto it = pcToSid.find(pc & vmask);
  if (it == pcToSid.end()) {
    return nullptr;
  }
  uint64_t lo = sidBegin[it->second], hi = sidBegin[it->second + 1];
  // first position whose inst_id >= anchor
  uint64_t l = lo, r = hi;
  while (l < r) {
    uint64_t m = (l + r) / 2;
    if (list[instIdx[m]].inst_id < anchor) {
      l = m + 1;
    } else {
      r = m;
    }
  }
  if (l > lo) {
    return &list[instIdx[l - 1]];
  }
  if (pastOnly) {
    stopFuture = true;
    return nullptr;
  }
  future = true;
  return &list[instIdx[lo]];
}

void TraceWrongPathOracle::place(const TraceWPRequest &req, TraceWPPacket &pkt) {
  memset(&pkt, 0, sizeof(pkt));
  if (disabled || !req.b0_valid || req.b0_size == 0) {
    return;
  }

  const uint8_t sizes[2] = { req.b0_size, (uint8_t)(req.b1_valid ? req.b1_size : 0) };
  const uint64_t starts[2] = { req.b0_start & vmask, req.b1_start & vmask };
  const uint8_t base[2] = { 0, req.b0_size };
  const unsigned window = sizes[0] + sizes[1];

  auto setRange = [&](unsigned logical) {
    if (logical < 32 && logical < window) {
      pkt.range |= 1u << logical;
    }
  };
  auto emit = [&](const Instruction *inst, uint8_t endOffset, bool selectBlock, bool cross) {
    TraceWPSlot &s = pkt.slots[pkt.count++];
    s.pc_va = inst->instr_pc_va;
    s.pc_pa = inst->instr_pc_pa ? inst->instr_pc_pa : inst->instr_pc_va;
    s.mem_va = inst->exu_data.memory_address.va;
    s.mem_pa = (inst->memory_type != MEM_TYPE_None && inst->exu_data.memory_address.pa == 0) ?
      inst->exu_data.memory_address.va : inst->exu_data.memory_address.pa;
    s.target = inst->target;
    s.inst = inst->instr;
    s.meta = ((uint64_t)(inst->memory_type & 0xf) << TRACE_WP_META_MEM_TYPE_SHIFT) |
      ((uint64_t)(inst->memory_size & 0xf) << TRACE_WP_META_MEM_SIZE_SHIFT) |
      ((uint64_t)inst->branch_type << TRACE_WP_META_BR_TYPE_SHIFT) |
      ((uint64_t)inst->branch_taken << TRACE_WP_META_BR_TAKEN_SHIFT) |
      ((uint64_t)inst->exception << TRACE_WP_META_EXCEPTION_SHIFT) |
      ((uint64_t)(endOffset & 0x1f) << TRACE_WP_META_END_OFFSET_SHIFT) |
      ((uint64_t)selectBlock << TRACE_WP_META_SELECT_BLOCK_BIT) |
      ((uint64_t)cross << TRACE_WP_META_CROSS_BLOCK_BIT) |
      (1ULL << TRACE_WP_META_VALID_BIT);
  };

  unsigned blk = 0, local = 0;
  uint64_t pc = starts[0];
  bool blockHead = true;
  while (pkt.count < TraceWPMaxSlots) {
    if (local >= sizes[blk]) {
      if (blk == 0 && sizes[1] != 0) {
        blk = 1;
        local = 0;
        pc = starts[1];
        blockHead = true;
        continue;
      }
      break;
    }
    if (blockHead) {
      blockHead = false;
      bool prevRVI = false;
      if (!known(pc) && known(pc - 2, &prevRVI) && prevRVI) {
        // the block starts at the tail halfword of an RVI
        stats.skip_rvi_tail++;
        pc += 2;
        local += 1;
        continue;
      }
    }
    bool future = false, stopFuture = false;
    const Instruction *inst = lookup(pc, req.anchor_id, future, stopFuture);
    if (inst == nullptr) {
      if (stopFuture) stats.stop_future++;
      else stats.stop_unknown++;
      break;
    }
    if (inst->exception != 0) {
      stats.stop_exception++;
      break;
    }
    const unsigned len = isRVIInst(inst->instr) ? 2 : 1;
    if (local + len <= sizes[blk]) {
      emit(inst, (uint8_t)(local + len - 1), blk == 1, false);
      setRange(base[blk] + local);
      if (len == 2) setRange(base[blk] + local + 1);
      local += len;
      pc += 2 * len;
    } else if (blk == 0 && sizes[1] != 0 && !req.b0_taken_valid && starts[1] == ((pc + 2) & vmask)) {
      // An RVI spanning two sequential fetch blocks belongs to block 1 and
      // ends at its halfword 0, as in TraceAlignParallel.
      emit(inst, 0, true, true);
      setRange(base[0] + local);
      setRange(base[1]);
      stats.cross_block++;
      blk = 1;
      local = 1;
      pc += 4;
      blockHead = false;
    } else {
      stats.drop_incomplete++;
      break;
    }
    if (future) stats.placed_future++;
    else stats.placed_past++;
  }
  stats.placed += pkt.count;
  if (pkt.count == 0) {
    stats.empty_packets++;
  }
}

const TraceWPPacket &TraceWrongPathOracle::query(const TraceWPRequest &req) {
  if (!hasLast || !(req == lastReq)) {
    stats.queries++;
    place(req, lastPkt);
    lastReq = req;
    hasLast = true;
  }
  return lastPkt;
}

void TraceWrongPathOracle::dumpStats() const {
  printf("[TraceRTL-WP] stats: queries %lu empty %lu placed %lu (past %lu future %lu) cross_block %lu "
         "skip_rvi_tail %lu stop_unknown %lu stop_future %lu stop_exception %lu drop_incomplete %lu\n",
    stats.queries, stats.empty_packets, stats.placed, stats.placed_past, stats.placed_future,
    stats.cross_block, stats.skip_rvi_tail, stats.stop_unknown, stats.stop_future,
    stats.stop_exception, stats.drop_incomplete);
  fflush(stdout);
}

static TraceWrongPathOracle *wp_oracle = nullptr;

static void trace_wp_dump_at_exit() {
  if (wp_oracle) {
    wp_oracle->dumpStats();
  }
}

static TraceWrongPathOracle *get_oracle(uint8_t vaddr_bits) {
  if (wp_oracle == nullptr) {
    if (trace_reader == nullptr) {
      return nullptr;
    }
    wp_oracle = new TraceWrongPathOracle(trace_reader->getInstList(), vaddr_bits);
    atexit(trace_wp_dump_at_exit);
  }
  return wp_oracle;
}

const TraceWPPacket &trace_wp_packet(const TraceWPRequest &req) {
  static TraceWPPacket empty = {};
  TraceWrongPathOracle *o = get_oracle(req.vaddr_bits);
  return o ? o->query(req) : empty;
}

uint8_t trace_wp_cfg(uint8_t vaddr_bits) {
  TraceWrongPathOracle *o = get_oracle(vaddr_bits);
  return o ? o->cfg() : 0;
}

extern "C" uint64_t trace_wp_query(
  uint8_t b0_valid, uint64_t b0_start, uint8_t b0_size, uint8_t b0_taken_valid,
  uint8_t b1_valid, uint64_t b1_start, uint8_t b1_size,
  uint64_t anchor_id, uint8_t vaddr_bits, uint8_t slot, uint8_t field) {
  TraceWPRequest req = { b0_start, b1_start, anchor_id, b0_valid, b0_size, b0_taken_valid,
    b1_valid, b1_size, vaddr_bits };
  const TraceWPPacket &pkt = trace_wp_packet(req);
  if (field == TRACE_WP_FIELD_COUNT) return pkt.count;
  if (field == TRACE_WP_FIELD_RANGE) return pkt.range;
  if (field == TRACE_WP_FIELD_CFG) return trace_wp_cfg(vaddr_bits);
  if (slot >= pkt.count) return 0;
  const TraceWPSlot &s = pkt.slots[slot];
  switch (field) {
    case TRACE_WP_FIELD_PC_VA:  return s.pc_va;
    case TRACE_WP_FIELD_PC_PA:  return s.pc_pa;
    case TRACE_WP_FIELD_MEM_VA: return s.mem_va;
    case TRACE_WP_FIELD_MEM_PA: return s.mem_pa;
    case TRACE_WP_FIELD_TARGET: return s.target;
    case TRACE_WP_FIELD_INST:   return s.inst;
    case TRACE_WP_FIELD_META:   return s.meta;
    default: return 0;
  }
}
