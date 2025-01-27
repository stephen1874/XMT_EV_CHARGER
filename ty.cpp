#include "ty.h"

// TuyaWiFi instance
TuyaWifi my_device;

#define DIP_NUM     20

unsigned char dp_array[][2] = {
    {PID_WORK_ST, DP_TYPE_VALUE},
    {PID_METRICS, DP_TYPE_STRING},
    {PID_SELFTEST, DP_TYPE_STRING},
    {PID_ALARM, DP_TYPE_STRING},
    {PID_RECORD, DP_TYPE_STRING},
    {PID_DINFO, DP_TYPE_STRING},
    {PID_ADJUST_CUR, DP_TYPE_STRING},
    {PID_DELAY_T, DP_TYPE_VALUE},
    {PID_WORK_ST_DBG, DP_TYPE_ENUM},
    {PID_WIFI_SIGNAL, DP_TYPE_VALUE},
    {PID_DOCHARGE, DP_TYPE_BOOL},
    {PID_FACTORY, DP_TYPE_BOOL},
    {PID_RST, DP_TYPE_BOOL},
    {PID_CURRENTSET, DP_TYPE_VALUE},
    {PID_CHARGEMODE, DP_TYPE_STRING},
    {PID_MAX_CUR, DP_TYPE_VALUE},
    {PID_LANG, DP_TYPE_STRING},
    {PID_GND_OPTION, DP_TYPE_VALUE},
    {PID_NFC, DP_TYPE_BOOL},
    {PID_PD_VER, DP_TYPE_VALUE},
};

#if PLATINET_VERSION
unsigned char pid[] = {"dbyqsgkpjt8n3puv"};             // PLATINET VERSION
#else
unsigned char pid[] = {"mn0smme1cyasqmaq"};             // SINOTEK VERSION
#endif

unsigned char mcu_ver[] = {"1.0.7"};
TUYA_WIFI_TIME local_time;
TUYA_WIFI_TIME start_time;

void tyInit() {
#if TY_VERSION
    // Initialize with PID and MCU software version
    my_device.init(pid, mcu_ver);
    // Set total DP commands and their types
    my_device.set_dp_cmd_total(dp_array, DIP_NUM);
    // Register DP download processing callback function
    my_device.dp_process_func_register(dp_process);
    // Register upload all DP callback function
    my_device.dp_update_all_func_register(dp_update_all);
#endif
}

unsigned long timer_tyconfig = 0;
void tyConfig() {
#if TY_VERSION
    my_device.mcu_set_wifi_mode(SMART_CONFIG);
    timer_tyconfig = millis();
#endif
}


int wifi_state_b = -1;
// 打开解绑通知
int flgInfoRstOk = 0;

uint8_t calculateChecksum(uint8_t *buffer, int length) {
  uint8_t checksum = 0;
  for (int i = 0; i < length; i++) {
    checksum += buffer[i];
  }
  return checksum;
}

// 发送命令打开解绑通知
int openRstInfo()
{
#if TY_VERSION
    unsigned char dta_get[500];

    unsigned char dta_s[8] = {0x55, 0xaa, 0x03, 0x34, 0, 1, 0x04, 0};
    
    dta_s[7] = calculateChecksum(dta_s, 7);
    Serial.write(dta_s, 8);

    unsigned long timer_s = millis();
    int len = 0;
    unsigned char dta[100];
    while(millis()-timer_s < 1000)              // 1s超时
    {
        if(Serial.available())
        {
            dta[len++] = Serial.read();
        }
        
        if(len ==8 && dta[0] == 0x55 && dta[3] == 0x34 && dta[7] == 0x00)return 1;
    }
    return 0;
#else
    return 0;
#endif
}

/**
 * @description: Process Tuya WiFi state and control LED accordingly.
 * Handle network errors and configuration.
 */
int flg_net_error = 0;
unsigned long timer_flg_net_error = 0;
int flg_ty_green_blink = 0;
unsigned long timer_ty_green_blink = 0;             // 持续配网时间

int flgRunConnectedFirstTime = 1;                   // 标志位，用于在第一次连接到服务器时执行

void tyProcess() 
{
#if TY_VERSION
    static unsigned long timer_s = millis();
    if (millis() - timer_s < 10) return;
    timer_s = millis();
    static unsigned long timer_led = millis();
    static unsigned int led_st = 0;
    static unsigned long timer_config_again = millis();

    if(!flg_setting_page)
    {
        if(flgTyConnected)
        {
            if(wifi_rssi<-80)showWifiOk(0);
            else if(wifi_rssi<-70)showWifiOk(1);
            else if(wifi_rssi<-50)showWifiOk(2);
            else showWifiOk(3);
        }
        else if(!flgTyBlueBlink)showWifiFail();
    }
    
    my_device.uart_service();

    // LED blinks when network is being connected
    int wifi_state = my_device.mcu_get_wifi_work_state();
    
    flgTyConnected = (wifi_state == 4) ? 1 : 0;  // 当wifi_state为4的时候，flgTyConnected设为1，否则为0
    
    if(flgRunConnectedFirstTime && flgTyConnected)      // 第一次连接时执行
    {
        flgRunConnectedFirstTime = 0;
    }
    
    // 网络状态发生变化
    if (wifi_state != wifi_state_b) 
    {
        // 如果网络状态从0变为2，判定为出错，可能是输入密码错误或者其他的错误，或者是短暂的中间状态
        if (wifi_state == 2 && wifi_state_b == 0 && flg_net_error == 0) 
        {
            flg_net_error = 1;
            timer_flg_net_error = millis();
        }

        // 涂鸦模块从其他模式进入配网模式
        if (wifi_state == 0 && wifi_state_b != 0) {  // Enter configuration mode
            flg_ty_green_blink = 1;
            timer_ty_green_blink = millis();
        }
        
        // 上线
        if(wifi_state == 4)
        {
            dp_update_all();
        }

        if (wifi_state != 0) flg_ty_green_blink = 0;
        wifi_state_b = wifi_state;
    }

    // 解绑
    if(my_device.ifAppReset())
    {
        tyConfig();
        return;
    }

    // 发生网络错误期间状态改变，清除标志位
    if (flg_net_error && (wifi_state != 2)) flg_net_error = 0;  // Reset network error flag if not in error state
    
    // 发生了网络错误，在15s后重新让模块进入可配对状态
    if (flg_net_error && (millis() - timer_flg_net_error > 15000))
    {
        flg_net_error = 0;
        tyConfig();
    }
    
    if ((wifi_state != WIFI_LOW_POWER) && (wifi_state != WIFI_CONN_CLOUD) && (wifi_state != WIFI_SATE_UNKNOW)) 
    {
        flgTyBlueBlink = 1;
    }
    else if ((millis()-timer_sys_start > 1000) && (millis() - timer_tyconfig < 8000) && (timer_tyconfig != 0))      // 长按强制闪8s
    {
        flgTyBlueBlink = 1;
    }
    else
    {
        flgTyBlueBlink = 0;
    }
    
#endif
}

/**
 * @description: DP download callback function.
 * @param {unsigned char} dpid
 * @param {const unsigned char} value
 * @param {unsigned short} length
 * @return {unsigned char}
 */

unsigned char dp_process(unsigned char dpid, const unsigned char value[], unsigned short length) {
#if TY_VERSION
    int dta_tmp = 0;
    updateSleepTimer();         // 如果机器休眠，唤醒它
    timer_ty_get_cmd = millis();        // 记录收到命令时刻
    switch (dpid) {
        
        // ----------------------------立即充电-------------------------------------
        case PID_DOCHARGE:
        dta_tmp = my_device.mcu_get_dp_download_data(PID_DOCHARGE, value, length);
        
        if(dta_tmp)         // 启动
        {
            if((__work_status == ST_PAUSE))             // 暂停中
            {
                pushWorkState(X_WS_WORKING);      
                delay(10);
                changeStatus(ST_WORKING);               // 切换到工作中  
            }
            else if(__work_status == ST_WAIT)                // 等待充电中
            {
                __sys_delay_time_ms = 0;
                __sys_delay_time = 0;
                showDelay(__sys_delay_time);
                
                __charge_mode = 0;
                __t_sch_delay = 0;
                
                // 清除预约信息
                __flg_schedule_set = 0;
                eeprom_write(EEP_SCH_SET, __flg_schedule_set);   
                pushChrgMode();
                pushWorkState(X_WS_WORKING);   
                delay(10);                
                changeStatus(ST_WORKING);               // 切换到工作中 
            }
        }
        else
        {
            if(__work_status == ST_WORKING)     // 工作中, 切换到暂停
            {
                pushWorkState(X_WS_PAUSE);
                delay(10);
                changeStatus(ST_PAUSE);         // 切换到暂停
                ledInsert();                    // 灯蓝色
                relayOff();                     // 继电器断开
                makePwm(100);
            }
        }
        buzOn();
        
        break;
        
        // ----------------------------设置充电电流-------------------------------------
        case PID_CURRENTSET:
        dta_tmp = my_device.mcu_get_dp_download_data(PID_CURRENTSET, value, length);
        
        if(dta_tmp == 6)__chrg_current = CHRG_6A;
        else if(dta_tmp == 8)__chrg_current = CHRG_8A;
        else if(dta_tmp == 10)__chrg_current = CHRG_10A;
        else if(dta_tmp == 13)__chrg_current = CHRG_13A;
        else if(dta_tmp == 16)__chrg_current = CHRG_16A;
        else if(dta_tmp == 20)__chrg_current = CHRG_20A;
        else if(dta_tmp == 25)__chrg_current = CHRG_25A;
        else if(dta_tmp == 32)__chrg_current = CHRG_32A;
        
        timer_current_set_ty = millis();
        
        showAset(dta_tmp);
        my_device.mcu_dp_update(PID_CURRENTSET, dta_tmp, 1);
        eeprom_write(EEP_DEF_CURRENT, __chrg_current);
        buzOn();
        break;

        // ----------------------------设置最大充电电流----------------------------------
        case PID_MAX_CUR:
        dta_tmp = my_device.mcu_get_dp_download_data(PID_MAX_CUR, value, length);
        
        if(dta_tmp == 6)__chrg_current_max = CHRG_6A;
        else if(dta_tmp == 8)__chrg_current_max = CHRG_8A;
        else if(dta_tmp == 10)__chrg_current_max = CHRG_10A;
        else if(dta_tmp == 13)__chrg_current_max = CHRG_13A;
        else if(dta_tmp == 16)__chrg_current_max = CHRG_16A;
        else if(dta_tmp == 20)__chrg_current_max = CHRG_20A;
        else if(dta_tmp == 25)__chrg_current_max = CHRG_25A;
        else if(dta_tmp == 32)__chrg_current_max = CHRG_32A;
        
        eeprom_write(EEP_MAX_CURRENT, __chrg_current_max);
        
        if(__chrg_current > __chrg_current_max)         // 设置后，默认电流大于最大电流
        {
            __chrg_current = __chrg_current_max;
            pushCurrentSet(__chrg_current_val[__chrg_current]);
            eeprom_write(EEP_DEF_CURRENT, __chrg_current);
        }
        
        
        my_device.mcu_dp_update(PID_MAX_CUR, dta_tmp, 1);
        __flg_ty_max_update = 1;
        buzOn();
        break;
        
        // ----------------------------设置下位机语言------------------------------------
        case PID_LANG:
        
        my_device.mcu_get_dp_download_data(PID_LANG, value, length);
        
        if(value[0] == 'e' && value[1] == 'n')__langUage = 0;
        else if(value[0] == 'd' && value[1] == 'e')__langUage = 1;
        else if(value[0] == 'f' && value[1] == 'r')__langUage = 2;
        else if(value[0] == 'i' && value[1] == 't')__langUage = 3;
        else if(value[0] == 'e' && value[1] == 's')__langUage = 4;
        
        my_device.mcu_dp_update(PID_LANG, value, length);
        __flg_ty_lang_update = 1;
        buzOn();
        if(flg_error_page)showErrorImg(buf_error_code);
        break;
        
        // ----------------------------设置充电模式---------------------------------------
        case PID_CHARGEMODE:
        
        my_device.mcu_get_dp_download_data(PID_CHARGEMODE, value, length);
        
        // 此处获取
        sscanf((char*)value,
           "{\"m\":%d,\"dt\":%d,\"ss\":\"%02d:%02d\",\"se\":\"%02d:%02d\"}",
           &__charge_mode, &__t_sch_delay, &__t_sch_s_hour, &__t_sch_s_min, &__t_sch_e_hour, &__t_sch_e_min);
        
        if(__charge_mode == 0)      // 立即充电
        {
            __sys_delay_time = 0;
            __sys_delay_time_ms = 0;
            __sys_delay_time_lastime = millis();
            showDelay(__sys_delay_time);
            
            __flg_schedule_set = 0;
            eeprom_write(EEP_SCH_SET, __flg_schedule_set);
            pushWorkState(X_WS_WAIT_CONNECT);   
            delay(10);
            changeStatus(ST_IDLE);           // 切换到暂停
        }
        else if(__charge_mode == 1)      // 设置延时
        {
            __sys_delay_time = __t_sch_delay*1800;
            __sys_delay_time_ms = __sys_delay_time*1000;
            __sys_delay_time_lastime = millis();
            showDelay(__sys_delay_time);
            
            __flg_schedule_set = 0;
            eeprom_write(EEP_SCH_SET, __flg_schedule_set);   
            
            if(__work_status == ST_WORKING && __sys_delay_time > 0)
            {
                relayOff();
                makePwm(100);
                ledInsert();
                pushWorkState(X_WS_WAIT_DELAY);
                delay(10);
                changeStatus(ST_WAIT);
            }              
        }
        else if(__charge_mode == 2)     // 定时充电
        {
            timer_get_chrgmode = millis();
            __flg_schedule_set = 1;
            
            __sys_delay_time = 0;
            __sys_delay_time_ms = 0;
            __sys_delay_time_lastime = millis();
            showDelay(__sys_delay_time);
            
            eeprom_write(EEP_SCH_SET, __flg_schedule_set);       // 默认没有计划充电
            eeprom_write(EEP_SCH_SH, __t_sch_s_hour);        // 默认0:00~8:00充电
            eeprom_write(EEP_SCH_SM, __t_sch_s_min);
            eeprom_write(EEP_SCH_EH, __t_sch_e_hour);
            eeprom_write(EEP_SCH_EM, __t_sch_e_min); 
        }
        
        my_device.mcu_dp_update(PID_CHARGEMODE, value, length);

        buzOn();

        break;
        
        
        // ----------------------------设置接地选线------------------------------------
        case PID_GND_OPTION:
        
        dta_tmp = my_device.mcu_get_dp_download_data(PID_GND_OPTION, value, length);
        
        __flg_ty_gnd_update = 1;
        __sys_gnd_option = dta_tmp;
        __sys_gnd_option_temp = __sys_gnd_option;
        eeprom_write(EEP_GND_OPTION, __sys_gnd_option);

        my_device.mcu_dp_update(PID_GND_OPTION, __sys_gnd_option, 1);
        buzOn();
        break;
        
        // ----------------------------设置NFC开关-------------------------------------
        case PID_NFC:
        
        dta_tmp = my_device.mcu_get_dp_download_data(PID_NFC, value, length);

        #if M3_22KW
            my_device.mcu_dp_update(PID_NFC, dta_tmp, 1);
            buzOn();
        #else
            my_device.mcu_dp_update(PID_NFC, false, 1);
            buzOn();
            delay(100);
            buzOn();
        #endif
        break;
        
        // ----------------------------恢复出厂设置-------------------------------------
        case PID_FACTORY:
        dta_tmp = my_device.mcu_get_dp_download_data(PID_FACTORY, value, length);
        if(dta_tmp)
        {
            __flg_factory_reset = 1;
        }
        break;
        
        
        
        // ----------------------------重启设备------------------------------------
        case PID_RST:
        dta_tmp = my_device.mcu_get_dp_download_data(PID_RST, value, length);
        if(dta_tmp)
        {
            IWatchdog.begin(100000);       // 看门狗，单位us
            buzOn();
            while(1);
        }

        break;
        
        default:
            break;
    }
    return TY_SUCCESS;
#else
    return 0;
#endif
}

/**
 * @description: Upload all DP status of the current device.
 */

void dp_update_all(void) {
#if TY_VERSION
    my_device.mcu_dp_update(PID_CURRENTSET, __chrg_current_val[__chrg_current], 1);
    my_device.mcu_dp_update(PID_WORK_ST, __work_ty_st, 1);
    pushCurOption();
    int __cur[] = {6, 8, 10, 13, 16, 20, 25, 32};
    my_device.mcu_dp_update(PID_MAX_CUR, __cur[__chrg_current_max], 1);
    pushLang(__langUage);
    pushDeviceInfo();
    pushGndOption(__sys_gnd_option);
    pushChrgMode();
    
    #if TRAVEL_AUTO_CHECK
    if(flg_travel)my_device.mcu_dp_update(PID_PD_VER, 2, 1);          // TBD
    else
    {
        #if M2_11KW||M2_22KW
        my_device.mcu_dp_update(PID_PD_VER, 1, 1);          // TBD
        #else
        my_device.mcu_dp_update(PID_PD_VER, 0, 1);          // TBD
        #endif
    }
    #elif M2_11KW||M2_22KW
    my_device.mcu_dp_update(PID_PD_VER, 1, 1);          // TBD
    #else
    my_device.mcu_dp_update(PID_PD_VER, 0, 1);          // TBD
    #endif
    
#endif
}

// 发送状态,PID-101
int __wstb = -1;
void pushWorkState(int __wst)
{
#if TY_VERSION
    if(__wstb == __wst)return;
    __wstb = __wst;
    __work_ty_st = __wst;
    if(!flgTyConnected)return;
    my_device.mcu_dp_update(PID_WORK_ST, __wst, 1);
#endif
}

void pushMetrics(float vL1, float aL1, float vL2, float aL2, float vL3, float aL3, long t, long d, float e) {
#if TY_VERSION
  // 计算每相的功率并转换为 kW，保留一位小数
  float pL1 = vL1 * aL1 / 1000.0;
  float pL2 = vL2 * aL2 / 1000.0;
  float pL3 = vL3 * aL3 / 1000.0;

  // 计算总功率（千瓦），保留一位小数
  float totalPower = pL1 + pL2 + pL3;

  // 创建 JSON 字符串，使用 snprintf 处理浮点数并限制到一位小数
  char jsonBuffer[256];  // 创建足够大的缓冲区
  int length = sprintf(jsonBuffer,
                        "{\"L1\":[%d,%d,%d],\"L2\":[%d,%d,%d],\"L3\":[%d,%d,%d],\"t\":%d,\"p\":%d,\"d\":%ld,\"e\":%d}",
                        (int)(vL1*10), (int)((aL1+0.05)*10), (int)((pL1+0.05)*10), (int)(vL2*10), (int)((aL2+0.05)*10), (int)((pL2+0.05)*10), (int)(vL3*10), (int)((aL3+0.05)*10), (int)((pL3+0.05)*10), t*10, (int)((totalPower+0.05)*10), d*10, (int)((e+0.05)*10));

  // 输出 JSON 字符串到串口，供调试使用
  Serial.println(jsonBuffer);

  // 调用 my_device.mcu_dp_update，发送数据
  my_device.mcu_dp_update(PID_METRICS, (unsigned char *)jsonBuffer, length);
#endif
}

/*推送自检信息,PID-103
  {'t":"2020-10-20 20:20:20","r":[0,1]}
  r[a, b]
  a - 继电器黏连自检结果， 1通过，0失败
  b - 漏保自检结果，1通过，0失败
*/
void pushSelfCheck(int relay_check, int rcd_check)
{
#if TY_VERSION
  if(!flgTyConnected)return;        // 没连上，返回
  if(!flgGetTimeOk)return;          // 没获取时间
  // 创建 JSON 字符串，使用 snprintf 处理浮点数并限制到一位小数
  char jsonBuffer[256];  // 创建足够大的缓冲区
  int length = sprintf(jsonBuffer,
                        "{\"t\":\"20%02d-%02d-%02d %02d:%02d:%02d\", \"r\":[%d,%d]}",
                        local_time.year, local_time.month, local_time.day, local_time.hour, local_time.minute, local_time.second, relay_check, rcd_check);
  // 调用 my_device.mcu_dp_update，发送数据
  my_device.mcu_dp_update(PID_SELFTEST, (unsigned char *)jsonBuffer, length);
#endif
}

/*推送设置电流, PID-150
*/
void pushCurrentSet(int __cur)
{
#if TY_VERSION
    my_device.mcu_dp_update(PID_CURRENTSET, __cur, 1);
#endif
}


void pushAlarm(int __alarm)
{
#if TY_VERSION
  if(!flgTyConnected)return;        // 没连上，返回
  if(!flgGetTimeOk)return;          // 没获取时间
  char jsonBuffer[256];  // 创建足够大的缓冲区
  int length = sprintf(jsonBuffer,
                        "{\"t\":\"20%02d-%02d-%02d %02d:%02d:%02d\", \"v\":%d}",
                        local_time.year, local_time.month, local_time.day, local_time.hour, local_time.minute, local_time.second, __alarm);
  // 调用 my_device.mcu_dp_update，发送数据
  my_device.mcu_dp_update(PID_ALARM, (unsigned char *)jsonBuffer, length);
#endif
}

// 判断是否为闰年
int isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// 每月的天数
int daysInMonth(int month, int year) {
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;  // 闰年2月有29天
    }
    return days_in_month[month - 1];
}

// 计算自某年初以来的秒数
long timeToSeconds(TUYA_WIFI_TIME time) {
    long total_seconds = 0;
    // 计算年到秒 (假设从 1970 年开始作为基准)
    for (int year = 1970; year < time.year; ++year) {
        total_seconds += isLeapYear(year) ? 366 * 24 * 3600 : 365 * 24 * 3600;
    }
    
    // 计算月到秒
    for (int month = 1; month < time.month; ++month) {
        total_seconds += daysInMonth(month, time.year) * 24 * 3600;
    }
    
    total_seconds += (time.day - 1) * 24 * 3600;
    total_seconds += time.hour * 3600;
    total_seconds += time.minute * 60;
    total_seconds += time.second;
    return total_seconds;
}

// 计算全局变量之间的时间差
long calculateTimeDifference() {
    long start_seconds = timeToSeconds(start_time);
    long local_seconds = timeToSeconds(local_time);
    return local_seconds - start_seconds;  // 返回秒差值
}

/*
需要考虑什么时候上传记录，当前只是简单的在继电器闭合记录当前时间，继电器断开上传
TBD:
应当加上一些判断，如果过于离谱，就不上传，包括：
1. 时间低于1分钟
2. 时间大于96小时
3. 电流低于1度电
4. 电流大于150度
*/
void pushRecord()
{
#if TY_VERSION
  if(!flgTyConnected)return;        // 没连上，返回
  if(!flgGetTimeOk)return;          // 没获取时间
  char jsonBuffer[256];             // 创建足够大的缓冲区
  unsigned long chrg_t = calculateTimeDifference();
  int length = sprintf(jsonBuffer,
                        "{\"t\":\"20%02d-%02d-%02d %02d:%02d:%02d\", \"s\":\"%02d:%02d\",\"e\":\"%02d:%02d\",\"d\":%ld,\"c\":%d}",
                        start_time.year, start_time.month, start_time.day, start_time.hour, start_time.minute, start_time.second, start_time.hour, start_time.minute, local_time.hour, local_time.minute, chrg_t, (int)(__sys_kwh*10));
  // 调用 my_device.mcu_dp_update，发送数据
  if(chrg_t < 60)return;         // 小于1分钟忽略
  if(chrg_t > 345600)return;     // 大于96小时忽略
  if(__sys_kwh < 0.5)return;                      // 充电量小于1度忽略
  if(__sys_kwh > 150)return;                      // 充电量大于150度忽略

  my_device.mcu_dp_update(PID_RECORD, (unsigned char *)jsonBuffer, length);
#endif
}

/*
推送可选电流
*/

/*
#define CHRG_6A                 0
#define CHRG_8A                 1
#define CHRG_10A                2
#define CHRG_13A                3
#define CHRG_16A                4
#define CHRG_20A                5
#define CHRG_25A                6
#define CHRG_32A                7
*/
void pushCurOption()
{
#if TY_VERSION

    char jsonBuffer[256];  // 创建足够大的缓冲区
    int length = 0;
    switch(__chrg_current_max)
    {
        case CHRG_6A:
        length = sprintf(jsonBuffer,"[%d]", 6);
        break;
            
        case CHRG_8A:
        length = sprintf(jsonBuffer,"[%d, %d]", 6, 8);
        break;
            
        case CHRG_10A:
        length = sprintf(jsonBuffer,"[%d, %d, %d]", 6, 8, 10);
        break;
            
        case CHRG_13A:
        length = sprintf(jsonBuffer,"[%d, %d, %d, %d]", 6, 8, 10, 13);
        break;
            
        case CHRG_16A:
        length = sprintf(jsonBuffer,"[%d, %d, %d, %d, %d]", 6, 8, 10, 13, 16);
        break;
            
        case CHRG_20A:
        length = sprintf(jsonBuffer,"[%d, %d, %d, %d, %d, %d]", 6, 8, 10, 13, 16, 20);
        break;
            
        case CHRG_25A:
        length = sprintf(jsonBuffer,"[%d, %d, %d, %d, %d, %d, %d]", 6, 8, 10, 13, 16, 20, 25);
        break;
            
        case CHRG_32A:
        length = sprintf(jsonBuffer,"[%d, %d, %d, %d, %d, %d, %d, %d]", 6, 8, 10, 13, 16, 20, 25, 32);
        break;
    }

    my_device.mcu_dp_update(PID_ADJUST_CUR, (unsigned char *)jsonBuffer, length);
    
#endif
}

/*
推送最大电流
*/
void pushMaxCur(int max_cur)
{
#if TY_VERSION
  int __cur[] = {6, 8, 10, 13, 16, 20, 25, 32};
  my_device.mcu_dp_update(PID_MAX_CUR, __cur[max_cur], 1);
#endif
}

/*推送语言*/
// en de fr it es
void pushLang(int _lang)
{
#if TY_VERSION
  if(_lang == 0)my_device.mcu_dp_update(PID_LANG, (unsigned char*)"en", 2);
  else if(_lang == 1)my_device.mcu_dp_update(PID_LANG, (unsigned char*)"de", 2);
  else if(_lang == 2)my_device.mcu_dp_update(PID_LANG, (unsigned char*)"fr", 2);
  else if(_lang == 3)my_device.mcu_dp_update(PID_LANG, (unsigned char*)"it", 2);
  else if(_lang == 4)my_device.mcu_dp_update(PID_LANG, (unsigned char*)"es", 2);
#endif
}

/*推送设备信息

{ "r": "xxx", "fv": "v1.0.0" }

*/
void pushDeviceInfo()
{
#if TY_VERSION
  char jsonBuffer[256];  // 创建足够大的缓冲区
  int length = sprintf(jsonBuffer,
                        "{\"r\":\"Type B, AC 30mA + DC 6mA\",\"fv\":\"%s\"}",
                        (char*)mcu_ver);
  //调用 my_device.mcu_dp_update，发送数据
  my_device.mcu_dp_update(PID_DINFO, (unsigned char *)jsonBuffer, length);
#endif
}


/*推送接地选项*/
void pushGndOption(int __gnd)
{
#if TY_VERSION
  my_device.mcu_dp_update(PID_GND_OPTION, __gnd, 1);
#endif
}

/*推送倒计时*/

unsigned long delay_t_buf = -1;
void pushDelay(unsigned long delay_t)
{
#if TY_VERSION
    if(!flgTyConnected)return;
    if(__work_ty_st != X_WS_WAIT_DELAY)return;
    
    if(delay_t == delay_t_buf)return;           // 如果数据一致，返回
    delay_t_buf = delay_t;
    my_device.mcu_dp_update(PID_DELAY_T, delay_t, 1);
#endif
}

// 推送下位机工作状态，此dp仅作为调试用
void pushWorkStateDebug(int st)
{
#if TY_VERSION
    if(!flgTyConnected)return;
    my_device.mcu_dp_update(PID_WORK_ST_DBG, st, 1);
#endif
}

/*推送充电模式
x_charge_mode: 上报和下发，设置（获取）充电模式，JSON串，{ m: 0/1/2, dt: 0.5/1, ss: "09:30", se: "13:20" }, 
0: 立即充电 1: 延迟充电，dt为对应的延迟时间，表示多少hours 2: 计划充电，ss: 开始时间, se: 结束时间
delay_t: 1:0.5h, 2:1h, 16:8h
*/
void pushChrgMode()
{
#if TY_VERSION
  char jsonBuffer[256];  // 创建足够大的缓冲区
  int length = sprintf(jsonBuffer,
                        "{\"m\":%d,\"dt\":%d,\"ss\":\"%02d:%02d\",\"se\":\"%02d:%02d\"}",
                        __charge_mode, __t_sch_delay, __t_sch_s_hour, __t_sch_s_min, __t_sch_e_hour, __t_sch_e_min);
  //调用 my_device.mcu_dp_update，发送数据
  my_device.mcu_dp_update(PID_CHARGEMODE, (unsigned char *)jsonBuffer, length);
#endif
}

int rssi_buf = 0;
int getRssi()
{
#if TY_VERSION
    if(!flgTyConnected)return 0;
    
    unsigned char dta_get[500];
    int len = 0;

    unsigned char dta_s[] = {0x55, 0xaa, 0x03, 0x24, 0, 0, 0x26};
    Serial.write(dta_s, 7);

    unsigned long timer_s = millis();
    while(millis()-timer_s < 1000)              // 1s超时
    {
        if(Serial.available())
        {
            dta_get[len++] = Serial.read();
            if(len >= 7)
            {
                if(dta_get[len-7] == 0x55 && dta_get[len-6] == 0xaa && dta_get[len-4] == 0x24)
                {
                    rssi_buf = wifi_rssi;
                    wifi_rssi = dta_get[len-1];
                    if(wifi_rssi > 127)wifi_rssi = wifi_rssi-256;
                    pushWifiSignal(wifi_rssi);
                    return wifi_rssi;
                }
            }
        }
    }
#endif
    return 0;

}


void pushWifiSignal(int __sig)
{
#if TY_VERSION
    if(!flgTyConnected)return;
    my_device.mcu_dp_update(PID_WIFI_SIGNAL, __sig, 1);
#endif
}

// END FILE
