/*
 * 实验功能：
 * 读取电位器中端 Vin 接到的 A/D 通道，将 0~5V 模拟电压转换成数字量，
 * 再换算成带 1 位小数的电压值，送到两位数码管显示。
 */
#include<reg51.h>       // 8051 单片机特殊功能寄存器定义
#include<intrins.h>
#include"absacc.h"      // 提供 XBYTE[]，用于访问外部扩展接口地址
#include"stdio.h"
#define uchar unsigned char
#define uint unsigned int

/* A/D 转换器通道 0 的片选地址。
 * 实验把 A/D 接口映射到外部数据存储器空间，所以用 XBYTE[0x7000] 访问。
 */
#define IN0 0X7000

uchar n;                // A/D 通道号；本实验 Vin 接在 IN0，所以 n 固定为0，代表用0号模拟输入口
uchar addata;           // 保存 A/D 转换后的 8 位结果，范围约为 0~255

/* 数码管的段码表，0~F 共 16 个字符。
 * 表中每个字节对应一个数字的 a、b、c、d、e、f、g、dp 段亮灭状态。
 */
uchar TAB[]={0XFC,0X60,0XDA,0XF2,0X66,0XB6,0XBE,0XE0,0XFE,0XF6,0XEE,0X3E,0X9C,0X7A,0X9E,0X8E};

void mydelay(unsigned int n)
{
	uchar i;
	
	/* 软件延时。
	 * 作用一：等待 A/D 转换完成；
	 * 作用二：让数码管显示保持一小段时间，避免刷新太快看不清。
	 */
	while(n--)
		for(i=150;i>0;i--);
		}
		
		void putch(uchar n)
		{
			SBUF=n;                 // 写 SBUF 后，串口开始发送 1 字节段码
			while(TI==0);           // 等待发送完成；TI=1 表示 1 字节已经移出给显示电路
			TI=0;                   // 手动清发送完成标志，为下次发送做准备
		}

void mydisplay(uchar Count)
{
	uchar i,j;
	uint a;
	
	a=Count;                // Count 是 A/D 结果：0 对应 0V，255 约对应 5V
	
	/* 电压换算：
	 * 8 位 A/D 满量程是 255，参考电压按 5V 计算。
	 * 为了显示 1 位小数，把电压放大 10 倍：
	 * a = Count * 5.0 / 255 * 10 = Count * 50 / 255 约等于 Count * 10 / 51。
	 * 例如 a=25 表示 2.5V。
	 */
	a=a*10/51;
	
	i=TAB[a/10]|0x01;       // 十位显示电压整数部分，并打开小数点，如 "2."
	j=TAB[a%10];            // 个位显示十分位，如 "5"
	putch(i);               // 先送第一位段码
	putch(j);               // 再送第二位段码，最终显示形如 2.5
	mydelay(200);
	}

void main()
{
	n=0;                    // 选择 A/D 的 IN0 通道，即电位器中端 Vin
	SCON=0x00;              // 串口工作方式 0：同步移位输出，常用于把段码送到显示电路
	TI=0;                   // 清发送完成标志
	while(1)
	{
		XBYTE[IN0+n]=0;         // 对 A/D 地址写一次，启动当前通道的 A/D 转换
		mydelay(300);           // 等待转换稳定；若太短，读数可能跳变或不准
		addata=XBYTE[IN0+n];    // 读取 A/D 转换结果，得到 0~255 的数字量
		mydelay(500);
		mydisplay(addata);      // 将数字量换算成电压，并送数码管显示
	}
	}
