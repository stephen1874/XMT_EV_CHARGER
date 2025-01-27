#ifndef __HW_H__
#define __HW_H__

#include <Arduino.h>
#include "dfs.h"

void pinInit();
void buzOn();
void relayOff();
void relayOn();
void makePwm(int __current);

// 控制LED
void setRgb(int r, int g, int b);
void setRgb(int i, int r, int g, int b);

void setLedMove(int __m);     // 0-9

void ledOff();
void ledIdle();
void ledInsert();
void ledWorking();
void ledError();
void ledErrorBlink();
void tyWifiLogoBlink();
int wallTempInsert();

int isStop();               // 是否按下急停按钮，1是，0否

int checkLb();
void checkGND();
int checkSensor(int t);

int curProcess(int v);
int volProcess(int v);

void float2char(float a, unsigned char *str);
float char2float(unsigned char *str);

void saveFloat2EEPROM(int addr, float v_jz);
float loadFloatFromEEPROM(int addr);

int getTemp();
int getTempWall();
void feedog();

int getBtnA();              // 按下设置电流按键
int getBtnC();              // 按下延时按键

int checkDiode();           // 0-无二极管，1-有二极管
void relayErrorCheck();          // 继电器黏连检测，0-无黏连，1-黏连

void rcdSelfCheck();        // 漏保自检
extern void updateSleepTimer();

void autoJzA();

void delayFeed(unsigned long ms);

void eeprom_init();
void eeprom_write(int addr, unsigned char dta);
unsigned char eeprom_read(int addr);

int isWaitSchedule();           // 是否可以开始充电，如果是否处于计划的时间里头
void rcdZero();

extern void delayCountDown();

int getTravelCurrent();
int L2Check();

#if GND_DETECT_ADC
int getGndAdc();
#endif

#endif