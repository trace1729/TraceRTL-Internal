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

#include "common.h"
#include "ram.h"

typedef union PageTableEntry {
  struct {
    uint32_t v : 1;
    uint32_t r : 1;
    uint32_t w : 1;
    uint32_t x : 1;
    uint32_t u : 1;
    uint32_t g : 1;
    uint32_t a : 1;
    uint32_t d : 1;
    uint32_t rsw : 2;
    uint64_t ppn : 44;
    uint32_t rsvd : 7;
    uint32_t pbmt : 2;
    uint32_t n : 1;
  };
  uint64_t val;
} PTE;

#define VPNi(vpn, i) (((vpn) >> (9 * (i))) & 0x1ff)
#define GET_LOWER32(data) ((data) & ((1UL << 32) - 1))
#define GET_UPPER32(data) ((data) >> 32)

enum {
  M_XA_SWAP = 4,
  M_XLR = 6,
  M_XSC = 7,
  M_XA_ADD = 8,
  M_XA_XOR = 9,
  M_XA_OR = 10,
  M_XA_AND = 11,
  M_XA_MIN = 12,
  M_XA_MAX = 13,
  M_XA_MINU = 14,
  M_XA_MAXU = 15
};

static uint64_t default_flash_words[] = {
  0x01f292930010029b,
  0x00028067,
};

extern "C" uint8_t pte_helper(uint64_t satp, uint64_t vpn, uint64_t *pte, uint8_t *level) {
  uint64_t pg_base = satp << 12, pte_addr;
  PTE *pte_p = (PTE *)pte;
  for (*level = 2; *level >= 0; (*level)--) {
    pte_addr = pg_base + VPNi(vpn, *level) * sizeof(uint64_t);
    pte_p->val = pmem_read(pte_addr);
    pg_base = pte_p->ppn << 12;
    if (!pte_p->v) {
      return 1;
    }
    if (pte_p->r || pte_p->x) {
      return 0;
    }
  }
  return 1;
}

extern "C" uint64_t amo_helper(uint8_t cmd, uint64_t addr, uint64_t wdata, uint8_t mask) {
  if (addr % 8 == 4 && mask == 0xf0) {
    addr -= 4;
  } else if (addr % 8 != 0) {
    printf("warning: amo address %lx not naturally aligned to %x!!\n", addr, mask);
  }
  if (mask != 0xff && mask != 0xf && mask != 0xf0) {
    printf("warning: amo data mask %x not aligned to 32-bit\n", mask);
  }
  static uint64_t lr_addr = 0, lr_valid = 0;
  uint64_t rdata = pmem_read(addr);
  uint64_t upper_r = GET_UPPER32(rdata), upper_w = GET_UPPER32(wdata);
  uint64_t lower_r = GET_LOWER32(rdata), lower_w = GET_LOWER32(wdata);
  uint64_t rop = (mask == 0xff) ? rdata : (mask == 0xf) ? lower_r : upper_r;
  uint64_t wop = (mask == 0xff) ? wdata : (mask == 0xf) ? lower_w : upper_w;
  uint64_t result = 0;
  switch (cmd) {
    case M_XA_SWAP: result = wop; break;
    case M_XLR:
      result = rdata;
      lr_valid = 1;
      lr_addr = addr;
      break;
    case M_XSC:
      rop = !(lr_valid && lr_addr == addr);
      lr_valid = 0;
      result = wop;
      break;
    case M_XA_ADD: result = wop + rop; break;
    case M_XA_XOR: result = wop ^ rop; break;
    case M_XA_OR: result = wop | rop; break;
    case M_XA_AND: result = wop & rop; break;
    case M_XA_MIN:
      if (mask == 0xff)
        result = ((int64_t)wop > (int64_t)rop) ? rop : wop;
      else
        result = ((int32_t)((uint32_t)wop) > (int32_t)((uint32_t)rop)) ? rop : wop;
      break;
    case M_XA_MAX:
      if (mask == 0xff)
        result = ((int64_t)wop > (int64_t)rop) ? wop : rop;
      else
        result = ((int32_t)((uint32_t)wop) > (int32_t)((uint32_t)rop)) ? wop : rop;
      break;
    case M_XA_MINU: result = (wop > rdata) ? rop : wop; break;
    case M_XA_MAXU: result = (wop > rdata) ? wop : rop; break;
    default: printf("unknown atomic op %d!!\n", cmd); break;
  }
  if (mask == 0xf) {
    result = (upper_r << 32) | (result & 0xffffffffUL);
  } else if (mask == 0xf0) {
    result = (result << 32) | lower_r;
  }
  pmem_write(addr, result);
  return (mask == 0xf0) ? rop << 32 : rop;
}

extern "C" void flash_read(uint32_t addr, uint64_t *data) {
  uint32_t aligned_addr = addr & FLASH_ALIGH_MASK;
  uint64_t index = aligned_addr / sizeof(uint64_t);
  if (index < sizeof(default_flash_words) / sizeof(default_flash_words[0])) {
    *data = default_flash_words[index];
  } else {
    *data = 0;
  }
}