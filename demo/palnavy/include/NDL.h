//
// Created by zhangjiantao on 2026/6/4.
//

// include/NDL.h
#ifndef BAREMETAL_NDL_H
#define BAREMETAL_NDL_H

#include <stdint.h>

#define NDL_MODE_DEFAULT 0

#ifdef __cplusplus
extern "C" {
#endif

  int  NDL_Init(uint32_t flags);
  void NDL_Quit(void);
  void NDL_OpenCanvas(int *w, int *h);
  void NDL_DrawRect(void *pixels, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif

#endif // BAREMETAL_NDL_H
