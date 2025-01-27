#include "fun.h"

unsigned long timer_pov;        // 过压保护计时器
unsigned long timer_lov;        // 欠压保护计时器
unsigned long timer_gnd;        // 接地保护计时器
unsigned long timer_oc;         // 过流保护计时器
int flg_pov = 0;                // 过压保护标志位0-没有过压保护，1-开始过压保护等待5s, 2-已经过压保护
int flg_pot = 0;                // 过温保护标志位，0没有触发过温保护，1. 过温断电，2-过温降流
int flg_pot_wall = 0;           // 墙插过温保护标志位
int flg_lov = 0;                // 过压保护标志位0-没有过压保护，1-开始过压保护等待5s, 2-已经过压保护
int flg_gnd_p = 0;              // 接地保护标志位012
int flg_oc  = 0;                // 过流保护标志位
int cnt_oc  = 0;                // 过流保护计数器，如果有两次过流保护，就停止充电
int flg_nodiode = 0;            // 二极管确实保护标志位
int flgGetTimeOk = 0;

// 检查车插入的状况
// 返回值1 - 有变化， 0没有变化

// 辅助函数：用于排序数组
void sort(int *array, int n) {
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (array[j] > array[j+1]) {
                // 交换元素
                int temp = array[j];
                array[j] = array[j+1];
                array[j+1] = temp;
            }
        }
    }
}

// 计算中位数
int getMiddle(int *str, int len) {
    // 对数组进行排序
    sort(str, len);
    return (len % 2 != 0) ? str[len/2] : (str[len/2 - 1] + str[len/2]) / 2;
}

PinName p = analogInputToPinName(adcCp);
int flg_cp_error = 0;

float getCpVoltage()
{
    int val[200];
    unsigned long sum = 0;
    unsigned long cnt = 0;
    
    unsigned long timer_s1 = micros();
    while(micros()-timer_s1 < 4000)        // 采样2ms的平均值
    {
        int a = adc_read_value(p, 10);
        if(a > 50)
        {
            val[cnt++] = a;
        }
    }
    if(cnt < 2)sum = 0;
    else sum = getMiddle(val, cnt);
    
    return ((float)sum/4096.0*3.3);            // cp电压
}

int checkInsert()
{
    if(!flg_init_ok)return 0;        // 开始1s后再执行
    if(flg_lb_happen)return 0;
    
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 10)return 0;                // 每隔10ms检查一次
    timer_s = millis();

    if(__sys_voltage < 50)return 0;           // 交流电压小于50V，可能在调试程序

    float v = getCpVoltage();            // cp电压
    
    static int cnt_cp_error = 0;
    
    //showCurrent(v);
    
    if(v > 2.1)                                 // 没有插入车, 2.5
    {
        if(flg_cp_error == 1)
        {
            flg_cp_error = 0;
            changeStatus(ST_IDLE);
            showNormal(3);
        }
        __car_status = CAR_NO_INSERT;            // 空闲
        relayOff();
        makePwm(100);
        cnt_cp_error = 0;
    }
    else if(v > 1.50)                 // 车已插入, 1.7
    {
        if(__car_status != CAR_INSERT)      // 原本不是这个状态，突然切换过来的
        {
            unsigned long timer_ttt = millis();
            while(millis()-timer_ttt < 20)     // 连续检测500ms
            {
                feedog();
                v = getCpVoltage();
                
                if(v>2.25)return 0;
                if(v<1.55)return 0;  
            }
        }
        
        __car_status = CAR_INSERT;
        if(flg_cp_error == 1)
        {
            flg_cp_error = 0;
            changeStatus(ST_IDLEINS);
            showNormal(3);
        }
        relayOff();
        cnt_cp_error = 0;
    }
#if MODE_TUV
    else if(v > 0.9)                 // 可以充电状态, 1.0
#else
    else if(v > 0.4)                 // 可以充电状态, 1.0
#endif
    {
        /*
        if(__car_status != CAR_READY)      // 原本不是这个状态，突然切换过来的
        {
            ledInsert();
            unsigned long timer_ttt = millis();
            while(millis()-timer_ttt < 500)     // 连续检测500ms
            {
                feedog();
                v = getCpVoltage();
                
                if(v>1.55)return 0;
                if(v<0.9)return 0;
            }
        }
        */
        __car_status = CAR_READY;
        
        if(flg_cp_error == 1)
        {
            flg_cp_error = 0;
            changeStatus(ST_WORKING);
            showNormal(3);
        }
        cnt_cp_error = 0;
    }
    else
    {
        if(flg_nodiode)return 0;
        cnt_cp_error++;
        if(cnt_cp_error > 10)           // 连续10次才算
        {
            // 如果设置为6A，那么把电流短暂设置为10A，再测一次
            makePwm(10);
            delay(11);
            v = getCpVoltage();
            
            if(v > 0.9)     // 测错了
            {
                makePwm(__chrg_current_val[__chrg_current]);         // 把电流设置回去
                cnt_cp_error = 0;
                return 0;
            }
            relayOff();         // 先关闭继电器

            // 以下为2024-3-15更新，解决cp阻抗异常继电器断开后依然有pwm的问题
            makePwm(100);       // 关闭PWM
            __car_status = CAR_ERROR;
            flg_cp_error = 1;
            showErrorImg(ERROR_CODE_CP);
        }
    }
    
    return 1;
}

// 过温保护
// ≥80摄氏度停止充电
// 温度降到75摄氏度降档开始充电
// 温度降到70摄氏度以下满功率充电
int origin_charg_cur = 0;           // 用来存放原始的充电电流
void protectTemp()
{
#if PROTECT_TEMP_EN
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 1000)return;
    timer_s = millis();
    
    // 如果没有触发过温保护同时不在充电中，不进入
    if((flg_pot == 0) && (__work_status != ST_WORKING))return;
    
    if((flg_pot == 0) && (getTemp() >= 80))         // 温度超过80度
    {
        ledError();
        relayOff();
        flg_pot = 1;
        makePwm(100);
        changeStatus(ST_ERRORPAUSE);
        showNormal(1);
        showErrorImg(ERROR_CODE_OT);
        origin_charg_cur = __chrg_current;
    }
    
    if(flg_pot == 1 && getTemp() <= 75)             // 如果已经触发过温保护，温度降到75度后降流充电
    {
        flg_pot = 2;
        relayOn();
        showNormal(1);
        changeStatus(ST_WORKING);
        __chrg_current = origin_charg_cur-1;
        if(__chrg_current < 0)__chrg_current = 0;
        showAset(__chrg_current_val[__chrg_current]);
        makePwm(__chrg_current_val[__chrg_current]); 
    }
    
    if(flg_pot && getTemp() < 70)              // 如果已经处于过温保护并且温度低于70，恢复正常状态
    {
        flg_pot = 0;
        changeStatus(ST_WORKING);
        __chrg_current = origin_charg_cur;
        showAset(__chrg_current_val[__chrg_current]);
        makePwm(__chrg_current_val[__chrg_current]);
        showNormal(0);
    }
#endif
}

void protectTempWall()
{
#if PROTECT_TEMP_WALL_EN||TRAVEL_AUTO_CHECK

#if TRAVEL_AUTO_CHECK
    if(flg_travel == 0)return;
#endif
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 1000)return;
    timer_s = millis();
    
    // 如果没有触发过温保护同时不在充电中，不进入
    if((flg_pot_wall == 0) && (__work_status != ST_WORKING))return;
    
    if((flg_pot_wall == 0) && (getTempWall() >= 68))         // 温度超过68度
    {
        ledError();
        relayOff();
        flg_pot_wall = 1;
        makePwm(100);
        changeStatus(ST_ERRORPAUSE);
        showNormal(1);
        showErrorImg(ERROR_CODE_WALL_TMP);
        origin_charg_cur = __chrg_current;
    }
    
    if(flg_pot_wall == 1 && getTempWall() <= 60)             // 如果已经触发过温保护，温度降到60度后降流充电
    {
        flg_pot_wall = 2;
        relayOn();
        showNormal(1);
        changeStatus(ST_WORKING);
        __chrg_current = origin_charg_cur-1;
        if(__chrg_current < 0)__chrg_current = 0;
        showAset(__chrg_current_val[__chrg_current]);
        makePwm(__chrg_current_val[__chrg_current]); 
    }
    
    if(flg_pot_wall && getTempWall() < 55)              // 如果已经处于过温保护并且温度低于55，恢复正常状态
    {
        flg_pot_wall = 0;
        changeStatus(ST_WORKING);
        __chrg_current = origin_charg_cur;
        showAset(__chrg_current_val[__chrg_current]);
        makePwm(__chrg_current_val[__chrg_current]);
        showNormal(0);
    }
#endif
}

// 过压保护
void protectOverVol()
{
#if PROTECT_OVER_VOLTAGE_EN
    if(!flg_init_ok)return;
    if((millis()-timer_sys_start) < 2000)return;
    
    if(flg_pov == 2 && __work_status == ST_ERRORPAUSE)
    {
        if(__sys_voltage <= (VAL_PROTECT_OVER_VOL-2))
        {
            flg_pov = 0;
            changeStatus(ST_IDLE);
            makePwm(__chrg_current_val[__chrg_current]);            // 开启PWM
            showNormal(3);
            return;
        }
    }
    
    if(__work_status != ST_WORKING)return;              // 如果不是充电中，退出
    
    if((__sys_voltage > VAL_PROTECT_OVER_VOL) && (flg_pov == 0))
    {
        flg_pov = 1;
        timer_pov = millis();
    }
    
    int ovTime = 5000;

    if((flg_pov==1) && ((millis()-timer_pov) > ovTime))            // 连续超过5s
    {
        
#if ENABLE_VOLTAGE_CALIBRATION
        // 如果还没有存入V280，此处存入
        if((voltage_cali_ok == 0) && (eeprom_read(EEP_IF_VHIGH)!=0x55))
        {
            if(__sys_voltage>260 && __sys_voltage<300)      // 需要处于这个范围
            {
                eeprom_write(EEP_IF_VHIGH, 0x55);
                saveFloat2EEPROM(EEP_VHIGH, (float)__sys_voltage);
                
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
            }
        }
#endif
        ledError();
        relayOff();
        flg_pov = 2;
        makePwm(100);
        changeStatus(ST_ERRORPAUSE);
        showErrorImg(ERROR_CODE_OV);
    }
    
    // 如果在等待阶段电压恢复了
    if(flg_pov == 1 && __sys_voltage <= VAL_PROTECT_OVER_VOL)            // 自行恢复了
    {
        flg_pov = 0;
        timer_pov = millis();
    }
#endif
}

// 欠压保护
void protectLowVol()
{
#if PROTECT_LESS_VOLTAGE_EN
    if(!flg_init_ok)return;
    if((millis()-timer_sys_start) < 2000)return;
    if(flg_lov == 2 && __work_status == ST_ERRORPAUSE)
    {
        if(__sys_voltage >= (VAL_PROTECT_LESS_VOL+2))
        {
            flg_lov = 0;
            changeStatus(ST_IDLE);
            makePwm(__chrg_current_val[__chrg_current]);            // 开启PWM
            showNormal(3);
            return;
        }
    }
    
    if(__work_status != ST_WORKING)return;                          // 如果不是充电中，退出
    
    if((__sys_voltage < VAL_PROTECT_LESS_VOL) && (flg_lov == 0))
    {
        flg_lov = 1;
        timer_lov = millis();
    }
    
    //int lcTime = 5000;
    // 2024-3-15更新，解决认证机构提出的5V/s速率下降的时候，误认为最低工作电压60V的问题
    int lcTime = 5000;

    if((flg_lov==1) && ((millis()-timer_lov) > lcTime))             // 连续超过100ms
    {
#if ENABLE_VOLTAGE_CALIBRATION
        // 如果还没有存入V70，此处存入
        if((voltage_cali_ok == 0) && (eeprom_read(EEP_IF_VLOW)!=0x55))
        {
            if(__sys_voltage>50 && __sys_voltage<90)              // 需要处于这个范围
            {
                eeprom_write(EEP_IF_VLOW, 0x55);
                saveFloat2EEPROM(EEP_VLOW, (float)__sys_voltage);
                makeVolCaliABC();
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
            }
        }
#endif
        ledError();
        relayOff();
        flg_lov = 2;
        makePwm(100);
        showErrorImg(ERROR_CODE_LV);
        changeStatus(ST_ERRORPAUSE);
    }
    
    // 如果在等待阶段电压恢复了
    if(flg_lov == 1 && __sys_voltage >= VAL_PROTECT_LESS_VOL)            // 自行恢复了
    {
        //ledWorking();
        flg_lov = 0;
        timer_lov = millis();
    }
#endif
}

// 接地保护
void protectGnd()
{
#if PROTECT_GND_EN
    if(__sys_gnd_option_temp == 1)return;      // 直接充电

    if(flg_gnd_p == 2 && __work_status == ST_ERRORPAUSE)
    {
        if(__sys_gnd)
        {
            flg_gnd_p = 0;
            changeStatus(ST_IDLE);
            makePwm(__chrg_current_val[__chrg_current]);            // 开启PWM
            showNormal(3);
            return;
        }
    }
    
    if(__work_status != ST_WORKING)return;              // 如果不是充电中，退出
    
    if((!__sys_gnd) && (flg_gnd_p == 0))
    {
        flg_gnd_p = 1;
        timer_gnd = millis();
    }
    
    if((flg_gnd_p==1) && ((millis()-timer_gnd) > 5000))            // 连续超过5s
    {
        ledError();
        relayOff();
        flg_gnd_p = 2;
        makePwm(100);
        showErrorImg(ERROR_CODE_GND1);
        changeStatus(ST_ERRORPAUSE);
        showGndFail();
#if ENABLE_VOLTAGE_CALIBRATION
        // 如果还没有存入V218，此处存入
        if((voltage_cali_ok == 0) && (eeprom_read(EEP_IF_V218)!=0x55))
        {
            if(__sys_voltage>200 && __sys_voltage<238)      // 需要处于这个范围
            {
                eeprom_write(EEP_IF_V218, 0x55);
                saveFloat2EEPROM(EEP_V218, (float)__sys_voltage);
                
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
            }
        }
#endif
        // 等待选择
        while(1)
        {
            // 选择继续充电
            feedog();
            checkSensor(100);
            taskDisplay();
            if(getBtnA())
            {
                delay(10);
                if(getBtnA())
                {
                    buzOn();
                    showNormal(3);
                    __sys_gnd_option_temp = 1;           // 继续充电
                }

                while(getBtnA()){feedog();}
                return;
            }

            // 选择停止充电
            if(getBtnC())
            {
                delay(10);
                if(getBtnC())
                {
                    buzOn();
                    delayFeed(100);
                    buzOn();
                }

                flg_gnd_protect_stop = 1;
                showErrorImg(ERROR_CODE_GND2);
                __sys_gnd_option_temp = 2;
                return;
            }

            checkGND();

            if(__sys_gnd)
            {
                showNormal(3);
                showGndOk();
                return;
            }
            updateSleepTimer();
        }
    }
    
    // 如果等待阶段GND回复了
    if(flg_gnd_p == 1 && __sys_gnd)            // 自行恢复了
    {
        //ledWorking();
        flg_gnd_p = 0;
        timer_lov = millis();
    }
#endif
}

void protectOverCur()
{
#if PROTECT_OVER_CURRENT_EN
    if(flg_oc == 2 && ((millis()-timer_oc) > 60000))           
    {
        flg_oc = 0;
        changeStatus(ST_IDLE);
        makePwm(__chrg_current_val[__chrg_current]);    // 开启PWM
        showNormal(3);
        return;
    }
    
    if(__work_status != ST_WORKING)return;              // 如果不是充电中，退出

    if((__sys_current1 > ((float)__chrg_current_val[__chrg_current]+(float)__chrg_current_over[__chrg_current])) && (flg_oc == 0))
    {
        flg_oc = 1;
        timer_oc = millis();
    }
    
    if((flg_oc==1) && ((millis()-timer_oc) > 5000))            // 连续超过5s断电
    {
        ledError();
        relayOff();
        flg_oc = 2;
        makePwm(100);
        changeStatus(ST_ERRORPAUSE);
        showErrorImg(ERROR_CODE_OC);
        cnt_oc++;
        
        if(cnt_oc == 1)
        {
            __chrg_current--;
            if(__chrg_current < 0)__chrg_current = 0;
            showAset(__chrg_current_val[__chrg_current]);
        }
        else if(cnt_oc == 2)
        {
            //makePwm(0);             // CP设为-12V
            analogWrite(pinPwm, 0);
            showCurrent(0);
            showKw(0);
            while(1){feedog();}               // 死机
        }
    }
    
    // 如果在等待阶段电流恢复
    if(flg_oc == 1 && (__sys_current1 <= ((float)__chrg_current_val[__chrg_current]+(float)__chrg_current_over[__chrg_current])))            // 自行恢复了
    {
        //ledWorking();
        flg_oc = 0;
        timer_oc = millis();
    }
#endif
}

void protectRCD()
{
#if PROTECT_RCD_EN

    if(!flg_init_ok)return;
    if(checkLb())
    {
        if(__work_status == ST_IDLE)
        {
            buzOn();
#if MODE_TUV
            if(!checkLb())return;
#else
            for(int i=0; i<5; i++)
            {
                if(!checkLb())return;
                delayFeed(10);
            }
#endif
        }
        flg_lb_happen = 1;
        relayOff();                     // 关闭继电器
        showErrorImg(ERROR_CODE_RCD);   // 显示错误信息
        ledError();                     // 显示红色LED
        makePwm(0);                     // 输出-12V
        while(1)
        {
            ledErrorBlink();
            checkGND();                 // 检查接地状态
            checkSensor(100);           // 检查传感器，每隔100ms检查一次
            taskDisplay();              // 显示任务
            feedog();
        }
    }
#endif
}

int __work_status_b = -1;
// 状态切换，如果有变化返回1
int changeStatus(int st)
{
    __work_status_b = __work_status;
    __work_status = st;
    
    pushWorkStateDebug(st);
    
    if(__work_status != __work_status_b)
    {
        timer_status_change = millis();
        clearShowBuf();
        return 1;
    }
    return 0;
}

void protectOverVolIdle()
{
#if PROTECT_OVER_VOLTAGE_EN
    if(__work_status != ST_IDLE)return;
    if(__sys_voltage <= VAL_PROTECT_OVER_VOL)return;
    if((millis()-timer_sys_start) < 2000)return;
    showErrorImg(ERROR_CODE_OV);
    ledError();
    // 等待电压恢复
    
#if ENABLE_VOLTAGE_CALIBRATION
        // 如果还没有存入V280，此处存入
        if((voltage_cali_ok == 0) && (eeprom_read(EEP_IF_VHIGH)!=0x55))
        {
            if(__sys_voltage>260 && __sys_voltage<300)      // 需要处于这个范围
            {
                eeprom_write(EEP_IF_VHIGH, 0x55);
                saveFloat2EEPROM(EEP_VHIGH, (float)__sys_voltage);
                
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
            }
        }
#endif
    while(1)
    {
        if(__sys_voltage <= (VAL_PROTECT_OVER_VOL-2))
        {
            delay(1);
            showNormal(3);
            return;
        }
        updateSleepTimer();
        checkSensor(100);
        taskDisplay();
        feedog();
    }
#endif
}

int cnt_protect_low_vol = 0;

void protectLowVolIdle()
{
#if PROTECT_LESS_VOLTAGE_EN
    
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 1000)return;
    timer_s = millis();

    if(!flg_init_ok)return;
    if((millis()-timer_sys_start) < 2000)return;
    if(__work_status != ST_IDLE)return;
    if(__sys_voltage >= VAL_PROTECT_LESS_VOL)
    {
        cnt_protect_low_vol = 0;
        return;
    }
    if(millis()<5000)return;            // 开机5s内不触发
    
    cnt_protect_low_vol++;
    
    if(cnt_protect_low_vol < 3)return;
    
    showErrorImg(ERROR_CODE_LV);
#if ENABLE_VOLTAGE_CALIBRATION
        // 如果还没有存入V70，此处存入
        if((voltage_cali_ok == 0) && (eeprom_read(EEP_IF_VLOW)!=0x55))
        {
            if(__sys_voltage>50 && __sys_voltage<90)              // 需要处于这个范围
            {
                eeprom_write(EEP_IF_VLOW, 0x55);
                saveFloat2EEPROM(EEP_VLOW, (float)__sys_voltage);
                makeVolCaliABC();
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
                buzOn();
                delayFeed(100);
            }
        }
#endif

    delayFeed(1000);
    ledError();
    // 等待电压恢复
    while(1)
    {
        updateSleepTimer();
        if(__sys_voltage >= (VAL_PROTECT_LESS_VOL+2))
        {
            delay(1);
            showNormal(3);
            return;
        }
        checkSensor(100);
        taskDisplay();
        feedog();
    }
#endif
}

void protectGndIdle()
{
    if(!flg_init_ok)return;
    if(millis()-timer_sys_start < 2000)return;
    
    if(__work_status != ST_IDLE)return;
    if(__sys_gnd != 0)return;
    if(flg_gnd_protect_stop && (__flg_ty_gnd_update==0))return;
    
    if(__flg_ty_gnd_update && flg_gnd_protect_stop && (__sys_gnd_option==1))
    {
        __flg_ty_gnd_update = 0;
        buzOn();
        showNormal(3);
        ledIdle();
        return;
    }

    if(__sys_gnd_option_temp == 0)       // 提示用户选择, 如果选择A，继续充电，如果选择C，进入错误显示界面
    {
        showErrorImg(ERROR_CODE_GND1);
        
        ledError();
        showGndFail();
        __flg_ty_gnd_update = 0;
#if TY_VERSION
        __work_ty_st = X_WS_GND;
#endif
        // 等待选择
        while(1)
        {
            // 选择继续充电
            delayFeed(2);
            updateTime();
#if ENABLE_VOLTAGE_CALIBRATION
            // 如果还没有存入V218，此处存入
            static int gnd_calc_once = 1;
            if(gnd_calc_once && (voltage_cali_ok == 0)/* && (eeprom_read(EEP_IF_V218)!=0x55)*/)
            {
                gnd_calc_once = 0;
                if(__sys_voltage>200 && __sys_voltage<245)              // 需要处于这个范围
                {
                    eeprom_write(EEP_IF_V218, 0x55);
                    saveFloat2EEPROM(EEP_V218, (float)__sys_voltage);
                    makeVolCaliABC();
                    buzOn();
                    delayFeed(100);
                    buzOn();
                    delayFeed(100);
                    buzOn();
                    delayFeed(100);
                }
            }
#endif
            if(__flg_ty_gnd_update)         // app改变了接地选项
            {
                if(__sys_gnd_option == 1)       // 直接充电
                {
                    buzOn();
                    showNormal(3);
                    ledIdle();
                    return;
                }
                else if(__sys_gnd_option == 2)  // 拒绝充电
                {
                    goto CANCEL_CHARGE;
                }
            }
            updateSleepTimer();
            feedog();
            checkSensor(100);
            taskDisplay();
            ledErrorBlink();            // 红灯闪烁
            if(getBtnA())
            {
                delay(10);
                if(getBtnA())
                {
                    buzOn();
                    showNormal(3);
                    __sys_gnd_option_temp = 1;              // 继续充电
                    ledIdle();                              // LED变为白色
                }

                while(getBtnA()){feedog();}
                return;
            }

            // 选择停止充电
            if(getBtnC())
            {
                delay(10);
                if(getBtnC())
                {
CANCEL_CHARGE:
                    buzOn();
                    delayFeed(100);
                    buzOn();
                }

                flg_gnd_protect_stop = 1;
                __sys_gnd_option_temp = 2;
                showErrorImg(ERROR_CODE_GND2);
                return;
            }

            checkGND();

            if(__sys_gnd)
            {
                showNormal(3);
                showGndOk();
                return;
            }
            
            #if TY_VERSION
            // 以下每隔5s上传当前状态
            static unsigned long timer_update_status_5s = millis();       
            if(millis()-timer_update_status_5s > 1000)     // 每隔5s进入一次
            {
                timer_update_status_5s = millis();
                static int flg_push_once_st = 1;
                static int flg_push_once_alarm = 1;
                if(flgTyConnected && flg_push_once_st)      // 如果涂鸦已经连接
                {
                    flg_push_once_st = 0;
                    my_device.mcu_dp_update(PID_WORK_ST, __work_ty_st, 1);  // 上传当前状态
                }
                
                if(flgTyConnected && flg_push_once_alarm && flgGetTimeOk)
                {
                    flg_push_once_alarm = 0;
                    pushAlarm(507);
                }
            }
            #endif
        }
    }
    else if(__sys_gnd_option_temp == 1)  // 正常充电
    {
        return;
    }
    else if(__sys_gnd_option_temp == 2)  // 不充电，进入错误显示界面
    {
        flg_gnd_protect_stop = 1;
        showErrorImg(ERROR_CODE_GND2);
        return;
    }
}

void updateTime()
{
#if TY_VERSION
    if(!flgTyConnected)
    {
        flgGetTimeOk = 0;
        return;
    }

    static unsigned long timer_s = millis();
    
    if(millis()-timer_s < 1000)return;
    
    unsigned long delay_t = 0;
    
    if((__work_ty_st == X_WS_WAIT_DELAY) || (__work_ty_st == X_WS_WAIT_SCHEDULE) || (__work_status == ST_WORKING))delay_t = 1000;
    else if(flgGetTimeOk == 0)delay_t = 1000;
    else delay_t = 10000;
    
    timer_s = millis();

    if (TY_SUCCESS == my_device.get_rtc_time(&local_time, 10)) {
        timer_get_time_ok = millis();
        
        if(flg_net_stop)
        {
            flg_net_stop = 0;
            __charge_mode = 2;
            __flg_schedule_set = 1;                                 // 恢复预约充电状态
            eeprom_write(EEP_SCH_SET, __flg_schedule_set);          // 取消计划充电
            pushChrgMode();
        }
        flgGetTimeOk = 1;
    } else {
        flgGetTimeOk = 0;
    }
#else
    flgGetTimeOk = 1;
#endif
}

void stopMode()
{
#if M3_22KW
    if(isStop())
    {
        delay(10);
        if(isStop())
        {
            showErrorImg(ERROR_CODE_STOP);
            showCarOff();
            ledError();
            relayOff();
            makePwm(100);
            digitalWrite(pinELed, HIGH);
            // 等待电压恢复
            while(1)
            {
                updateSleepTimer();
                checkSensor(100);
                taskDisplay();
                feedog();
                makePwm(0);
                
                if(!isStop())
                {
                    delay(10);
                    if(!isStop())
                    {
                        changeStatus(ST_IDLE);
                        relayOff();
                        makePwm(100);            // 开启PWM
                        showNormal(3);
                        ledIdle();
                        digitalWrite(pinELed, LOW);
                        return;
                    }
                }
            }
        }
    }
#endif
}



// END FILE