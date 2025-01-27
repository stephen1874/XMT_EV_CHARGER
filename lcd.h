/***************************************************
  This is a library for the ST7789 IPS SPI display.

  Originally written by Limor Fried/Ladyada for
  Adafruit Industries.

  Modified by Ananev Ilia
  Modified by Kamran Gasimov
 ****************************************************/

#ifndef _ADAFRUIT_ST7789H_
#define _ADAFRUIT_ST7789H_

#include "Arduino.h"
#include "Print.h"
#include "dfs.h"

// #define SPI_FREQ 16000000

#define ST7789_TFTWIDTH   240   
#define ST7789_TFTHEIGHT  320

#define ST7789_240x240_XSTART 0
#define ST7789_240x240_YSTART 0

#define ST_CMD_DELAY   0x80

#define ST7789_NOP     0x00
#define ST7789_SWRESET 0x01

#define ST7789_SLPIN   0x10  // sleep on
#define ST7789_SLPOUT  0x11  // sleep off
#define ST7789_PTLON   0x12  // partial on
#define ST7789_NORON   0x13  // partial off
#define ST7789_INVOFF  0x20  // invert off
#define ST7789_INVON   0x21  // invert on
#define ST7789_DISPOFF 0x28  // display off
#define ST7789_DISPON  0x29  // display on
#define ST7789_IDMOFF  0x38  // idle off
#define ST7789_IDMON   0x39  // idle on

#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_RAMRD   0x2E

#define ST7789_COLMOD  0x3A
#define ST7789_MADCTL  0x36

#define ST7789_PTLAR    0x30   // partial start/end
#define ST7789_VSCRDEF  0x33   // SETSCROLLAREA
#define ST7789_VSCRSADD 0x37

#define ST7789_WRDISBV  0x51
#define ST7789_WRCTRLD  0x53
#define ST7789_WRCACE   0x55
#define ST7789_WRCABCMB 0x5e

#define ST7789_POWSAVE    0xbc
#define ST7789_DLPOFFSAVE 0xbd

// bits in MADCTL
#define ST7789_MADCTL_MY  0x80
#define ST7789_MADCTL_MX  0x40
#define ST7789_MADCTL_MV  0x20
#define ST7789_MADCTL_ML  0x10
#define ST7789_MADCTL_RGB 0x00


#define RGBto565(r,g,b) ((((255-r) & 0xF8) << 8) | (((255-g) & 0xFC) << 3) | ((255-b) >> 3))

// Color definitions
#define BLACK   0xFFFF
#define WHITE   0x0000
#define YELLOW  0xA7E           // RGBto565(242, 176, 8)
#define GREEN   0XF81F          // RGBto565(0, 255, 0)
#define RED     0X7FF           // RGBto565(255, 0, 0)
#define BLUE1   0XD449          // RGBto565(46, 117, 181)
#define GRAY    0X632C          // RGBto565(153, 153, 153)
#define COLOR_S 0x0000          // COLOR_S，白色
#define GRAY2   RGBto565(80, 80, 80)

extern int _xstart;
extern int _ystart;
extern int _colstart;
extern int _rowstart;
extern int _height;
extern int _width;

void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color);
void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void setRotation(uint8_t r);
void invertDisplay(boolean i);
void lcd_init(uint16_t width, uint16_t height);
void lcd_init2(uint16_t width, uint16_t height);
void dispOn();
void dispOff();

void LCD_DrawLine(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t color);

void displayInit(const uint8_t *addr);
//void     spiwrite(uint8_t),
void writecommand(uint8_t c);
void writedata(uint8_t d);

void commonInit(const uint8_t *cmdList);
void LCD_WR_DATA(uint16_t dat);

void lcdOn();
void lcdOff();

void kwPointOn();
void kwPointOff();

#endif
