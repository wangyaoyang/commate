/* 
 * File:   kbd.h
 * Author: root
 *
 * Created on 2009��12��23��, ����9:13
 */

#ifndef _KBD_H
#define	_KBD_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef  __ARM_ARCH_3__
#define RELEASE_ON_ARM
#else
#ifdef _MIPS_ARCH_MIPS1
#define RELEASE_ON_ARM
#endif
#endif  //__ARM_ARCH_3__

#ifndef bool
#define bool    int
#endif
#ifndef true
#define true    1
#endif
#ifndef false
#define false   0
#endif
typedef struct __key_board {
    int             m_keyboard;
    int             m_oldmode;
    bool            m_bEcho;
    struct termios  m_oldtermios;
} CKeyboard;

bool	KeyOpen(CKeyboard* kb,bool echo);
void    KeyClose(CKeyboard* kb);
int     KeyGetKeyBuff(CKeyboard* kb,char* keys,int max);
void    KeyPressKey(CKeyboard* kb,char cKey);
bool    KeySetEcho(CKeyboard* kb,bool bEcho,bool bEnter);
bool    KeyIsEchoOn(CKeyboard* kb);



#ifdef	__cplusplus
}
#endif

#endif	/* _KBD_H */

