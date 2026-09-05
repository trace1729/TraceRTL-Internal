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

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>
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
class TraceRandomExtentPAddrAllocator {
private:
  struct VirtualExtent {
    uint64_t firstVpn;
    uint64_t lastVpn;

    uint64_t pageCount() const {
      return lastVpn - firstVpn + 1;
    }
  };

  struct PhysicalExtent {
    uint64_t firstPpn;
    uint64_t pageCount;
  };

  static_assert((baseAddr & TRACE_PAGE_OFFSET_MASK) == 0,
    "synthetic PA base must be page aligned");
  static_assert((regionSize & TRACE_PAGE_OFFSET_MASK) == 0,
    "synthetic PA region must contain whole pages");
  static_assert(regionSize >= TRACE_PAGE_SIZE,
    "synthetic PA region must contain at least one page");

  static constexpr uint64_t basePpn = baseAddr >> TRACE_PAGE_SHIFT;
  static constexpr uint64_t pageCount = regionSize >> TRACE_PAGE_SHIFT;

  std::set<uint64_t> observedVpns;
  std::map<uint64_t, uint64_t> v2pMap;
  std::unordered_set<uint64_t> unavailablePpns;
  uint64_t placementSeed = seed;
  uint64_t allocationIndex = 0;
  uint64_t extentCount = 0;
  uint64_t maxExtentPages = 0;
  bool finalized = false;

  static uint64_t mix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }

  static uint64_t mergePageAndOff(uint64_t ppn, uint64_t fullAddr) {
    return (ppn << TRACE_PAGE_SHIFT) | (fullAddr & TRACE_PAGE_OFFSET_MASK);
  }

  void mapExtentAt(uint64_t firstVpn, uint64_t lastVpn, uint64_t firstPpn) {
    const uint64_t extentPages = lastVpn - firstVpn + 1;
    for (uint64_t offset = 0; offset < extentPages; ++offset) {
      v2pMap.emplace(firstVpn + offset, firstPpn + offset);
      unavailablePpns.insert(firstPpn + offset);
    }
    ++extentCount;
    if (extentPages > maxExtentPages) {
      maxExtentPages = extentPages;
    }
  }

  std::vector<VirtualExtent> collectExtents() const {
    std::vector<VirtualExtent> extents;
    if (observedVpns.empty()) {
      return extents;
    }

    auto vpn = observedVpns.begin();
    uint64_t firstVpn = *vpn;
    uint64_t lastVpn = *vpn;
    for (++vpn; vpn != observedVpns.end(); ++vpn) {
      if (lastVpn == UINT64_MAX || *vpn != lastVpn + 1) {
        extents.push_back({firstVpn, lastVpn});
        firstVpn = *vpn;
      }
      lastVpn = *vpn;
    }
    extents.push_back({firstVpn, lastVpn});
    return extents;
  }

  void mapUnreservedExtents(std::vector<VirtualExtent> extents) {
    uint64_t totalPages = 0;
    for (const auto &extent : extents) {
      if (extent.pageCount() > pageCount - totalPages) {
        throw std::runtime_error(
          "TraceRandomExtentPAddrAllocator: synthetic PA region exhausted");
      }
      totalPages += extent.pageCount();
    }

    // Shuffle the virtual extents, then distribute the unused pages as random
    // gaps. Planning all placements together preserves randomization without
    // letting early allocations fragment the region needed by a later extent.
    for (size_t i = extents.size(); i > 1; --i) {
      const size_t j = mix64(placementSeed + allocationIndex++) % i;
      std::swap(extents[i - 1], extents[j]);
    }

    uint64_t remainingGapPages = pageCount - totalPages;
    uint64_t nextPpn = basePpn;
    for (const auto &extent : extents) {
      const uint64_t gapPages = remainingGapPages == 0 ? 0 :
        mix64(placementSeed + allocationIndex++) % (remainingGapPages + 1);
      nextPpn += gapPages;
      remainingGapPages -= gapPages;
      mapExtentAt(extent.firstVpn, extent.lastVpn, nextPpn);
      nextPpn += extent.pageCount();
    }
  }

  void mapReservedExtents(std::vector<VirtualExtent> extents) {
    std::sort(extents.begin(), extents.end(),
      [](const VirtualExtent &lhs, const VirtualExtent &rhs) {
        if (lhs.pageCount() != rhs.pageCount()) {
          return lhs.pageCount() > rhs.pageCount();
        }
        return lhs.firstVpn < rhs.firstVpn;
      });

    std::vector<uint64_t> reservedPpns(unavailablePpns.begin(), unavailablePpns.end());
    std::sort(reservedPpns.begin(), reservedPpns.end());
    std::vector<PhysicalExtent> freeExtents;
    uint64_t freeStart = basePpn;
    for (const uint64_t reservedPpn : reservedPpns) {
      if (reservedPpn > freeStart) {
        freeExtents.push_back({freeStart, reservedPpn - freeStart});
      }
      freeStart = reservedPpn + 1;
    }
    const uint64_t endPpn = basePpn + pageCount;
    if (freeStart < endPpn) {
      freeExtents.push_back({freeStart, endPpn - freeStart});
    }

    for (const auto &extent : extents) {
      size_t best = freeExtents.size();
      for (size_t i = 0; i < freeExtents.size(); ++i) {
        if (freeExtents[i].pageCount >= extent.pageCount() &&
            (best == freeExtents.size() ||
             freeExtents[i].pageCount < freeExtents[best].pageCount)) {
          best = i;
        }
      }
      if (best == freeExtents.size()) {
        throw std::runtime_error(
          "TraceRandomExtentPAddrAllocator: no contiguous physical extent available");
      }

      auto &freeExtent = freeExtents[best];
      const bool placeAtHighEnd = (mix64(placementSeed + allocationIndex++) & 1) != 0;
      const uint64_t firstPpn = placeAtHighEnd ?
        freeExtent.firstPpn + freeExtent.pageCount - extent.pageCount() :
        freeExtent.firstPpn;
      mapExtentAt(extent.firstVpn, extent.lastVpn, firstPpn);
      if (!placeAtHighEnd) {
        freeExtent.firstPpn += extent.pageCount();
      }
      freeExtent.pageCount -= extent.pageCount();
    }
  }

public:
  void setSeed(uint64_t newSeed) {
    if (finalized || !observedVpns.empty() || allocationIndex != 0) {
      throw std::logic_error(
        "TraceRandomExtentPAddrAllocator: cannot change seed after use");
    }
    placementSeed = newSeed;
  }

  uint64_t getSeed() const {
    return placementSeed;
  }

  void reserve(uint64_t paddr) {
    if (finalized) {
      throw std::logic_error(
        "TraceRandomExtentPAddrAllocator: cannot reserve after finalize");
    }
    const uint64_t ppn = paddr >> TRACE_PAGE_SHIFT;
    if (ppn >= basePpn && ppn < basePpn + pageCount) {
      unavailablePpns.insert(ppn);
    }
  }

  void observe(uint64_t vaddr) {
    if (finalized) {
      throw std::logic_error(
        "TraceRandomExtentPAddrAllocator: cannot observe after finalize");
    }
    observedVpns.insert(vaddr >> TRACE_PAGE_SHIFT);
  }

  void finalize() {
    if (finalized) {
      return;
    }
    auto extents = collectExtents();
    if (unavailablePpns.empty()) {
      mapUnreservedExtents(std::move(extents));
    } else {
      mapReservedExtents(std::move(extents));
    }
    finalized = true;
  }

  uint64_t va2pa(uint64_t vaddr) {
    const uint64_t vpn = vaddr >> TRACE_PAGE_SHIFT;
    const auto mapping = v2pMap.find(vpn);
    if (!finalized || mapping == v2pMap.end()) {
      throw std::logic_error(
        "TraceRandomExtentPAddrAllocator: unprepared virtual address");
    }
    return mergePageAndOff(mapping->second, vaddr);
  }

  uint64_t mappedPageCount() const {
    return v2pMap.size();
  }

  uint64_t mappedExtentCount() const {
    return extentCount;
  }

  uint64_t largestExtentPages() const {
    return maxExtentPages;
  }

  void dump() const {
    printf("Trace Random Extent PAddr Allocator V2P Map:\n");
    for (const auto &mapping : v2pMap) {
      printf("  vpn %016lx -> ppn %016lx\n", mapping.first, mapping.second);
    }
  }
};

#endif
