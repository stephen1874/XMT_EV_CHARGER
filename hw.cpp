#include "wiring_constants.h"
#include <Adafruit_NeoPixel.h>
#include <FlashStorage_STM32F1.h>
//#include "CHARGER_CHECKV.h"
#include "hw.h"

// How many NeoPixels are attached to the Arduino?

#define LED_COUNT 9

// Declare our NeoPixel strip object:

#if M3_22KW
Adafruit_NeoPixel strip(LED_COUNT+3, pinWS2812, NEO_GRB + NEO_KHZ800);
#else
Adafruit_NeoPixel strip(LED_COUNT, pinWS2812, NEO_GRB + NEO_KHZ800);
#endif

// 电压的滑动平均参数
const int NUM_READINGS_V1 = 10;         // 滑动窗口的大小
int vol_readings[NUM_READINGS_V1];      // 用于存储读数的数组
int vol_readindex = 0;                  // 用于读取哪个位置
long vol_total = 0;                     // 用于存储总和
int vol_average = 0;                    // 用于存储平均值

// 电流的滑动平均参数
const int NUM_READINGS_A = 10;          // 滑动窗口的大小
int cur_readings[NUM_READINGS_A];       // 用于存储读数的数组
int cur_readindex = 0;                  // 用于读取哪个位置
long cur_total = 0;                     // 用于存储总和
int cur_average = 0;                    // 用于存储平均值

#if M2_11KW || M2_22KW||M3_22KW
int cur_readings2[NUM_READINGS_A];      // 用于存储读数的数组
int cur_readindex2 = 0;                 // 用于读取哪个位置
long cur_total2 = 0;                    // 用于存储总和
int cur_average2 = 0;                   // 用于存储平均值

int cur_readings3[NUM_READINGS_A];      // 用于存储读数的数组
int cur_readindex3 = 0;                 // 用于读取哪个位置
long cur_total3 = 0;                    // 用于存储总和
int cur_average3 = 0;                   // 用于存储平均值
#endif

// EEPROM
void eeprom_init()
{
    EEPROM.init();
}

void eeprom_write(int addr, unsigned char dta)
{
    EEPROM.write(addr, dta);
    EEPROM.commit();
}

unsigned char eeprom_read(int addr)
{
    return EEPROM.read(addr);
}

// 初始化IO
void pinInit()
{
    
#if M2_3KW||M2_7KW
    pinMode(PA10, INPUT_PULLUP);    
    pinMode(PA9, INPUT_PULLUP);         // 如果这个IO拉低，表示在治具上，就不对电压电流进行校准
#endif
    pinMode(PB15, INPUT_PULLUP);
    pinMode(PB14, INPUT_PULLUP);
    pinMode(PB13, INPUT_PULLUP);
    
    pinMode(pinRelayH, OUTPUT);
    pinMode(pinRelayL, OUTPUT);
    digitalWrite(pinRelayH, LOW);
    digitalWrite(pinRelayL, LOW);

#if M3_22KW
    pinMode(pinEBtn, INPUT_PULLUP);
    pinMode(pinELed, OUTPUT);
    digitalWrite(pinELed, LOW);
#endif
    // 漏保上电初始化
    pinMode(pinLbOut, INPUT);         // TRIP配置为输入
    pinMode(pinLbTest, OUTPUT);       // TEST配置为输出
    digitalWrite(pinLbTest, LOW);     // TEST拉低
    pinMode(pinLbZero, OUTPUT);       // ZERO配置为输出

#if RCD_RC
    pinMode(pinLbZero, HIGH);
#else
    pinMode(pinLbZero, HIGH);          // ZERO引脚设置为高电平, 由于单片机IO接了MOS管，漏保的端口与单片机断开电平相反
    delay(120);                       // T1, 延时120ms

    digitalWrite(pinLbZero, LOW);    // ZERO拉低
    delay(75);                        // T2, 持续75ms
    digitalWrite(pinLbZero, HIGH);     // ZERO拉高
#endif
    pinMode(pinCS, OUTPUT);
    digitalWrite(pinCS, LOW);

    pinMode(pinScl, OUTPUT);
    pinMode(pinSda, OUTPUT);
    pinMode(pinRst, OUTPUT);
    pinMode(pinDc, OUTPUT);

    pinMode(pinBL, OUTPUT);
    digitalWrite(pinBL, LOW);

    relayOff();
    pinMode(pinPwm, OUTPUT);
    pinMode(pinDetGnd, INPUT);

    pinMode(adcV, INPUT);
    pinMode(adcI, INPUT);

    pinMode(adcCp, INPUT);

    pinMode(pinBtnA, INPUT);
    pinMode(pinBtnD, INPUT);
    
    pinMode(pinBuz, OUTPUT);

    strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();            // Turn OFF all pixels ASAP
    strip.setBrightness(100); // Set BRIGHTNESS to about 1/5 (max = 255)
    
#if M3_22KW
    strip.setPixelColor(9, 255, 255, 255);
    strip.setPixelColor(10, 255, 255, 255);
    strip.setPixelColor(11, 255, 255, 255);
    strip.show();
#endif

    for(int i=0; i<NUM_READINGS_A; i++)
    {
        cur_readings[i] = 0;
#if M2_22KW||M2_11KW||M3_22KW
        cur_readings2[i] = 0;
        cur_readings2[i] = 0;
#endif
    }

    pinMode(pinDiodeCheck, INPUT_PULLDOWN);         // 必须pulldown，否则低温有问题


#if RELAY_ERROR_EN
    pinMode(RELAY_ERROR1, INPUT);
    pinMode(RELAY_ERROR2, INPUT);
    #if M2_22KW||M2_11KW||M3_22KW
    pinMode(RELAY_ERROR3, INPUT);
    pinMode(RELAY_ERROR4, INPUT);
    #endif
#endif

#if TY_VERSION
    pinMode(WIFI_EN, OUTPUT);
    digitalWrite(WIFI_EN, LOW);
    delay(100);
    digitalWrite(WIFI_EN, HIGH);
#endif
}

int isStop()
{
#if M3_22KW
    return (1-digitalRead(pinEBtn));
#else
    return 0;
#endif
}

// 蜂鸣器
void buzOn()
{
#if M2_3KW||M2_7KW      // 副板上的无源蜂鸣器
    unsigned long timer_s = millis();
    while((millis()-timer_s) < 50)
    {
        digitalWrite(pinBuz, HIGH);
        delayMicroseconds(800);
        digitalWrite(pinBuz, LOW);
        delayMicroseconds(800);
    }
#else                   // 主板上的有源蜂鸣器
    digitalWrite(pinBuz, HIGH);
    delay(50);
    digitalWrite(pinBuz, LOW);
#endif
}

// 继电器控制
int relayStatus = -1;
extern int __battery_st;
extern int __led_move_loca;

void relayOff()
{
    if(relayStatus == 0)return;
    relayStatus = 0;

    //pushWorkState(6);
    //pushRecord();
    timer_relay_off = millis();
    
    pinMode(pinRelayH, OUTPUT);
    pinMode(pinRelayL, OUTPUT);
    digitalWrite(pinRelayH, LOW);
    digitalWrite(pinRelayL, LOW);

    showBattery(0);
    __battery_st = 0;
    __led_move_loca = 0;
}

void relayOn()
{
    if(relayStatus == 1)return;
    relayStatus = 1;
    timer_relay_on = millis();
    pushSelfCheck(1, 1);
    
#if TY_VERSION
    if(flgGetTimeOk)
    {
        start_time = local_time;
    }
#endif

#if M3_22KW
    if(isStop())return;
#endif
    makeIstatic();
    rcdZero();

    pinMode(pinRelayH, OUTPUT);
    pinMode(pinRelayH, OUTPUT);
    digitalWrite(pinRelayH, HIGH);
    digitalWrite(pinRelayL, HIGH);

    // 以下为2024-3-15更新，解决合闸漏电超时的问题。 
    // 如果这里延时200ms，那么会导致pinRelay保持200ms的高电平，即使漏电也不会立刻跳闸
    unsigned long timer_s = millis();

    while(millis()-timer_s < 200)
    {
        if(checkLb())       // 发生了合闸漏电
        {
            digitalWrite(pinRelayH, LOW);
            digitalWrite(pinRelayL, LOW);
            protectRCD();
        }
        feedog();
    }

    pinMode(pinRelayH, INPUT);
}

// 产生PWM
int __current_b = -1;
void makePwm(int __current)
{
    if(__current_b == __current)return;
    __current_b = __current;
    
#if 1
   // analogWrite(pinPwm, 255);
   // return;
#endif
    if(__current == 100)
    {
        analogWrite(pinPwm, 0);
        return;
    }

    if(__current == 0)
    {
        analogWrite(pinPwm, 255);
        return;
    }

    if(__current < 6)__current = 6;
#if M2_11KW || M2_3KW
    if(__current > 16)__current = 16;
#elif M2_7KW || M2_22KW||M3_22KW
    if(__current > 32)__current = 32;
#endif
    float __pwm = 10.0*(float)__current/6.0;
    __pwm = float(__pwm) *255.0/100.0;
    
    analogWrite(pinPwm, 255-(int)__pwm);

}

// 设置灯条
int __r = -1;
int __g = -1;
int __b = -1;
void setRgb(int r, int g, int b)
{
    if(r == __r && g == __g && b == __b)return;
    __r = r;
    __g = g;
    __b = b;

    for(int i=0; i<LED_COUNT; i++)
    {
        strip.setPixelColor(i, r, g, b);
    }
    strip.show();
}

void setRgb(int i, int r, int g, int b)
{
    i = LED_COUNT-1-i;
    strip.setPixelColor(i, r, g, b);
    strip.show();
}

void setLedMove(int __m)
{
    __r = -1;
    __g = -1;
    __b = -1;
    
    for(int i=0; i<__m; i++)
    {
        strip.setPixelColor(LED_COUNT-1-i, 0, 255, 0);
    }
    for(int i=__m; i<9; i++)
    {
        strip.setPixelColor(LED_COUNT-1-i, 0, 0, 0);
    }
    strip.show();
}

int checkLb()
{
    if(!PROTECT_RCD_EN)return 0;
    return flg_lb_happen ? 0 : digitalRead(pinLbOut);
}

// checkGnd
void checkGND()
{
#if !PROTECT_GND_EN
    return;
#endif

    if(flg_init_ok && (millis()-timer_sys_start < 1000))goto START;

    // 每隔500ms检查一次接地
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 500)return;
    timer_s = millis();
    
START:

#if GND_DETECT_ADC
    if(getGndAdc() > GND_DETECT_VALUE)      
    {
        if(flg_gnd_protect_stop)        // 如果处于接地保护停止状态，说明地线恢复了
        {
            flg_gnd_protect_stop = 0;
            showNormal(3);
        }
        __sys_gnd = 1;
        return;
    }
#else
    unsigned long timer_t = millis();
    while((millis()-timer_t) < 20)      // 连续检测20ms
    {
        if(digitalRead(pinDetGnd))
        {
            if(flg_gnd_protect_stop)        // 如果处于接地保护停止状态，说明地线恢复了
            {
                flg_gnd_protect_stop = 0;
                showNormal(3);
            }
            __sys_gnd = 1;
            return;
        }
    }
#endif
    __sys_gnd  = 0;
}

// 获取电压电流接地信息
int volProcess(int v) {
    vol_total = vol_total - vol_readings[vol_readindex];  // 减去旧的读数
    vol_readings[vol_readindex] = v;                      // 读取新的数据并存储
    vol_total = vol_total + vol_readings[vol_readindex];  // 添加新的读数到总和中
    vol_readindex++;            // 移动到下一个位置
    
    // 如果我们到达了数组的末尾，则回到开始位置
    if (vol_readindex >= NUM_READINGS_V1) {
        vol_readindex = 0;
    }

    // 计算平均值
    vol_average = vol_total / NUM_READINGS_V1;

    return vol_average;
}


int curProcess(int v) {
    cur_total = cur_total - cur_readings[cur_readindex];  // 减去旧的读数
    cur_readings[cur_readindex] = v;              // 读取新的数据并存储
    cur_total = cur_total + cur_readings[cur_readindex];  // 添加新的读数到总和中
    cur_readindex++;            // 移动到下一个位置

    // 如果我们到达了数组的末尾，则回到开始位置
    if (cur_readindex >= NUM_READINGS_A) {
        cur_readindex = 0;
    }

    // 计算平均值
    cur_average = cur_total / NUM_READINGS_A;

    return cur_average;
}

#if M2_11KW || M2_22KW || M3_22KW
int curProcess2(int v) {
    cur_total2 = cur_total2 - cur_readings2[cur_readindex2];  // 减去旧的读数
    cur_readings2[cur_readindex2] = v;              // 读取新的数据并存储
    cur_total2 = cur_total2 + cur_readings2[cur_readindex2];  // 添加新的读数到总和中
    cur_readindex2++;            // 移动到下一个位置

    // 如果我们到达了数组的末尾，则回到开始位置
    if (cur_readindex2 >= NUM_READINGS_A) {
        cur_readindex2 = 0;
    }

    // 计算平均值
    cur_average2 = cur_total2 / NUM_READINGS_A;

    return cur_average2;
}

int curProcess3(int v) {
    cur_total3 = cur_total3 - cur_readings3[cur_readindex3];  // 减去旧的读数
    cur_readings3[cur_readindex3] = v;              // 读取新的数据并存储
    cur_total3 = cur_total3 + cur_readings3[cur_readindex3];  // 添加新的读数到总和中
    cur_readindex3++;            // 移动到下一个位置

    // 如果我们到达了数组的末尾，则回到开始位置
    if (cur_readindex3 >= NUM_READINGS_A) {
        cur_readindex3 = 0;
    }

    // 计算平均值
    cur_average3 = cur_total3 / NUM_READINGS_A;

    return cur_average3;
}
#endif

int current_index = 0;      // 不同的时间更新不同的电流信息

int checkSensor(int t)
{
    if(t == 0)goto START;
    if((millis()-timer_btn_last < 2000) && flg_init_ok)return 0;
    if(flg_gnd_protect_stop)return 0;
#if M2_3KW || M2_7KW
    if(cnt_current_error || cnt_vol176_error || cnt_vol264_error)goto START;
#elif M2_11KW || M2_22KW||M3_22KW
    if(cnt_current_error || cnt_current_error2 || cnt_current_error3 || cnt_vol176_error || cnt_vol264_error)goto START;
#endif

    static unsigned long timer_s = millis();
    if(millis()-timer_s < t)return 0;
    timer_s  = millis();

START:

    __sys_temp = getTemp();
    __sys_temp_wall = getTempWall();
    
#if M2_11KW||M2_22KW||M3_22KW
    current_index++;
    if(current_index > 2)current_index = 0;
#elif M2_3KW||M2_7KW
    current_index = 0;
#endif
    unsigned long timer_t = micros();
    int __vol[500];
    int cnt_vol = 0;
    
//-----------------------电压采样并计算，连续采样20ms，一个周期-----------------------------------
    timer_t = micros();
    while(micros()-timer_t < POWER_FREQ)
    {
        __vol[cnt_vol++] = analogRead(adcV);
    }

    delayCountDown();
    
    long sum = 0;
    int cnt2 = 0;

    // 计算均方根值
    for (int i = 0; i < cnt_vol; i++) {
        if (__vol[i] > 0) { // 只统计大于0的数据
            sum += __vol[i] * __vol[i];
            cnt2++;
        }
    }
    sum /= cnt2; // 计算平均平方和
    sum = sqrt(sum); // 开方获取均方根值

    // 如果是第一次运行，则初始化滑动平均数组
    static int flg_first_time = 1;
    if (flg_first_time) {
        flg_first_time = 0;
        for (int i = 0; i < NUM_READINGS_V1; i++) volProcess(sum);
    }

    float v1 = volProcess(sum); // 更新滑动平均值
    float v = 0.515*(float)v1-13.62;        // 2024-3-20送样tuv的3.5kw
    float __v3 = 0.515*(float)sum - 13.62;

    if(__v3 < 80)       // 如果低于80V，快速响应
    {
        for(int i=0; i<NUM_READINGS_V1; i++)
        {
            v1 = volProcess(sum);
        }
    }

#if TY_VERSION      // 如果是涂鸦模式，且电压突然掉落50V以上
    static int flg_off_line_check = 1;
    if(flgTyConnected && (millis()-timer_sys_start > 2000) && (v - __v3 > 50))      // 突然掉落50V以上，可能是离线了，赶紧上传一个离线的dp
    {
        int workst_buf = __work_ty_st;
        pushWorkState(X_WS_OFF_LINE);
        delayFeed(2000);            
        pushWorkState(workst_buf);      // 过了2s还有点，说明是无检测，不见进行此项检测
        flg_off_line_check = 0;
    }
#endif

#if ENABLE_VOLTAGE_CALIBRATION
    __sys_voltage = makeVolCorrect(v);
#else
    __sys_voltage = v*__sys_voltage_jz;
#endif
    if(__sys_voltage < 20)__sys_voltage  = 0;
    
//-----------------------电流采样并计算，连续采样20ms，一个周期-----------------------------------
    timer_t = micros();
    cnt_vol = 0;
    while(micros()-timer_t < POWER_FREQ)
    {
        // 电流采样
        switch(current_index)
        {
            case 0:
            __vol[cnt_vol++] = analogRead(adcI)-i_static1;
            break;
#if M2_11KW||M2_22KW||M3_22KW
            case 1:
            __vol[cnt_vol++] = analogRead(adcI2)-i_static2;
            break;
            
            case 2:
            __vol[cnt_vol++] = analogRead(adcI3)-i_static3;
            break;
#endif   
            default:;
        }
    }
    
    delayCountDown();
    
    sum = 0;
    cnt2 = 0;
    
    // 计算均方根值
    for(int i=0; i<cnt_vol; i++)
    {
#if M2_3KW||M2_7KW
        sum += __vol[i]*__vol[i];
        cnt2++;
#elif M2_11KW||M2_22KW||M3_22KW
        if(__vol[i] > 0)
        {
            sum += __vol[i]*__vol[i];
        }
        cnt2++;
#endif
    }
        
    sum /= cnt2;
    sum = sqrt(sum);

    switch(current_index)
    {
        case 0:
        
        v1 = curProcess(sum);
        
        if(abs(v1-sum) > 5)
        {
            for(int i=0; i<NUM_READINGS_A; i++)
            {
                v1 = curProcess(sum);
            }
        }
    
        __sys_current1 = (float)v1*0.135233-0.021557;
#if M2_22KW||M2_11KW||M3_22KW
        __sys_current1 *= 1.67;
#endif
        __sys_current1 *= __sys_current_jz;
        if(__sys_current1 < 1.0)__sys_current1 = 0;
        break;

#if M2_11KW||M2_22KW||M3_22KW
        case 1:
        v1 = curProcess2(sum);
        
        if(abs(v1-sum) > 5)
        {
            for(int i=0; i<NUM_READINGS_A; i++)
            {
                v1 = curProcess2(sum);
            }
        }
        
        __sys_current2 = (float)v1*0.135233-0.021557;
#if M2_22KW||M2_11KW||M3_22KW
        __sys_current2 *= 1.67;
#endif
        __sys_current2 *= __sys_current_jz2;

        if(__sys_current2 < 1.0)__sys_current2 = 0;

        break;
        
        case 2:
        v1 = curProcess3(sum);
        
        if(abs(v1-sum) > 5)
        {
            for(int i=0; i<NUM_READINGS_A; i++)
            {
                v1 = curProcess3(sum);
            }
        }
        __sys_current3 = (float)v1*0.135233-0.021557;
#if M2_22KW||M2_11KW||M3_22KW
        __sys_current3 *= 1.67;
#endif
        __sys_current3 *= __sys_current_jz3;
        
        if(__sys_current3 < 1.0)__sys_current3 = 0;
        
        break;
#endif
        default:;
    }
    
    if(relayStatus == 0)            // 如果继电器没有闭合，恒定为0
    {
        __sys_current1 = 0;
#if M2_11KW||M2_22KW||M3_22KW
        __sys_current2 = 0;
        __sys_current3 = 0;
#endif
    }
    
    return 1;
}

void float2char(float a, unsigned char *str) {
    // 创建一个指向浮点数的指针
    unsigned char *bytePtr = (unsigned char*)(&a);

    // 从浮点数的每个字节复制数据
    for (int i = 0; i < sizeof(float); i++) {
        str[i] = bytePtr[i];
    }
}

float char2float(unsigned char *str) {
    float result;

    // 创建一个指向结果浮点数的指针
    unsigned char *bytePtr = (unsigned char*)(&result);

    // 将字节数组复制到浮点数的字节中
    for (int i = 0; i < sizeof(float); i++) {
        bytePtr[i] = str[i];
    }

    return result;
}


int getTemp()
{
    float B = 3950;
    float R0 = 10000;
    float a = analogRead(pinTemp);
    float R = R0*a/(1024-a);
    float temp = 1.0/(log(R/R0)/B+1.0/298.15)-273.15;

#if M2_22KW
    if(temp >= 40) temp = map(temp, 40, 100, 40, 80);
#endif
    
    return (int)temp;
}


// 1-墙插温感插入，0-墙插温感没插入
int isTempWallInsert()      // 是否插入温感
{
#if PROTECT_TEMP_WALL_EN&&M2_3KW
    float a = 0;
    for(int i=0; i<8; i++)
    {
        a += (float)analogRead(pinTempWall);
    }
    a /= 8.0;
    
    if(a < 10)return 0;
    return 1;
#else
    return 1;
#endif
}

int wallTempInsert()
{
#if TRAVEL_AUTO_CHECK
    float B = 3950;
    float R0 = 10000;
    
    float a = 0;
    for(int i=0; i<16; i++)
    {
        a += (float)analogRead(pinTempWall);
    }
    a /= 16.0;
    if(a>900)return 0;
    return 1;
#else
    return 0;
#endif
}

int getTempWall()
{
#if PROTECT_TEMP_WALL_EN&&M2_3KW                    // 模式2 3kw
    if(isTempWallInsert() == 0)return 25;           // 如果没插，就不检测了
    
    float B = 3950;
    float R0 = 10000;
    float a = 0;
    for(int i=0; i<16; i++)
    {
        a += (float)analogRead(pinTempWall);
    }
    a /= 16.0;
    
    if(a<1.0)a = 1.0;
    
    float V0 = a/1023.0*3.3;
    
    float R = 3.3*R0/V0-R0;

    float temp = 1.0/(log(R/R0)/B+1.0/298.15)-273.15;
    return (int)temp;
#elif TRAVEL_AUTO_CHECK
    float B = 3950;
    float R0 = 10000;
    
    float a = 0;
    for(int i=0; i<16; i++)
    {
        a += (float)analogRead(pinTempWall);
    }
    a /= 16.0;
    
    if(a<1.0)a = 1.0;
    
    float R = R0*a/(1024-a);
    float temp = 1.0/(log(R/R0)/B+1.0/298.15)-273.15;
    return (int)temp;
#else 
    return 25;
#endif
}

void ledOff(){setRgb(0, 0, 0);}

void ledInsert()
{
    setRgb(0, 0, 255);
    tyWifiLogoBlink();
}

void ledWorking()
{
    setRgb(0, 255, 0);
    tyWifiLogoBlink();
}

void ledError()
{
    setRgb(255, 0, 0);
    tyWifiLogoBlink();
}

void ledIdle()
{
#if TY_VERSION
    if(!flgTyBlueBlink || !flg_ty_en)setRgb(255, 255, 255);
#else
    if(1)setRgb(255, 255, 255);
#endif
    else
    {
        static unsigned long timer_ty = millis();
        static unsigned int flg_led_ty = 1;

        if(millis()-timer_ty > 500)
        {
            timer_ty = millis();
            flg_led_ty = 1-flg_led_ty;
            setRgb(0, 0, 255*flg_led_ty);
            tyWifiLogoBlink();
        }
    }
}

void tyWifiLogoBlink()
{
#if TY_VERSION
    if(flgTyBlueBlink && flg_ty_en)
    {
        static unsigned long timer_s = millis();
        if(millis()-timer_s < 500)return;
        timer_s = millis();
        
        static int __st = 0;
        __st = 1-__st;
        showWifiConnecting(__st);
    }
#endif
}

void ledErrorBlink()
{
    static unsigned long timer_s = millis();
    if(millis()-timer_s < 500)return;
    timer_s = millis();
    static unsigned int led_st = 0;
    led_st = 1-led_st;
    setRgb(255*led_st, 0, 0);
    tyWifiLogoBlink();
}

int getBtnA()     // 按下设置电流按键
{
    return (digitalRead(pinBtnA) == BTN_ON);
}

int getBtnC()     // 按下延时按键
{
    return (digitalRead(pinBtnD) == BTN_ON);
}

int checkDiode()
{
#if DIODE_CHECK_EN
    if(__work_status == ST_WORKING)return 1;      // 如果已经处于工作模式，不再进入
    
#if !MODE_TUV
    if(getTemp() > 50)return 1;         // 如果大于50摄氏度，不再进入
#endif

    int cnt_fail = 0;
    
START:

    for(int i=0; i<20; i++)
    {
        int cnt_check1_h = 0;
        int cnt_check1_l = 0;
        unsigned long timer_t = millis();
        while(millis()-timer_t < 10)
        {
            if(digitalRead(pinDiodeCheck))cnt_check1_h++;
            else cnt_check1_l++;
        }
        if(cnt_check1_l < 10)
        {
            cnt_fail++;
            if(cnt_fail > 10)return 0;
            goto START;
        }
    }
    return 1;
#else
    return 1;
#endif
}

// 继电器黏连检测，0无黏连，1黏连
void relayErrorCheck()
{
#if RELAY_ERROR_EN
    int __max1 = 0;
    int __min1 = 2000;
    int __max2 = 0;
    int __min2 = 2000;
    int __max3 = 0;
    int __min3 = 2000;
    int __max4 = 0;
    int __min4 = 2000;
        
    unsigned long timer_s = micros();
        
    while(micros()-timer_s < 10000)
    {
        feedog();
        int a = analogRead(RELAY_ERROR1);
            
        if(a>__max1)__max1 = a;
        if(a<__min1)__min1 = a;
            
        a = analogRead(RELAY_ERROR2);
            
        if(a>__max2)__max2 = a;
        if(a<__min2)__min2 = a;
#if M2_11KW||M2_22KW||M3_22KW
        a = analogRead(RELAY_ERROR3);
            
        if(a>__max3)__max3 = a;
        if(a<__min3)__min3 = a;
        
        a = analogRead(RELAY_ERROR4);
            
        if(a>__max4)__max4 = a;
        if(a<__min4)__min4 = a;
#endif
    }
    int d1 = __max1 - __min1;
    int d2 = __max2 - __min2;
#if M2_11KW||M2_22KW||M3_22KW
    int d3 = __max3 - __min3;
    int d4 = __max4 - __min4;
#endif
    
#if M2_11KW||M2_22KW||M3_22KW
    if(d1>100 || d2>100 || d3>100 || d4>100)
#else
    if(d1>100 || d2>100)
#endif
    {
        relayOff();                 // 关闭继电器
        showErrorImg(ERROR_CODE_RELAY);           // 显示错误信息
        ledError();                 // 显示黄色LED
        makePwm(0);                 // 输出-12V
        checkGND();
        if(__sys_gnd)showGndOk();
        else showGndFail();
        
        pushSelfCheck(0, 1);
        
        while(1)
        {
            ledErrorBlink();
            checkSensor(100);
            taskDisplay();
            checkGND();
            feedog();
        }
    }
#endif
}

// 检查是否有L2，如果有，说明是三相电
int L2Check()
{
#if L2_CHECK
    int __max1 = 0;
    int __min1 = 2000;
        
    unsigned long timer_s = micros();
    while(micros()-timer_s < 20000)
    {
        feedog();
        int a = analogRead(pinL2Check);
            
        if(a>__max1)__max1 = a;
        if(a<__min1)__min1 = a; 
    }
    return (__max1 - __min1)>100?1:0;
#endif
}


void rcdSelfCheck()        // 漏保自检
{
#if PROTECT_RCD_EN

#if RCD_SX    // 盛相漏保
    protectRCD();

    unsigned long timer_rsch = millis();
    digitalWrite(pinLbTest, HIGH);
    delay(150);
    
    unsigned long timer_lbc = millis();
    int flg_lb_check_ok = 0;
    while(millis()-timer_lbc < 400)
    {
        if(digitalRead(pinLbOut))                   // 自检通过
        {
            flg_lb_check_ok = 1;
            while(millis()-timer_rsch < 400)
            {
                feedog();
            }
            digitalWrite(pinLbTest, LOW);
            unsigned long timer_w = millis();
            while(digitalRead(pinLbOut))           // 等待漏保输出恢复低电平
            {
                if(millis()-timer_w > 1000) // 超时，自检错误
                {
                    flg_lb_check_ok = 0;
                    break;
                }
                feedog();
            }
            break;
        }
        feedog();
    }
    
    if(flg_lb_check_ok == 0)
    {
        relayOff();                             // 关闭继电器
        showErrorImg(ERROR_CODE_RCDERROR);      // 显示错误信息
        ledError();                             // 显示黄色LED
        makePwm(0);                             // 输出-12V
        checkGND();
        if(__sys_gnd)showGndOk();
        else showGndFail();
        
        pushSelfCheck(1, 0);
        while(1)
        {
            ledErrorBlink();
            checkSensor(100);
            taskDisplay();
            checkGND();
            feedog();
        }
    }

#elif RCD_RC   // 瑞磁漏保

    digitalWrite(pinLbZero, LOW);
    delay(80);
    digitalWrite(pinLbZero, HIGH);       // Zero拉低80ms

    delay(550);                         // 等待550ms

START:
    
    int flg_lb_check_ok = 0;
    digitalWrite(pinLbTest, HIGH);      // 自检拉高
    delay(160);                         // 140ms后读取漏保输出
    
    unsigned long timer_t = millis();
    while(millis()-timer_t < 100)       // 检测100ms
    {
        if(digitalRead(pinLbOut) == HIGH)
        {
            flg_lb_check_ok++;
            break;
        }
    }
    
    if(flg_lb_check_ok == 0)
    {
        digitalWrite(pinLbTest, LOW);      // 自检拉高
        goto END;
    }
    
    delay(260);
    digitalWrite(pinLbTest, LOW);       // TEST持续400ms后，拉低
    
    delay(160);                         // 150ms后读取漏保输出
    
    timer_t = millis();
    while(millis()-timer_t < 100)       // 检测100ms
    {
        if(digitalRead(pinLbOut) == LOW)
        {
            flg_lb_check_ok++;
            break;
        }
    }

    if(flg_lb_check_ok >= 2)
    {
        return;         // 自检成功，返回
    }
    
END:

    if(flg_lb_check_ok == 0)
    {
        relayOff();                             // 关闭继电器
        showErrorImg(ERROR_CODE_RCDERROR);      // 显示错误信息
        ledError();                             // 显示黄色LED
        makePwm(0);                             // 输出-12V
        checkGND();
        if(__sys_gnd)showGndOk();
        else showGndFail();
        
        while(1)
        {
            ledErrorBlink();
            checkSensor(100);
            taskDisplay();
            checkGND();
            feedog();
        }
    }
#endif    

#endif
}

void saveFloat2EEPROM(int addr, float v_jz) {
    unsigned char bytes[4];

    // 将浮点数转换为字节数组
    float2char(v_jz, bytes);

    // 将每个字节写入EEPROM
    for (int i = 0; i < 4; i++) {
        eeprom_write(i+addr, bytes[i]);
    }
}

float loadFloatFromEEPROM(int addr) {
    unsigned char bytes[4];

    // 从EEPROM读取每个字节
    for (int i = 0; i < 4; i++) {
        bytes[i] = eeprom_read(i+addr);
    }

    // 将字节数组转换回浮点数
    return char2float(bytes);
}

// 利用综合测试仪的电流自校准
void autoJzA()
{
#if AUTO_JZA
    if(millis() > 60000)return;             // 60s后不再进入
    if(__sys_chrg_time <= 15)return;        // 应为15
    
    if(eeprom_read(EEP_IF_AJZ) == 0x55)return;

#if M2_3KW
    if(__sys_current1 > 11.0)
    {
        __sys_current_jz = 16.1/__sys_current1;
        saveFloat2EEPROM(EEP_A_JZ, __sys_current_jz);
        eeprom_write(EEP_IF_AJZ, 0x55);
        
        buzOn();
        delayFeed(100);
        buzOn();
        delayFeed(100);
        buzOn();
        
        showCurrent(10.0*__sys_current_jz);
        delayFeed(1000);;
    }
#elif M2_7KW
    if(__sys_current1 > 25.0)
    {
        __sys_current_jz = 32.0/__sys_current1;
        saveFloat2EEPROM(EEP_A_JZ, __sys_current_jz);
        eeprom_write(EEP_IF_AJZ, 0x55);
        
        buzOn();
        delayFeed(100);
        buzOn();
        delayFeed(100);
        buzOn();
        
        showCurrent(10.0*__sys_current_jz);
        delayFeed(1000);;
    }
#elif M2_11KW
    if(__sys_current1 > 11.0 && __sys_current2 > 11.0 && __sys_current3 > 11.0)
    {
        __sys_current_jz = 16.1/__sys_current1;
        __sys_current_jz2 = 16.1/__sys_current2;
        __sys_current_jz3 = 16.1/__sys_current3;
        
        saveFloat2EEPROM(EEP_A_JZ, __sys_current_jz);
        saveFloat2EEPROM(EEP_A2_JZ, __sys_current_jz2);
        saveFloat2EEPROM(EEP_A3_JZ, __sys_current_jz3);
        
        eeprom_write(EEP_IF_AJZ, 0x55);
        
        buzOn();
        delayFeed(100);
        buzOn();
        delayFeed(100);
        buzOn();
        
        showCurrent(10.0*__sys_current_jz);
        delayFeed(1000);;
    }
#elif M2_22KW||M3_22KW
    if(__sys_current1 > 20.0 && __sys_current2 > 20.0 && __sys_current3 > 20.0)
    {
        __sys_current_jz = 32.0/__sys_current1;
        __sys_current_jz2 = 32.0/__sys_current2;
        __sys_current_jz3 = 32.0/__sys_current3;
        
        saveFloat2EEPROM(EEP_A_JZ, __sys_current_jz);
        saveFloat2EEPROM(EEP_A2_JZ, __sys_current_jz2);
        saveFloat2EEPROM(EEP_A3_JZ, __sys_current_jz3);
        
        eeprom_write(EEP_IF_AJZ, 0x55);
        
        buzOn();
        delayFeed(100);
        buzOn();
        delayFeed(100);
        buzOn();
        
        showCurrent(10.0*__sys_current_jz);
        delayFeed(1000);;
    }
#endif

#endif
}

void feedog()
{
#if WATCH_DOG_EN
    IWatchdog.reload();
#endif
    tyProcess();
    
    if(__flg_factory_reset)         // 恢复出厂设置
    {
        buzOn();

        for(int i=0; i<100; i++)
        {
            tyConfig();
            tyProcess();
            delay(10);
        }
        eeprom_write(EEP_FIRST_RUN, 0x11);      // 清除EEPROM数据
        IWatchdog.begin(100000);                 // 看门狗，单位us
        while(1);
    }
}

void delayFeed(unsigned long ms)
{
    unsigned long timer_s = millis();
    while(millis()-timer_s < ms)
    {
        feedog();
        tyProcess();
    }
}

// 是否设置了预约，且不再预约范围
// 1, 设置了预约，且不在预约范围，不充电
// 0, 没有设置预约，或者在预约规定的范围，充电

int isWaitSchedule()           // 是否可以开始充电，如果是否处于计划的时间里头
{
    
#if TY_VERSION
    if(!__flg_schedule_set)return 0;        // 如果没有设置预约，返回1
    
#if TEST_VERSION
    if(millis()-timer_get_time_ok > 15000)
#else
    if(millis()-timer_get_time_ok > 300000)
#endif
    {
        __charge_mode = 0;
        __flg_schedule_set = 0;
        eeprom_write(EEP_SCH_SET, __flg_schedule_set);       // 取消计划充电
        showDelay(0);
        flg_net_stop = 1;
        return 0;      // 连续5分钟获取不到时间
    }
    
    int __t_now = 60*local_time.hour + local_time.minute;
    int __t_start = 60*__t_sch_s_hour+__t_sch_s_min;
    int __t_end = 60*__t_sch_e_hour + __t_sch_e_min;
    
    if(__t_end == __t_start)return 0;           // 如果相等，直接充电
    else if(__t_end > __t_start)                // 结束时间>开始时间，同一天
    {
        //0000-0800
        if((__t_now >= __t_start) && (__t_now< __t_end))return 0;       // 可以充电
        else return 1;  // 不充电
    }
    else        // 开始时间>结束时间，跨天
    {
        if(__t_now >= __t_start)return 0;       // 可以充电
        else if(__t_now < __t_end)return 0;     // 不充电
        else return 1;
    }
    
    return 0;
#else
    return 1;
#endif
}

// 漏保归零
void rcdZero()
{
    digitalWrite(pinLbZero, HIGH);
    delay(80);
    digitalWrite(pinLbZero, LOW);       // Zero拉低80ms
}

/*
  6A - 56k - 800
  8A - 22k - 660
  10A - 12k - 540
  13A - 6.2k - 360
  16A - 3.3k - 250
  32A - 1k - 106
  
#define CHRG_6A                 0
#define CHRG_8A                 1
#define CHRG_10A                2
#define CHRG_13A                3
#define CHRG_16A                4
#define CHRG_20A                5
#define CHRG_25A                6
#define CHRG_32A                7

*/

/*
    如果检测到墙插电阻，但是没有Travel电阻，那么强制把max设为16A
*/
int getTravelCurrent()
{
#if TRAVEL_AUTO_CHECK
    unsigned long sum = 0;
    for(int i=0; i<16; i++)
    {
        sum += analogRead(A14);
    }
    sum /= 16;
    
    if(sum > 50 && sum < 178)
    {
        if(MAX_CURRENT == 32)return CHRG_32A;       // 32A
        else return CHRG_16A;
    }
    else if(sum > 178 && sum < 305)return CHRG_16A;   // 16A
    else if(sum > 305 && sum < 450)return CHRG_13A;   // 13A
    else if(sum > 450 && sum < 600)return CHRG_10A;   // 10A
    else if(sum > 600 && sum < 730)return CHRG_8A;    // 8A
    else if(sum > 730 && sum < 850)return CHRG_6A;    // 6A
    //else return 100;
    else
    {
        if(wallTempInsert())        // 检测到温度
        {
            return CHRG_16A;
        }
        else return 100;
    }
#else
    return CHRG_16A;
#endif
}

#if GND_DETECT_ADC
int getGndAdc()
{
    int __max = 0;
    unsigned long timer_s = micros();
    while(micros()-timer_s < 20000)
    { 
        int a = analogRead(A11);
        if(a>__max)__max = a;
    }
    
    return __max;
}
#endif
// END FILE