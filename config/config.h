/***************************************************************************************
* Copyright (c) 2020-2024 Institute of Computing Technology, Chinese Academy of Sciences
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

#ifndef __CONFIG_H
#define __CONFIG_H

#define DEFAULT_EMU_RAM_SIZE (8 * 1024 * 1024 * 1024UL) // 8 GB
#define PMEM_BASE 0x80000000UL
#define FLASH_ALIGH_MASK 0xfffffff8

// #define SHOW_SCREEN

#define SLOT_SIZE 2
#define FAIT_EXIT    exit(EXIT_FAILURE);
#define WAIT_INTERVAL 5

#endif
