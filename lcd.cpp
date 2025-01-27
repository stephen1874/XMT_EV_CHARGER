/***************************************************
  This is a library for the ST7789 IPS SPI display.

  Originally written by Limor Fried/Ladyada for
  Adafruit Industries.

  Modified by Ananev Ilia
  Modified by Kamran Gasimov
 ****************************************************/

#include "lcd.h"
#include <limits.h>
#include "pins_arduino.h"
#include "wiring_private.h"

int _xstart = 0;
int _ystart = 0;
int _colstart = 0;
int _rowstart = 0;
int _height = 0;
int _width = 0;

// 写SPI
int __lastsda = -1;
void spiwrite(uint8_t c)
{
    for(int i=0;i<8;i++)
    {
        SCK_LOW();
        if(c&0x80)
        {
            if(1 != __lastsda)
            {
                __lastsda = 1;
                SDA_HIGH();
            }
        }
        else
        {
            if(0 != __lastsda)
            {
                __lastsda = 0;
                SDA_LOW();
            }
        }
    
        SCK_HIGH();
        c<<=1;
    }
}

static const uint8_t PROGMEM
cmd_240x240[] = {                     // Initialization commands for 7789 screens
    10,                               // 9 commands in list:
    ST7789_SWRESET,   ST_CMD_DELAY,     // 1: Software reset, no args, w/delay
    150,                            // 150 ms delay
    ST7789_SLPOUT ,   ST_CMD_DELAY,     // 2: Out of sleep mode, no args, w/delay
    255,                            // 255 = 500 ms delay
    ST7789_COLMOD , 1+ST_CMD_DELAY,     // 3: Set color mode, 1 arg + delay:
    0x55,                           // 16-bit color
    10,                             // 10 ms delay
    ST7789_MADCTL , 1,            // 4: Memory access ctrl (directions), 1 arg:
    0x00,                           // Row addr/col addr, bottom to top refresh
    ST7789_CASET  , 4,            // 5: Column addr set, 4 args, no delay:
    0x00, ST7789_240x240_XSTART,          // XSTART = 0
    (ST7789_TFTWIDTH+ST7789_240x240_XSTART) >> 8,
    (ST7789_TFTWIDTH+ST7789_240x240_XSTART) & 0xFF,   // XEND = 240
    ST7789_RASET  , 4,            // 6: Row addr set, 4 args, no delay:
    0x00, ST7789_240x240_YSTART,          // YSTART = 0
    (ST7789_TFTHEIGHT+ST7789_240x240_YSTART) >> 8,
    (ST7789_TFTHEIGHT+ST7789_240x240_YSTART) & 0xFF,  // YEND = 240
    ST7789_INVON ,   ST_CMD_DELAY,      // 7: Inversion ON
    10,
    ST7789_NORON  ,   ST_CMD_DELAY,     // 8: Normal display on, no args, w/delay
    10,                             // 10 ms delay
    ST7789_DISPOFF ,   ST_CMD_DELAY,     // 9: Main screen turn on, no args, w/delay
255 };                          // 255 = 500 ms delay


inline uint16_t swapcolor(uint16_t x) {
    return (x << 11) | (x & 0x07E0) | (x >> 11);
}


void writecommand(uint8_t c) 
{
    CS_LOW();
    DC_LOW();
    spiwrite(c);
    DC_HIGH();
    CS_HIGH();
}

void writedata(uint8_t c) {
    CS_LOW();
    //DC_HIGH();
    spiwrite(c);
    CS_HIGH();
}

void dispOn()
{
    writecommand(0x29);
}

void dispOff()
{
    writecommand(0x28);
}

// Companion code to the above tables.  Reads and issues
// a series of LCD commands stored in PROGMEM byte array.
void displayInit(const uint8_t *addr) {

    uint8_t  numCommands, numArgs;
    uint16_t ms;
    //<-----------------------------------------------------------------------------------------
    DC_HIGH();

    SCK_HIGH();


    numCommands = pgm_read_byte(addr++);   // Number of commands to follow
    while(numCommands--) {                 // For each command...
        writecommand(pgm_read_byte(addr++)); //   Read, issue command
        numArgs  = pgm_read_byte(addr++);    //   Number of args to follow
        ms       = numArgs & ST_CMD_DELAY;   //   If hibit set, delay follows args
        numArgs &= ~ST_CMD_DELAY;            //   Mask out delay bit
        while(numArgs--) {                   //   For each argument...
            writedata(pgm_read_byte(addr++));  //     Read, issue argument
        }

        if(ms) {
            ms = pgm_read_byte(addr++); // Read post-command delay time (ms)
            if(ms == 255) ms = 500;     // If 255, delay for 500 ms
            delay(ms);
        }
    }
}


// Initialization code common to all ST7789 displays
void commonInit(const uint8_t *cmdList) {
    _ystart = _xstart = 0;
    _colstart  = _rowstart = 0; // May be overridden in init func

    if(cmdList)
    displayInit(cmdList);
}

void setRotation(uint8_t m) {

  writecommand(ST7789_MADCTL);
  int rotation = m & 3;
  switch (rotation) {
   case 0:
     writedata(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
     _xstart = _colstart;
     _ystart = _rowstart;
     break;
   case 1:
     writedata(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
     _ystart = _colstart;
     _xstart = _rowstart;
     break;
  case 2:
     writedata(ST7789_MADCTL_RGB);
     _xstart = 0;
     _ystart = 0;
     break;
   case 3:
     writedata(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
     _xstart = 0;
     _ystart = 0;
     break;
  }
}

void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1,
uint16_t y1) {

    uint16_t x_start = x0 + _xstart, x_end = x1 + _xstart;
    uint16_t y_start = y0 + _ystart, y_end = y1 + _ystart;


    writecommand(ST7789_CASET); // Column addr set
    writedata(x_start >> 8);
    writedata(x_start & 0xFF);     // XSTART
    writedata(x_end >> 8);
    writedata(x_end & 0xFF);     // XEND

    writecommand(ST7789_RASET); // Row addr set
    writedata(y_start >> 8);
    writedata(y_start & 0xFF);     // YSTART
    writedata(y_end >> 8);
    writedata(y_end & 0xFF);     // YEND

    writecommand(ST7789_RAMWR); // write to RAM
}

void invertDisplay(boolean i) {
    writecommand(i ? ST7789_INVON : ST7789_INVOFF);
}

void lcd_init(uint16_t width, uint16_t height) {
    
    RST_HIGH();
    delay(50);
    RST_LOW();
    delay(50);
    RST_HIGH();
   
    commonInit(NULL);
    _colstart = ST7789_240x240_XSTART;
    _rowstart = ST7789_240x240_YSTART;
    _height = height;
    _width = width;
    displayInit(cmd_240x240);
    setRotation(2);
}

void lcd_init2(uint16_t width, uint16_t height) {
    
   /* RST_HIGH();
    delay(50);
    RST_LOW();
    delay(50);
    RST_HIGH();
   */
    commonInit(NULL);
    _colstart = ST7789_240x240_XSTART;
    _rowstart = ST7789_240x240_YSTART;
    _height = height;
    _width = width;
    displayInit(cmd_240x240);
    setRotation(2);
}

void LCD_WR_DATA(uint16_t dat)
{
    writedata(dat>>8);
    writedata(dat);
}

/******************************************************************************
      ����˵��������
      ������ݣ�x1,y1   ��ʼ����
                x2,y2   ��ֹ����
      ����ֵ��  ��
******************************************************************************/
void LCD_DrawLine(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t color)
{
    int len = 0;
    
    if(x1 == x2)        // 竖线
    {
        len = abs(y2-y1)+1;
    }
    else if(y1 == y2)   // 横线
    {
        len = abs(x2-x1)+1;
    }
    else return;
    
    setAddrWindow(x1, y1, x2, y2);
    for(int i=0; i<len; i++)
    {
        LCD_WR_DATA(color);
    }
}

void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{
    setAddrWindow(x,y,x,y);
    LCD_WR_DATA(color);
}

/******************************************************************************
  关闭显示
******************************************************************************/
void lcdOn()
{
    writecommand(0x29);
    delay(10);
    digitalWrite(pinBL, HIGH);
}

/******************************************************************************
  打开显示
******************************************************************************/
void lcdOff()
{
    digitalWrite(pinBL, LOW);
    writecommand(0x28);
}


void kwPointOn()
{
    LCD_DrawLine(70, 145, 70, 150, WHITE);
    LCD_DrawLine(71, 144, 71, 151, WHITE);
    LCD_DrawLine(72, 144, 72, 151, WHITE);
    LCD_DrawLine(73, 144, 73, 151, WHITE);
    LCD_DrawLine(74, 145, 74, 150, WHITE);
    
}
void kwPointOff()
{
    LCD_DrawLine(70, 145, 70, 150, BLACK);
    LCD_DrawLine(71, 144, 71, 151, BLACK);
    LCD_DrawLine(72, 144, 72, 151, BLACK);
    LCD_DrawLine(73, 144, 73, 151, BLACK);
    LCD_DrawLine(74, 145, 74, 150, BLACK);
}