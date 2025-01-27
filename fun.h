#ifndef __FUN_H__
#define __FUN_H__
#include "dfs.h"

extern int flg_pov;     // 过压保护标志位
extern int flg_lov;     // 欠压保护标志位
extern int flg_pot;     // 过温保护标志位
extern int flg_pot_wall;    // 墙插过温保护标志位
extern int flg_gnd_p;   // 接地保护标志位
extern int flg_oc;      // 过流保护标志位
extern int cnt_oc;
extern int flg_nodiode; // 二极管缺失标志位

void sort(int *array, int n);
int getMiddle(int *str, int len);
int checkInsert();

void protectOverVolIdle();
void protectLowVolIdle();
void protectGndIdle();
void protectTempIdle();

void protectOverVol();
void protectLowVol();
void protectGnd();
void protectOverCur();
void protectRCD();
void protectTemp();
void protectTempWall();
void stopMode();


int changeStatus(int st);
void updateTime();
#endif