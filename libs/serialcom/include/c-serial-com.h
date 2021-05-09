#ifndef	__C_SERIAL_COM_H__
#define	__C_SERIAL_COM_H__

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef bool
#define bool    int
#endif
#ifndef true
#define true    1
#endif
#ifndef false
#define false   0
#endif
#define	MAX_COM_BUFFER_SIZE		4096
#define COM_RECEIVE_TIME_OUT            -1
#define COM_RECEIVE_INTERVAL            50000   //10000us = 10ms

#define BOARD_TO_OUTPUT 0

#define ENABLE_TRACE0  true
#define ENABLE_TRACE1  true
#define ENABLE_TRACE2  true
#define ENABLE_TRACE3  true
#define ENABLE_TRACE4  true
#define ENABLE_TRACE5  false
#define ENABLE_TRACE6  false

#define TRACE0          if(ENABLE_TRACE0) printf
#define TRACE1          if(ENABLE_TRACE1) printf
#define TRACE2          if(ENABLE_TRACE2) printf
#define TRACE3          if(ENABLE_TRACE3) printf
#define TRACE4          if(ENABLE_TRACE4) printf
#define TRACE5          if(ENABLE_TRACE5) printf
#define TRACE6          if(ENABLE_TRACE6) printf
#define TC4(x)          if(ENABLE_TRACE4 && BOARD_TO_OUTPUT == x) printf

//===============================================ANSI控制码的说明
#define VT100_FLAG0         "\x1b"          //ESC
#define VT100_FLAG1         "["
#define VT100_FLAG          VT100_FLAG0 VT100_FLAG1
#define VT100_HEAD          VT100_FLAG
#define VT100_UP(n)         VT100_HEAD #n "A"      //ESC[nA 光标上移n行
#define VT100_DOWN(n)       VT100_HEAD #n "B"      //ESC[nB 光标下移n行
#define VT100_RIGHT(n)      VT100_HEAD #n "C"      //ESC[nC 光标右移n行
#define VT100_LEFT(n)       VT100_HEAD #n "D"      //ESC[nD 光标左移n行
#define VT100_CLR2EOF       VT100_HEAD "J"      //ESC[J 清除从光标到尾的内容
#define VT100_CLEAR         VT100_HEAD "2J"     //ESC[2J 清屏
#define VT100_CLR_EOL       VT100_HEAD "K"      //ESC[K 清除从光标到行尾的内容
#define VT100_CLR_LINE      VT100_HEAD "2K"     //ESC[2K 清行
#define VT100_SAVE_CURSOR   VT100_HEAD "s"      //ESC[s 保存光标位置
#define VT100_LOAD_CURSOR   VT100_HEAD "u"      //ESC[u 恢复光标位置
#define VT100_HIDE_CURSOR   VT100_HEAD "?25l"   //ESC[?25l 隐藏光标
#define VT100_SHOW_CURSOR   VT100_HEAD "?25h"   //ESC[?25h 显示光标
#define VT100_RESET         VT100_HEAD "0m"     //ESC[0m 关闭所有属性
#define VT100_HIGHT_LIGHT   VT100_HEAD "1m"     //ESC[1m 设置高亮度
#define VT100_UNDER_LINE    VT100_HEAD "4m"     //ESC[4m 下划线
#define VT100_FLASH         VT100_HEAD "5m"     //ESC[5m 闪烁
#define VT100_INVERT        VT100_HEAD "7m"     //ESC[7m 反显
#define VT100_INVISIBLE     VT100_HEAD "8m"     //ESC[8m 消隐
#define VT100_GOTO(x,y)     VT100_HEAD #x ";" #y "H" VT100_CLR_EOL    //ESC[y;xH设置光标位置
#define VT100_COLOR(b,f)    VT100_HEAD b ";" f "m"                  //
#define VT100_GET_CURSOR    VT100_HEAD "6n"     //ESC[s 保存光标位置
//ESC[30m&nbsp-- ESC[37m 设置前景色
//ESC[40m&nbsp-- ESC[47m 设置背景色

//字颜色:30-----------39
#define VT100_F_BLACK       "30"    //黑
#define VT100_F_RED         "31"    //红
#define VT100_F_GREEN       "32"    //绿
#define VT100_F_YELLOW      "33"    //黄
#define VT100_F_BLUE        "34"    //蓝色
#define VT100_F_PINK        "35"    //紫色
#define VT100_F_DARK_G      "36"    //深绿
#define VT100_F_WHITE       "37"    //白色

//字背景颜色范围:40----49
#define VT100_B_BLACK       "40"    //黑
#define VT100_B_RED         "41"    //深红
#define VT100_B_GREEN       "42"    //绿
#define VT100_B_YELLOW      "43"    //黄色
#define VT100_B_BLUE        "44"    //蓝色
#define VT100_B_PINK        "45"    //紫色
#define VT100_B_DARK_G      "46"    //深绿
#define VT100_B_WHITE       "47"    //白色

//#define VT100_STYLE0        VT100_RESET
#define VT100_STYLE0        VT100_COLOR(VT100_B_BLACK,VT100_F_GREEN)
#define VT100_STYLE1        VT100_COLOR(VT100_B_BLACK,VT100_F_WHITE) VT100_HIGHT_LIGHT VT100_INVERT
#define VT100_STYLE2        VT100_COLOR(VT100_B_DARK_G,VT100_F_WHITE) VT100_HIGHT_LIGHT
#define VT100_STYLE_BLUE    VT100_COLOR(VT100_B_BLUE,VT100_F_WHITE) VT100_HIGHT_LIGHT
#define VT100_STYLE_ALERT   VT100_COLOR(VT100_B_RED,VT100_F_YELLOW) VT100_HIGHT_LIGHT
#define VT100_STYLE_NORMAL  VT100_COLOR(VT100_B_GREEN,VT100_F_WHITE) VT100_HIGHT_LIGHT
#define VT100_STYLE_MENU    VT100_RESET VT100_COLOR(VT100_B_BLACK,VT100_F_GREEN) VT100_HIGHT_LIGHT
#define VT100_STYLE_KEY     VT100_COLOR(VT100_B_BLACK,VT100_F_YELLOW) VT100_HIGHT_LIGHT VT100_UNDER_LINE
#define VT100_STYLE_HOT     VT100_RESET VT100_COLOR(VT100_B_BLACK,VT100_F_RED) VT100_HIGHT_LIGHT

char*   VT100_GOTO_XY(int x,int y);

#include "pthread.h"


typedef struct __serial_comm {
    pthread_mutex_t m_lockBuffer;
    pthread_t       m_reader;
    int             m_fd;
    int             m_nBuff;
    int             m_nTimeout;
    char            m_buffer[MAX_COM_BUFFER_SIZE];
} SerialCom;

void ScInit(SerialCom* sc,int nTimeout);/*ms*/
void ScExit(SerialCom* sc);
bool ScOpen(SerialCom* sc,int port,int baud,int data,int stop,char parity);
void ScSetDevName(SerialCom* sc,int i,char* name);
void ScClose(SerialCom* sc);
void ScClear(SerialCom* sc);
int ScSend(SerialCom* sc, const char* pData, int nData );
int ScRead(SerialCom* sc, const char* pData, int nSkip, const int nMax);

#ifdef	__cplusplus
}
#endif

#endif		//__C_SERIAL_COM_H__
