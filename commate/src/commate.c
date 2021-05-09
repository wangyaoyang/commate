/* 
 * File:   main.cc
 * Author: liveuser
 *
 * Created on 2009年2月10日, 下午9:35
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

//using namespace std;

#include "../../libs/serialcom/include/c-serial-com.h"
#include "../../libs/serialcom/include/kbd.h"
#include "commate.h"

#define VKEY_MOVE_UP    0x0100
#define VKEY_MOVE_DOWN  0x0200
#define VKEY_MOVE_RIGHT 0x0300
#define VKEY_MOVE_LEFT  0x0400
#define VKEY_CTRL_LN    0x0A00
#define VKEY_CTRL_LR    0x0D00
#define VKEY_CTRL_SEND  0x1000
#define VKEY_POSITION   0x8000
#define VKEY_MOVE_DEL   0x7E00
#define VKEY_MOVE_BACK  0x7F00

/*
 * 
 */
static int CharToHex(char cKey,BYTE* cHex) {
    static char nHalfByte;      //  0---left half byte(high 4 bits) 1---right half byte(low 4 bits)
    static char sHexValue[3];
    int nHex = 0;

    switch (cKey) {
        case 0:     nHalfByte = 0;     return 0;
        case 'a':   case 'A':   case 'b':   case 'B':
        case 'c':   case 'C':   case 'd':   case 'D':
        case 'e':   case 'E':   case 'f':   case 'F':
        case '1':   case '2':   case '3':   case '4':   case '5':
        case '6':   case '7':   case '8':   case '9':   case '0':
            if (cHex ) {
                sHexValue[nHalfByte] = cKey;
                nHalfByte ++;
            } else return 0;    //Just verify that the cKey is a valid Hex character
            break;
        case '\n':  case ' ':   if (nHalfByte > 0) nHalfByte = 2;  break;
        default: return -1;     //Invalidate Hex character 
    }
    if (nHalfByte >= 2) {
        nHalfByte = 0;
        sscanf(sHexValue,"%x",&nHex);
        memset(sHexValue,0x00,3);
        *cHex = (BYTE)nHex;
        return 1;
    }
    return 0;
}

static int HexToStr(BYTE* sHex, int nHex, char* sText) {
    if (sText) {
        sText[0] = 0;
        if (NULL == sHex || nHex <= 0) return 0;
        
        sprintf(sText, "%02x", sHex[0]);
        for (int i = 1; i < nHex; i ++) {
            char sByte[4] = {0};
            
            sprintf(sByte, " %02x", sHex[i]);
            strcat (sText, sByte);
        }
        return strlen (sText);
    } else return 0;
}

static int StrToHex(char* sText,BYTE* sHex) {
    if (NULL == sText || NULL == sHex) return 0;
    int nHex = 0;
    for (int i=0; sText[i]; i ++) {
        BYTE    cHex = 0;
        if (CharToHex(sText[i], &cHex) > 0) {
            sHex[nHex] = cHex;
            nHex ++;
        }
    }
    return nHex;
}

static int VT100_CharEcho(char cKey) {
#ifdef _MIPS_ARCH_MIPS1
    if (KEY_BACKSPACE == cKey)  {   return VKEY_MOVE_BACK;
    } else if (KEY_DEL == cKey) {   return VKEY_MOVE_DEL;
#else
    if (KEY_DEL == cKey ||
        KEY_BACKSPACE == cKey) {    return VKEY_MOVE_BACK;
#endif
    } else if (KEY_LN == cKey)  {   return VKEY_CTRL_LN;
    } else if (KEY_LR == cKey)  {   return VKEY_CTRL_LR;
    } else if (IsModeHex() && CharToHex(cKey, NULL) < 0) {
        return 0;
    } else if (IsEchoOn())      {
        printf("%c" VT100_SAVE_CURSOR, cKey);   fflush(stdout);
    }
    return cKey;
}

/*
 * Parse cursor position by given VT100 sequence ESC[<n>;<m>R
 * which <n> is the row number, and <m> is the column number
 * argument cKeys is the VT100 sequence,
 * argument nKeys specifies the length of the sequence
 * result stored in row/column
 */
bool VT100_GetCursor(char* cKeys, int nKeys, int* row, int* col) {
    if (cKeys && nKeys >= 6) {  //valid VT100 sequence has minimum 6 bytes
        char*   strN = strchr(cKeys, '[');
        char*   strM = strchr(cKeys, ';');
        char*   strE = strchr(cKeys, 'R');
        if (strN && strN < strM && strM < strE) {
            *strM = 0;
            *strE = 0;
            if (row) sscanf(strN + 1, "%d", row);
            if (col) sscanf(strM + 1, "%d", col);
            return true;
        }
    }
    return false;
 }

static int  static_CursorMode;
static int  static_CursorRow[2];
static int  static_CursorCol[2];

void VT100_SaveCursor(int TxRx) {
    static_CursorMode = (TxRx % 2);
    printf(VT100_GET_CURSOR ); fflush(stdout);
}

bool VT100_PickCursor(char* sVT100, int nVT100) {
    int*    row = &static_CursorRow[static_CursorMode];
    int*    col = &static_CursorCol[static_CursorMode];
    return VT100_GetCursor(sVT100, nVT100, row, col);
}

void VT100_LoadCursor(int TxRx) {
    int n = TxRx % 2;
    int min[2] = { 2, 11 };
    int max[2] = { 10, 20 };
    if (static_CursorRow[n] < min[n] ||
        static_CursorRow[n] >= max[n])
        static_CursorRow[n] = min[n];
    if (static_CursorCol[n] == 0)
        static_CursorCol[n] = 1;
    printf(VT100_HEAD "%d;%dH" VT100_CLR_EOL,    //ESC[y;xH设置光标位置
            static_CursorRow[n], static_CursorCol[n]);
}

void VT100_MoveUp(int n)    { printf(VT100_HEAD "%dA", n);    fflush(stdout); }
void VT100_MoveDown(int n)  { printf(VT100_HEAD "%dB", n);    fflush(stdout); }
void VT100_MoveRight(int n) { printf(VT100_HEAD "%dC", n);    fflush(stdout); }
void VT100_MoveLeft(int n)  { printf(VT100_HEAD "%dD", n);    fflush(stdout); }

static int VT100_Char(int cKey) {
    static int  nVT100;
    static char sVT100[16];

#define RET_VT100(code) do { sVT100[0] = 0; nVT100 = 0; return code; } while (0)
#define CLR_VT100(cKey) do { sVT100[0] = 0; sVT100[nVT100] = 0;\
                             printf("%s%c", sVT100 + 1, cKey);\
                             fflush(stdout);nVT100 = 0;\
                        } while (0)
    
    SYSLOG_INFO("key = %c (%d)[0x%x]", (char) cKey, cKey, 0x00ff & cKey);
    switch (cKey) {
        case KEY_CTRL_A:    //Return to menu
            sVT100[0] = 0;  nVT100 = 0; printf("\n" VT100_RESET);   return -1;
        case KEY_CTRL_B:
        case KEY_CTRL_C:
        case KEY_CTRL_D:
        case KEY_CTRL_E:
        case KEY_CTRL_F:
        case KEY_CTRL_R:    sVT100[0] = 0;  nVT100 = 0;     break;
        case KEY_CTRL_T:    sVT100[0] = 0;  nVT100 = 0;     return VKEY_CTRL_SEND;
        case VT100_H0:      sVT100[0] = cKey;   nVT100 = 1; break;  //ESC
        case VT100_H1:      if (sVT100[0] == VT100_H0) {    //'['
                                sVT100[1] = VT100_H1;   nVT100 = 2;
                            } else VT100_CharEcho(cKey);
            break;
        case KEY_TABLE:     sVT100[0] = 0;  nVT100 = 0;
        default:
            if (sVT100[0] == VT100_H0 && sVT100[1] == VT100_H1){
                switch(cKey) {  //Handle VT100 key
                    case 'A':   RET_VT100(VKEY_MOVE_UP);
                    case 'B':   RET_VT100(VKEY_MOVE_DOWN);
                    case 'C':   RET_VT100(VKEY_MOVE_RIGHT);
                    case 'D':   RET_VT100(VKEY_MOVE_LEFT);
                    case 'R':   sVT100[nVT100 ++] = cKey;
                                VT100_PickCursor(sVT100, nVT100);
                                RET_VT100(VKEY_POSITION);
                    case '~':   if (nVT100 == 3 && sVT100[2] == '3') {
                                    RET_VT100(VKEY_MOVE_DEL);  //DEL                        
                                } else { CLR_VT100(cKey);  break; }
                    case '3':
                    default:    if (nVT100 > 9) { CLR_VT100(cKey);  break; }
                                sVT100[nVT100 ++] = cKey;   break;
                }
            } else {
                if (sVT100[0]) sVT100[0] = 0;
                return VT100_CharEcho(cKey);
            }
    }
    return 0;
}

static char     staticTxIndex;
static char*    staticTxColor[2] = {VT100_COLOR(VT100_B_BLACK,VT100_F_WHITE) VT100_HIGHT_LIGHT,
                                    VT100_COLOR(VT100_B_BLACK,VT100_F_YELLOW) VT100_HIGHT_LIGHT };
static RecordHistory    static_RecHistory[2];   //0 for Ascii and 1 for Hexadecimal

typedef int (*RECORD_CB) (SingleRecord* rec, void* pParam);
typedef struct __record_data {
    int     nData;
    char*   sData;
} RecordDataRef;

static int RecordCompareCB(SingleRecord* rec, void* pParam) {
    RecordDataRef*  ref = (RecordDataRef*) pParam;
    if (ref && ref->nData > 0) {    //ref exists
        if (ref->nData == rec->len &&
            memcmp(ref->sData, rec->record, ref->nData) == 0) return 0;
        return 1;
    } else return -1;
}

static int RecordSaveHexCB(SingleRecord* rec, void* pParam) {
    FILE*   fp = (FILE*) pParam;
    if (fp && rec && 0 < rec->len && rec->len < MAX_REC_LEN) {
        char sHex[MAX_REC_LEN];
        
        memset (sHex, 0x00, MAX_REC_LEN);
        if (HexToStr(rec->record, rec->len, sHex) > 0)
            fprintf(fp, "%s\n", sHex);
    }
    return 1;
}

static int RecordSaveTextCB(SingleRecord* rec, void* pParam) {
    FILE*   fp = (FILE*) pParam;
    if (fp && rec && 0 < rec->len && rec->len < MAX_REC_LEN) {
        rec->record[rec->len] = 0;
        fprintf(fp, "%s\n", rec->record);
    }
    return 1;
}
/*
 * Go through ring buffer,
 * return 0 when something happened before all records traversed
 * return 1, if all records been scanned and nothing happened.
 */
static int RecordTraverse(RecordHistory* his, RECORD_CB cb, void* pParam) {
    if (his->count > 0) {
        unsigned char start = his->head;
        DEC_INDEX(start, his->count);   //head is empty, start from last one
        unsigned char curr = start;
        do {
            SingleRecord*   r = &his->records[curr];
            int             ret = cb(r, pParam);
            if (0 == ret) return 0;
            DEC_INDEX(curr, his->count);            
        } while (curr != start); 
    }
    return 1;
}
/*
 * Append the most recent command to the ring buffer;
 */
static int RecordAdd(RecordHistory* his, char* sData, int nData) {
    if (sData && nData > 0) {
        SingleRecord*   rec = &his->records[his->head];
        char*           ln = strchr(sData, '\n');
        if (ln) { *ln = 0; nData = ln - sData; }
        //Check whether a duplicated record exists
        RecordDataRef   ref;
        ref.nData = nData;
        ref.sData = sData;
        if (RecordTraverse(his, RecordCompareCB, &ref) == 0)
            return his->head;   //same record found, do not add, just skip
        memcpy (rec->record, sData, nData);
        rec->record[nData] = 0;
        rec->len = nData;
        INC_HIS_IDX(his->head);
        if (his->count < MAX_RECORDS)
            his->count ++;
    }
    his->curr = his->head;
    return his->head;
}

static int RecordScroll(RecordHistory* his, char* sData, bool scroll_up) {
    if (NULL == sData) return 0;
    if (his->count > 0) {
        if (scroll_up) DEC_INDEX(his->curr, his->count);
            else INC_INDEX(his->curr, his->count);
        SingleRecord*   rec = &his->records[his->curr];
        if (rec->len > 0) {
            if (IsModeHex()) {
                memset (sData, 0x00, rec->len * 3 + 1);
                return HexToStr(rec->record, rec->len, sData);
            } else {
                memset (sData, 0x00, rec->len + 1);
                memcpy (sData, rec->record, rec->len);
                if (sData[rec->len - 1] == '\n') {
                    sData[rec->len - 1] = 0;
                    return (rec->len - 1);
                } else {
                    sData[rec->len] = 0;
                    return rec->len;
                }
            }
        } else return 0;
    } else return 0;
}

#define FN_HISTOR_ASCIIY    "commate_ascii.cmd"
#define FN_HISTOR_HEXA      "commate_hex.cmd"

int RecordSave(void) {
    char hist_name[2][32] = { FN_HISTOR_ASCIIY, FN_HISTOR_HEXA };
    for (int n = 0; n < 2; n ++) {
        RecordHistory*  his = &static_RecHistory[n];
        FILE*   fp = fopen(hist_name[n], "w");
        if (n) RecordTraverse(his, RecordSaveHexCB, fp);
        else RecordTraverse(his, RecordSaveTextCB, fp);
        if (fp) fclose(fp);
    }
    return 0;
}

int RecordLoad(void) {
    char hist_name[2][32] = { FN_HISTOR_ASCIIY, FN_HISTOR_HEXA };
    char sData[MAX_REC_LEN];
    BYTE sHex[MAX_REC_LEN];
    
    for (int n = 0; n < 2; n ++) {
        RecordHistory*  his = &static_RecHistory[n];
        FILE*   fp = fopen(hist_name[n], "r");
        if (fp) {
            while (!feof(fp)) {
                memset (sData, 0x00, MAX_REC_LEN);
                fgets(sData, MAX_REC_LEN - 1, fp);
                int nData = strlen(sData), nHex = 0;
                if (nData > 1) {
                    if (n) {    //Hex
                        memset (sHex , 0x00, MAX_REC_LEN);
                        if ((nHex = StrToHex(sData,sHex)) > 0)
                            RecordAdd(his, sHex, nHex);
                    } else RecordAdd(his, sData, nData);    //Ascii
                }
            }
            fclose(fp);
        }
    }
    return 0;
}

static int SendUserString(SerialCom* pcomm, char* sData, int nData) {
    bool            bHex = !!IsModeHex();
    RecordHistory*  his = &static_RecHistory[bHex];
    int nSent = 0, nHex = 0;

    if (NULL == sData || nData <= 0) return 0;
    if (bHex) {  //in Hex mode
        BYTE    sHex[MAX_REC_LEN];
        memset (sHex , 0x00, MAX_REC_LEN);
        if ((nHex = StrToHex(sData,sHex)) > 0) {
            nSent = ScSend(pcomm, (char*)sHex, nHex);
            RecordAdd(his, sHex, nHex);
        }
    } else {            //in Ascii mode
        nSent = ScSend(pcomm, sData, nData);
        if (nData > 1 || sData[0] != '\n')
            RecordAdd(his, sData, nData);
    }
    memset(sData, 0x00, nData);
    return nSent;
}


static char* ParseUserInput(void) {
    static char sQueue[MAX_REC_LEN];
    static int  nPos;
//    memset(sQueue, 0x00, 1024);          //from stdin input.(need ENTER key)
//    fgets(sQueue,1023,stdin);            //WAIT here until ENTER key pressed.
    while (nPos < MAX_REC_LEN) {
        int key = VT100_Char(getchar());
        int len = strlen(sQueue);

        if (key < 0) {  //switch to menu mode
            memset (sQueue, 0x00, MAX_REC_LEN);  return NULL;
        } else if (0 < key && key < 256) {
            if (nPos < len) {  //Insert a character at position nPos
                int nRest = len - nPos;
                memmove(sQueue + nPos + 1, sQueue + nPos, nRest);
                PRINT_AND_STAY("%s", sQueue + nPos + 1);
            } else nPos = len;
            sQueue[nPos ++] = (char) key;
            sQueue[len + 1] = 0;
        } else {
            bool bHex = !!IsModeHex();
            switch (key) {
                case VKEY_MOVE_UP:
                case VKEY_MOVE_DOWN:
                    nPos = RecordScroll(&static_RecHistory[bHex], sQueue, (VKEY_MOVE_UP == key));
                    LOAD_AND_PRINT("%s", sQueue);
                    break;
                case VKEY_MOVE_RIGHT:
                    if (sQueue[nPos]) { printf(VT100_RIGHT(1)); fflush(stdout); nPos ++;   }
                    break;
                case VKEY_MOVE_LEFT:
                    if (nPos > 0) {    printf(VT100_LEFT(1));  fflush(stdout); nPos --;   }
                    break;
                case VKEY_MOVE_DEL:
                    if (nPos >= 0 && len >= nPos) {    //Delete a character at position nPos
                        int nRest = len - nPos - 1;
                        if (nRest > 0) {
                            memmove(sQueue + nPos, sQueue + nPos + 1, nRest);
                            sQueue[len - 1] = 0;
                            PRINT_AND_STAY("%s", sQueue + nPos);
                            SYSLOG_INFO("DEL> %s[%d], pos = %d, rest = %d", sQueue, len - 1, nPos, nRest);
                        } else if (nRest == 0) {
                            sQueue[nPos] = 0;   printf(VT100_CLR_EOL);  fflush(stdout);
                        }
                    }
                    break;
                case VKEY_MOVE_BACK:
                    if (nPos > 0 && len >= nPos) {  //nPos left shift by 1
                        int nRest = len - nPos;
                        if (nRest > 0) memmove(sQueue + nPos - 1, sQueue + nPos, nRest);
                        nPos --;    sQueue[len - 1] = 0;
                        printf(VT100_LEFT(1));
                        PRINT_AND_STAY("%s", sQueue + nPos);
                        SYSLOG_INFO("BACK> %s[%d], pos = %d", sQueue, len - 1, nPos);
                    }
                    break;
                case VKEY_CTRL_LN:
                case VKEY_CTRL_SEND:
                    if (len > 0) {
                        if (nPos < len) {   //Cursor is currently in the middle
                            printf("%s" VT100_CLR_EOL, sQueue + nPos);
                        }
                        if (VKEY_CTRL_LN == key) {
                            strncat (sQueue, "\n", sizeof(sQueue));
                            printf(VT100_STYLE2  "%c" VT100_RESET, '|');
                        }
                        nPos = 0;
                        staticTxIndex ++;
                        staticTxIndex %= 2;
                        PRINT_AND_MOVE(VT100_CLR_EOL "%s", staticTxColor[staticTxIndex]);
                        return sQueue;
                    }
                    break;
                default:    break;                
            }
        }
    }
    sQueue[1023] = 0;
    return sQueue;
}

int SendUserInput(SerialCom* pcomm, CKeyboard* pkeyb) {
    int     nData = 0;
    char*   sData = ParseUserInput();
    if (sData && (nData = strlen(sData)) > 0) {  //
        return SendUserString(pcomm, sData, nData);
    } else {                //ESC key pressed
//        printf(VT100_UP(1) VT100_CLR_EOL); fflush(stdout);
        return -1;  //switch to menu mode
    }
}

char* SendUserKey(SerialCom* pcomm, char* cKeys, int nKeys) {
    char nLen = 0, cKey = cKeys[0];
    static char sRecently[MAX_KEYS_BUFF+8];
    //(menu mode or one arm shot mode).

    ScSend(pcomm, cKeys, nKeys);
    if ((nLen = strlen(sRecently)) > MAX_KEYS_BUFF ) {
        int nExceed = nLen - MAX_KEYS_BUFF;
        memmove(sRecently, sRecently + nExceed, MAX_KEYS_BUFF);
        sRecently[MAX_KEYS_BUFF] = 0;
    }
    if (cKey) {
        char sKey[16] = {0};
        if (0 < cKey && cKey < 32) sprintf(sKey, "<%d>", (char)cKey);
        else if (cKey < 128) snprintf(sKey, 2, "%c", (char)cKey);
        else snprintf(sKey, 8, "<%d>", (char)cKey);
        strcat (sRecently, sKey);
    }
    return sRecently;
}