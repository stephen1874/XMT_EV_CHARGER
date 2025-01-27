#ifndef __DISP_H__
#define __DISP_H__

#include <Arduino.h>
#include "dfs.h"
#include "ui_dfs.h"

void lcdStart();

void drawSmallNum(int x, int y, int num, unsigned int color, unsigned int color_bk);
void drawBigNum(int x, int y, int num, unsigned int color, unsigned int color_bk);
void drawMidNum(int x, int y, int num, unsigned int color, unsigned int color_bk);

void showCurrent(float __a);
void showVoltage(float __v);
void showKwh(float __kwh);
void showKw(float __kw);
void showAset(int __aset);
void showAsetOff();
void showDelay(int __h, int __m, int __s);
void showDelayOff();
void showTime(int __h, int __m);
void showDelay(unsigned long t);
void showTime(unsigned long t);
void showTempError(int temp);
void showVersion();

void showV3();
void showV1();
void showCarOn();       // 车标绿色
void showCarOff();      // 车标灰色
void showGndOk();
void showGndFail();

void showWifiOk(int n);
void showWifiFail();
void showWifiConnecting(int st);

void clearShowBuf();        // 清除缓存

void fillRec(int x, int y, int len, int wid, unsigned int color);
void showImage();
void showImage(int x, int y, int len, int wid, unsigned int color, unsigned int color_bk, const unsigned char *img, int img_len);
void showImage(int x, int y, int len, int wid, unsigned int color, unsigned int color_bk, const unsigned char *img);
void showImage2(int x, int y, int len, int wid, unsigned int color, unsigned int color_bk, const unsigned char *img, int page);

void showErrorImg(int error_code);
void showNormal(int mode);

void showBattery(int val);

void taskDisplay();
void taskDisplay(int t);

void showTemp(int temp);

void clearDisp();
void dispBuf(int y, int width, int color, int color_bk, int page);
void pushBuf(int x, int y, int len, int wid, const unsigned char *dta, int img_len);
void pushBuf(int x, int y, int len, int wid, const unsigned char *dta);

void decompressMonochromePixels(const unsigned char *compressed, int length, unsigned char *decompressed, int *decompressedLength);

extern int flg_wifi_show;
extern int buf_error_code;

#endif