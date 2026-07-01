#include <reg51.h>

/*
 *
 * 交通灯模块按路口方向成组亮灯：
 * P1.2：横向通行组合灯，对应 LED4、LED7、LED12、LED15。
 * P1.3：黄灯组合灯，对应 LED5、LED8、LED11、LED14。
 * P1.4：竖向通行组合灯，对应 LED6、LED9、LED10、LED13。
 *
 * 拨码开关功能：
 * SW2-1 / S0 -> P1.5：临时强制横向通行。
 * SW2-2 / S1 -> P1.6：临时强制黄灯。
 * SW2-3 / S2 -> P1.7：临时强制竖向通行。
 *
 * 拨码开关拨上去后：暂停定时器，灯切到目标状态，倒计时数字保持不变。
 * 拨码开关拨下来后：恢复拨上去前的灯状态，并从原来的数字继续倒计时。
 *
 * 数码管显示模块保持原接线：DATA--DATA，CLK--CLK。
 * 定时使用定时器0中断实现，不使用 delay 空循环。
 */

#define uchar unsigned char

#define STATE_H 1      /* 横向通行 */
#define STATE_Y 2      /* 黄灯过渡 */
#define STATE_V 3      /* 竖向通行 */

#define SWITCH_ACTIVE_LOW 1
/* 功能参数：横向/竖向通行 10 秒，黄灯 5 秒，100 个 10ms 中断合成 1 秒。 */

#define START_SECONDS 10

#define YELLOW_SECONDS 5

#define ONE_SECOND_TICKS 100

/* 12MHz 晶振下，定时器0 10ms 初值：65536 - 10000 = 0xD8F0。 */
#define TIMER0_RELOAD_H 0xD8
#define TIMER0_RELOAD_L 0xF0

/* 三档拨码开关输入。 */
sbit KEY_H = P1^5;
sbit KEY_Y = P1^6;
sbit KEY_V = P1^7;

/* 交通灯模块的三个组合状态输出。 */
sbit LAMP_H = P1^2;
sbit LAMP_Y = P1^3;
sbit LAMP_V = P1^4;

uchar code table[] = {
    0xFC, 0x60, 0xDA, 0xF2,
    0x66, 0xB6, 0xBE, 0xE0,
    0xFE, 0xF6, 0xEE, 0x3E,
    0x9C, 0x7A, 0x9E, 0x8E
};

uchar state;              /* 当前正常状态：横向、黄灯、竖向。 */
uchar countDown;          /* 当前倒计时数值。 */
uchar nextAfterYellow;    /* 黄灯结束后进入横向还是竖向。 */
uchar savedState;         /* 拨码强制前保存的正常状态。 */
uchar savedNextAfterYellow; /* 拨码强制前保存的黄灯后续方向。 */
uchar overrideKey;        /* 0 表示未强制；非 0 表示当前被拨码强制。 */
volatile uchar tick10ms;  /* 定时器0累加的 10ms 次数。 */
volatile uchar secFlag;   /* 1 秒到达标志，由中断置 1，主循环清 0。 */

void portInit(void)
{
    /* P1 准双向口：输入位先写 1，释放引脚，方便拨码开关拉低。 */
    KEY_H = 1;
    KEY_Y = 1;
    KEY_V = 1;

    LAMP_H = 0;
    LAMP_Y = 0;
    LAMP_V = 0;
}

/*

 * 功能实现点1：定时器初始化。

 * 设置定时器0为方式1，并开启定时器0中断。

 */

void timer0Init(void)

{
    TMOD &= 0xF0;
    TMOD |= 0x01;         /* 定时器0方式1，16 位定时。 */

    TH0 = TIMER0_RELOAD_H;
    TL0 = TIMER0_RELOAD_L;

    ET0 = 1;
    EA = 1;
    TR0 = 1;
}

/*

 * 功能实现点2：定时器中断。

 * 每 10ms 进入一次中断，累计 100 次后置 secFlag=1，表示 1 秒到。

 * 中断里只做计数和置标志，复杂逻辑放在主循环，效率更高。

 */

void timer0Isr(void) interrupt 1

{
    TH0 = TIMER0_RELOAD_H;
    TL0 = TIMER0_RELOAD_L;

    tick10ms++;
    if(tick10ms >= ONE_SECOND_TICKS)
    {
        tick10ms = 0;
        secFlag = 1;
    }
}

void putch(uchar n)
{
    SBUF = n;
    while(TI == 0);
    TI = 0;
}

/*

 * 功能实现点3：数码管显示倒计时。

 * n/10 得到十位，n%10 得到个位，通过 SBUF 发送给显示模块。

 */

void display(uchar n)

{
    putch(table[n / 10]);
    putch(table[n % 10]);
}

bit switchOn(bit pinValue)
{
#if SWITCH_ACTIVE_LOW
    return pinValue == 0;
#else
    return pinValue == 1;
#endif
}

/*

 * 功能实现点4：读取三档拨码开关。

 * 只允许一个开关有效：开关1横向，开关2黄灯，开关3竖向。

 */

uchar readKey(void)

{
    bit h, y, v;

    h = switchOn(KEY_H);
    y = switchOn(KEY_Y);
    v = switchOn(KEY_V);

    if(h && !y && !v) return STATE_H;
    if(y && !h && !v) return STATE_Y;
    if(v && !h && !y) return STATE_V;

    return 0;
}

/*

 * 功能实现点5：切换交通灯状态。

 * newState 决定点亮横向通行、黄灯、竖向通行中的哪一组。

 */

void setLamp(uchar newState)

{
    /* 只改 P1.2、P1.3、P1.4，避免影响 P1.5~P1.7 的拨码输入。 */
    LAMP_H = 0;
    LAMP_Y = 0;
    LAMP_V = 0;

    state = newState;

    if(state == STATE_H)
        LAMP_H = 1;
    else if(state == STATE_Y)
        LAMP_Y = 1;
    else
        LAMP_V = 1;
}

void resetSecondTimer(void)
{
    tick10ms = 0;
    secFlag = 0;
}

/*

 * 功能实现点6：不同状态的持续时间。

 * 黄灯返回 5 秒，横向/竖向通行返回 10 秒。

 */

uchar stateSeconds(uchar newState)

{
    if(newState == STATE_Y)
        return YELLOW_SECONDS;

    return START_SECONDS;
}

void enterState(uchar newState)
{
    setLamp(newState);
    countDown = stateSeconds(newState);
    resetSecondTimer();
    display(countDown);
}

/*

 * 功能实现点7：正常交通灯轮换。

 * 横向通行结束 -> 黄灯 -> 竖向通行；竖向通行结束 -> 黄灯 -> 横向通行。

 */

void nextState(void)

{
    if(state == STATE_H)
    {
        nextAfterYellow = STATE_V;
        enterState(STATE_Y);
    }
    else if(state == STATE_V)
    {
        nextAfterYellow = STATE_H;
        enterState(STATE_Y);
    }
    else
    {
        enterState(nextAfterYellow);
    }
}

/*

 * 功能实现点8：拨码开关拨上去后的临时强制状态。

 * 保存原灯状态，停止定时器 TR0=0，切到目标灯，倒计时数字保持不变。

 */

void startOverride(uchar key)

{
    /* 保存原状态，暂停定时器，只切换灯，不改变 countDown。 */
    savedState = state;
    savedNextAfterYellow = nextAfterYellow;
    overrideKey = key;

    TR0 = 0;
    resetSecondTimer();
    setLamp(key);
    display(countDown);
}

/*

 * 功能实现点9：拨码开关拨下来后的恢复。

 * 恢复原灯状态，保持 countDown，重新启动定时器 TR0=1。

 */

void stopOverride(void)

{
    /* 恢复拨码前状态，保持 countDown，从当前数字继续倒计时。 */
    setLamp(savedState);
    nextAfterYellow = savedNextAfterYellow;
    overrideKey = 0;

    resetSecondTimer();
    display(countDown);
    TR0 = 1;
}

void main(void)
{
    uchar key;

    portInit();

    state = STATE_H;
    nextAfterYellow = STATE_V;
    savedState = STATE_H;
    savedNextAfterYellow = STATE_V;
    overrideKey = 0;
    tick10ms = 0;
    secFlag = 0;

    enterState(STATE_H);
    timer0Init();

    while(1)

    {

        /* 主循环第一步：持续扫描拨码开关，看是否需要暂停并强制显示。 */

        key = readKey();

        if(key != 0)
        {
            if(overrideKey == 0)
                startOverride(key);
            else if(key != overrideKey)
            {
                overrideKey = key;
                setLamp(key);
                display(countDown);
            }
            continue;
        }

        if(overrideKey != 0)
        {
            stopOverride();
            continue;
        }

        /* 主循环第二步：没有拨码强制时，才根据 1 秒标志更新倒计时。 */

        if(secFlag)

        {
            secFlag = 0;

            if(countDown == 0)
                nextState();
            else
            {
                countDown--;
                display(countDown);
            }
        }
    }
}