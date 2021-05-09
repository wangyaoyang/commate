#ifndef	__COMMATE_H__
#define	__COMMATE_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <syslog.h>
#define MAX_REC_LEN     1024
#define MAX_RECORDS     32
typedef struct __Record {
    unsigned short len;
    char record[MAX_REC_LEN];    
} SingleRecord;

typedef struct __LoopHistory {
    unsigned char   head;
    unsigned char   curr;
    unsigned char   count;
    SingleRecord    records[MAX_RECORDS];
} RecordHistory;

#define INC_INDEX(idx, max)     do { idx ++; idx %= max; } while (0)
#define DEC_INDEX(idx, max)     do { idx += max; idx --; idx %= max; } while (0)
#define INC_HIS_IDX(idx)        INC_INDEX(idx, MAX_RECORDS)

#define LANG_CHINESE    936
#define LANG_ENGLISH    437

#ifdef RELEASE_ON_ARM
#define LANGUAGE        LANG_ENGLISH
#else
#define LANGUAGE        LANG_CHINESE
#endif

#ifndef BYTE
#define BYTE        unsigned char
#endif  //BYTE
#define KEY_LN          ((short)10)     //0x0A
#define KEY_LR          ((short)13)     //0x0D
#define KEY_ESC         ((short)27)     //0x1B
#define KEY_CTRL_A      ((short)1)      //0x01
#define KEY_CTRL_B      ((short)2)      //0x01
#define KEY_CTRL_C      ((short)3)      //0x01
#define KEY_CTRL_D      ((short)4)      //0x01
#define KEY_CTRL_E      ((short)5)      //0x01
#define KEY_CTRL_F      ((short)6)      //0x01
#define KEY_BACKSPACE   ((short)8)      //0x08
#define KEY_TABLE       ((short)9)      //0x09
#define KEY_CTRL_Q      ((short)17)     //0x11
#define KEY_CTRL_R      ((short)18)     //0x11
#define KEY_CTRL_T      ((short)20)     //0x14
#define KEY_CTRL_Z      ((short)26)     //0x1A
#define KEY_DEL         ((short)127)    //0x7F
#define VT100_H0        0x1b
#define VT100_H1        '['

#ifdef  RELEASE_ON_ARM
#define IS_ENTER_KEY(nKey)  true
#else   //RELEASE_ON_ARM
#define IS_ENTER_KEY(nKey)  (KEY_ENTER == nKey || KEY_LF == nKey)
#endif  //RELEASE_ON_ARM
#define MAX_KEYS_BUFF   20
#define CONFIG_FILE     "/etc/commate.conf"

#define CURSOR_TX       0
#define CURSOR_RX       1

bool VT100_PickCursor(char* sVT100, int nVT100);
void VT100_SaveCursor(int TxRx);
void VT100_LoadCursor(int TxRx);
bool IsEchoOn(void);
bool IsModeHex(void);
int SendUserInput(SerialCom* pcomm, CKeyboard* pkeyb);
char* SendUserKey(SerialCom* pcomm, char* cKeys, int nKeys);
int RecordSave(void);
int RecordLoad(void);

#define SYSLOG_INFO(fmt, ...)   do { \
        syslog(LOG_INFO, "Commate : " fmt, ##__VA_ARGS__); } while (0)

#define LOAD_AND_PRINT(fmt, ...)    \
    do {    VT100_LoadCursor(CURSOR_TX);\
            printf(fmt VT100_CLR_EOL VT100_SAVE_CURSOR, \
                    ##__VA_ARGS__); fflush(stdout);\
    } while (0)

#define PRINT_AND_BACK(fmt, ...)    \
    do {    printf(VT100_CLR_EOL fmt  VT100_LOAD_CURSOR, \
                    ##__VA_ARGS__); fflush(stdout);\
    } while (0)

#define PRINT_AND_STAY(fmt, ...)    \
    do {    char sAppend[MAX_REC_LEN] = {0};\
            snprintf(sAppend, MAX_REC_LEN - 1, fmt, ##__VA_ARGS__);\
            int nAppend = strlen(sAppend);\
            if (nAppend > 0) {\
                printf(VT100_CLR_EOL "%s", sAppend);\
                VT100_MoveLeft(nAppend);\
            } else { printf(VT100_CLR_EOL); fflush(stdout); }\
    } while (0)

#define PRINT_AND_MOVE(fmt, ...)    \
    do {    printf(fmt, ##__VA_ARGS__); fflush(stdout);\
            VT100_SaveCursor(CURSOR_TX);\
    } while (0)

#ifdef	__cplusplus
}
#endif

#endif	//__COMMATE_H__
