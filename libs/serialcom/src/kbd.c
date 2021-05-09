#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/kd.h>
#include <termios.h>
#include <string.h>

#include "../include/c-serial-com.h"
#include "../include/kbd.h"


/* KBDUS means US Keyboard Layout. This is a scancode table
*  used to layout a standard US keyboard. I have left some
*  comments in to give you an idea of what key is what, even
*  though I set it's array index to 0. You can change that to
*  whatever you want using a macro, if you wish! */
//unsigned char kbdus[128] =
//{
//    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
//  '9', '0', '-', '=', '\b',	/* Backspace */
//  '\t',			/* Tab */
//  'q', 'w', 'e', 'r',	/* 19 */
//  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
//    0,			/* 29   - Control */
//  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
// '\'', '`',   0,		/* Left shift */
// '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
//  'm', ',', '.', '/',   0,				/* Right shift */
//  '*',
//    0,	/* Alt */
//  ' ',	/* Space bar */
//    0,	/* Caps lock */
//    0,	/* 59 - F1 key ... > */
//    0,   0,   0,   0,   0,   0,   0,   0,
//    0,	/* < ... F10 */
//    0,	/* 69 - Num lock*/
//    0,	/* Scroll Lock */
//    0,	/* Home key */
//    0,	/* Up Arrow */
//    0,	/* Page Up */
//  '-',
//    0,	/* Left Arrow */
//    0,
//    0,	/* Right Arrow */
//  '+',
//    0,	/* 79 - End key*/
//    0,	/* Down Arrow */
//    0,	/* Page Down */
//    0,	/* Insert Key */
//    0,	/* Delete Key */
//    0,   0,   0,
//    0,	/* F11 Key */
//    0,	/* F12 Key */
//    0,	/* All other keys are undefined */
//};
//

bool KeyOpen(CKeyboard* kb,bool echo) {
//#ifdef  RELEASE_ON_ARM
////    const char* sKeyboard = "/dev/tty";
//    const char* sKeyboard = "/dev/console";
//#else   //RELEASE_ON_ARM
//    const char* sKeyboard = "/dev/tty0";
//#endif  //RELEASE_ON_ARM
    kb->m_keyboard = -1;
    kb->m_oldmode = 0;
    kb->m_bEcho = false;
    char* sKeyboard = ttyname(STDIN_FILENO);
    if (sKeyboard != NULL )
        kb->m_keyboard = open(sKeyboard, O_RDONLY | O_NONBLOCK);
    if (kb->m_keyboard == -1) { /*设置数据位数*/
        char    sError[128];
        sprintf( sError,"Can't Open Keyboard : %s",sKeyboard);
        perror(sError);
        return false;
    }
    //saving console mode(echo off)
    ioctl(fileno(stdin), KDGKBMODE, &kb->m_oldmode);
    if ((tcgetattr(fileno(stdin), &kb->m_oldtermios)) < 0) {
        perror("tcgetaddr   error");
        close(kb->m_keyboard);
        kb->m_keyboard = -1;
        return false;
    } else ioctl(kb->m_keyboard, KDSKBMODE, K_XLATE);
    KeySetEcho(kb,echo,false);
    return true;
}

void KeyClose(CKeyboard* kb) {
    KeySetEcho(kb,true, true);
    if (kb->m_keyboard > 0) {
        tcsetattr(fileno(stdin), 0, &kb->m_oldtermios);
        ioctl(fileno(stdin), KDSKBMODE, kb->m_oldmode);
        close(kb->m_keyboard);
    }
}

bool KeyIsEchoOn(CKeyboard* kb) {
    return kb->m_bEcho;
}

bool KeySetEcho(CKeyboard* kb,bool bEcho,bool bEnter) {
    struct termios  newtermios;
    if (kb->m_keyboard <= 0) return false;
    if ((tcgetattr(fileno(stdin), &newtermios)) < 0) {
        printf("\n Terminal keyboard : failed on tcgetattr()\n");
        return false;
    }
    kb->m_bEcho = bEcho;
    if (kb->m_bEcho && bEnter) {    //default console mode
        tcsetattr(fileno(stdin), TCSANOW, &kb->m_oldtermios);
        ioctl(fileno(stdin), KDSKBMODE, kb->m_oldmode);
    } else if (kb->m_bEcho) {   //echo on, do not wait ENTER
        newtermios.c_lflag   &=   ~(ICANON|ISIG);
        tcsetattr(fileno(stdin), TCSANOW, &newtermios);
        ioctl(fileno(stdin), KDSKBMODE, kb->m_oldmode);
    } else if (bEnter) { //No echo but wait for ENTER
        newtermios.c_lflag   &=   ~(ECHO|ISIG);
        tcsetattr(fileno(stdin), TCSANOW, &newtermios);
        ioctl(fileno(stdin), KDSETMODE, KD_GRAPHICS);
    } else {    //graphic(echo off) console mode
//        newtermios.c_lflag   &=   ~ECHO;
        newtermios.c_lflag   &=   ~(ICANON|ECHO|ISIG);
//        newtermios.c_iflag   =   0;
        tcsetattr(fileno(stdin), TCSANOW, &newtermios);
        ioctl(fileno(stdin), KDSETMODE, KD_GRAPHICS);
    }
    return true;
}

void KeyPressKey(CKeyboard* kb,char cKey) {
    if (kb->m_keyboard <= 0) return;
    write(kb->m_keyboard,&cKey,1);
}

int KeyGetKeyBuff(CKeyboard* kb,char* keys,int max) {
    if (kb->m_keyboard <= 0) return 0;
    if (NULL == keys) return -1;
    if (max <= 0) return -2;

    fd_set readfd;
    int nKey = 0x0000;
    int ret = 0, i = 0;
    struct timeval timeout;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    FD_ZERO(&readfd);
    FD_SET(kb->m_keyboard, &readfd);
    ret = select(kb->m_keyboard + 1, &readfd, NULL, NULL, &timeout);
    if (FD_ISSET(kb->m_keyboard, &readfd)) {
        i = read(kb->m_keyboard, keys, max);
        return i;
    }
    return 0;
}

