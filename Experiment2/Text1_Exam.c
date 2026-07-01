#include <reg51.h>

/*
 * 实验二交通灯：应试讲解精简版
 *
 * 接线不变：
 * P3.0 -> 数码管 DATA，P3.1 -> 数码管 CLK。
 * P1.2 -> 横向通行灯组：LED4、LED7、LED12、LED15。
 * P1.3 -> 黄灯灯组：LED5、LED8、LED11、LED14。
 * P1.4 -> 竖向通行灯组：LED6、LED9、LED10、LED13。
 * P1.5 / P1.6 / P1.7 -> 拨码开关 1 / 2 / 3，低电平有效。
 *
 * 正常状态：横向通行10s -> 黄灯5s -> 竖向通行10s -> 黄灯5s，循环。
 * 拨码开关拨上：暂停定时器，切到指定灯组，倒计时数字保持。
 * 拨码开关拨下：恢复拨上前的灯组，从原数字继续倒计时。
 */

#define uchar unsigned char

#define H 1          /* 横向通行 */
#define Y 2          /* 黄灯 */
#define V 3          /* 竖向通行 */

#define NORMAL_SEC 10
#define YELLOW_SEC 5
#define TICKS_1S 100    /* 100 个 10ms = 1s */

/* 12MHz 晶振：定时器0每 10ms 溢出一次，65536-10000=0xD8F0。 */
#define TH0_INIT 0xD8
#define TL0_INIT 0xF0

sbit KEY_H = P1^5;
sbit KEY_Y = P1^6;
sbit KEY_V = P1^7;

sbit LAMP_H = P1^2;
sbit LAMP_Y = P1^3;
sbit LAMP_V = P1^4;

uchar code seg[] = {
    0xFC,0x60,0xDA,0xF2,0x66,0xB6,0xBE,0xE0,
    0xFE,0xF6,0xEE,0x3E,0x9C,0x7A,0x9E,0x8E
};

uchar state = H;          /* 当前正常灯态 */
uchar count = NORMAL_SEC; /* 当前倒计时 */
uchar next = V;           /* 黄灯结束后去哪个方向 */
uchar hold = 0;           /* 0=正常运行；非0=拨码强制状态 */
uchar oldState, oldNext;  /* 拨码前保存的状态 */
volatile uchar tick = 0;
volatile bit oneSecond = 0;

void sendByte(uchar x)
{
    SBUF = x;
    while(TI == 0);
    TI = 0;
}

void show(uchar n)
{
    sendByte(seg[n / 10]);
    sendByte(seg[n % 10]);
}

/* 功能点1：定时器0。每10ms中断一次，凑够100次后通知主循环“1秒到了”。 */
void timer0(void) interrupt 1
{
    TH0 = TH0_INIT;
    TL0 = TL0_INIT;

    if(++tick >= TICKS_1S)
    {
        tick = 0;
        oneSecond = 1;
    }
}

void timerInit(void)
{
    TMOD = (TMOD & 0xF0) | 0x01;  /* 定时器0方式1：16位定时 */
    TH0 = TH0_INIT;
    TL0 = TL0_INIT;
    ET0 = 1;
    EA = 1;
    TR0 = 1;
}

/* 功能点2：灯态输出。只改 P1.2~P1.4，不影响 P1.5~P1.7 拨码输入。 */
void lamp(uchar s)
{
    LAMP_H = 0;
    LAMP_Y = 0;
    LAMP_V = 0;

    if(s == H) LAMP_H = 1;
    else if(s == Y) LAMP_Y = 1;
    else LAMP_V = 1;
}

uchar stateTime(uchar s)
{
    if(s == Y) return YELLOW_SEC;
    return NORMAL_SEC;
}

void enter(uchar s)
{
    state = s;
    count = stateTime(s);
    tick = 0;
    oneSecond = 0;
    lamp(s);
    show(count);
}

/* 功能点3：正常交通灯顺序。横向后进黄灯，黄灯后进竖向；竖向后同理。 */
void nextState(void)
{
    if(state == H)
    {
        next = V;
        enter(Y);
    }
    else if(state == V)
    {
        next = H;
        enter(Y);
    }
    else
    {
        enter(next);
    }
}

/* 功能点4：读取拨码。开关拨上为0，所以判断 KEY_x == 0。 */
uchar key(void)
{
    if(KEY_H == 0 && KEY_Y == 1 && KEY_V == 1) return H;
    if(KEY_Y == 0 && KEY_H == 1 && KEY_V == 1) return Y;
    if(KEY_V == 0 && KEY_H == 1 && KEY_Y == 1) return V;
    return 0;
}

/* 功能点5：拨码拨上暂停，拨下恢复。count 不被修改，所以数字能保持。 */
void keyProcess(void)
{
    uchar k = key();

    if(k != 0)
    {
        if(hold == 0)
        {
            oldState = state;
            oldNext = next;
            TR0 = 0;         /* 暂停定时器 */
        }
        hold = k;
        lamp(k);             /* 切到预设灯态 */
        show(count);          /* 数字保持 */
        return;
    }

    if(hold != 0)
    {
        hold = 0;
        state = oldState;
        next = oldNext;
        tick = 0;
        oneSecond = 0;
        lamp(state);          /* 恢复拨码前灯态 */
        show(count);
        TR0 = 1;              /* 继续倒计时 */
    }
}

void main(void)
{
    KEY_H = 1;
    KEY_Y = 1;
    KEY_V = 1;

    enter(H);
    timerInit();

    while(1)
    {
        keyProcess();

        if(hold == 0 && oneSecond)
        {
            oneSecond = 0;
            if(count == 0) nextState();
            else
            {
                count--;
                show(count);
            }
        }
    }
}