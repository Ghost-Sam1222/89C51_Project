#include <reg51.h>
#include "absacc.h"

typedef unsigned char uchar;
typedef unsigned int  uint;

#define DAC_AD  0x7000

// DAC引脚
sbit CS   = P2^0;
sbit SCLK = P2^1;
sbit DIN  = P2^2;

// 波形按键
sbit SW1 = P1^0;   // 方波
sbit SW2 = P1^1;   // 锯齿波
sbit SW3 = P1^2;   // 梯形波
sbit SW4 = P1^3;   // 三角波
sbit SW5 = P1^4;   // 正弦波

#define M_NONE  0
#define M_SQ    1
#define M_SAW   2
#define M_TRAP  3
#define M_TRI   4
#define M_SIN   5

uint code sin_tab[256] = {
     512, 525, 537, 550, 562, 575, 587, 599, 
     612, 624, 636, 648, 660, 672, 684, 696,
     708, 719, 730, 742, 753, 764, 775, 785,
     796, 806, 816, 826, 836, 846, 855, 864,
     873, 882, 891, 899, 907, 915, 922, 930,
     937, 944, 950, 957, 963, 968, 974, 979,
     984, 989, 993, 997,1001,1004,1008,1011,
     1013,1015,1017,1019,1021,1022,1022,1023,
     1023,1023,1022,1022,1021,1019,1017,1015,
     1013,1011,1008,1004,1001, 997, 993, 989,
     984, 979, 974, 968, 963, 957, 950, 944, 
     937, 930, 922, 915, 907, 899, 891, 882,
     873, 864, 855, 846, 836, 826, 816, 806,
     796, 785, 775, 764, 753, 742, 730, 719,
     708, 696, 684, 672, 660, 648, 636, 624,
     612, 599, 587, 575, 562, 550, 537, 525,
     512, 499, 487, 474, 462, 449, 437, 425,
     412, 400, 388, 376, 364, 352, 340, 328,
     316, 305, 294, 282, 271, 260, 249, 239,
     228, 218, 208, 198, 188, 178, 169, 160,
     151, 142, 133, 125, 117, 109, 102,  94,
     87,  80,  74,  67,  61,  56,  50,  45,
     40,  35,  31,  27,  23,  20,  16,  13,
     11,   9,   7,   5,   3,   2,   2,   1,
     1,   1,   2,   2,   3,   5,   7,   9,
     11,  13,  16,  20,  23,  27,  31,  35,
     40,  45,  50,  56,  61,  67,  74,  80,
     87,  94, 102, 109, 117, 125, 133, 142,
     151, 160, 169, 178, 188, 198, 208, 218,
     228, 239, 249, 260, 271, 282, 294, 305,
     316, 328, 340, 352, 364, 376, 388, 400,
     412, 425, 437, 449, 462, 474, 487, 499,
};

uint freq;
uint amp = 1000;

void delay(uint ms) {
    uint i, j;
    for (i = ms; i > 0; i--)
        for (j = 124; j > 0; j--);
}

void DAC(uint val) {
    uchar i;
    if (val > 1000) val = 1000;
    val <<= 6;
    SCLK = 0; CS = 1; CS = 0;
    for (i = 0; i < 12; i++) {
        DIN = (bit)(val & 0x8000);
        SCLK = 1;
        val <<= 1;
        SCLK = 0;
    }
    SCLK = 0; CS = 1;
}

uchar ADC(uchar ch) {
    XBYTE[DAC_AD + ch] = 0;     //启动AD转换
    delay(1);
    return XBYTE[DAC_AD + ch];
}

uchar GetMode(void) {
    if (SW1 == 0) return M_SQ;
    if (SW2 == 0) return M_SAW;
    if (SW3 == 0) return M_TRAP;
    if (SW4 == 0) return M_TRI;
    if (SW5 == 0) return M_SIN;
    return M_NONE;
}

void main() {
    uchar mode;
    uint  step;
    unsigned long index;
    P1 = 0xFF;

    while (1) {
        // 只在整周期结束更新频率与波形，不中途打断波形
        freq = 50 + (uint)ADC(0) * 1950 / 255;
        mode = GetMode();

        // 一整周期完整输出，中间不检测按键，波形连续
        for (step = 0; step < freq; step++) {
            switch (mode) {
                case M_NONE:
                    DAC(0);
                    break;
                case M_SQ:
                    DAC(step < freq / 2 ? amp : 0);
                    break;
                case M_SAW:
                    DAC((unsigned long)amp * step / freq);
                    break;
                case M_TRAP:
                {
                    uint q = freq / 4;
                    if (step < q)
                        DAC((unsigned long)amp * step / q);
                    else if (step < 3 * q)
                        DAC(amp);
                    else
                        DAC((unsigned long)amp * (freq - step) / q);
                    break;
                }
                case M_TRI:
                {
                    uint h = freq / 2;
                    if (step < h)
                        DAC((unsigned long)amp * step / h);
                    else
                        DAC((unsigned long)amp * (freq - step) / h);
                    break;
                }
                case M_SIN:
                {
                    uint dat;
                    // 精准映射0~freq → 0~255，无越界
                    index = ((unsigned long)step * 256) / freq;
                    if(index >= 256) index = 255;
                    // 32位运算彻底防溢出
                    dat = ((unsigned long)amp * sin_tab[(uchar)index]) / 1000;
                    DAC(dat);
                    break;
                }
            }
            delay(1);
        }
    }
}