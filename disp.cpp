#include "disp.h"
#include "lcd.h"
#include "bmp.h"
#include "hw.h"

// 显示缓存
unsigned char dtaDisp[9600];
unsigned char str_disp[6000];

// 以下变量用于控制显示内容只在变化的时候显示
int buf_error_code = -1;
int buf_lang       = -1;
int buf_gnd_st = -1;        // 1-gndok, 0-gnd fail
int buf_cat_st = -1;        // 1-caron, 0-caroff
int buf_v1v3 = -1;      // 0-v1, 1-v3
int buf_show_time = -1;
int buf_show_delay = -1;
int buf_show_aset = -1;
int buf_temp = -100;
int buf_kw = -1;
int buf_kwh = -1;
int buf_voltage = -1;
int buf_current = -1;
int buf_battery = -1;

int flg_wifi_show = -1;         // 0, 1, 2, ok, fail, connecting

void clearShowBuf()     // 清除显示换成，此时会立即刷新
{
    buf_error_code = -1;
    buf_gnd_st = -1;        // 1-gndok, 0-gnd fail
    buf_cat_st = -1;        // 1-caron, 0-caroff
    buf_v1v3 = -1;      // 0-v1, 1-v3
    buf_show_time = -1;
    buf_show_delay = -1;
    buf_show_aset = -1;
    buf_temp = -100;
    buf_kw = -1;
    buf_kwh = -1;
    buf_voltage = -1;
    buf_current = -1;
    //buf_battery = -1;
}

void clearDisp()
{
    memset(dtaDisp, 0x00, 9600);
}

void dispBuf(int y, int width, int color, int color_bk, int page)
{
    showImage2(0, y, 240, width, color, color_bk, (const unsigned char *)dtaDisp, page);
}

// x必须为8的倍数, len必须为8的倍数
void pushBuf(int x, int y, int len, int wid, const unsigned char *dta, int img_len)
{
    int len_8 = len/8;
    
    int decompressed_size = 0;
    decompressMonochromePixels(dta, img_len, str_disp, &decompressed_size);
    
    pushBuf(x, y, len, wid, str_disp);
}

// x必须为8的倍数, len必须为8的倍数
void pushBuf(int x, int y, int len, int wid, const unsigned char *dta)
{
    int len_8 = len/8;
    
    for(int i=0; i<wid; i++)
    {
        for(int j=0; j<(len/8); j++)
        {
            dtaDisp[(y+i)*30+j+(x/8)] = dta[i*len_8+j];
        }
    }
}

// LCD初始化
void lcdStart()
{
    CS_LOW();
    lcd_init(240, 320);   // initialize a ST7789 chip, 240x240 pixels
    lcdOff();
    delay(10);
    showImage();
#if GND_DETECT_ADC
   // if(getGndAdc() > GND_DETECT_VALUE)showGndOk();
   // else showGndFail();
    showGndOk();
#else
    if(digitalRead(pinDetGnd))showGndOk();
    else showGndFail();
#endif
    
    showCarOff();
    LCD_DrawLine(0, 235, 239, 235, GRAY);
    LCD_DrawLine(132, 235, 132, 319, GRAY);
    showVoltage(__sys_voltage);
    showTemp(__sys_temp);
    showAset(__chrg_current_val[__chrg_current]);
    delay(10);
    lcdOn();
    CS_HIGH();
    digitalWrite(pinBL, HIGH);
}

// 7段数码管每个数字的编码（0-9）
const uint8_t segmentData[10] = {
    0b00111111,  // 0
    0b00000110,  // 1
    0b01011011,  // 2
    0b01001111,  // 3
    0b01100110,  // 4
    0b01101101,  // 5
    0b01111101,  // 6
    0b00000111,  // 7
    0b01111111,  // 8
    0b01101111   // 9
};

void drawSegMini(int x, int y, int seg, unsigned int color)
{
    switch(seg)
    {
        case 0b1:     // a
        LCD_DrawLine(x+1, y+0, x+6, y+0, color);
        break;

        case 0b10:     // b
        LCD_DrawLine(x+7, y+1, x+7, y+4, color);
        break;

        case 0b100:     // c
        LCD_DrawLine(x+7, y+6, x+7, y+10, color);
        break;

        case 0b1000:     // d
        LCD_DrawLine(x+1, y+11, x+6, y+11, color);
        break;

        case 0b10000:     // e
        LCD_DrawLine(x+0, y+6, x+0, y+10, color);
        break;

        case 0b100000:     // f
        LCD_DrawLine(x+0, y+1, x+0, y+4, color);
        break;

        case 0b1000000:     // g
        LCD_DrawLine(x+1, y+5, x+6, y+5, color);
        break;

        default:;
    }
}


void drawSegSmall(int x, int y, int seg, unsigned int color)
{
    switch(seg)
    {
        case 0b1:     // a

        LCD_DrawLine(x+1, y+0, x+11, y+0, color);
        LCD_DrawLine(x+0, y+1, x+11, y+1, color);
        LCD_DrawLine(x+2, y+2, x+11, y+2, color);
        
        break;

        case 0b10:     // b

        LCD_DrawLine(x+12, y+2, x+12, y+10, color);
        LCD_DrawLine(x+13, y+0, x+13, y+11, color);
        LCD_DrawLine(x+14, y+1, x+14, y+11, color);

        break;

        case 0b100:     // c

        LCD_DrawLine(x+12, y+14, x+12, y+22, color);
        LCD_DrawLine(x+13, y+13, x+13, y+24, color);
        LCD_DrawLine(x+14, y+13, x+14, y+23, color);

        break;

        case 0b1000:     // d

        LCD_DrawLine(x+2, y+22, x+10, y+22, color);
        LCD_DrawLine(x+0, y+23, x+11, y+23, color);
        LCD_DrawLine(x+1, y+24, x+11, y+24, color);
        
        break;

        case 0b10000:     // e

        LCD_DrawLine(x+0, y+13, x+0, y+21, color);
        LCD_DrawLine(x+1, y+13, x+1, y+21, color);
        LCD_DrawLine(x+2, y+14, x+2, y+20, color);
        
        break;

        case 0b100000:     // f

        LCD_DrawLine(x+0, y+3, x+0, y+11, color);
        LCD_DrawLine(x+1, y+3, x+1, y+11, color);
        LCD_DrawLine(x+2, y+4, x+2, y+10, color);
        
        break;

        case 0b1000000:     // g
        LCD_DrawLine(x+3, y+11, x+11, y+11, color);
        LCD_DrawLine(x+2, y+12, x+12, y+12, color);
        LCD_DrawLine(x+3, y+13, x+11, y+13, color);
        break;

        default:;
    }
}


void drawSegMid(int x, int y, int seg, unsigned int color)
{
    switch(seg)
    {
        case 0b1:     // a

        LCD_DrawLine(x+1, y+0, x+15, y+0, color);
        LCD_DrawLine(x+0, y+1, x+15, y+1, color);
        LCD_DrawLine(x+0, y+2, x+15, y+2, color);
        LCD_DrawLine(x+3, y+3, x+14, y+3, color);
        
        break;

        case 0b10:     // b

        LCD_DrawLine(x+16, y+3, x+16, y+13, color);
        LCD_DrawLine(x+17, y+0, x+17, y+14, color);
        LCD_DrawLine(x+18, y+0, x+18, y+15, color);
        LCD_DrawLine(x+19, y+0, x+19, y+16, color);

        break;

        case 0b100:     // c

        LCD_DrawLine(x+16, y+20, x+16, y+30, color);
        LCD_DrawLine(x+17, y+19, x+17, y+33, color);
        LCD_DrawLine(x+18, y+18, x+18, y+33, color);
        LCD_DrawLine(x+19, y+17, x+19, y+32, color);

        break;

        case 0b1000:     // d

        LCD_DrawLine(x+3, y+30, x+14, y+30, color);
        LCD_DrawLine(x+0, y+31, x+15, y+31, color);
        LCD_DrawLine(x+0, y+32, x+15, y+32, color);
        LCD_DrawLine(x+1, y+33, x+15, y+33, color);
        
        break;

        case 0b10000:     // e

        LCD_DrawLine(x+0, y+17, x+0, y+29, color);
        LCD_DrawLine(x+1, y+18, x+1, y+29, color);
        LCD_DrawLine(x+2, y+19, x+2, y+29, color);
        LCD_DrawLine(x+3, y+20, x+3, y+28, color);
        
        break;

        case 0b100000:     // f

        LCD_DrawLine(x+0, y+4, x+0, y+16, color);
        LCD_DrawLine(x+1, y+4, x+1, y+15, color);
        LCD_DrawLine(x+2, y+4, x+2, y+14, color);
        LCD_DrawLine(x+3, y+5, x+3, y+13, color);
        
        break;

        case 0b1000000:     // g
        LCD_DrawLine(x+3, y+15, x+16, y+15, color);
        LCD_DrawLine(x+2, y+16, x+17, y+16, color);
        LCD_DrawLine(x+2, y+17, x+17, y+17, color);
        LCD_DrawLine(x+3, y+18, x+16, y+18, color);
        break;

        default:;
    }
}

// 0-7, a-g
void drawSegBig(int x, int y, int seg, unsigned int color)
{
    switch(seg)
    {
        case 0b1:     // a

        LCD_DrawLine(x+4, y+0, x+34, y+0, color);
        LCD_DrawLine(x+2, y+1, x+33, y+1, color);
        LCD_DrawLine(x+1, y+2, x+33, y+2, color);
        LCD_DrawLine(x+1, y+3, x+33, y+3, color);
        LCD_DrawLine(x+0, y+4, x+32, y+4, color);
        LCD_DrawLine(x+1, y+5, x+32, y+5, color);
        LCD_DrawLine(x+4, y+6, x+32, y+6, color);
        LCD_DrawLine(x+7, y+7, x+31, y+7, color);


        break;

        case 0b10:     // b

        LCD_DrawLine(x+34, y+7, x+34, y+29, color);
        LCD_DrawLine(x+35, y+4, x+35, y+30, color);
        LCD_DrawLine(x+36, y+1, x+36, y+31, color);
        LCD_DrawLine(x+37, y+0, x+37, y+32, color);
        LCD_DrawLine(x+38, y+0, x+38, y+33, color);
        LCD_DrawLine(x+39, y+1, x+39, y+34, color);
        LCD_DrawLine(x+40, y+2, x+40, y+34, color);
        LCD_DrawLine(x+41, y+4, x+41, y+33, color);
        break;

        case 0b100:     // c

        LCD_DrawLine(x+34, y+42, x+34, y+64, color);
        LCD_DrawLine(x+35, y+41, x+35, y+67, color);
        LCD_DrawLine(x+36, y+40, x+36, y+70, color);
        LCD_DrawLine(x+37, y+39, x+37, y+71, color);
        LCD_DrawLine(x+38, y+38, x+38, y+71, color);
        LCD_DrawLine(x+39, y+37, x+39, y+70, color);
        LCD_DrawLine(x+40, y+37, x+40, y+69, color);
        LCD_DrawLine(x+41, y+38, x+41, y+67, color);
        
        break;

        case 0b1000:     // d

        LCD_DrawLine(x+7, y+64, x+31, y+64, color);
        LCD_DrawLine(x+4, y+65, x+32, y+65, color);
        LCD_DrawLine(x+1, y+66, x+32, y+66, color);
        LCD_DrawLine(x+0, y+67, x+32, y+67, color);
        LCD_DrawLine(x+1, y+68, x+33, y+68, color);
        LCD_DrawLine(x+1, y+69, x+33, y+69, color);
        LCD_DrawLine(x+2, y+70, x+33, y+70, color);
        LCD_DrawLine(x+4, y+71, x+34, y+71, color);

        break;

        case 0b10000:     // e

        LCD_DrawLine(x+0, y+38, x+0, y+64, color);
        LCD_DrawLine(x+1, y+37, x+1, y+63, color);
        LCD_DrawLine(x+2, y+37, x+2, y+63, color);
        LCD_DrawLine(x+3, y+38, x+3, y+63, color);
        LCD_DrawLine(x+4, y+39, x+4, y+62, color);
        LCD_DrawLine(x+5, y+40, x+5, y+62, color);
        LCD_DrawLine(x+6, y+41, x+6, y+62, color);
        LCD_DrawLine(x+7, y+42, x+7, y+61, color);
        
        break;
        
        case 0b100000:     // f

        LCD_DrawLine(x+0, y+7, x+0, y+33, color);
        LCD_DrawLine(x+1, y+8, x+1, y+34, color);
        LCD_DrawLine(x+2, y+8, x+2, y+34, color);
        LCD_DrawLine(x+3, y+8, x+3, y+33, color);
        LCD_DrawLine(x+4, y+9, x+4, y+32, color);
        LCD_DrawLine(x+5, y+9, x+5, y+31, color);
        LCD_DrawLine(x+6, y+9, x+6, y+30, color);
        LCD_DrawLine(x+7, y+10, x+7, y+29, color);

        break;

        case 0b1000000:     // g
        
        LCD_DrawLine(x+7, y+32, x+32, y+32, color);
        LCD_DrawLine(x+6, y+33, x+33, y+33, color);
        LCD_DrawLine(x+5, y+34, x+34, y+34, color);
        LCD_DrawLine(x+4, y+35, x+35, y+35, color);
        LCD_DrawLine(x+4, y+36, x+35, y+36, color);
        LCD_DrawLine(x+5, y+37, x+34, y+37, color);
        LCD_DrawLine(x+6, y+38, x+33, y+38, color);
        LCD_DrawLine(x+7, y+39, x+32, y+39, color);

        break;

        default:;
    }
}

// 用于显示版本号的数字
void drawMiniNum(int x, int y, int num, unsigned int color, unsigned int color_bk)
{
    
    if (num > 9) return;  // 只处理0-9的数字
    
    uint8_t data = segmentData[num];
    drawSegMini(x, y, 0b00000001, data&0b1 ? color : color_bk);         // A
    drawSegMini(x, y, 0b00000010, data&0b00000010 ? color : color_bk);  // B
    drawSegMini(x, y, 0b00000100, data&0b00000100 ? color : color_bk);  // C
    drawSegMini(x, y, 0b00001000, data&0b00001000 ? color : color_bk);  // D
    drawSegMini(x, y, 0b00010000, data&0b00010000 ? color : color_bk);  // E
    drawSegMini(x, y, 0b00100000, data&0b00100000 ? color : color_bk);  // F
    drawSegMini(x, y, 0b01000000, data&0b01000000 ? color : color_bk);  // G
}

void drawSmallNum(int x, int y, int num, unsigned int color, unsigned int color_bk)
{
    
    if (num > 9) return;  // 只处理0-9的数字
    
    uint8_t data = segmentData[num];
    drawSegSmall(x, y, 0b00000001, data&0b1 ? color : color_bk);        // A
    drawSegSmall(x, y, 0b00000010, data&0b00000010 ? color : color_bk); // B
    drawSegSmall(x, y, 0b00000100, data&0b00000100 ? color : color_bk); // C
    drawSegSmall(x, y, 0b00001000, data&0b00001000 ? color : color_bk); // D
    drawSegSmall(x, y, 0b00010000, data&0b00010000 ? color : color_bk); // E
    drawSegSmall(x, y, 0b00100000, data&0b00100000 ? color : color_bk); // F
    drawSegSmall(x, y, 0b01000000, data&0b01000000 ? color : color_bk); // G

}

void drawMidNum(int x, int y, int num, unsigned int color, unsigned int color_bk)
{
    if (num > 9) return;  // 只处理0-9的数字
    
    uint8_t data = segmentData[num];
    drawSegMid(x, y, 0b00000001, data&0b1 ? color : color_bk);          // A
    drawSegMid(x, y, 0b00000010, data&0b00000010 ? color : color_bk);   // B
    drawSegMid(x, y, 0b00000100, data&0b00000100 ? color : color_bk);   // C
    drawSegMid(x, y, 0b00001000, data&0b00001000 ? color : color_bk);   // D
    drawSegMid(x, y, 0b00010000, data&0b00010000 ? color : color_bk);   // E
    drawSegMid(x, y, 0b00100000, data&0b00100000 ? color : color_bk);   // F
    drawSegMid(x, y, 0b01000000, data&0b01000000 ? color : color_bk);   // G
}

void drawBigNum(int x, int y, int num, unsigned int color, unsigned int color_bk)
{
    if (num > 9) return;  // 只处理0-9的数字
    uint8_t data = segmentData[num];

    drawSegBig(x, y, 0b00000001, data&0b1 ? color : color_bk);          // A
    drawSegBig(x, y, 0b00000010, data&0b00000010 ? color : color_bk);   // B
    drawSegBig(x, y, 0b00000100, data&0b00000100 ? color : color_bk);   // C
    drawSegBig(x, y, 0b00001000, data&0b00001000 ? color : color_bk);   // D
    drawSegBig(x, y, 0b00010000, data&0b00010000 ? color : color_bk);   // E
    drawSegBig(x, y, 0b00100000, data&0b00100000 ? color : color_bk);   // F
    drawSegBig(x, y, 0b01000000, data&0b01000000 ? color : color_bk);   // G
}

extern unsigned char mcu_ver[];
void showVersion()
{
    int x = 175;
    int y = 20;
    
    showImage(x, y, 64, 13, WHITE, BLUE1, img_version, sizeof(img_version));
    drawMiniNum(x+32, y+1, mcu_ver[0]-'0', WHITE, BLUE1);
    drawMiniNum(x+43, y+1, mcu_ver[2]-'0', WHITE, BLUE1);
    drawMiniNum(x+54, y+1, mcu_ver[4]-'0', WHITE, BLUE1);
}

void showCurrent(float __a)
{
    __a *= 10.0;
    int __at = __a;
    
    if(__at == buf_current)return;
    buf_current = __at;

    int x_start = 17;
    int y_start = 26;

    
    if(__at/100 == 0)drawSmallNum(x_start, y_start, __at/100, BLACK, BLACK);
    else drawSmallNum(x_start, y_start, __at/100, WHITE, BLACK);
    drawSmallNum(x_start+18, y_start, __at/10%10, WHITE, BLACK);
    drawSmallNum(x_start+43, y_start, __at%10, WHITE, BLACK);
}

void showTempError(int temp)
{
    int x_start = 35;
    int y_start = 26;

    drawSmallNum(x_start, y_start, temp/10, WHITE, BLACK);
    drawSmallNum(x_start+18, y_start, temp%10, WHITE, BLACK); 
}


void showVoltage(float __v)
{
    if(buf_voltage == __v)return;
    buf_voltage = __v;
    
    int x_start = 151;
    int y_start = 26;

    __v += 0.5;
    
    int _vv = __v;
    if(_vv/100 == 0) drawSmallNum(x_start, y_start, _vv/100, BLACK, BLACK);
    else drawSmallNum(x_start, y_start, _vv/100, WHITE, BLACK);
    
    drawSmallNum(x_start+18, y_start, _vv/10%10, WHITE, BLACK);
    drawSmallNum(x_start+36, y_start, _vv%10, WHITE, BLACK);
}


void showKwh(float __kwh)
{
    if(flg_error_page)return;       // 错误界面不显示
    __kwh *= 10.0;
    int __kwht = __kwh;
    
    if(buf_kwh == __kwht)return;
    buf_kwh == __kwht;

    int x_start = 125;
    int y_start = 182;
    
    // 114
    if(__kwht/100 == 0)drawSmallNum(x_start, y_start, __kwht/100, BLACK, BLACK);
    else drawSmallNum(x_start, y_start, __kwht/100, WHITE, BLACK);
    
    drawSmallNum(x_start+18, y_start, __kwht/10%10, WHITE, BLACK);
    drawSmallNum(x_start+43, y_start, __kwht%10, WHITE, BLACK);
}


void showKw(float __kw)
{
    if(flg_error_page)return;       // 错误界面不显示
    
    __kw *= 10.0;
    __kw += 0.5;
    
    int __kwt = __kw;
    
    if(buf_kw == __kwt)return;
    buf_kw = __kwt;

#if M2_3KW||M2_7KW
    int x_start = 18;
    int y_start = 80;
    
    drawBigNum(x_start, y_start, __kwt/10%10, WHITE, BLACK);
    drawBigNum(x_start+65, y_start, __kwt%10, WHITE, BLACK);
#else
    int x_start = 6;
    int y_start = 80;
    
    drawBigNum(6, y_start, __kwt/100%10, WHITE, BLACK);
    drawBigNum(55, y_start, __kwt/10%10, WHITE, BLACK);
    drawMidNum(112, 118, __kwt%10, WHITE, BLACK);
#endif
}


void showTemp(int temp)
{
    if(temp == buf_temp)return;
    buf_temp = temp;
    
    if(flg_error_page)return;       // 错误界面不显示
    int x_start = 137;
    int y_start = 80;
    drawSmallNum(x_start, y_start, temp/10, WHITE, BLACK);
    drawSmallNum(x_start+18, y_start, temp%10, WHITE, BLACK);
}


void showAset(int __aset)
{
    if(__aset == buf_show_aset)return;
    buf_show_aset = __aset;
    
    int x_start = 43;
    int y_start = 246;
    if(__aset/10 == 0)drawSmallNum(x_start, y_start, __aset/10, BLACK, BLACK);
    else drawSmallNum(x_start, y_start, __aset/10, WHITE, BLACK);
    drawSmallNum(x_start+18, y_start, __aset%10, WHITE, BLACK);
}

void showAsetOff()
{
    int x_start = 43;
    int y_start = 246;
    buf_show_aset = -1;
    drawSmallNum(x_start, y_start, 8, BLACK, BLACK);
    drawSmallNum(x_start, y_start, 8, BLACK, BLACK);
    drawSmallNum(x_start+18, y_start, 8, BLACK, BLACK);
}

void showDelay(int __h, int __m, int __s)
{
    int x_start = 43;
    int y_start = 283;

    int __color = WHITE;
    
    if(__h == 24)
    {
        __h = 0;
        __m = 0;
        __s = 0;
    }

    if(__h == 0 && __m == 0 && __s == 0)__color = GRAY2;
    else if(__work_status == ST_WORKING && __flg_schedule_set)
    {
        __color = GREEN;
    }
    else if(__h == 0 && __work_status == ST_WAIT)
    {
       __color = BLUE1;
    }
    
    if(__h > 0 || ((__work_status != ST_WAIT)&& !(__work_status == ST_WORKING && __flg_schedule_set)))
    {
        drawSmallNum(x_start, y_start, __h/10, __color, BLACK);
        drawSmallNum(x_start+18, y_start, __h%10, __color, BLACK);
        drawSmallNum(x_start+45, y_start, __m/10, __color, BLACK);
        drawSmallNum(x_start+63, y_start, __m%10, __color, BLACK);
    }
    else
    {
        drawSmallNum(x_start, y_start, __m/10, __color, BLACK);
        drawSmallNum(x_start+18, y_start, __m%10, __color, BLACK);
        drawSmallNum(x_start+45, y_start, __s/10, __color, BLACK);
        drawSmallNum(x_start+63, y_start, __s%10, __color, BLACK);
    }
}

void showDelayOff()
{
    int x_start = 43;
    int y_start = 283;
    
    buf_show_delay = -1;

    drawSmallNum(x_start, y_start, 8, BLACK, BLACK);
    drawSmallNum(x_start+18, y_start, 8, BLACK, BLACK);
    
    drawSmallNum(x_start+45, y_start, 8, BLACK, BLACK);
    drawSmallNum(x_start+63, y_start, 8, BLACK, BLACK);
}

void showTime(int __h, int __m)
{
    if(flg_error_page)return;       // 错误界面不显示
    int x_start = 13;
    int y_start = 182;

    drawSmallNum(x_start, y_start, __h/10, WHITE, BLACK);
    drawSmallNum(x_start+18, y_start, __h%10, WHITE, BLACK);
    
    drawSmallNum(x_start+43, y_start, __m/10, WHITE, BLACK);
    drawSmallNum(x_start+61, y_start, __m%10, WHITE, BLACK);
}


void showDelay(unsigned long t)
{
    if(t == buf_show_delay)return;
    buf_show_delay = t;
    pushDelay(t);
    showDelay(t/3600, (t%3600)/60, t%60);
}

void showTime(unsigned long t)
{
    if(t == buf_show_time)return;
    buf_show_time = t;
    showTime(t/3600, (t%3600)/60);
}

// 通过压缩算法显示图片
void showImage(int x, int y, int len, int wid, unsigned int color, unsigned int color_bk, const unsigned char *img, int img_len)
{
    int decompressed_size = 0;
    decompressMonochromePixels(img, img_len, str_disp, &decompressed_size);
    showImage(x, y, len, wid, color, color_bk, str_disp);
}

// 直接显示图片
void showImage(int x, int y, int len, int wid, unsigned int color, unsigned int color_bk, const unsigned char *img)
{
    setAddrWindow(x, y, x+len-1, y+wid-1);
    
    int lendiv8 = len/8;
    
    for(int i=0; i<wid; i++)
    {
        for(int j=0; j<lendiv8; j++)
        {
            unsigned char dta = img[lendiv8*i+j];
            
            for(int k=0; k<8; k++)
            {
                if(dta & 0x80)
                {
                    LCD_WR_DATA(color);
                }
                else
                {
                    LCD_WR_DATA(color_bk);
                }
                dta <<= 1;
            }
        }
    }
}

// 用于dispBuf
void showImage2(int x, int y, int len, int wid, unsigned int color, unsigned int color_bk, const unsigned char *img, int page)
{
    setAddrWindow(x, y, x+len-1, y+wid-1);
    
    int lendiv8 = len/8;
    
    for(int i=0; i<wid; i++)
    {
        unsigned int __color = color;
        
        if(page == 1 && i>168 && i<250)__color = BLUE1;
        else if(page == 3 && i>150 && i<260)__color = BLUE1;
        else if(page == 4 && i>150 && i<275)__color = BLUE1;
        else if(page == 2 && i>170 && i<232)__color = BLUE1;
        else if(page == 0)
        { 
            if(i>50 && i<77)
            {
                #if TRAVEL_AUTO_CHECK
                if(flg_travel)__color = GRAY2;
                else __color = BLUE1;
                #else
                __color = BLUE1;
                #endif
            }

#if M2_3KW
            else if(i>100 && i<123)__color = BLUE1;
#else
            else if(i>100 && i<123)__color = GRAY2;
#endif
            else if(i>146 && i <172)__color = BLUE1;
            else if(i>195 && i <221)__color = BLUE1;
            else if(i>246 && i <272)__color = BLUE1;
            else if(i<=41)__color = WHITE;
            else if(i>=278)__color = WHITE;
            else __color == color;
        }
        
        unsigned int __colorbk = color_bk;
        
        if(page <= 4 && (i<=40 || i>=278))__colorbk = BLUE1;
        
        for(int j=0; j<lendiv8; j++)
        {
            unsigned char dta = img[y*30+lendiv8*i+j];
            
            for(int k=0; k<8; k++)
            {
                if(dta & 0x80)
                {
                    LCD_WR_DATA(__color);
                }
                else
                {
                    LCD_WR_DATA(__colorbk);
                }
                dta <<= 1;
            }
        }
    }
}


// 画一个矩形
void fillRec(int x, int y, int len, int wid, unsigned int color)
{
    setAddrWindow(x, y, x+len-1, y+wid-1);
    
    for(int i=0; i<wid; i++)
    {
        for(int j=0; j<len; j++)
        {
            LCD_WR_DATA(color);
        }
    }
}

void showV3()
{
    if(buf_v1v3 == 1)return;
    buf_v1v3 = 1;
    int color = WHITE;              
    int color_bk = BLACK;
   
    showImage(214, 24, 16, 28, color, color_bk, img_v3, sizeof(img_v3));
}

void showV1()
{
    if(buf_v1v3 == 0)return;
    buf_v1v3 = 0;
    
    int color = WHITE;              
    int color_bk = BLACK;
   
    showImage(214, 24, 16, 28, color, color_bk, img_v1, sizeof(img_v1));
}

void showCarOn()
{
    if(buf_cat_st == 1)return;      // 如果上次显示过
    buf_cat_st = 1;
    int color = GREEN;              // 绿色
    int color_bk = BLACK;
   
    showImage(UI_CAR_X_START, UI_CAR_Y_START, UI_CAR_LEN, UI_CAR_WID, color, color_bk, img_car, sizeof(img_car));
}

void showCarOff()
{
    if(buf_cat_st == 0)return;
    buf_cat_st = 0;
    
    int color = GRAY;            // 灰色
    int color_bk = BLACK;

    showImage(UI_CAR_X_START, UI_CAR_Y_START, UI_CAR_LEN, UI_CAR_WID, color, color_bk, img_car, sizeof(img_car));
}

void showGndOk()
{
    if(buf_gnd_st == 1)return;
    buf_gnd_st = 1;
    int x_start = 144;
    int y_start = 261;
    int len = 40;
    int wid = 31;
    
    int color = GREEN;            // 绿色
    int color_bk = BLACK;

    showImage(x_start, y_start, len, wid, color, color_bk, img_gnd, sizeof(img_gnd));
}

void showGndFail()
{
    if(buf_gnd_st == 0)return;
    buf_gnd_st = 0;
    
    int x_start = 144;
    int y_start = 261;
    int len = 40;
    int wid = 31;
    
    int color = RED;            // 红色
    int color_bk = BLACK;

    showImage(x_start, y_start, len, wid, color, color_bk, img_gnd, sizeof(img_gnd));
}

const unsigned char img_wifi[] = {
/*--  调入了一幅图像：C:\Users\lowai\Desktop\无标题.bmp  --*/
/*--  宽度x高度=40x31  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,
0xF8,0x00,0x00,0x00,0x7F,0xFF,0x00,0x00,0x01,0xFF,0xFF,0xC0,0x00,0x03,0xFC,0x1F,
0xE0,0x00,0x07,0xC0,0x01,0xF0,0x00,0x0F,0x00,0x00,0x78,0x00,0x06,0x03,0xE0,0x30,
0x00,0x00,0x1F,0xFC,0x00,0x00,0x00,0x7F,0xFF,0x00,0x00,0x00,0xFF,0xFF,0x80,0x00,
0x00,0x70,0x07,0x00,0x00,0x00,0x20,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x07,0xF0,0x00,0x00,0x00,0x0F,0xF8,0x00,0x00,0x00,0x07,0xF0,0x00,0x00,0x00,0x03,
0xE0,0x00,0x00,0x00,0x01,0xC0,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};


/*
flg_wifi_show
0 绿色全灰色
1 绿色1点
2 绿色2点
3 绿色3点
4 红色
5 蓝色
6 透明
*/
void showWifiOk(int n)
{
#if TY_VERSION
    if(!flg_ty_en)return;
    
    if(n == flg_wifi_show)return;
    
    flg_wifi_show = n;

    int x_start = 108;
    int y_start = 2;
    int len = 40;
    int wid = 31;
    
    int color = GREEN;            // 绿色
    int color_bk = BLACK;
    //showImage(x_start, y_start, len, wid, color, color_bk, img_wifi);
    
    switch(n)
    {
        case 0:     // 信号<-70db
        showImage(x_start, y_start, 32, 6, GRAY2, color_bk, img_wifi2, sizeof(img_wifi2));
        showImage(x_start, y_start+6, 32, 6, GRAY2, color_bk, img_wifi1, sizeof(img_wifi1));
        showImage(x_start, y_start+12, 32, 6, GRAY2, color_bk, img_wifi0, sizeof(img_wifi0));
        break;
        
        case 1:     // -60~-70DB
        showImage(x_start, y_start, 32, 6, GRAY2, color_bk, img_wifi2, sizeof(img_wifi2));
        showImage(x_start, y_start+6, 32, 6, GRAY2, color_bk, img_wifi1, sizeof(img_wifi1));
        showImage(x_start, y_start+12, 32, 6, GREEN, color_bk, img_wifi0, sizeof(img_wifi0));
        break;
        
        case 2:     // -50~-60DB
        showImage(x_start, y_start, 32, 6, GRAY2, color_bk, img_wifi2, sizeof(img_wifi2));
        showImage(x_start, y_start+6, 32, 6, GREEN, color_bk, img_wifi1, sizeof(img_wifi1));
        showImage(x_start, y_start+12, 32, 6, GREEN, color_bk, img_wifi0, sizeof(img_wifi0));
        break;
        
        case 3:     // -50~-60DB
        showImage(x_start, y_start, 32, 6, GREEN, color_bk, img_wifi2, sizeof(img_wifi2));
        showImage(x_start, y_start+6, 32, 6, GREEN, color_bk, img_wifi1, sizeof(img_wifi1));
        showImage(x_start, y_start+12, 32, 6, GREEN, color_bk, img_wifi0, sizeof(img_wifi0));
        break;
    }
#endif
}

void showWifiFail()
{
#if TY_VERSION
    if(!flg_ty_en)return;
    if(flg_wifi_show == 4)return;
    flg_wifi_show = 4;
    
    int x_start = 108;
    int y_start = 2;
    int len = 40;
    int wid = 31;
    
    int color = RED;            // 绿色
    int color_bk = BLACK;

    showImage(x_start, y_start, 32, 6, color, color_bk, img_wifi2, sizeof(img_wifi2));
    showImage(x_start, y_start+6, 32, 6, color, color_bk, img_wifi1, sizeof(img_wifi1));
    showImage(x_start, y_start+12, 32, 6, color, color_bk, img_wifi0, sizeof(img_wifi0));
    
#endif
}

// flg_wifi_show = 5/蓝色
// flg_wifi_show = 6/透明
void showWifiConnecting(int st)
{
#if TY_VERSION

    if(!flg_ty_en)return;
    if(st)
    {
        if(flg_wifi_show == 5)return;
        flg_wifi_show = 5;
    }
    else
    {
        if(flg_wifi_show == 6)return;
        flg_wifi_show = 6;
    }
    
    int x_start = 108;
    int y_start = 2;
    int len = 40;
    int wid = 31;
    
    int color = st ? BLUE1 : BLACK;            // 蓝色
    int color_bk = BLACK;

    //showImage(x_start, y_start, len, wid, color, color_bk, img_wifi);
    
    showImage(x_start, y_start, 32, 6, color, color_bk, img_wifi2, sizeof(img_wifi2));
    showImage(x_start, y_start+6, 32, 6, color, color_bk, img_wifi1, sizeof(img_wifi1));
    showImage(x_start, y_start+12, 32, 6, color, color_bk, img_wifi0, sizeof(img_wifi0));
    
#endif
}

/*
0 - 漏电
1 - 过流
2 - 过压
3 - 欠压
4 - 接地1
5 - 接地2
6 - 过温
7 - 二极管击穿
8 - 继电器黏连
9 - CP错误
10 - 漏保自检错误
11 - 停止
12 - 墙插过热
13 - NFC
*/

void showErrorImg(int error_code)
{
    if(error_code > 13)return;
    flg_error_page = 1;         // 进入错误显示状态
    showBattery(0);             // 清除电池logo
    
    if((buf_error_code == error_code) && (buf_lang == __langUage))return;
    buf_error_code = error_code;
    buf_lang = __langUage;
    
    int x_start = 0;
    int y_start = 80;
    int len = 240;
    int wid = 130;
    int lendiv8 = len/8;

    int decompressed_size = 0;
    
    if(error_code < 11)
    {
        int error_work_st[11] = {501, 400, 401, 402, 507, 507, 403, 506, 502, 504, 503};
        pushWorkState(error_work_st[error_code]);
        pushAlarm(error_work_st[error_code]);
    }
    
    if(error_code == 12)        // wall temp overheat
    {
        pushWorkState(404);
        pushAlarm(404);
    }
    
#if MODE_TUV||PHASE1_ONLY_ENGLISH||PHASE3_ONLY_ENGLISH
    __langUage = 0;
#endif
    switch(__langUage)
    {
        case 0:     // 英文
        if(error_code == 0)decompressMonochromePixels(img_error_00, sizeof(img_error_00), str_disp, &decompressed_size);
        else if(error_code == 1)decompressMonochromePixels(img_error_01, sizeof(img_error_01), str_disp, &decompressed_size);
        else if(error_code == 2)decompressMonochromePixels(img_error_02, sizeof(img_error_02), str_disp, &decompressed_size);
        else if(error_code == 3)decompressMonochromePixels(img_error_03, sizeof(img_error_03), str_disp, &decompressed_size);
        else if(error_code == 4)decompressMonochromePixels(img_error_04, sizeof(img_error_04), str_disp, &decompressed_size);
        else if(error_code == 5)decompressMonochromePixels(img_error_05, sizeof(img_error_05), str_disp, &decompressed_size);
        else if(error_code == 6)decompressMonochromePixels(img_error_06, sizeof(img_error_06), str_disp, &decompressed_size);
        else if(error_code == 7)decompressMonochromePixels(img_error_07, sizeof(img_error_07), str_disp, &decompressed_size);
        else if(error_code == 8)decompressMonochromePixels(img_error_08, sizeof(img_error_08), str_disp, &decompressed_size);
        else if(error_code == 9)decompressMonochromePixels(img_error_09, sizeof(img_error_09), str_disp, &decompressed_size);
        else if(error_code == 10)decompressMonochromePixels(img_error_010, sizeof(img_error_010), str_disp, &decompressed_size);
        else if(error_code == 11)decompressMonochromePixels(img_stop, sizeof(img_stop), str_disp, &decompressed_size);
        else if(error_code == 12)decompressMonochromePixels(img_error_012, sizeof(img_error_012), str_disp, &decompressed_size);
        else if(error_code == 13)decompressMonochromePixels(img_nfc, sizeof(img_nfc), str_disp, &decompressed_size);    
        else return;

        break;
#if (!MODE_TUV)&&(!PHASE1_ONLY_ENGLISH)&&(!PHASE3_ONLY_ENGLISH)
        case 1:     // 德语
        if(error_code == 0)decompressMonochromePixels(img_error_10, sizeof(img_error_10), str_disp, &decompressed_size);
        else if(error_code == 1)decompressMonochromePixels(img_error_11, sizeof(img_error_11), str_disp, &decompressed_size);
        else if(error_code == 2)decompressMonochromePixels(img_error_12, sizeof(img_error_12), str_disp, &decompressed_size);
        else if(error_code == 3)decompressMonochromePixels(img_error_13, sizeof(img_error_13), str_disp, &decompressed_size);
        else if(error_code == 4)decompressMonochromePixels(img_error_14, sizeof(img_error_14), str_disp, &decompressed_size);
        else if(error_code == 5)decompressMonochromePixels(img_error_15, sizeof(img_error_15), str_disp, &decompressed_size);
        else if(error_code == 6)decompressMonochromePixels(img_error_16, sizeof(img_error_16), str_disp, &decompressed_size);
        else if(error_code == 7)decompressMonochromePixels(img_error_17, sizeof(img_error_17), str_disp, &decompressed_size);
        else if(error_code == 8)decompressMonochromePixels(img_error_18, sizeof(img_error_18), str_disp, &decompressed_size);
        else if(error_code == 9)decompressMonochromePixels(img_error_19, sizeof(img_error_19), str_disp, &decompressed_size);
        else if(error_code == 10)decompressMonochromePixels(img_error_110, sizeof(img_error_110), str_disp, &decompressed_size);
        else if(error_code == 11)decompressMonochromePixels(img_stop, sizeof(img_stop), str_disp, &decompressed_size);
        else if(error_code == 12)decompressMonochromePixels(img_error_112, sizeof(img_error_112), str_disp, &decompressed_size);
        else if(error_code == 13)decompressMonochromePixels(img_nfc, sizeof(img_nfc), str_disp, &decompressed_size); 
        else return;
        break;
        
        case 2:     // 法语
        if(error_code == 0)decompressMonochromePixels(img_error_20, sizeof(img_error_20), str_disp, &decompressed_size);
        else if(error_code == 1)decompressMonochromePixels(img_error_21, sizeof(img_error_21), str_disp, &decompressed_size);
        else if(error_code == 2)decompressMonochromePixels(img_error_22, sizeof(img_error_22), str_disp, &decompressed_size);
        else if(error_code == 3)decompressMonochromePixels(img_error_23, sizeof(img_error_23), str_disp, &decompressed_size);
        else if(error_code == 4)decompressMonochromePixels(img_error_24, sizeof(img_error_24), str_disp, &decompressed_size);
        else if(error_code == 5)decompressMonochromePixels(img_error_25, sizeof(img_error_25), str_disp, &decompressed_size);
        else if(error_code == 6)decompressMonochromePixels(img_error_26, sizeof(img_error_26), str_disp, &decompressed_size);
        else if(error_code == 7)decompressMonochromePixels(img_error_27, sizeof(img_error_27), str_disp, &decompressed_size);
        else if(error_code == 8)decompressMonochromePixels(img_error_28, sizeof(img_error_28), str_disp, &decompressed_size);
        else if(error_code == 9)decompressMonochromePixels(img_error_29, sizeof(img_error_29), str_disp, &decompressed_size);
        else if(error_code == 10)decompressMonochromePixels(img_error_210, sizeof(img_error_210), str_disp, &decompressed_size);
        else if(error_code == 11)decompressMonochromePixels(img_stop, sizeof(img_stop), str_disp, &decompressed_size);
        else if(error_code == 12)decompressMonochromePixels(img_error_212, sizeof(img_error_212), str_disp, &decompressed_size);
        else if(error_code == 13)decompressMonochromePixels(img_nfc, sizeof(img_nfc), str_disp, &decompressed_size); 
        else return;
        break;
        
        case 3:     // 意大利语
        if(error_code == 0)decompressMonochromePixels(img_error_30, sizeof(img_error_30), str_disp, &decompressed_size);
        else if(error_code == 1)decompressMonochromePixels(img_error_31, sizeof(img_error_31), str_disp, &decompressed_size);
        else if(error_code == 2)decompressMonochromePixels(img_error_32, sizeof(img_error_32), str_disp, &decompressed_size);
        else if(error_code == 3)decompressMonochromePixels(img_error_33, sizeof(img_error_33), str_disp, &decompressed_size);
        else if(error_code == 4)decompressMonochromePixels(img_error_34, sizeof(img_error_34), str_disp, &decompressed_size);
        else if(error_code == 5)decompressMonochromePixels(img_error_35, sizeof(img_error_35), str_disp, &decompressed_size);
        else if(error_code == 6)decompressMonochromePixels(img_error_36, sizeof(img_error_36), str_disp, &decompressed_size);
        else if(error_code == 7)decompressMonochromePixels(img_error_37, sizeof(img_error_37), str_disp, &decompressed_size);
        else if(error_code == 8)decompressMonochromePixels(img_error_38, sizeof(img_error_38), str_disp, &decompressed_size);
        else if(error_code == 9)decompressMonochromePixels(img_error_39, sizeof(img_error_39), str_disp, &decompressed_size);
        else if(error_code == 10)decompressMonochromePixels(img_error_310, sizeof(img_error_310), str_disp, &decompressed_size);
        else if(error_code == 11)decompressMonochromePixels(img_stop, sizeof(img_stop), str_disp, &decompressed_size);
        else if(error_code == 12)decompressMonochromePixels(img_error_312, sizeof(img_error_312), str_disp, &decompressed_size);
        else if(error_code == 13)decompressMonochromePixels(img_nfc, sizeof(img_nfc), str_disp, &decompressed_size); 
        else return;
        break;
        
        case 4:     // 葡萄牙语或者西班牙语或西班牙语
        if(error_code == 0)decompressMonochromePixels(img_error_40, sizeof(img_error_40), str_disp, &decompressed_size);
        else if(error_code == 1)decompressMonochromePixels(img_error_41, sizeof(img_error_41), str_disp, &decompressed_size);
        else if(error_code == 2)decompressMonochromePixels(img_error_42, sizeof(img_error_42), str_disp, &decompressed_size);
        else if(error_code == 3)decompressMonochromePixels(img_error_43, sizeof(img_error_43), str_disp, &decompressed_size);
        else if(error_code == 4)decompressMonochromePixels(img_error_44, sizeof(img_error_44), str_disp, &decompressed_size);
        else if(error_code == 5)decompressMonochromePixels(img_error_45, sizeof(img_error_45), str_disp, &decompressed_size);
        else if(error_code == 6)decompressMonochromePixels(img_error_46, sizeof(img_error_46), str_disp, &decompressed_size);
        else if(error_code == 7)decompressMonochromePixels(img_error_47, sizeof(img_error_47), str_disp, &decompressed_size);
        else if(error_code == 8)decompressMonochromePixels(img_error_48, sizeof(img_error_48), str_disp, &decompressed_size);
        else if(error_code == 9)decompressMonochromePixels(img_error_49, sizeof(img_error_49), str_disp, &decompressed_size);
        else if(error_code == 10)decompressMonochromePixels(img_error_410, sizeof(img_error_410), str_disp, &decompressed_size);
        else if(error_code == 11)decompressMonochromePixels(img_stop, sizeof(img_stop), str_disp, &decompressed_size);
        else if(error_code == 12)decompressMonochromePixels(img_error_412, sizeof(img_error_412), str_disp, &decompressed_size);
        else if(error_code == 13)decompressMonochromePixels(img_nfc, sizeof(img_nfc), str_disp, &decompressed_size); 
        else return;
        break;
#endif
    }

    if(error_code <= 12)
    {
        unsigned char str_trangle[500];
        decompressMonochromePixels(img_error_sjx, sizeof(img_error_sjx), str_trangle, &decompressed_size);
        for(int i=0; i<50; i++)
        {
            for(int j=0; j<7; j++)
            {
                str_disp[30*i+j] = str_trangle[7*i+j];
            }
        }
    }
    
    setAddrWindow(x_start, y_start, x_start+len-1, y_start+wid-1);
    unsigned int color_yellow = YELLOW;
    unsigned int color_white  = WHITE;
    
    if(error_code == 13)color_yellow = WHITE;
    
    for(int i=0; i<wid; i++)
    {
        for(int j=0; j<lendiv8; j++)
        {
            unsigned char dta = str_disp[lendiv8*i+j];
            
            for(int k=0; k<8; k++)
            {
                if(dta & 0x80)
                {
                    
                    if(i<50 && j<7)LCD_WR_DATA(color_yellow);
                    else LCD_WR_DATA(color_white);
                }
                else
                {
                    LCD_WR_DATA(BLACK);
                }
                dta <<= 1;
            }
        }
    }
}

// mode - 0: 电流，电压
// mode - 1: 温度，电压
// mode - 2: 只刷新中间部分
void showNormal(int mode)
{
    CS_LOW();
    flg_error_page = 0;         // 没有处于错误界面
    clearShowBuf();
    clearDisp();
    
    flg_wifi_show = -1;
    
    if(mode == 0)pushBuf(0, 26, 240, 25, img_start_0, sizeof(img_start_0));
    else if(mode == 1)pushBuf(0, 26, 240, 25, img_start_0_1, sizeof(img_start_0_1));
    
    pushBuf(0, 80, 192, 72, img_start_1, sizeof(img_start_1));
    pushBuf(0, 182, 240, 25, img_start_2, sizeof(img_start_2));
    
    if(mode == 3)dispBuf(80, 150, WHITE, BLACK, 5);
    else dispBuf(0, 230, WHITE, BLACK, 5);
    
#if M2_11KW||M2_22KW||M3_22KW
    #if L2_CHECK
    if((__sys_current1 > 1 && __sys_current2 > 1 && __sys_current3 > 1) && (flg_phase_3 == 0))      // 三相都有电流，但是开始检测为单相
    {
        flg_phase_3 = 1;
    }
    
    if(flg_phase_3)showV3();
    else showV1();
    #else
    if(__sys_current1 > 1 && __sys_current2 > 1 && __sys_current3 > 1)showV3();
    else showV1();
    #endif
#endif
    
    CS_HIGH();
}

void showImage()
{
    clearDisp();
    pushBuf(0, 26, 240, 25, img_start_0, sizeof(img_start_0));
    pushBuf(0, 80, 192, 72, img_start_1, sizeof(img_start_1));
    pushBuf(0, 182, 240, 25, img_start_2, sizeof(img_start_2));
    pushBuf(0, 246, 128, 63, img_start_3, sizeof(img_start_3));
    
    // 让屏幕显示内容
    dispBuf(0, 320, WHITE, BLACK, 5);
#if M2_11KW||M2_22KW||M3_22KW
    #if L2_CHECK
    if(flg_phase_3)showV3();
    else showV1();
    #else
    if(__sys_current1 > 1 && __sys_current2 > 1 && __sys_current3 > 1)showV3();
    else showV1();
    #endif
#endif
}

void showBattery(int val)
{
    if(val > 5)return;
    if(buf_battery == val)return;
    buf_battery = val;
    
    CS_LOW();
    
    int x_start = 200;
    int y_start = 81;
    int len = 32;
    int wid = 72;
    int lendiv8 = len/8;
    
    setAddrWindow(x_start, y_start, x_start+len-1, y_start+wid-1);
    
    int decompressed_size = 0;

    if(val == 0)decompressMonochromePixels(img_battery_0, sizeof(img_battery_0), str_disp, &decompressed_size);
    else if(val == 1)decompressMonochromePixels(img_battery_1, sizeof(img_battery_1), str_disp, &decompressed_size);
    else if(val == 2)decompressMonochromePixels(img_battery_2, sizeof(img_battery_2), str_disp, &decompressed_size);
    else if(val == 3)decompressMonochromePixels(img_battery_3, sizeof(img_battery_3), str_disp, &decompressed_size);
    else if(val == 4)decompressMonochromePixels(img_battery_4, sizeof(img_battery_4), str_disp, &decompressed_size);
    else if(val == 5)decompressMonochromePixels(img_battery_5, sizeof(img_battery_5), str_disp, &decompressed_size);
    else return;
    
    for(int i=0; i<wid; i++)
    {
        for(int j=0; j<len/8; j++)
        {
            unsigned char dta = str_disp[lendiv8*i+j];
            
            for(int k=0; k<8; k++)
            {
                if(dta & 0x80)
                {
                    LCD_WR_DATA(WHITE);
                }
                else
                { 
                    LCD_WR_DATA(BLACK);
                }
                dta <<= 1;
            }
        }
    }
    
    CS_HIGH();
}

void taskDisplay()
{
    if((millis()-timer_btn_last < 2000) && flg_init_ok)return;
    taskDisplay(500);
}

void taskDisplay(int t)
{
    if((millis()-timer_btn_last < 2000) && flg_init_ok && (t != 0))return;
    static unsigned long timer_s = millis();
    if(millis()-timer_s < t)return;
    timer_s = millis();

    CS_LOW();

    unsigned long timer_t = millis();
    
    if(flg_pot)showTempError(getTemp());
    else if(flg_pot_wall)showTempError(getTempWall());
    else showCurrent(__sys_current1);
       
    checkInsert();
    delayCountDown();
    
#if M2_3KW || M2_7KW
    showKw(__sys_current1*(float)__sys_voltage/1000.0);
#elif M2_11KW || M2_22KW||M3_22KW
    showKw((__sys_current1+__sys_current2+__sys_current3)*(float)__sys_voltage/1000.0);
#endif
    checkInsert();
    delayCountDown();
    showAset(__chrg_current_val[__chrg_current]);
    checkInsert();
    delayCountDown();
    showVoltage(__sys_voltage);
    checkInsert();
    delayCountDown();
    showKwh(__sys_kwh);
    checkInsert();
    delayCountDown();

#if TY_VERSION
    if((__work_ty_st != X_WS_WAIT_SCHEDULE) && !(__work_status == ST_WORKING && __flg_schedule_set)) showDelay(__sys_delay_time);
#endif
    checkInsert();
    delayCountDown();
    showTime(__sys_chrg_time);
    checkInsert();
    delayCountDown();
    showTemp(getTemp());
    checkInsert();
    delayCountDown();
    if(__sys_gnd)
    {
        showGndOk();
    }
    else showGndFail();
    checkInsert();
    delayCountDown();
    
#if M2_11KW||M2_22KW||M3_22KW
    #if L2_CHECK
    if(flg_phase_3)showV3();
    else showV1();
    #else
    if(__sys_current1 > 1 && __sys_current2 > 1 && __sys_current3 > 1)showV3();
    else showV1();
    #endif
#endif

    CS_HIGH();
}


void decompressMonochromePixels(const unsigned char *compressed, int length, unsigned char *decompressed, int *decompressedLength) {
    int decompressedIndex = 0;
    unsigned char currentDecompressedByte = 0;
    unsigned char bitCount = 0;

    for (int i = 0; i < length; i++) {
        unsigned char byte = compressed[i];
        unsigned char value = (byte >> 7) & 0x01; // Get the highest bit (value)
        unsigned char count = byte & 0x7F; // Get the lower 7 bits (count)

        for (int j = 0; j < count; j++) {
            currentDecompressedByte = (currentDecompressedByte << 1) | value;
            bitCount++;

            if (bitCount == 8) {
                decompressed[decompressedIndex++] = currentDecompressedByte;
                currentDecompressedByte = 0;
                bitCount = 0;
            }
        }
    }

    // If the last byte is not full, pad with 0s
    if (bitCount > 0) {
        currentDecompressedByte = currentDecompressedByte << (8 - bitCount);
        decompressed[decompressedIndex++] = currentDecompressedByte;
    }

    *decompressedLength = decompressedIndex; // Correctly assign the decompressed length
}
// END FILE