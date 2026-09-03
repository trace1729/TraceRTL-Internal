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

#ifndef __TRACE_PADDR_ALLOCATOR_H__
#define __TRACE_PADDR_ALLOCATOR_H__

#include <cstdint>
#include <map>
#include <stdexcept>
#include <unordered_set>
#include "trace_common.h"

template<uint64_t baseAddr>
class TracePAddrAllocator {

private:
  uint64_t curAddr = baseAddr;
  std::map<uint64_t, uint64_t> v2pMap;

  inline uint64_t mergePageAndOff(uint64_t pn, uint64_t fullAddr) {
    return (pn << TRACE_PAGE_SHIFT) | (fullAddr & TRACE_PAGE_OFFSET_MASK);
  }

  uint64_t pop() {
    uint64_t last = curAddr;
    curAddr += TRACE_PAGE_SIZE;
    return last;
  };

public:
  uint64_t va2pa(uint64_t va) {
    uint64_t vpn = va >> TRACE_PAGE_SHIFT;
    uint64_t ppn;
    if (v2pMap.find(vpn) != v2pMap.end()) {
      ppn = v2pMap[vpn];
    } else {
      ppn = pop() >> TRACE_PAGE_SHIFT;
      v2pMap[vpn] = ppn;
    }
    return mergePageAndOff(ppn, va);
  }

  void dump() {
    printf("Trace PAddr Allocator V2P Map:\n");
    for (auto pair : v2pMap) {
      printf("  vpn %016lx -> ppn %016lx\n", pair.first, pair.second);
    }
  }
};

template<uint64_t baseAddr, uint64_t regionSize, uint64_t seed>
class TraceRandomPAddrAllocator {
private:
  static_assert((baseAddr & TRACE_PAGE_OFFSET_MASK) == 0,
    "synthetic PA base must be page aligned");
  static_assert((regionSize & TRACE_PAGE_OFFSET_MASK) == 0,
    "synthetic PA region must contain whole pages");
  static_assert(regionSize >= TRACE_PAGE_SIZE,
    "synthetic PA region must contain at least one page");

  static constexpr uint64_t basePpn = baseAddr >> TRACE_PAGE_SHIFT;
  static constexpr uint64_t pageCount = regionSize >> TRACE_PAGE_SHIFT;

  std::map<uint64_t, uint64_t> v2pMap;
  std::unordered_set<uint64_t> unavailablePpns;
  uint64_t allocationIndex = 0;

  static uint64_t mix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }

  static uint64_t mergePageAndOff(uint64_t ppn, uint64_t fullAddr) {
    return (ppn << TRACE_PAGE_SHIFT) | (fullAddr & TRACE_PAGE_OFFSET_MASK);
  }

  uint64_t allocatePpn() {
    if (unavailablePpns.size() >= pageCount) {
      throw std::runtime_error("TraceRandomPAddrAllocator: synthetic PA region exhausted");
    }

    uint64_t offset = mix64(seed + allocationIndex++) % pageCount;
    uint64_t ppn = basePpn + offset;
    while (!unavailablePpns.insert(ppn).second) {
      offset = (offset + 1) % pageCount;
      ppn = basePpn + offset;
    }
    return ppn;
  }

public:
  void reserve(uint64_t paddr) {
    const uint64_t ppn = paddr >> TRACE_PAGE_SHIFT;
    if (ppn >= basePpn && ppn < basePpn + pageCount) {
      unavailablePpns.insert(ppn);
    }
  }

  uint64_t va2pa(uint64_t vaddr) {
    const uint64_t vpn = vaddr >> TRACE_PAGE_SHIFT;
    auto mapping = v2pMap.find(vpn);
    if (mapping == v2pMap.end()) {
      mapping = v2pMap.emplace(vpn, allocatePpn()).first;
    }
    return mergePageAndOff(mapping->second, vaddr);
  }

  void dump() const {
    printf("Trace Random PAddr Allocator V2P Map:\n");
    for (const auto &mapping : v2pMap) {
      printf("  vpn %016lx -> ppn %016lx\n", mapping.first, mapping.second);
    }
  }
};

#endif
