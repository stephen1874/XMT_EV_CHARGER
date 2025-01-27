/* 信美通代码管理
    1.0.0，2025-1-13日正式发布
   
    1.0.1 2024-1-14
        1. 增加50/60Hz自动检测
        2. 高压低压校准设为285/65V
        3. 修复了过压保护没有校准的bug
        
    1.0.2 2024-1-14
        1. 由于自动检测频率在三相的时候不稳定，去掉
        
    1.0.3 2024-1-15
        1. 一些错误提示的图片有误

    1.0.7 2014-1-26
        1. 利用ADC来检测接地，已解决低压接地错报的问题
*/
#include "lcd.h"
#include <Adafruit_NeoPixel.h>
#include <DFRobot_PN532.h>
#include "disp.h"
#include "dfs.h"
#include "hw.h"
#include "fun.h"
#include "bmp.h"

int out_len = 0;
extern unsigned char str_disp[6000];
// 错误计数器
int cnt_current_error   = 0;
#if M2_22KW|M2_11KW||M3_22KW
int cnt_current_error2  = 0;
int cnt_current_error3  = 0;
#endif
int cnt_vol264_error    = 0;
int cnt_vol176_error    = 0;
int cnt_gnd_error       = 0;

int flg_vol_temp_mode   = 0;                // 0-显示电压，1-显示温度
int flgTyConnected      = 0;
int flgTyBlueBlink      = 0;                // 涂鸦配置状态标志位
int freq_cycle          = 20000;            // 50Hz-20000, 60Hz, 16667
#if L2_CHECK
int flg_phase_3 = 0;
#endif

#if TRAVEL_AUTO_CHECK
int flg_travel = 0;                         // 是否Travel版本
#endif

unsigned long timer_status_change = 0;      // 状态改变计时器
unsigned long timer_sleep = 0;              // 休眠计数器，如果处于Idle状态切连续5分钟没有任何操作进入休眠
unsigned long timer_relay_on = 0;           // 继电器闭合时间
unsigned long timer_get_chrgmode = 0;       // 收到设置充电模式命令的时间
unsigned long timer_relay_off = 0;          // 继电器断开时间
int flg_error_page = 0;                     // 是否处于错误界面，调用

#if TY_VERSION
int flg_ty_en = 0;                          // 是否启用涂鸦模块，当第一次长按后，启用
int wifi_rssi = 0;                          // 信号强度
unsigned long timer_ty_get_cmd    = 0;
unsigned long timer_get_time_ok = 0;
int flg_net_stop = 0;           // 断网标志位
#endif

#if NFC_VERSION
#define  BLOCK_SIZE      16
#define  PN532_IRQ      (2)
#define  INTERRUPT      (1)
#define  POLLING        (0)
// Use this line for a breakout or shield with an I2C connection
// Check the card by polling
DFRobot_PN532_IIC  nfc(PN532_IRQ, POLLING); 
DFRobot_PN532::sCard_t NFCcard;

unsigned char nfcRead[16] = {0};
unsigned char nfc_if_last_got = 0;          // 上一次是否读取到NFC，用于判断是否拿开NFC
unsigned char cnt_nfc_get_continue = 0;     // 连续多少次读到，用来判断NFC卡片放了多久

void readNFC()
{
#if NFC_VERSION
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 1000)return;
    timer_s = millis();
    
    feedog();
    if (nfc.scan())
    {
        //Read the basic information of the card
        feedog();
        nfc.readData(nfcRead, 2);
        feedog();
        
        buzOn();
        delay(100);
        buzOn();
    }
    else
    {
        nfc_if_last_got = 0;
        cnt_nfc_get_continue = 0;
    }
#endif
}

void waitNfc()
{
#if NFC_VERSION
    showErrorImg(ERROR_CODE_NFC);
    ledInsert();
    while(1)
    {
        updateSleepTimer();
        checkSensor(100);
        taskDisplay();
        feedog();
        
        if(nfc.scan())
        {
            feedog();
            nfc.readData(nfcRead, 2);
            feedog();
            
            buzOn();
            break;
        }
        
        delayFeed(500);
    }
    
    changeStatus(ST_IDLE);
    relayOff();
    makePwm(100);            // 开启PWM
    showNormal(3);
    ledIdle();
#endif
}

#endif
void updateSleepTimer()
{
    timer_sleep = millis();
    if(__work_status == ST_SLEEP)           // 如果处于休眠状态
    {
        // 休眠后清除数据，2024-5-23增加 
        __sys_chrg_time = 0;
        __sys_chrg_time_ms = 0;
        __sys_kwh = 0;
        __sys_delay_time = 0;
        __sys_delay_time_ms = 0;
        showDelay(__sys_delay_time);
        showTime(__sys_chrg_time);
        showKwh(__sys_kwh);
        
        changeStatus(ST_IDLE);
        CS_LOW();
        lcdOn();
        CS_HIGH();
        ledIdle();     
#if NFC_VERSION
        waitNfc();
#endif
    }
}

// 设置电流及过流保护电流
#if M2_11KW || M2_3KW
int __chrg_current_val[5] = {6, 8, 10, 13, 16};
int __chrg_current_over[5] = {2, 2, 2, 3, 4};
int __chrg_current = CHRG_16A;      //
int __chrg_current_max = CHRG_16A;
#elif M2_7KW || M2_22KW||M3_22KW
int __chrg_current_val[8] = {6, 8, 10, 13, 16, 20, 25, 32};
int __chrg_current_over[8] = {2, 2, 2, 3, 4, 4, 4, 5};
int __chrg_current = CHRG_32A;      // 
int __chrg_current_max = CHRG_32A;
#endif

int __langUage = 0;
int flg_setting_page = 0;
int __flg_ty_gnd_update = 0;

int __flg_factory_reset = 0;            // 恢复出厂设置标志位

int __charge_mode       = 0;            // 充电模式，0-立即充电，1-延时充电，2-计划充电
int __flg_schedule_set  = 0;            // 是否设置了定时充电
int __t_sch_s_hour      = 0;            // 开始时间 - hour
int __t_sch_s_min    = 0;               // 开始时间 - minute         
int __t_sch_e_hour      = 0;            // 结束时间 - hour
int __t_sch_e_min    = 0;               // 结束时间 - minute
int __t_sch_delay       = 0;            // 延迟时间

// 工作参数
int __sys_voltage   = 0;
float __sys_current1 = 0;
int __sys_temp      = 0;
int __sys_temp_wall = 0;
float __sys_voltage_jz = 1.0;
float __sys_current_jz = 1.0;

#if M2_11KW || M2_22KW||M3_22KW
float __sys_current2 = 0;
float __sys_current3 = 0;
float __sys_current_jz2 = 1.0;
float __sys_current_jz3 = 1.0;
#endif

int __sys_gnd = 1;
float __sys_kwh = 0;

#if M2_11KW||M2_22KW||M2_7KW||M3_22KW
int __sys_wall_temp         = 0;            // 墙插温度设置，0，关闭，1，打开
#elif M2_3KW
int __sys_wall_temp         = 1;            // 墙插温度设置，0，关闭，1，打开
#endif

int __sys_gnd_option        = 0;            // 0: 默认选型，检测并提示， 1: 直接充电， 2：不充电
int __sys_gnd_option_temp   = 0;            // 接地选项选项
long __sys_delay_time       = 0;            // 延迟充电时间，8小时内
long __sys_delay_time_ms    = 0;            // 延时充电时间，单位ms
unsigned long __sys_delay_time_lastime = 0; // 控制延时倒计时进入时间
long __sys_chrg_time        = 0;            // 充电时间
unsigned long __sys_chrg_time_lastime = 0;  // 统计充电时间计数器

int flg_temp_protect        = 0;            // 触发过温降流保护标志位

int flg_gnd_protect_stop    = 0;            // 接地停止充电界面
int flg_lb_happen           = 0;            // 漏电发生标志位

// 工作状态
int __work_status  = ST_IDLE;
#if TY_VERSION
int __work_ty_st = X_WS_WAIT_CONNECT;
unsigned long timer_current_set_ty = 0;     // 调节电流的时刻
#endif
int __car_status   = CAR_NO_INSERT;          // 检查车的插入情况

unsigned long timer_sys_start = 0;          // 系统开始时间
int flg_init_ok = 0;                    // 初始化完成标志位
unsigned long timer_btn_last;        // 按键后时间

int i_static1      = 0;
int i_static2      = 0;
int i_static3      = 0;

#if ENABLE_VOLTAGE_CALIBRATION

int voltage_cali_ok = 0;            // 是否完成校准

float __volA = 0.0;
float __volB = 0.0;
float __volC = 0.0;

// 定义函数 makeABC 来计算并存储二次拟合的系数 a, b, c
void makeVolCaliABC()
{
    if((eeprom_read(EEP_IF_V218)==0x55) && (eeprom_read(EEP_IF_VHIGH)==0x55) && (eeprom_read(EEP_IF_VLOW)==0x55))
    {
        int v_low = loadFloatFromEEPROM(EEP_VLOW);
        int v_218 = loadFloatFromEEPROM(EEP_V218);
        int v_high = loadFloatFromEEPROM(EEP_VHIGH);
            
        float x1 = v_low, y1 = JZ_V_LOW;
        float x2 = v_218, y2 = 218.0;
        float x3 = v_high, y3 = JZ_V_HIGH;

        // 计算二次多项式的系数：y = a*x^2 + b*x + c
        float denominator = (x1 - x2)*(x1 - x3)*(x2 - x3);
        __volA = (x3*(y2 - y1) + x2*(y1 - y3) + x1*(y3 - y2)) / denominator;
        __volB = (x3*x3*(y1 - y2) + x2*x2*(y3 - y1) + x1*x1*(y2 - y3)) / denominator;
        __volC = (x2*x3*(x2 - x3)*y1 + x3*x1*(x3 - x1)*y2 + x1*x2*(x1 - x2)*y3) / denominator;
    
        voltage_cali_ok = 1;
    }
    else voltage_cali_ok = 0;
}

// 定义函数 makeVolCorrect 来根据输入电压计算拟合电压值
int makeVolCorrect(int v_input) {
    
    if(voltage_cali_ok == 0)return v_input;     // 如果没有完成校准，返回原来的值
    // 使用全局变量 __volA, __volB 和 __volC 进行二次拟合计算
    float result = __volA * (float)v_input * (float)v_input + __volB * (float)v_input + __volC;
    
    // 返回拟合后的电压值，转换为整数
    return (int)(result+0.5);
  
}
#endif


// 更新已充电量
void updatekwh()
{
    if(__work_status != ST_WORKING)return;
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 10000)return;
    timer_s = millis();
#if M2_3KW || M2_7KW
    __sys_kwh += __sys_current1*(float)__sys_voltage/1000.0*10.0/3600.0;
#elif M2_11KW || M2_22KW || M3_22KW
    __sys_kwh += (__sys_current1 + __sys_current2 + __sys_current3)*(float)__sys_voltage/1000.0*10.0/3600.0;
#endif
}

// 倒计时
void delayCountDown()
{
    if(__work_status != ST_WAIT)return;           // 插入车开始倒计时

    if(__sys_delay_time == 0)return;

    if(millis()-__sys_delay_time_lastime < 100)return;
    __sys_delay_time_ms -= (millis()-__sys_delay_time_lastime);
    
    __sys_delay_time_lastime = millis();
    
    if(__sys_delay_time != 0)
    {
        __sys_delay_time = __sys_delay_time_ms/1000; 
        if(__sys_delay_time<0)__sys_delay_time = 0;
        
        showDelay(__sys_delay_time);

        if(__sys_delay_time == 0)
        {
            __charge_mode = 0;
            __t_sch_delay = 0;
            pushChrgMode();
            changeStatus(ST_IDLE);             // 倒计时到0，设置为闲置状态
        }
    }
}

unsigned long __sys_chrg_time_ms = 0;

void chrgTimeAdd()
{
    if(__work_status != ST_WORKING)return;
    if(__sys_current1 < 1.5)return;          // 小于1.5A返回

    if(millis()-__sys_chrg_time_lastime < 1000)return;
    unsigned long dt = millis()-__sys_chrg_time_lastime;

    __sys_chrg_time_lastime = millis();
    __sys_chrg_time_ms += dt < 1500 ? dt : 1000;
    __sys_chrg_time = __sys_chrg_time_ms/1000;
    
    __sys_chrg_time++;
    if(__sys_chrg_time > 356000)
    {
        __sys_chrg_time = 0;
        __sys_chrg_time_ms = 0;
    }
}

void dispLang()
{
START:
    clearDisp();
    int y_start = 45;
    pushBuf(12, 12, 216, 19, setting_4_0, sizeof(setting_4_0));
    pushBuf(24, y_start, 192, 228, setting_4_1, sizeof(setting_4_1));
    
    dispBuf(0, 275, WHITE, BLACK, 4);


    __flg_ty_lang_update = 0;
    int __xStart = 92;

    int __select = __langUage;

    decompressMonochromePixels(setting_4_3, sizeof(setting_4_3), str_disp, &out_len);
    
    if(__select == 0)
    {
        showImage(__xStart, y_start+118-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
        showImage(__xStart-16, y_start+118+3, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    else if(__select == 1)
    {
        showImage(__xStart, y_start+141-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
        showImage(__xStart-16, y_start+141+3, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    else if(__select == 2)
    {
        showImage(__xStart, y_start+164-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
        showImage(__xStart-16, y_start+164+3, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    else if(__select == 3)
    {
        showImage(__xStart, y_start+186-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
        showImage(__xStart-16, y_start+186+3, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    else if(__select == 4)
    {
        showImage(__xStart, y_start+210-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
        showImage(__xStart-16, y_start+210+3, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    
    while(1)
    {
        if(__flg_ty_lang_update)goto START;
        feedog();
        updateSleepTimer();
        if(getBtnA())
        {
            delay(10);
            if(getBtnA())
            {
                buzOn();
                
                decompressMonochromePixels(setting_4_3, sizeof(setting_4_3), str_disp, &out_len);
                
                switch(__select)
                {
                    case 0:
                    showImage(__xStart, y_start+118-2, 80, 21, BLUE1, BLACK, &str_disp[230*__select]);
                    __select = 1;
                    showImage(__xStart, y_start+141-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
                    
                    break;
                    
                    case 1:

                    showImage(__xStart, y_start+141-2, 80, 21, BLUE1, BLACK, &str_disp[230*__select]);
                    __select = 2;
                    showImage(__xStart, y_start+164-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);

                    break;
                    
                    case 2:
                    showImage(__xStart, y_start+164-2, 80, 21, BLUE1, BLACK, &str_disp[230*__select]);
                    __select = 3;
                    showImage(__xStart, y_start+186-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
                    break;
                    
                    case 3:
                    showImage(__xStart, y_start+186-2, 80, 21, BLUE1, BLACK, &str_disp[230*__select]);
                    __select = 4;
                    showImage(__xStart, y_start+210-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
                    break;
                    
                    case 4:
                    showImage(__xStart, y_start+210-2, 80, 21, BLUE1, BLACK, &str_disp[230*__select]);
                    __select = 0;
                    showImage(__xStart, y_start+118-2, 80, 21, BLUE1, COLOR_S, &str_disp[230*__select]);
                    break;
                }
                while(getBtnA()){feedog();}
            }
        }
        
        if(getBtnC())       // 确认
        {
            delay(10);
            if(getBtnC())
            {
                buzOn();
                __langUage = __select;
                pushLang(__langUage);
                eeprom_write(EEP_LANG, __langUage);
                while(getBtnC()){feedog();}
                return;
            }
        }
    }
}

void dispSetGnd()
{
    
START:
    clearDisp();
    pushBuf(32, 14, 184, 15, setting_2_0, sizeof(setting_2_0));
    pushBuf(16, 69, 216, 182, setting_2_1, sizeof(setting_2_1));
    
    dispBuf(0, 275, WHITE, BLACK, 3);

    __flg_ty_gnd_update = 0;
    int __xStart = 37;

    int __select = __sys_gnd_option;

    decompressMonochromePixels(setting_2_3, sizeof(setting_2_3), str_disp, &out_len);

    if(__select == 0)
    {
        showImage(__xStart, 168-1, 168, 21, BLUE1, COLOR_S, &str_disp[441*__select]);
        showImage(__xStart-16, 168+4, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    else if(__select == 1)
    {
        showImage(__xStart, 200-1, 168, 21, BLUE1, COLOR_S, &str_disp[441*__select]);
        showImage(__xStart-16, 200+4, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
   /* else if(__select == 2)
    {
        showImage(__xStart, 232-1, 168, 21, BLUE1, COLOR_S, &str_disp[441*__select]);
        showImage(__xStart-16, 232+4, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }*/
    
    while(1)
    {
        feedog();
        updateSleepTimer();
        if(__flg_ty_gnd_update)goto START;
        
        if(getBtnA())
        {
            delay(10);
            if(getBtnA())
            {
                buzOn();
                decompressMonochromePixels(setting_2_3, sizeof(setting_2_3), str_disp, &out_len);
                switch(__select)
                {
                    case 0:
                    showImage(__xStart, 168-1, 168, 21, BLUE1, BLACK, &str_disp[441*__select]);
                    __select = 1;
                    showImage(__xStart, 200-1, 168, 21, BLUE1, COLOR_S, &str_disp[441*__select]);
                    
                    break;
                    
                    case 1:

                    showImage(__xStart, 200-1, 168, 21, BLUE1, BLACK, &str_disp[441*__select]);
                    __select = 0;
                    showImage(__xStart, 168-1, 168, 21, BLUE1, COLOR_S, &str_disp[441*__select]);

                    break;
                }
                while(getBtnA()){feedog();}
            }
        }
        
        if(getBtnC())       // 确认
        {
            delay(10);
            if(getBtnC())
            {
                buzOn();
                __sys_gnd_option = __select;
                __sys_gnd_option_temp = __sys_gnd_option;
                eeprom_write(EEP_GND_OPTION, __sys_gnd_option);
                pushGndOption(__sys_gnd_option);
                while(getBtnC()){feedog();}
                return;
            }
        }
    }
}

void dispSetMaxCurrent()
{
    
START:
    clearDisp();
    pushBuf(16, 14, 208, 19, setting_1_0, sizeof(setting_1_0));
    pushBuf(24, 66, 200, 182, setting_1_1, sizeof(setting_1_1));
    
    dispBuf(0, 275, WHITE, BLACK, 1);
    __flg_ty_max_update = 0;
// 3kw和11kw需要把20,25,32A设置成灰色
#if M2_3KW||M2_11KW

    decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);

    showImage(41-2, 169-2, 40, 15+4, GRAY2, BLACK, &str_disp[95*7]);
    showImage(113-2, 169-2, 40, 15+4, GRAY2, BLACK, &str_disp[95*6]);
    showImage(185-2, 169-2, 40, 15+4, GRAY2, BLACK, &str_disp[95*5]);
#endif
    int __select = 7-__chrg_current_max;

    if(__select == 0)       // 32A
    {
        showImage(42-16, 169+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(42-2, 169-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[7*95]); 
    }
    else if(__select == 1)  // 25A
    {
        showImage(114-16, 169+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(114-2, 169-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[6*95]);  
    }
    else if(__select == 2)  // 20A
    {
        showImage(186-16, 169+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(186-2, 169-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[5*95]);
    }
    else if(__select == 3)       // 16A
    {
        showImage(42-16, 201+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(42-2, 201-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[4*95]); 
    }
    else if(__select == 4)  // 13A
    {
        showImage(114-16, 201+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(114-2, 201-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[3*95]);  
    }
    else if(__select == 5)  // 10A
    {
        showImage(186-16, 201+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(186-2, 201-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[2*95]);
    }
    else if(__select == 6)  // 8A
    {
        showImage(42-16, 233+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(42-2, 233, 40, 15+4, BLUE1, COLOR_S, &str_disp[1*95]); 
    }
    else if(__select == 7)  // 6A
    {
        showImage(114-16, 233+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
        decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
        showImage(114-2, 233, 40, 15+4, BLUE1, COLOR_S, &str_disp[0*95]);   
    }

    while(1)
    {
        if(__flg_ty_max_update)goto START;
        feedog();
        updateSleepTimer();
        if(getBtnA())
        {
            delay(10);
            if(getBtnA())
            {
                buzOn();
                
                decompressMonochromePixels(setting_1_5, sizeof(setting_1_5), str_disp, &out_len);
                switch(__select)
                {
                    case 0:
                    showImage(42-2, 169-2, 40, 15+4, BLUE1, BLACK, &str_disp[7*95]);
                    __select = 1;
                    showImage(114-2, 169-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[6*95]);

                    break;
                    
                    case 1:
                    showImage(114-2, 169-2, 40, 15+4, BLUE1, BLACK, &str_disp[6*95]);
                    __select = 2;
                    showImage(186-2, 169-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[5*95]);

                    break;
                    
                    case 2:
                    showImage(186-2, 169-2, 40, 15+4, BLUE1, BLACK, &str_disp[5*95]);
                    __select = 3;
                    showImage(42-2, 201-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[4*95]);

                    break;
                    
                    // 16A
                    case 3:
                    showImage(42-2, 201-2, 40, 15+4, BLUE1, BLACK, &str_disp[4*95]);       // 16变黑
                    __select = 4;
                    showImage(114-2, 201-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[3*95]);    // 13变白
                    
                    break;
                    
                    // 13A
                    case 4:
                    showImage(114-2, 201-2, 40, 15+4, BLUE1, BLACK, &str_disp[3*95]);      // 13黑
                    __select = 5;   
                    showImage(186-2, 201-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[2*95]);    // 10白

                    break;
                    
                    // 10A
                    case 5:
                    showImage(186-2, 201-2, 40, 15+4, BLUE1, BLACK, &str_disp[2*95]);      // 10黑
                    __select = 6;
                    showImage(42-2, 233, 40, 15+4, BLUE1, COLOR_S, &str_disp[1*95]);       // 8白

                    break;
                    
                    // 8A
                    case 6:
                    showImage(42-2, 233, 40, 15+4, BLUE1, BLACK, &str_disp[1*95]);         // 8黑
                    __select = 7;
                    showImage(114-2, 233, 40, 15+4, BLUE1, COLOR_S, &str_disp[0*95]);      // 6白

                    break;
                    
                    // 6A
                    case 7:
                    showImage(114-2, 233, 40, 15+4, BLUE1, BLACK, &str_disp[0*95]);        // 6黑， 16白
#if M2_7KW||M2_22KW||M3_22KW
                    __select = 0;
                    showImage(42-2, 169-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[7*95]);
#elif M2_3KW||M2_11KW
                    __select = 3;
                    showImage(42-2, 201-2, 40, 15+4, BLUE1, COLOR_S, &str_disp[4*95]);    
#endif
                    break;
                }
                while(getBtnA()){feedog();}
            }
        }
        
        if(getBtnC())       // 确认
        {
            delay(10);
            if(getBtnC())
            {
                buzOn();
                __chrg_current_max = 7-__select;
                eeprom_write(EEP_MAX_CURRENT, __chrg_current_max);
                
                if(__chrg_current > __chrg_current_max)         // 设置后，默认电流大于最大电流
                {
                    __chrg_current = __chrg_current_max;
                    eeprom_write(EEP_DEF_CURRENT, __chrg_current);
                }
                
                pushMaxCur(__chrg_current_max);
                
                while(getBtnC()){feedog();}
                return;
            }
        }
    }
}

void dispSetWallTemp()
{
    clearDisp();
    pushBuf(16, 14, 208, 15, setting_3_0, sizeof(setting_3_0));
    pushBuf(14, 88, 216, 74, setting_3_1, sizeof(setting_3_1));      // 134
    
    dispBuf(0, 275, WHITE, BLACK, 2);

    int __select = 1-__sys_wall_temp;
    
    if(__select == 0)    // ON
    {
        decompressMonochromePixels(setting_3_3, sizeof(setting_3_3), str_disp, &out_len);
        showImage(106, 182-1, 32, 17, BLUE1, COLOR_S, &str_disp[0]);
        showImage(106, 209-1, 32, 17, BLUE1, BLACK, &str_disp[68]);
        showImage(106-16, 182+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    else
    {
        decompressMonochromePixels(setting_3_3, sizeof(setting_3_3), str_disp, &out_len);
        showImage(106, 182-1, 32, 17, BLUE1, BLACK, &str_disp[0]);
        showImage(106, 209-1, 32, 17, BLUE1, COLOR_S, &str_disp[68]); 
        showImage(106-16, 209+2, 16, 11, BLUE1, BLACK, setting_arrow, sizeof(setting_arrow));
    }
    
    while(1)
    {
        feedog();
        updateSleepTimer();
        if(getBtnA())
        {
            delay(10);
            if(getBtnA())
            {
                buzOn();
                
                decompressMonochromePixels(setting_3_3, sizeof(setting_3_3), str_disp, &out_len);
                switch(__select)
                {
                    case 0:
                    showImage(106, 182-1, 32, 17, BLUE1, BLACK, &str_disp[0]);
                    showImage(106, 209-1, 32, 17, BLUE1, COLOR_S, &str_disp[68]); 
                    __select = 1;
                    break;
                    
                    case 1:
                    showImage(106, 182-1, 32, 17, BLUE1, COLOR_S, &str_disp[0]);
                    showImage(106, 209-1, 32, 17, BLUE1, BLACK, &str_disp[68]); 
                    __select = 0;
                    
                    break;
                }
                while(getBtnA()){feedog();}
            }
        }
        
        if(getBtnC())       // 确认
        {
            delay(10);
            if(getBtnC())
            {
                buzOn();
                __sys_wall_temp = 1-__select;
                eeprom_write(EEP_WALL_TEMP, __sys_wall_temp);
                while(getBtnC()){feedog();}
                return;
            }
        }
    }
}

int __flg_ty_lang_update = 0;           // 语言有更新
int __flg_ty_max_update  = 0;           // 最大电流有更新

void dispSetting()
{
    int __select = 0;
    flg_gnd_protect_stop = 0;           // 清空接地保护停止状态标志位
    flg_setting_page = 1;
START:
    CS_LOW();
    clearDisp();
    __flg_ty_gnd_update = 0;
    __flg_ty_max_update = 0;
    __flg_ty_lang_update = 0;
    
    pushBuf(16, 9, 112, 25, setting_0_0, sizeof(setting_0_0));
    decompressMonochromePixels(setting_0_1, sizeof(setting_0_1), str_disp, &out_len);
    
    pushBuf(0, 57, 240, 21, &str_disp[630*0]);
    pushBuf(0, 104, 240, 21, &str_disp[630*1]);
    pushBuf(0, 152, 240, 21, &str_disp[630*2]);
    pushBuf(0, 201, 240, 21, &str_disp[630*3]);
    pushBuf(0, 251, 240, 21, &str_disp[630*4]);
    pushBuf(24, 284, 200, 30, setting_0, sizeof(setting_0));
    
    // 显示设置内容
#if TRAVEL_AUTO_CHECK
    if(flg_travel)
    {
        decompressMonochromePixels(setting_1_4, sizeof(setting_1_4), str_disp, &out_len);
        pushBuf(32, 82, 56, 15, &str_disp[__chrg_current_max*105]);
    }
    else
    {
        decompressMonochromePixels(setting_1_4, sizeof(setting_1_4), str_disp, &out_len);
        pushBuf(32, 82, 56, 15, &str_disp[__chrg_current_max*105]);
    }
#else
    decompressMonochromePixels(setting_1_4, sizeof(setting_1_4), str_disp, &out_len);
    pushBuf(32, 82, 56, 15, &str_disp[__chrg_current_max*105]);
#endif
    decompressMonochromePixels(setting_2_2, sizeof(setting_2_2), str_disp, &out_len);
    pushBuf(32, 177, 184, 19, &str_disp[__sys_gnd_option*437]);
#if M2_3KW
    decompressMonochromePixels(setting_3_2, sizeof(setting_3_2), str_disp, &out_len);
    pushBuf(32, 129, 48, 15, &str_disp[(1-__sys_wall_temp)*90]);
#else
    decompressMonochromePixels(setting_3_2, sizeof(setting_3_2), str_disp, &out_len);
    pushBuf(32, 129, 48, 15, &str_disp[(1-1)*90]);
#endif
    decompressMonochromePixels(setting_4_2, sizeof(setting_4_2), str_disp, &out_len);
    pushBuf(32, 227, 104, 20, &str_disp[__langUage*260]);
    
    // 让屏幕显示内容
    dispBuf(0, 320, WHITE, BLACK, 0);
    
    decompressMonochromePixels(setting_0_1, sizeof(setting_0_1), str_disp, &out_len);

#if TRAVEL_AUTO_CHECK
    if(flg_travel)__select = 2;
#endif
    if(__select == 0)showImage(0, 58-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
    else if(__select == 1)showImage(0, 105-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
    else if(__select == 2)showImage(0, 153-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
    else if(__select == 3)showImage(0, 202-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
    else if(__select == 4)showImage(0, 252-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
    
    showVersion();
    while(1)
    {
        if(__flg_ty_gnd_update || __flg_ty_lang_update || __flg_ty_max_update)goto START;
        
        updateSleepTimer();
        feedog();
        if(getBtnA())
        {
            delay(10);
            if(getBtnA())
            {
                buzOn();
                
                decompressMonochromePixels(setting_0_1, sizeof(setting_0_1), str_disp, &out_len);
                
                if(__select == 0)
                {
                    showImage(0, 58-1, 240, 21, BLUE1, BLACK, &str_disp[630*__select]);
                    #if M2_3KW
                    __select = 1;
                    showImage(0, 105-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
                    #else
                    __select = 2;
                    showImage(0, 153-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
                    #endif
  
                }
                else if(__select == 1)
                {
                    showImage(0, 105-1, 240, 21, BLUE1, BLACK, &str_disp[630*__select]);
                    __select = 2;
                    showImage(0, 153-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
                }
                else if(__select == 2)
                {
                    showImage(0, 153-1, 240, 21, BLUE1, BLACK, &str_disp[630*__select]);
                    __select = 3;
                    showImage(0, 202-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
                }
                else if(__select == 3)
                {
                    showImage(0, 202-1, 240, 21, BLUE1, BLACK, &str_disp[630*__select]);
                    __select = 4;
                    showImage(0, 252-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
                }
                else if(__select == 4)
                {
                    showImage(0, 252-1, 240, 21, BLUE1, BLACK, &str_disp[630*__select]);
                    
#if TRAVEL_AUTO_CHECK
                    if(flg_travel)
                    {
                        __select = 2;
                        showImage(0, 153-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
                    }
                    else
                    {
                        __select = 0;
                        showImage(0, 58-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
                    }
#else
                    __select = 0;
                    showImage(0, 58-1, 240, 21, BLUE1, COLOR_S, &str_disp[630*__select]);
#endif
                }
            }
            while(getBtnA()){feedog();}
        }
        
        if(getBtnC())
        {
            delay(10);
            if(getBtnC())
            {
                buzOn();
                if(__select == 0)dispSetMaxCurrent();
                else if(__select == 1)dispSetWallTemp();
                else if(__select == 2)dispSetGnd();
                else if(__select == 3)dispLang();
                else if(__select == 4)
                {
                    clearShowBuf();
                    showImage();
                    if(__sys_gnd)showGndOk();
                    else showGndFail();
                    showCarOff();
                    taskDisplay(0);
                    LCD_DrawLine(0, 235, 239, 235, GRAY);
                    LCD_DrawLine(132, 235, 132, 319, GRAY);
                    flg_setting_page = 0;
                    flg_wifi_show = -1;
                    return ;
                }
                __select = 4;
                goto START;
            }
        }
        
        tyProcess();
    }
    CS_HIGH();
}

void makeIstatic()
{
    unsigned long sum1 = 0;

#if M2_11KW||M2_22KW||M3_22KW
    unsigned long sum2 = 0;
    unsigned long sum3 = 0;
#endif
    int cnt = 0;
    
    unsigned long timer_t = millis();
    
    if(millis()-timer_t < 100)
    {
        sum1 += analogRead(adcI);
#if M2_11KW||M2_22KW||M3_22KW
        sum2 += analogRead(adcI2);
        sum3 += analogRead(adcI3);
#endif
        cnt++;
    }
    
    i_static1 = sum1/cnt;
#if M2_11KW||M2_22KW||M3_22KW
    i_static2 = sum2/cnt;
    i_static3 = sum3/cnt;
#endif
}

// 初始化EEPROM的数据
void EEP_INIT()
{
    if(eeprom_read(EEP_FIRST_RUN) != 0x55)          // 第一次开机，需要做一些初始化
    {
        eeprom_write(EEP_FIRST_RUN, 0x55);
        __sys_gnd_option = 0;
        eeprom_write(EEP_GND_OPTION, __sys_gnd_option);
        
        __langUage = 0;     // 默认英语
        eeprom_write(EEP_LANG, __langUage);
        
        __sys_gnd_option_temp = __sys_gnd_option;

#if M2_3KW||M2_7KW
        __sys_wall_temp = 1;
#else
        __sys_wall_temp = 1;
#endif
        eeprom_write(EEP_WALL_TEMP, __sys_wall_temp);

#if TRAVEL_AUTO_CHECK
        if(flg_travel)
        {
            __chrg_current_max = getTravelCurrent();
            __chrg_current = __chrg_current_max;
        }
        else
        {
            __chrg_current = DEFAULT_CURRENT;
            eeprom_write(EEP_DEF_CURRENT, __chrg_current);
            __chrg_current_max = DEFAULT_CURRENT;
            eeprom_write(EEP_MAX_CURRENT, __chrg_current_max);
        }
#else
        __chrg_current = DEFAULT_CURRENT;
        eeprom_write(EEP_DEF_CURRENT, __chrg_current);
        __chrg_current_max = DEFAULT_CURRENT;
        eeprom_write(EEP_MAX_CURRENT, __chrg_current_max);
#endif

#if TY_VERSION
        __charge_mode       = 0;
        __flg_schedule_set  = 0;
        __t_sch_s_hour      = 0;
        __t_sch_s_min       = 0;
        __t_sch_e_hour      = 8;
        __t_sch_e_min       = 0;
        __t_sch_delay       = 0;
        
        eeprom_write(EEP_SCH_SET, 0);       // 默认没有计划充电
        eeprom_write(EEP_SCH_SH, 0);        // 默认0:00~8:00充电
        eeprom_write(EEP_SCH_SM, 0);
        eeprom_write(EEP_SCH_EH, 8);
        eeprom_write(EEP_SCH_EM, 0);
        
        flg_ty_en = 0;
        eeprom_write(EEP_TY_EN, flg_ty_en);
#endif
    }
    else
    {
#if TY_VERSION
        flg_ty_en = eeprom_read(EEP_TY_EN);
#endif
        __flg_schedule_set = eeprom_read(EEP_SCH_SET) ;
        if(__flg_schedule_set)__charge_mode = 2;        // 计划充电模式
        else __charge_mode = 0;
        
        __t_sch_s_hour   = eeprom_read(EEP_SCH_SH);
        __t_sch_s_min = eeprom_read(EEP_SCH_SM);
        __t_sch_e_hour   = eeprom_read(EEP_SCH_EH);
        __t_sch_e_min = eeprom_read(EEP_SCH_EM);
        
        __sys_gnd_option = eeprom_read(EEP_GND_OPTION);

        if(__sys_gnd_option > 2)
        {
            __sys_gnd_option = 0;
            eeprom_write(EEP_GND_OPTION, __sys_gnd_option);
        }
        
        __sys_gnd_option_temp = __sys_gnd_option;
        
        __sys_wall_temp = eeprom_read(EEP_WALL_TEMP);
        
        if(__sys_wall_temp > 1)
        {
            __sys_wall_temp = 1;
            eeprom_write(EEP_WALL_TEMP, __sys_wall_temp);
        }
        
        __langUage = eeprom_read(EEP_LANG);

        __chrg_current = eeprom_read(EEP_DEF_CURRENT);

#if TRAVEL_AUTO_CHECK
        if(flg_travel)
        {
            __chrg_current_max = getTravelCurrent();
            __chrg_current = __chrg_current_max;
        }
        else
        {
            __chrg_current_max = eeprom_read(EEP_MAX_CURRENT);
        }
#else
        __chrg_current_max = eeprom_read(EEP_MAX_CURRENT);
#endif

        if(__chrg_current > DEFAULT_CURRENT || __chrg_current_max > DEFAULT_CURRENT)           // 如果超过16A，强制设置为16A
        {
            __chrg_current = DEFAULT_CURRENT;
            eeprom_write(EEP_DEF_CURRENT, __chrg_current);
            __chrg_current_max = DEFAULT_CURRENT;
            eeprom_write(EEP_MAX_CURRENT, __chrg_current_max);
        }
    }
}

void setup()
{
#if TY_VERSION
    Serial.begin(115200);
    // 涂鸦初始化，如果有的话
#endif
    pinInit();
#if L2_CHECK
    flg_phase_3 = L2Check();
#endif

    delay(100);
    
    eeprom_init();
    
#if ENABLE_VOLTAGE_CALIBRATION
    makeVolCaliABC();
#endif
    ledIdle();
    relayOff();
    delay(10);

#if TRAVEL_AUTO_CHECK
    // 检查是否Travel版本
    flg_travel = getTravelCurrent() == 100 ? 0 : 1;
#endif
    EEP_INIT();
    
    tyInit();
    
    checkSensor(0);
    lcdStart();
    makePwm(100);
    makeIstatic();          // 获取电流原始值
    delayFeed(500);
    rcdSelfCheck();         // 漏保自检
    checkGND();             // 检查接地状态
    relayErrorCheck();      // 继电器黏连检测
    protectGndIdle();

    pushSelfCheck(1, 1);
    // 提取校准数据
#if ENABLE_VOLTAGE_CALIBRATION

#else
    if(eeprom_read(EEP_IF_VJZ) == 0x55)            // 是否校准电压
    {
        __sys_voltage_jz = loadFloatFromEEPROM(EEP_V_JZ);
        if(__sys_voltage_jz > 1.3 || __sys_voltage_jz < 0.7)__sys_voltage_jz = 1.0;
    }
#endif
    if(eeprom_read(EEP_IF_AJZ) == 0x55)         // 是否校准电流
    {
        __sys_current_jz = loadFloatFromEEPROM(EEP_A_JZ);
#if M2_11KW||M2_22KW||M3_22KW
        __sys_current_jz2 = loadFloatFromEEPROM(EEP_A2_JZ);
        __sys_current_jz3 = loadFloatFromEEPROM(EEP_A3_JZ);
#endif
        if(__sys_current_jz > 2.0 || __sys_current_jz < 0.5)__sys_current_jz = 1.0;
#if M2_11KW||M2_22KW||M3_22KW
        if(__sys_current_jz2 > 2.0 || __sys_current_jz2 < 0.5)__sys_current_jz2 = 1.0;
        if(__sys_current_jz3 > 2.0 || __sys_current_jz3 < 0.5)__sys_current_jz3 = 1.0;
#endif
    }
    
#if WATCH_DOG_EN
    IWatchdog.begin(3000000);       // 看门狗，单位us
#endif

    // 如果NFC初始化没过，响2下，持续30s
#if NFC_VERSION
    while (!nfc.begin())
    {
        buzOn();
        delayFeed(50);
        buzOn();
        delayFeed(1000);
    }
    
    waitNfc();
    // 等待NFC刷卡
    
#endif
    timer_sys_start = millis();     // 记录初始化完成时间
    flg_init_ok = 1;
    buzOn();
}

// 状态机
void stMachine()
{
    // 开机1s内不执行
    if(flg_lb_happen)return;
    if(flg_init_ok && (millis()-timer_sys_start < 2100))return;
    if((millis()-timer_btn_last < 2000) && flg_init_ok)return;
    
    // 当处于充电停止充电状态，不再检测
    if(flg_gnd_protect_stop)return;
    
    if(!checkInsert())return;
    
    if(millis()-timer_sleep > SLEEP_TIME)                // 休眠
    {
        changeStatus(ST_SLEEP);
        CS_LOW();
        lcdOff();
        CS_HIGH();
        ledOff();
    }
    
    // 没有插入
    if(CAR_NO_INSERT == __car_status)           // 任何状态到车没有插入状态，切断pwm
    {
        if(__work_status == ST_ERRORPAUSE || flg_pov || flg_lov || flg_gnd_p || flg_oc || flg_pot || flg_pot_wall)           // 如果处于暂停的状态
        {
            flg_pov = 0;
            flg_lov = 0;
            flg_gnd_p = 0;
            flg_oc  = 0;
            cnt_oc  = 0;
            flg_pot = 0;
            flg_pot_wall = 0;
            showNormal(0);
            showNormal(3);
        }
        
        // 没有处于休眠的时候
        if(__work_status != ST_SLEEP)
        {
            
#if TY_VERSION
            if((__work_status != ST_IDLE) && (__work_status != ST_WAIT))
            {
                pushRecord();
            }
#endif
            changeStatus(ST_IDLE);

            relayOff();
            makePwm(100);
            ledIdle();
            showCarOff();
        }
        
        delay(10);
        pushWorkState(X_WS_WAIT_CONNECT);
        showBattery(0);
    }

    // 插入
    if(CAR_INSERT == __car_status)
    {
        updateSleepTimer();
        showCarOn();
        showBattery(0);
        if(__sys_delay_time)            // 倒计时没有清零
        {
            // 更新延时倒计时计数器
            if(__work_status != ST_WAIT)
            {
                __sys_delay_time_lastime = millis();
            }
            
            relayOff();
            ledInsert();
            makePwm(100);
            pushWorkState(X_WS_WAIT_DELAY);
            delay(10);
            changeStatus(ST_WAIT);       // 等待
            showBattery(0);
        }
#if TY_VERSION
        else if(isWaitSchedule())            // 是否设置了预约，且不在预约范围
        {
            relayOff();
            ledInsert();
            makePwm(100);
            showBattery(0);
            // 如果上一个状态是充电中，切换到complete, 否则切换到等待
            if(__work_ty_st == X_WS_COMPLETE)
            {
                pushWorkState(X_WS_COMPLETE);
            }
            else if(__work_ty_st == X_WS_WAIT_SCHEDULE)
            {
                pushWorkState(X_WS_WAIT_SCHEDULE);
            }
            else if(millis()-timer_get_chrgmode < 1000)          // 刚收到
            {
                pushWorkState(X_WS_WAIT_SCHEDULE);
            }
            else if((millis()-timer_relay_off < 1000))          // 如果刚闭合继电器不久
            {
                pushWorkState(X_WS_COMPLETE);
            }
            else
            {
                pushWorkState(X_WS_WAIT_SCHEDULE);
            }
            delay(10);
            changeStatus(ST_WAIT);          // 等待
        }
#endif
        else if((__work_status != ST_ERRORPAUSE) && (__work_status != ST_PAUSE))
        {
            
#if TY_VERSION
            if((__work_status == ST_IDLE || __work_status == ST_SLEEP) && (millis()-timer_current_set_ty > 5000))
            {
                __sys_chrg_time = 0;
                __sys_chrg_time_ms = 0;
                showTime(__sys_chrg_time);
                __sys_kwh = 0;
                showKwh(__sys_kwh);

                if(flgGetTimeOk)
                {
                    start_time = local_time;
                }
            }

            // 以防得出错误的开始时间
            if(start_time.year < 24 || start_time.year > 34)
            {
                if(flgGetTimeOk)
                {
                    start_time = local_time;
                }
            }
#endif
            if((__work_status == ST_IDLE) || (__work_status == ST_WORKING) || (__work_status == ST_WAIT))       // 如果处在闲置状态
            {
                changeStatus(ST_IDLEINS);         // 状态设置为插入
                makePwm(__chrg_current_val[__chrg_current]);            // 开启PWM
            }
            relayOff();
            ledInsert();
            delay(10);
            pushWorkState(X_WS_READY);
        }
    }

    // 准备好
    if(CAR_READY == __car_status)           // 任何状态进入ready状态
    {
        updateSleepTimer();
        showCarOn();
        
        // 预约充电
        if(__sys_delay_time)                // 倒计时没有清零
        {
            // 更新延时倒计时计数器
            if(__work_status != ST_WAIT)
            {
                __sys_delay_time_lastime = millis();
            }
            
            pushWorkState(X_WS_WAIT_DELAY);
            delay(10);
            changeStatus(ST_WAIT);          // 等待
            showBattery(0);
            relayOff();
            makePwm(100);
            ledInsert();                    // 蓝灯
        }
#if TY_VERSION
        else if(isWaitSchedule())            // 是否设置了预约，且不在预约范围
        {
            relayOff();
            ledInsert();
            makePwm(100);
            changeStatus(ST_WAIT);          // 等待
            showBattery(0);
            delayFeed(50);
            
            // 如果上一个状态是充电中，切换到complete, 否则切换到等待
            if(__work_ty_st == X_WS_COMPLETE)
            {
                pushWorkState(X_WS_COMPLETE);
                pushRecord();
            }
            else if(__work_ty_st == X_WS_WAIT_SCHEDULE)
            {
                pushWorkState(X_WS_WAIT_SCHEDULE);
            }
            else if(millis()-timer_get_chrgmode < 1000)          // 刚收到
            {
                pushWorkState(X_WS_WAIT_SCHEDULE);
            }
            else if((millis()-timer_relay_off < 1000))          // 如果刚闭合继电器不久
            {
                pushWorkState(X_WS_COMPLETE);
                pushRecord();
            }
            else
            {
                pushWorkState(X_WS_WAIT_SCHEDULE);
            }
        }
#endif
        else if((__work_status != ST_ERRORPAUSE) && (__work_status != ST_PAUSE))
        {
            makePwm(__chrg_current_val[__chrg_current]);            // 开启PWM
            delay(5);

            if((__work_status == ST_WORKING) || checkDiode())
            {
#if TY_VERSION
                if((__work_status == ST_IDLE || __work_status == ST_SLEEP) && (millis()-timer_current_set_ty > 5000))
                {
                    __sys_chrg_time = 0;
                    __sys_chrg_time_ms = 0;
                    showTime(__sys_chrg_time);
                    __sys_kwh = 0;
                    showKwh(__sys_kwh);

                    if(flgGetTimeOk)
                    {
                        start_time = local_time;
                    }
                }

                // 以防得出错误的开始时间
                if(start_time.year < 24 || start_time.year > 34)
                {
                    if(flgGetTimeOk)
                    {
                        start_time = local_time;
                    }
                }
#endif  
                pushWorkState(X_WS_WORKING); 
                delay(10);
                changeStatus(ST_WORKING);    // 状态设置为插入
                showCarOn();
                relayOn();                   // 开启继电器
                
            }
            else    // 二极管击穿(或者缺失)
            {
                pushWorkState(X_WS_DIODE);
                showCarOff();
                showErrorImg(ERROR_CODE_DIODE);
                makePwm(100);
                ledError();
                flg_nodiode = 1;
                while(1)
                {
                    feedog();
                    if(checkInsert())
                    {
                        if(CAR_NO_INSERT == __car_status)     // 直到车拔出
                        {
                            showNormal(3);
                            ledIdle();
                            changeStatus(ST_IDLE);
                            flg_nodiode = 0;
                            return;
                        }
                    }
                    checkSensor(100);
                    taskDisplay();
                    updateSleepTimer();
                    delay(10);
                }
            }
        }
    }

    if(CAR_ERROR == __car_status)               // CP信号错误
    {
        pushWorkState(X_WS_CP);
        delay(10);
        changeStatus(ST_ERRORPAUSE);
        relayOff();
        makePwm(100);
        ledError();
    }
}

// 按键处理函数
void btnProcessIdle()
{
    if(__work_status != ST_IDLE)return;

    if(getBtnA())
    {
        delay(5);
        unsigned long timer_sbtn = millis();
        if(getBtnA())
        {
            updateSleepTimer();
            timer_btn_last = millis();
            buzOn();
            unsigned long timer_s = millis();
            while(getBtnA())
            {
#if TY_VERSION
                if(millis()-timer_sbtn > 5000)          // 长按5s，配网
                {
                    if(!flg_ty_en)
                    {
                        flg_ty_en = 1;
                        eeprom_write(EEP_TY_EN, flg_ty_en);
                    }
                    buzOn();
                    tyConfig();
                    while(getBtnA())
                    {
                        delayFeed(10);
                    }
                    delayFeed(100);
                    return;
                }
#endif
                feedog();
                if(getBtnC())           // 如果D也按下，可能是想设置了
                {
                    timer_s = millis();
                    
                    int flg_both_press = 0;
                    while(getBtnA() && getBtnC())
                    {
                        feedog();
                        if(millis()-timer_s > 3000)         // 长按大于3s
                        {
                            buzOn();
                            delayFeed(100);
                            buzOn();
                            dispSetting();
                            timer_btn_last = millis();
                            return;
                        }
                        flg_both_press = 1;
                    }
                    if(flg_both_press)
                    {
                        timer_btn_last = millis();
                        return;
                    }
                }
            }
            
            if(!flg_error_page)
            {
                __chrg_current++;
                if(__chrg_current > __chrg_current_max)__chrg_current = CHRG_6A;

                eeprom_write(EEP_DEF_CURRENT, __chrg_current);
                showAset(__chrg_current_val[__chrg_current]);
                pushCurrentSet(__chrg_current_val[__chrg_current]);
            }
            while(getBtnA()){feedog();}
            timer_btn_last = millis();
        }
    }

    if(getBtnC())
    {
        delay(10);
        if(getBtnC())
        {
            updateSleepTimer();
            timer_btn_last = millis();
            buzOn();
            
            unsigned long timer_s = millis();
            while(getBtnC())
            {
                feedog();
                if(getBtnA())           // 如果D也按下，可能是想设置了
                {
                    timer_s = millis();
                    int flg_both_press = 0;
                    while(getBtnA() && getBtnC())
                    {
                        feedog();
                        if(millis()-timer_s > 3000)         // 长按大于3s
                        {
                            buzOn();
                            delayFeed(100);
                            buzOn();
                            dispSetting();
                            timer_btn_last = millis();
                            return;
                        }
                        flg_both_press = 1;
                    }
                    if(flg_both_press)
                    {
                        timer_btn_last = millis();
                        return;
                    }
                }
            }
            
            if(!flg_error_page)
            {
                if(__sys_delay_time%1800 == 0)          // 如果是1800的整数倍
                {
                    __sys_delay_time += DELAY_TIME_ADD;
                    __sys_delay_time_ms = __sys_delay_time*1000;
                    __sys_delay_time_lastime = millis();
                }
                else
                {
                    int a = __sys_delay_time/1800;
                    __sys_delay_time = 1800*a;
                    __sys_delay_time += DELAY_TIME_ADD;
                    __sys_delay_time_ms = __sys_delay_time*1000;
                    __sys_delay_time_lastime = millis();
                }
                
                if(__sys_delay_time > DELAY_TIME_TOP*3600)
                {
                    __sys_delay_time = 0;
                    __sys_delay_time_ms = 0;
                    __sys_delay_time_lastime = millis();
                }

                showDelay(__sys_delay_time);
                
#if TY_VERSION
                flg_net_stop = 0;
                __flg_schedule_set = 0;
                eeprom_write(EEP_SCH_SET, __flg_schedule_set);       // 默认没有计划充电
                __t_sch_delay = __sys_delay_time/1800;
                __charge_mode = __t_sch_delay ? 1 : 0;
                pushChrgMode();
                delayFeed(20);
                my_device.mcu_dp_update(PID_DELAY_T, __sys_delay_time, 1);  // 上传当前延时时间
#endif
            }
            while(getBtnC()){feedog();}
            timer_btn_last = millis();
        }
    }
}


// 设置电流闪烁
void setABlink()
{
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 500)return;
    timer_s = millis();
    
    static unsigned int st = 0;
    st = 1-st;
    
    if(st)showAset(__chrg_current_val[__chrg_current]);
    else showAsetOff();
}

// 设置预约时间闪烁
void setCBlink()
{
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 500)return;
    timer_s = millis();
    
    static unsigned int st = 0;
    st = 1-st;
    
    if(st)showDelay(__sys_delay_time);
    else showDelayOff();
}

// 按键处理函数，用于工作中设置电流
// 长按A 5s后，数字闪烁，短按A可以设置电流，连续5s没有操作闪烁停止，设置生效
void btnProcessWorking()
{
    if(!(__work_status == ST_WORKING || __work_status == ST_IDLEINS || __work_status == ST_WAIT))return;
    if(!getBtnA() && !getBtnC())return;
    delay(5);
    if(!getBtnA() && !getBtnC())return;
    
    unsigned long timer_t = millis();       // 计时开始
    
    if(getBtnA())
    {
        while(1)
        {
            feedog();
            updateSleepTimer();
            if(millis()-timer_t > 3000)         // 超过5s，进入设置电流模式
            {
                // 蜂鸣器响两下
                buzOn();
                delayFeed(100);
                buzOn();
                
                while(getBtnA())
                {
                    feedog();
                    setABlink();
                    taskBatteryMove();      // 充电中电池流动
                    taskLedMove();          // 充电中LED流动
                }
                
                timer_t = millis();
                
                while(1)
                {
                    updateSleepTimer();
                    feedog();
                    setABlink();
                    taskBatteryMove();      // 充电中电池流动
                    taskLedMove();          // 充电中LED流动
                    if(getBtnA())
                    {
                        timer_t = millis();
                        buzOn();
                        __chrg_current++;
                        
                        if(__chrg_current > __chrg_current_max)__chrg_current = CHRG_6A;
                        
                        showAset(__chrg_current_val[__chrg_current]);
                        pushCurrentSet(__chrg_current_val[__chrg_current]);
                        
                        while(getBtnA())
                        {
                            feedog();
                            setABlink();
                            taskBatteryMove();      // 充电中电池流动
                            taskLedMove();          // 充电中LED流动
                        }
                    }
                    
                    if(millis()-timer_t > 5000)
                    {
                        eeprom_write(EEP_DEF_CURRENT, __chrg_current);
                        showAset(__chrg_current_val[__chrg_current]);
                        makePwm(__chrg_current_val[__chrg_current]);
                        return;
                    }
                }
            }
            
            if(!getBtnA())return;
            
            taskBatteryMove();      // 充电中电池流动
            taskLedMove();          // 充电中LED流动
        }
    }
    
    if(getBtnC())
    {
        while(1)
        {
            updateSleepTimer();
            feedog();
            if(millis()-timer_t > 3000)         // 超过5s，进入设置电流模式
            {
                // 蜂鸣器响两下
                buzOn();
                delayFeed(100);
                buzOn();
                
                while(getBtnC())
                {
                    setCBlink();
                    taskBatteryMove();      // 充电中电池流动
                    taskLedMove();          // 充电中LED流动
                    feedog();
                }
                
                timer_t = millis();
                
                while(1)
                {
                    feedog();
                    setCBlink();
                    taskBatteryMove();      // 充电中电池流动
                    taskLedMove();          // 充电中LED流动
                    updateSleepTimer();
                    if(getBtnC())
                    {
                        timer_t = millis();
                        buzOn();
                        
                        if(__sys_delay_time%1800 == 0)          // 如果是1800的整数倍
                        {
                            __sys_delay_time += DELAY_TIME_ADD;
                            __sys_delay_time_ms = __sys_delay_time*1000;
                            __sys_delay_time_lastime = millis();
                        }
                        else
                        {
                            int a = __sys_delay_time/1800;
                            __sys_delay_time = 1800*a;
                            __sys_delay_time += DELAY_TIME_ADD;
                            __sys_delay_time_ms = __sys_delay_time*1000;
                            __sys_delay_time_lastime = millis();
                        }
                        
                        if(__sys_delay_time > DELAY_TIME_TOP*3600)
                        {
                            __sys_delay_time = 0;
                            __sys_delay_time_ms = 0;
                            __sys_delay_time_lastime = millis();
                        }
                        showDelay(__sys_delay_time);
                        
#if TY_VERSION
                        __flg_schedule_set = 0;
                        eeprom_write(EEP_SCH_SET, __flg_schedule_set);       // 默认没有计划充电
                        __t_sch_delay = __sys_delay_time/1800;
                        __charge_mode = __t_sch_delay ? 1 : 0;
                        pushChrgMode();
#endif

                        while(getBtnC())
                        {
                            setCBlink();
                            taskBatteryMove();      // 充电中电池流动
                            taskLedMove();          // 充电中LED流动
                            feedog();
                        }
                    }
                    
                    if(millis()-timer_t > 5000)
                    {  
                        showDelay(__sys_delay_time);
#if TY_VERSION
                        __flg_schedule_set = 0;
                        eeprom_write(EEP_SCH_SET, __flg_schedule_set);       // 默认没有计划充电
                        __t_sch_delay = __sys_delay_time/1800;
                        __charge_mode = __t_sch_delay ? 1 : 0;
                        pushChrgMode();
#endif
                        // 如果已经充电，设置了时间
                        if(__work_status == ST_WORKING && __sys_delay_time > 0)
                        {
                            relayOff();
                            ledInsert();
                            changeStatus(ST_WAIT);
                        }
                        // 如果本来就设置了预约时间，把预约时间取消
                        if(__work_status == ST_WAIT && __sys_delay_time == 0)
                        {
                            __sys_delay_time = 1;
                            changeStatus(ST_WAIT);
                        }
                        __sys_delay_time_lastime = millis();
                        return;
                    }
                }
            }
            
            if(!getBtnC())return;
            
            taskBatteryMove();      // 充电中电池流动
            taskLedMove();          // 充电中LED流动
        }
    }
}

void loop()
{
    checkGND();             // 检查接地状态
    protectRCD();           // 漏电保护
    stopMode();
    protectOverVol();       // 过压保护
    protectLowVol();        // 欠压保护
    protectGnd();           // 接地保护
    protectOverCur();       // 过流保护
    protectTemp();          // 过温保护
    protectTempWall();      // 墙插过温保护

    protectOverVolIdle();   // 空闲状态的过压保护
    protectLowVolIdle();    // 空闲状态的欠压保护
    protectGndIdle();       // 空闲状态的接地保护

    checkSensor(100);       // 检查传感器，每隔100ms检查一次
    taskDisplay();          // 显示任务
    chrgTimeAdd();          // 充电时间计时
    updatekwh();            // 统计电量
    delayCountDown();       // 倒计时
    
    stMachine();            // 状态切换
    btnProcessIdle();       // 空闲状态的按键处理
    btnProcessWorking();    // 充电中的按键处理
    taskBatteryMove();      // 充电中电池流动
    taskLedMove();          // 充电中LED流动
    
    if(flg_gnd_protect_stop == 1 && __sys_gnd_option_temp == 2)
    {
        ledErrorBlink();
    }
    
    feedog();
    
    tyProcess();
    
    // 休眠唤醒
    if(__work_status == ST_SLEEP)             // 休眠模式下按键检测
    {
        if(getBtnA() || getBtnC())
        {
            delay(10);
            if(getBtnA() || getBtnC())
            {
                buzOn();
                updateSleepTimer();
                while(getBtnA() || getBtnC());
            }
        }
    }

    autoJzA();
    updateMetrics();                // 推送
    updateTime();
    updateSelfCheckOnce();
    
#if TY_VERSION
    if(flgTyConnected && (flg_ty_en == 0))
    {
        flg_ty_en = 1;
        eeprom_write(EEP_TY_EN, flg_ty_en);
    }
    
    if((millis()-flg_init_ok > 20000) && (flgTyConnected != 1) && (__flg_schedule_set == 2))     // 如果开机20秒还没连上，清除设置的定时充电信息
    {
        __charge_mode = 0;
        __flg_schedule_set = 0;
        eeprom_write(EEP_SCH_SET, __flg_schedule_set);       // 默认没有计划充电
    }
    
    if(__work_ty_st == X_WS_WAIT_SCHEDULE)      // 处于预约等待
    {
        showDelay(calcSecond(local_time.hour, local_time.minute, local_time.second, __t_sch_s_hour, __t_sch_s_min, 0));
    }
    
    if(__work_status == ST_WORKING && __flg_schedule_set)       // 预约模式充电中
    {
        showDelay(calcSecond(local_time.hour, local_time.minute, local_time.second, __t_sch_e_hour, __t_sch_e_min, 0));
    }
    
    // 每隔2小时上传一次基本数据，以免由于用户长时间不关机导致dp丢失
    static unsigned long timer_update_all_2h = millis();
    if(millis()-timer_update_all_2h > 7200000)
    {
        timer_update_all_2h = millis();
        if(flgTyConnected)
        {
            dp_update_all();
        }
    }

    // 以下每隔5s上传当前状态
    static unsigned long timer_update_status_5s = millis();       
    if(millis()-timer_update_status_5s > 5000)     // 每隔5s进入一次
    {
        timer_update_status_5s = millis();
        if(flgTyConnected)      // 如果涂鸦已经连接
        {
            my_device.mcu_dp_update(PID_WORK_ST, __work_ty_st, 1);  // 上传当前状态
            delay(5);
            if(__sys_delay_time)my_device.mcu_dp_update(PID_DELAY_T, __sys_delay_time, 1);  // 上传当前延时时间
            delay(5);
            pushChrgMode();
        }
    }
    
    // 以下每个10s执行一次
    static unsigned long timer_update_status_10s = millis();
    if(millis()-timer_update_status_10s > 10000)     // 每隔10s进入一次
    {
        timer_update_status_10s = millis();
        getRssi();
    }

#endif
}

int calcSecond(int t_now_h, int t_now_m, int t_now_s, int t_end_h, int t_end_m, int t_end_s) {
    // 将当前时间和目标时间转换为秒
    int now_in_seconds = t_now_h * 3600 + t_now_m * 60 + t_now_s;
    int end_in_seconds = t_end_h * 3600 + t_end_m * 60 + t_end_s;

    // 计算时间差
    int diff = end_in_seconds - now_in_seconds;

    // 如果当前时间大于目标时间，目标时间在第二天
    if (diff <= 0) {
        diff += 24 * 3600; // 加上一天的秒数
    }

    return diff;
}


// 推送数据，推送的频率需要调整，暂时定位每秒钟一次
// 后续可以想办法降低推送频率
void updateMetrics()
{
#if TY_VERSION
    
    if(!flgTyConnected)return;        // 没连上，返回
    
    unsigned long dtime = 1000;
    
    static unsigned long timer_s = millis();
    
    if(millis() < 120000)dtime = 1000;                          // 开机120s内每秒钟推送一次
    else if(millis()-timer_ty_get_cmd < 300000)dtime = 1000;     // 收到命令的30s内，没秒钟推送一次
    else if(__work_status == ST_WORKING || __work_status == ST_PAUSE)dtime = 5000;       // 工作中每1s推送一次
    else if(__work_status == ST_SLEEP)dtime = 1800000;          // 休眠中每30min推送一次
    else if(__work_status == ST_IDLE)dtime = 60000;             // IDLE状态5s推送一次
    else if(__work_status == ST_WAIT)dtime = 60000;             // 等待状态60s推送一次
    else dtime = 20000;    // 其他20s推送一次
    
    if(millis()-timer_s < dtime)return;
    timer_s = millis();
    #if L2_CHECK
    if(flg_phase_3)pushMetrics(__sys_voltage, __sys_current1, __sys_voltage, __sys_current2, __sys_voltage, __sys_current3, __sys_temp, __sys_chrg_time, __sys_kwh);
    else pushMetrics(__sys_voltage, __sys_current1, 0.0, __sys_current2, 0.0, __sys_current3, __sys_temp, __sys_chrg_time, __sys_kwh);
    #else
    pushMetrics(__sys_voltage, __sys_current1, __sys_voltage, __sys_current2, __sys_voltage, __sys_current3, __sys_temp, __sys_chrg_time, __sys_kwh);
    #endif
#endif
}

int flgPshSelfCheckOnce = 0;
void updateSelfCheckOnce()
{
#if TY_VERSION
    if(flgPshSelfCheckOnce)return;
    if(!flgTyConnected)return;        // 没连上，返回
    if(!flgGetTimeOk)return;
    
    flgPshSelfCheckOnce = 1;
    pushSelfCheck(1, 1);
#endif
}


int __led_move_loca = 0;        // 充电位置记录

void taskLedMove()
{
    if(__work_status != ST_WORKING)return;
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 500)return;
    timer_s = millis();
    
    __led_move_loca++;
    if(__led_move_loca > 9)__led_move_loca = 1;

    setLedMove(__led_move_loca);
}

int __battery_st = 0;           // 记录电池显示的位置

void taskBatteryMove()
{
    if(__work_status != ST_WORKING)return;
    
    if(__sys_delay_time)          // 还有延时充电
    {
        showBattery(0);
        return;
    }
    
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 500)return;
    timer_s = millis();

    __battery_st++;

    if(__battery_st > 5)__battery_st = 1;

    showBattery(__battery_st);
}


// END FILE