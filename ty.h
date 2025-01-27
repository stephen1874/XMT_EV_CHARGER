// Tuya Module

#ifndef __TY_H__
#define __TY_H__

#include <TuyaWifi.h>
#include "dfs.h"

/*PID - PID_WORK_ST
// 灰色
100: 'Off-Line',
101: 'Waiting for Connection',
// 蓝色
200: 'Ready to Charge',
201: 'Charging Complete',
202: 'Waiting for Scheduled Charging',
203: 'Waiting for Delayed Charging',
204: Pause
// 绿色
300: 'Charging in Process',
// 橙黄
400: 'Over Current Protection',
401: 'Over Voltage Protection',
402: 'Low Voltage Protection',
403: 'Control Box Overheat Protection',
// 红色
500: 'Self-test Failure',
501: 'Leakage Protection',
502: 'System Error - Sticky Relay',
503: 'Leakage Protection Error',
504: 'System Error - CP Failure',
505: 'System Error - Other Fault',
506: 'EV Diode not Detected',
507: 'Unearthed Socket'
*/
#define PID_WORK_ST     101

/*PID - PID_METRICS
JSON格式
x_metrics: {"L1": [220,16.1,3], "L2", [2,3,4], "L3": [4, 5,6], 't': 10, 'p': 10, 'd': 100, 'e': 1000 }
L1/L2/L2：每一个相位的电压，电流，功率
t: 温度(temperature)
p: 功率(power)
d: 总时长(total duration)
e: 总能量(total energy)

电压和总时长整数，其余的小数点后一位
*/
#define PID_METRICS     102

/*PID - PID_SELFTEST
开机自检信息 - Json格式

*/
#define PID_SELFTEST    103

/*PID - PID_ALARM
报警信息 - JSON格式

*/
#define PID_ALARM       104

/*PID  - PID_DOCHARGE
400: 'Over Current Protection',
401: 'Over Voltage Protection',
402: 'Loww Voltage Protection',
403: 'Control Box Overheat Protection',

500: 'Self-test Failure',
501: 'Leakage Protection',
502: 'System Error - Sticky Relay',
503: 'Leakage Protection Error',
504: 'System Error - CP Failure',
505: 'System Error - Other Fault',
506: 'EV Diode not Detected',
507: 'Unearthed Socket'
*/

#define PID_RECORD      105

/*设备信息*/
#define PID_DINFO       106

/*可选充电电流，只上报，Json格式*/
#define PID_ADJUST_CUR  107

/* 倒计时剩余时间 */
#define PID_DELAY_T     108

#define PID_WORK_ST_DBG 109

#define PID_WIFI_SIGNAL 110

#define PID_DOCHARGE    140

#define PID_FACTORY     141
#define PID_RST         142

/*PID - PID_CURRENTSET
数值型，设置充电电流
6/8/10/13/16/20/25/32A
*/
#define PID_CURRENTSET  150

/*PID - PID_CHARGEMODE
JSON格式
*/
#define PID_CHARGEMODE  151
// Function prototypes

/*PID - PID_MAX_CUR
最大电流选择，数值型
*/
#define PID_MAX_CUR     152

/*PID - PID_LANG
下位机语言选择
*/
#define PID_LANG        153

#define PID_GND_OPTION  154
#define PID_NFC         155
#define PID_PD_VER      157             // 产品变种，0默认，1不带NFC，2Travel版本

#define X_WS_OFF_LINE       100
#define X_WS_WAIT_CONNECT   101         // 未插车
#define X_WS_READY          200         // 插上车
#define X_WS_COMPLETE       201         // 充电完成，通常不会出现
#define X_WS_WAIT_SCHEDULE  202         // 等待充电，预约
#define X_WS_WAIT_DELAY     203         // 等待充电，延时
#define X_WS_PAUSE          204         // 暂停充电
#define X_WS_WORKING        300         // 充电中
#define X_WS_OCP            400         // 过流保护
#define X_WS_OVP            401         // 过压保护
#define X_WS_LVP            402         // 欠压保护
#define X_WS_TEMP           403         // 过温保护
#define X_WS_WALL_TEMP      404         // 墙插温度过热
#define X_WS_STF            500         // 自检失败
#define X_WS_RCD            501         // 漏电保护
#define X_WS_RELAY          502         // 继电器黏连保护
#define X_WS_RCD_SC         503         // 漏电自检失败
#define X_WS_CP             504         // cp错误
#define X_WS_OF             505         // 其他错误
#define X_WS_DIODE          506         // 二极管击穿
#define X_WS_GND            507         // 接地保护

void tyInit();
void tyConfig();
void tyProcess();

void dp_update_all(void);
unsigned char dp_process(unsigned char dpid, const unsigned char value[], unsigned short length);

void pushWorkState(int __wst);
void pushMetrics(float vL1, float aL1, float vL2, float aL2, float vL3, float aL3, long t, long d, float e);
void pushSelfCheck(int relay_check, int rcd_check);
void pushCurrentSet(int __cur);
void pushAlarm(int __alarm);
void pushRecord();
void pushCurOption();
void pushMaxCur(int max_cur);
void pushLang(int _lang);
void pushDeviceInfo();
void pushGndOption(int __gnd);

void pushChrgMode();
void pushDelay(unsigned long delay_t);
void pushWorkStateDebug(int st);
void pushWifiSignal(int __sig);
int getRssi();


extern TuyaWifi my_device;
extern TUYA_WIFI_TIME local_time;
extern TUYA_WIFI_TIME start_time;
#endif

// END FILE