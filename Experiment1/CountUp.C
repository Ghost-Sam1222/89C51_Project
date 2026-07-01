#include <reg51.h>

/*
 * 本程序在一个 C 文件中同时实现正计数和倒计时功能。
 * 显示模块接线不变：P3.0--DATA，P3.1--CLK。
 *
 * SW2 第 1 位 / S0 接 P1.0：正计数，00 到 99 后回到 00。
 * SW2 第 2 位 / S1 接 P1.1：倒计时，60 到 00 后回到 60。
 * 两个开关都不拨，或者两个都拨上去：显示 00，不计数。
 *
 * 定时要求：本程序不用软件空循环延时，改用 8051 定时器0实现定时。
 * 晶振按开发板常见的 12MHz 计算，定时器0每 10ms 产生一次中断。
 */

#define COUNT_DELAY_TICKS 100  /* 秒表速度参数：100 个 10ms 节拍约为 1 秒。 */
#define SWITCH_ACTIVE_LOW 1    /* 1 表示拨码 ON 为低电平；0 表示拨码 ON 为高电平。 */

#define TIMER0_RELOAD_H 0xD8   /* 12MHz 晶振下，定时器0 10ms 初值高字节。 */
#define TIMER0_RELOAD_L 0xF0   /* 12MHz 晶振下，定时器0 10ms 初值低字节。 */

/* 用 sbit 把 SW2 的两路开关映射到 P1 口的两个引脚，方便直接读取。 */
sbit SW_UP = P1^0;
sbit SW_DOWN = P1^1;

/* 数码管显示模块使用的段码表，下标 0-9 对应数字 0-9。 */
unsigned char code table[] = {
    0xFC, 0x60, 0xDA, 0xF2, 0x66, 0xB6, 0xBE, 0xE0,
    0xFE, 0xF6, 0xEE, 0x3E, 0x9C, 0x7A, 0x9E, 0x8E
};

unsigned char Count;       /* 当前要显示的计数值。 */
unsigned char Mode;        /* 当前模式：0 停止，1 正计数，2 倒计时。 */
unsigned char LastMode;    /* 上一次的模式，用来判断拨码开关是否刚刚改变。 */
volatile unsigned int TimerTicks;  /* 定时器0中断累加的 10ms 节拍数。 */

void timer0Init(void)
{
    /*
     * TMOD 的低 4 位控制定时器0。
     * 0001B 表示定时器0工作在方式1，即 16 位定时器模式。
     */
    TMOD &= 0xF0;
    TMOD |= 0x01;

    /* 装入 10ms 定时初值：65536 - 10000 = 55536 = 0xD8F0。 */
    TH0 = TIMER0_RELOAD_H;
    TL0 = TIMER0_RELOAD_L;

    ET0 = 1;    /* 允许定时器0中断。 */
    EA = 1;     /* 打开总中断。 */
    TR0 = 1;    /* 启动定时器0。 */
}

void timer0Isr(void) interrupt 1
{
    /*
     * 定时器0溢出后会进入这里。
     * 每次进入代表约 10ms 到达一次，需要重新装入初值。
     */
    TH0 = TIMER0_RELOAD_H;
    TL0 = TIMER0_RELOAD_L;

    TimerTicks++;
}

void waitTimerTicks(unsigned int ticks)
{
    unsigned int start;

    /*
     * 记录当前定时器节拍，然后等待节拍差达到指定值。
     * 这里的等待依据来自定时器0中断，不再是软件空循环延时。
     */
    start = TimerTicks;
    while((unsigned int)(TimerTicks - start) < ticks);
}

void putch(unsigned char n)
{
    /* 通过原实验程序的串口方式，向显示模块发送 1 个字节段码。 */
    SBUF = n;

    /* 等待发送完成。这里查询的是串口发送标志 TI，不是定时器中断。 */
    while(TI == 0);

    /* 软件清零 TI，准备发送下一个字节。 */
    TI = 0;
}

void display(unsigned char n)
{
    /*
     * 本实验显示模块一次显示两位数。
     * 先发送十位段码，再发送个位段码。
     */
    putch(table[n / 10]);
    putch(table[n % 10]);
}

bit switchOn(bit pinValue)
{
    /*
     * 把端口读到的电平转换成“开关是否拨上去”的逻辑结果。
     * 若低电平有效，则端口读到 0 表示开关 ON。
     */
#if SWITCH_ACTIVE_LOW
    return pinValue == 0;
#else
    return pinValue == 1;
#endif
}

unsigned char getMode(void)
{
    bit upOn;
    bit downOn;

    /* Debug 单步观察点：拨动 SW2-1/SW2-2 后，看 P1.0 和 P1.1 的值是否变化。 */
    upOn = switchOn(SW_UP);
    downOn = switchOn(SW_DOWN);

    /* 只有 SW2-1 拨上去：选择正计数模式。 */
    if(upOn && !downOn)
        return 1;

    /* 只有 SW2-2 拨上去：选择倒计时模式。 */
    if(downOn && !upOn)
        return 2;

    /* 都不拨或都拨上去时，不进入计数模式。 */
    return 0;
}

void main(void)
{
    Count = 0;
    LastMode = 0;
    TimerTicks = 0;

    timer0Init();

    while(1)
    {
        /* 每轮循环都读取一次拨码开关，所以运行中也能切换模式。 */
        Mode = getMode();

        /*
         * 如果检测到模式改变，就把 Count 设置为该模式的起始值。
         * 单步调试时重点看 Mode、LastMode、Count 三个变量。
         */
        if(Mode != LastMode)
        {
            if(Mode == 1)
                Count = 0;
            else if(Mode == 2)
                Count = 60;
            else
                Count = 0;

            LastMode = Mode;
        }

        if(Mode == 1)
        {
            /* 正计数：先显示当前 Count，再用定时器0节拍等待，最后更新 Count。 */
            display(Count);
            waitTimerTicks(COUNT_DELAY_TICKS);

            /* 两位数码管最大显示到 99，所以到 99 后回到 00。 */
            if(Count >= 99)
                Count = 0;
            else
                Count++;
        }
        else if(Mode == 2)
        {
            /* 倒计时：从 60 开始，每次定时到达后减 1，直到 00。 */
            display(Count);
            waitTimerTicks(COUNT_DELAY_TICKS);

            /* 到 00 后重新回到 60，继续循环倒计时。 */
            if(Count == 0)
                Count = 60;
            else
                Count--;
        }
        else
        {
            /* 默认状态：显示 00，等待拨码开关选择正计数或倒计时。 */
            display(0);
            waitTimerTicks(COUNT_DELAY_TICKS);
        }
    }
}