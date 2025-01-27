#ifndef __DFS_H__
#define __DFS_H__

#include <Arduino.h>
#include <IWatchdog.h>
#include "hw.h"
#include "disp.h"
#include "fun.h"
#include "ty.h"

// 通过以下设置版本号1
#ifdef BOARD_M2_1P16A
#define M2_3KW                      1
#define PROTECT_TEMP_WALL_EN        1                   
#define PHASE1_SPAIN                1
#define MAX_CURRENT                 16
#define PHASE_NUM                   1

#elif defined(BOARD_M2_1P32A)
#define M2_7KW                      1                   
#define PHASE1_SPAIN                1
#define MAX_CURRENT                 32
#define PHASE_NUM                   1

#elif defined(BOARD_M2_1P16A_BRAZIL)
#define M2_3KW                      1
#define PROTECT_TEMP_WALL_EN        1                   
#define PHASE1_PORTUGAL             1
#define MAX_CURRENT                 16
#define PHASE_NUM                   1
#define FREQ_60HZ                   1

#elif defined(BOARD_M2_1P32A_BRAZIL)
#define M2_7KW                      1                   
#define PHASE1_PORTUGAL             1
#define MAX_CURRENT                 32
#define PHASE_NUM                   1
#define FREQ_60HZ                   1

#elif defined(BOARD_M2_3P16A)
#define M2_11KW                     1
#define TY_VERSION                  1                                     
#define PHASE3_PORTUGAL             1               // PHASE3_PORTUGAL, PHASE3_SPAIN
#define MAX_CURRENT                 16
#define PHASE_NUM                   3

#elif defined(BOARD_M2_3P16A_PLATINET)
#define M2_11KW                     1
#define TY_VERSION                  1                                    
#define PHASE3_POLAND               1
#define MAX_CURRENT                 16
#define PHASE_NUM                   3
#define PLATINET_VERSION            1

#elif defined(BOARD_M2_3P32A)
#define M2_22KW                     1
#define TY_VERSION                  1                                    
#define PHASE3_PORTUGAL                1
#define MAX_CURRENT                 32
#define PHASE_NUM                   3

#elif defined(BOARD_M2_3P32A_PLATINET)
#define M2_22KW                     1
#define TY_VERSION                  1                                  
#define PHASE3_POLAND               1
#define MAX_CURRENT                 32
#define PHASE_NUM                   3
#define PLATINET_VERSION            1

#elif defined(BOARD_M2_3P32A_TEST)
#define M2_22KW                     1
#define TY_VERSION                  1                                  
#define PHASE3_ONLY_ENGLISH         1
#define TEST_VERSION                1
#define MAX_CURRENT                 32
#define PHASE_NUM                   3

#elif defined(BOARD_M3_1P32A)
#define M3_22KW                     1
#define TY_VERSION                  1                   
#define NFC_VERSION                 1                   // 是否开启NFC              
#define PHASE3_SPAIN                1
#define MAX_CURRENT                 32
#define PHASE_NUM                   1

#elif defined(BOARD_M3_3P16A)
#define M3_22KW                     1
#define TY_VERSION                  1                   
#define NFC_VERSION                 1                   // 是否开启NFC              
#define PHASE3_SPAIN                1
#define MAX_CURRENT                 16
#define PHASE_NUM                   3
#define NEW_CP_VERSION              1
#elif defined(BOARD_M3_3P32A)
#define M3_22KW                     1
#define TY_VERSION                  1                   
#define NFC_VERSION                 1                   // 是否开启NFC              
#define PHASE3_SPAIN                1
#define MAX_CURRENT                 32
#define PHASE_NUM                   3

#elif defined(BOARD_M3_3P32A_TEST)
#define M3_22KW                     1
#define TY_VERSION                  1   
#define NFC_VERSION                 1                   // 是否开启NFC                               
#define PHASE3_ONLY_ENGLISH         1
#define TEST_VERSION                1
#define MAX_CURRENT                 32
#define PHASE_NUM                   3
#endif

#if M2_11KW||M2_22KW
#define GND_DETECT_VALUE            250
#define GND_DETECT_ADC              1           // 使用模拟数值来判断接地
#define L2_CHECK                    1                   // 是否检查L2，对于单相以及旧版本的三相，应当设为0
#define NEW_GND_CHECK               1                   // 对于模式2三相量产版本，采用了模拟输入的方式检测接地
#define TRAVEL_AUTO_CHECK           1                   // 自动判断是否旅行版
#else
#define GND_DETECT_VALUE            250
#define GND_DETECT_ADC              0           // 使用模拟数值来判断接地
#define L2_CHECK                    0                   // 是否检查L2，对于单相以及旧版本的三相，应当设为0
#define NEW_GND_CHECK               0                   // 对于模式2三相量产版本，采用了模拟输入的方式检测接地
#define TRAVEL_AUTO_CHECK           0                   // 自动判断是否旅行版
#endif 

#define   MODE_TUV                  0                   // 认证模式，所有设置满足认证的要求

#define   RCD_SX            0                   // 盛相漏保
#define   RCD_RC            1                   // 瑞磁漏保

#define   ENABLE_VOLTAGE_CALIBRATION    1       // 是否采用70/218/280来校准电压
#define   AUTO_JZA                      1       // 自动校准电流

#if FREQ_60HZ
#define POWER_FREQ                  16667
#else
#define POWER_FREQ                  20000
#endif
// 保护开关，可以通过这些设置是否开启某个保护
#define   WATCH_DOG_EN              1           // 是否开启看门狗
#define   PROTECT_OVER_CURRENT_EN   1
#define   PROTECT_OVER_VOLTAGE_EN   1
#define   PROTECT_LESS_VOLTAGE_EN   1
#define   PROTECT_GND_EN            1
#define   RELAY_ERROR_EN            1           // 继电器黏连检测开关
#define   DIODE_CHECK_EN            1           // 是否检测二极管击穿（cp信号上的二极管）
#define   PROTECT_TEMP_EN           1           // 过温保护开关
#define   PROTECT_RCD_EN            1

// 触摸按键有效电平设置
#define   BTN_ON                    LOW
#define   BTN_OFF                   HIGH

// 保护相关参数设置
#define   VAL_PROTECT_OVER_VOL      270         // 过压保护电压
#define   VAL_PROTECT_LESS_VOL      80          // 欠压保护电压

// 关键参数设置
#define DELAY_TIME_TOP              8           // 延时最长时间，单位小时
#define DELAY_TIME_ADD              1800        // 延时时间梯度，单位秒

#define SLEEP_TIME                  600000      // IDLE状态下多久没有操作进入休眠，单位ms

/*错误代号*/
#define ERROR_CODE_RCD          0       // 漏电
#define ERROR_CODE_OC           1       // 过流
#define ERROR_CODE_OV           2       // 过压
#define ERROR_CODE_LV           3       // 欠压
#define ERROR_CODE_GND1         4       // 接地选择
#define ERROR_CODE_GND2         5       // 接地错误
#define ERROR_CODE_OT           6       // 过温保护
#define ERROR_CODE_DIODE        7       // 二极管击穿
#define ERROR_CODE_RELAY        8       // 继电器黏连
#define ERROR_CODE_CP           9       // CP错误
#define ERROR_CODE_RCDERROR     10      // 漏电自检
#define ERROR_CODE_STOP         11      // 按下停止按钮
#define ERROR_CODE_WALL_TMP     12
#define ERROR_CODE_NFC          13      // NFC

// EEPROM地址
#define EEP_GND_OPTION          1       // 存放接地选型,0每次检测，1直接充电，3不充电
#define EEP_DEF_CURRENT         2       // 存放默认电流
#define EEP_MAX_CURRENT         3       // 存放最大电流
#define EEP_WALL_TEMP           4       // 存储墙插温度检测设置，0关闭警告，1开启警告
#define EEP_FIRST_RUN           5       // 是否第一次开机，第一次开机后，这个值设为0x55
#define EEP_LANG                6       // 设置语言
#define EEP_TY_EN               7       // 是否启用涂鸦模块
#define EEP_IF_VJZ              8       // 电压是否校准
#define EEP_IF_AJZ              9       // 电流是否校准
#define EEP_IS_JIAOZHUN         10      // 是否校准标志位，0x55为已经校准

#define EEP_V_JZ                11
#define EEP_A_JZ                15
#define EEP_A2_JZ               19
#define EEP_A3_JZ               23

#define EEP_IF_V218             30
#define EEP_V218                31
#define EEP_IF_VHIGH            40
#define EEP_VHIGH               41
#define EEP_IF_VLOW             50
#define EEP_VLOW                51

#define JZ_V_HIGH               285
#define JZ_V_LOW                65

#define EEP_SCH_SET             60      // 是否设置计划充电，0x55，设置，其他，未设置
#define EEP_SCH_SH              61      // 计划充电开始hour
#define EEP_SCH_SM              62      // 计划充电开始minute
#define EEP_SCH_EH              63      // 计划充电结束hour
#define EEP_SCH_EM              64      // 计划充电结束minute

// 工作状态
#define ST_SLEEP                0                   // 休眠，按键或者插上车退出
#define ST_IDLE                 1                   // 闲置状态
#define ST_IDLEINS              2                   // 插入
#define ST_WORKING              3                   // 充电中
#define ST_WAIT                 4                   // 等待，设置了延时充电
#define ST_ERRORPAUSE           5                   // 过压，欠压，保护接地，过流，CP错误等待恢复
#define ST_PAUSE                6                   // 手动暂停
#define ST_STOP                 7                   // 停止充电

// 车插入情况
#define CAR_NO_INSERT           0                   // PWM 12V, 未插入车
#define CAR_INSERT              1                   // PWM 9V, 插上车
#define CAR_READY               2                   // PWM 6V, 车准备好，可以充电
#define CAR_ERROR               3                   // 

//----------------------------------------------------------------------
//充电电流
#define CHRG_6A                 0
#define CHRG_8A                 1
#define CHRG_10A                2
#define CHRG_13A                3
#define CHRG_16A                4
#define CHRG_20A                5
#define CHRG_25A                6
#define CHRG_32A                7


#if M2_11KW||M2_3KW
#define DEFAULT_CURRENT  CHRG_16A
#else
#define DEFAULT_CURRENT  CHRG_32A
#endif

//-------------------------------3.5KW/7KW引脚设置-------------------------
#if M2_3KW || M2_7KW

#define pinBuz          PC15
#define pinBtnA         PC14
#define pinWS2812       PB9
#define pinBtnD         PB8           // 按键2
#define pinScl          PB7
#define pinSda          PB6
#define pinCS           PB5
#define pinDc           PB4
#define pinRst          PB3
#define pinBL           PD2

// 其他引脚定义
#define pinRelayH       PB10
#define pinRelayL       PB11          // 最新的板子
#define pinPwm          PB0

#define pinDetGnd       PB1

#define adcV            A0
#define adcCp           A2
#define adcI            A3
#define RELAY_ERROR2    A5
#define pinTemp         A6
#define pinTempWall     A1
#define RELAY_ERROR1    A7

#define pinLedOnboard   PB12

#define pinLbOut        PA4           // 漏电保护输出，高电平有效
#define pinLbTest       PA12          // 漏电保护测试引脚，拉高可以模拟漏电
#define pinLbZero       PA11          // 漏电归零，在上电时操作一次

#define pinDiodeCheck   PA8

//-------------------------------22KW引脚设置-------------------------
#elif M2_11KW || M2_22KW

#define pinSda      PB6
#define pinScl      PB7
#define pinRst      PD2
#define pinDc       PB4
#define pinCS       PB5

#define pinWS2812   PB9
#define pinBtnA     PC14
#define pinBuz      PC15

// 其他引脚定义
#define pinRelayH   PB14
#define pinRelayL   PB13            // 最新的板子
#define pinPwm      PB3

#if NEW_GND_CHECK
#define pinDetGnd   PC1
#else
#define pinDetGnd   PB2
#endif
#define adcV        A0
#define adcI        A1
#define adcI2       A9
#define adcI3       A3
#define adcCp       A2

#define pinTemp         A6
#define pinLedOnboard   PB11

// 信美通电路板的引脚
#define pinBtnD     PB8     // 按键2
#define pinLbOut    PB10    // 漏电保护输出，高电平有效
#define pinLbTest   PA12    // 漏电保护测试引脚，拉高可以模拟漏电
#define pinLbZero   PA11    // 漏电归零，在上电时操作一次

#define pinBL       PC12

#define RELAY_ERROR1    A4
#define RELAY_ERROR2    A5
#define RELAY_ERROR3    A7
#define RELAY_ERROR4    A8

#define pinDiodeCheck   PA8
#define WIFI_EN         PC13

#if L2_CHECK
#define pinL2Check      A10
#endif

#if TRAVEL_AUTO_CHECK
#define pinTravelCheck  A14
#define pinTempWall     A15
#endif

//-------------------------------模式3，22kw-------------------------
#elif M3_22KW

#define pinSda      PB4
#define pinScl      PB3
#define pinRst      PB9
#define pinDc       PB8
#define pinCS       PB5

#define pinWS2812   PC12
#define pinBtnA     PC11
#define pinBuz      PA12

// 其他引脚定义
#define pinRelayH   PC14
#define pinRelayL   PC15            // 最新的板子
#define pinPwm      PB0

#define pinDetGnd   PB2
#define adcV        A0
#define adcI        A7
#define adcI2       A3
#define adcI3       A9
#define adcCp       A2

#define pinTemp         A10
#define pinLedOnboard   PB12

// 信美通电路板的引脚
#define pinBtnD         PD2     // 按键2
#define pinLbOut        PB14    // 漏电保护输出，高电平有效
#define pinLbTest       PC6    // 漏电保护测试引脚，拉高可以模拟漏电
#define pinLbZero       PB15    // 漏电归零，在上电时操作一次

#define pinBL           PC13

#define RELAY_ERROR1    A1
#define RELAY_ERROR2    A5
#define RELAY_ERROR3    A6
#define RELAY_ERROR4    A4

#define pinDiodeCheck   PB10
#define WIFI_EN         PB11

#define pinEBtn         PC1   
#define pinELed         PC2
#endif
//-------------------------------全局变量-------------------------
extern int __car_status;
extern int __insert_status_b;

extern int __sys_voltage;
extern float __sys_current1;
#if M2_11KW || M2_22KW||M3_22KW
extern float __sys_current2;
extern float __sys_current3;
#endif
extern int __sys_gnd;
extern float __sys_kwh;

extern int cnt_current_error;
#if M2_11KW || M2_22KW||M3_22KW
extern int cnt_current_error2;
extern int cnt_current_error3;
#endif

extern int cnt_vol264_error;
extern int cnt_vol176_error;
extern int cnt_gnd_error;

extern long __sys_delay_time;          // 延迟充电时间，8小时内
extern long __sys_delay_time_ms;        // 延时充电时间，单位ms
extern unsigned long __sys_delay_time_lastime;
extern unsigned long __sys_chrg_time_lastime;
extern long __sys_chrg_time;
extern int __sys_temp;
extern int __sys_temp_wall;
extern unsigned long timer_sleep;
extern float __sys_voltage_jz;
extern float __sys_current_jz;

#if M2_11KW||M2_22KW||M3_22KW
extern float __sys_current_jz2;
extern float __sys_current_jz3;
#endif

#if M2_3KW||M2_11KW
extern int __chrg_current_val[5];
extern int __chrg_current_over[5];
#elif M2_22KW||M2_7KW||M3_22KW
extern int __chrg_current_val[8];
extern int __chrg_current_over[8];
#endif
extern int __chrg_current;
extern int flg_vol_temp_mode;
extern int __sys_gnd_option;
extern int __sys_gnd_option_temp;
extern int __langUage;

extern int flg_gnd_protect_stop;

extern int __work_status;

#if TY_VERSION
extern int __work_ty_st;
extern unsigned long timer_current_set_ty;
#endif
extern int __car_status;            // 检查车的插入情况

extern int flg_error_page;          // 是否处于错误界面
extern int flg_lb_happen;           // 处于漏电检测状态

extern unsigned long timer_sys_start;
extern int flg_init_ok;
extern int __chrg_current_max;

extern int i_static1;
extern int i_static2;
extern int i_static3;

extern int flgTyConnected;
extern int flgTyBlueBlink;

#define dWF(a,b) digitalWriteFast(digitalPinToPinName(a),b)

#define CS_HIGH()   dWF(pinCS, HIGH)
#define CS_LOW()   dWF(pinCS, LOW)
#define DC_HIGH()   dWF(pinDc, HIGH)
#define DC_LOW()   dWF(pinDc, LOW)

#define SCK_LOW()       dWF(pinScl, LOW)
#define SCK_HIGH()      dWF(pinScl, HIGH)

#define SDA_LOW()       dWF(pinSda, LOW)
#define SDA_HIGH()      dWF(pinSda, HIGH)

#define RST_LOW()       dWF(pinRst, LOW)
#define RST_HIGH()      dWF(pinRst, HIGH)

extern void makeIstatic();
extern unsigned long timer_btn_last;
extern unsigned long __sys_chrg_time_ms;
extern int flgGetTimeOk;
extern int __flg_ty_gnd_update;             // 接地选项有更新
extern int __flg_ty_lang_update;            // 语言有更新
extern int __flg_ty_max_update;             // 最大电流有更新
extern int flg_setting_page;
extern int __flg_factory_reset;

extern int __charge_mode;                   // 充电模式，0-立即充电，1-延时充电，2-计划充电
extern int __flg_schedule_set;              // 是否设置了定时充电
extern int __t_sch_s_hour;                  // 开始时间 - hour
extern int __t_sch_s_min;                // 开始时间 - minute         
extern int __t_sch_e_hour;                  // 结束时间 - hour
extern int __t_sch_e_min;                // 结束时间 - minute
extern int __t_sch_delay;                   // 延迟时间

extern unsigned long timer_relay_on;        // 继电器闭合时间
extern unsigned long timer_relay_off;       // 继电器断开时间
extern unsigned long timer_get_chrgmode;    // 收到设置充电模式命令的时间
extern int freq_cycle;
#if L2_CHECK
extern int flg_phase_3;
#endif

#if ENABLE_VOLTAGE_CALIBRATION
extern void makeVolCaliABC();
extern int makeVolCorrect(int v_input);        // 定义函数 makeVolCorrect 来根据输入电压计算拟合电压值
extern int voltage_cali_ok;
#endif

#if TY_VERSION
extern int flg_ty_en;                          // 是否启用涂鸦模块，当第一次长按后，启用
extern unsigned long timer_ty_get_cmd;
extern int wifi_rssi;
extern unsigned long timer_get_time_ok;     // 最后一次获取时间
extern int flg_net_stop;
#endif

#if TRAVEL_AUTO_CHECK
extern int flg_travel;                         // 是否Travel版本
#endif

extern unsigned long timer_status_change;

extern void taskBatteryMove();
extern void taskLedMove();
extern void updateMetrics();
extern void updateSelfCheckOnce();
extern int calcSecond(int t_now_h, int t_now_m, int t_now_s, int t_end_h, int t_end_m, int t_end_s);

#if (PHASE1_PORTUGAL || PHASE1_SPAIN) && (M2_11KW || M2_22KW || M3_22KW)
#error "PHASEn_LANG ERROR"
#endif

#if (M2_3KW && (!RCD_RC))
#error "RCD TYPE ERROR"
#endif

#if (NFC_VERSION && M2_3KW)
#error "NFC_VERSION ERROR"
#endif

#if (TY_VERSION && M2_3KW)
#error "TY_VERSION ERROR"
#endif

#endif
// END FILE