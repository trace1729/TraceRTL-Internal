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

#include "device.h"
#ifdef SHOW_SCREEN
#include <SDL2/SDL.h>
#endif

void init_sdl(void);

void init_uart(void);
void finish_uart(void);
extern "C" void init_sd(void);
extern "C" void finish_sd(void);

void init_device(void) {
#ifdef SHOW_SCREEN
  init_sdl();
#endif
  init_uart();
  init_sd();
}

void finish_device(void) {
#ifdef SHOW_SCREEN
  finish_sdl();
#endif
  finish_uart();
  finish_sd();
}

void poll_event() {
#ifdef SHOW_SCREEN
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        break; //set_abort();
      default: break;
    }
  }
#endif
}
