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
#include "../../libs/readconf/include/readconf.h"
#include "commate.h"


/*
 * 
 */
#define VERSION_STRING      "1.0.8 " __DATE__ " " __TIME__

static bool bEchoOn = true;
static bool bDisplayHex = false;
static bool bInvertData = true;
static bool bMenuMode = false;

static int PrintHelp() {
    printf("\n-------- commate version " VERSION_STRING "-------------");
    printf("\nUssage:");
    printf("\ncommate [-h Help] [-dev Device] [-comm PortNumber] [-rate BaudRate]\n");
    printf("            [-byte DataSize] [-stop StopBits] [-parity ParityBit]\n");
    printf("\n -dev    Specify Device Name of serial port;                  (default=/dev/ttyS0)");
    printf("\n -comm   1..8 (Index of Device Name in /etc/commate.conf);    (default=1)");
    printf("\n -rate   1200/2400/4800/9600/19200/38400/57600/115200;        (default=9600)");
    printf("\n -byte   7..8;                                                (default=8)");
    printf("\n -stop   1..2;                                                (default=1)");
    printf("\n -parity N--None|O--Odd|E--Even;                              (default=N)");
    printf("\n Without parameters means using configurations in /etc/commate.conf\n");
    return 0;
}

bool IsEchoOn(void) { return bEchoOn; }
bool IsModeHex(void) { return bDisplayHex; }

static char     staticRxIndex;
static char*    staticRxColor[2] = {VT100_COLOR(VT100_B_GREEN,VT100_F_WHITE) VT100_HIGHT_LIGHT,
                                    VT100_COLOR(VT100_B_GREEN,VT100_F_YELLOW) VT100_HIGHT_LIGHT };
static void* ThreadPrintData(void* pParam) {
    if (pParam) {
        int         i = 0;
        int         nLeft = 0;
        int         nData = 0;
        char        sData[512];
        char*       pData = sData;
        SerialCom*  pComm = (SerialCom*) pParam;

        memset(sData, 0x00, 512);
        for(;true;usleep(10000)) {
            nData = ScRead(pComm, sData + nLeft, 0, 512 - nLeft);
            if (nData > 0) {    //sData[0] is preserved for VT100 flag char '\1b'
                nData += nLeft; nLeft = 0;
                VT100_LoadCursor(CURSOR_RX);
                if (bInvertData) printf("%s", staticRxColor[staticRxIndex]);  //invert text color
                if (!bDisplayHex) {    //Ascii mode (with VT100 support)
                    for (i = nData - 4; i < nData; i ++) {  //Check the last 5 bytes
                        pData = sData + i;
                        if (VT100_H0 == *pData) {   //When ESC appears in the last 4 bytes,
                            nLeft = nData - i;      //it might be a start of VT100 code
                            *pData = 0;             //we need to ensure it's integrity by 
                            printf("%s", sData);    //holding it in buffer for further use
                            sData[0] = VT100_H0;    //and just print the part before ESC
                            if (nLeft > 1) memcpy(sData + 1, pData + 1, nLeft - 1);
                        }
                    }   //Normally, when there is no ESC in last 4 bytes, we just simply
                    if (nLeft == 0) printf("%s",sData);     //print all buffer at once.
                } else {   //Hex mode
                    for (i = 0; i < nData; i++) printf("%02X ",
                                            (unsigned char)(0x00ff & sData[i]));
                }
                staticRxIndex ++;
                staticRxIndex %= 2;
                printf(VT100_RESET);    fflush(stdout);
                VT100_SaveCursor(CURSOR_RX);
                VT100_LoadCursor(CURSOR_TX);
                memset(sData, 0x00, nData);
            } else if (nLeft > 0) { //was held to wait for the rest parts of VT100 code,
                sData[nLeft] = 0;   //but they didn't show up, therefore flush it here.
                if (bInvertData) printf("%s", staticRxColor[staticRxIndex]);  //invert text color
                printf("%s",sData);
                staticRxIndex ++;
                staticRxIndex %= 2;
                printf(VT100_RESET);    fflush(stdout);
                VT100_SaveCursor(CURSOR_RX);
                VT100_LoadCursor(CURSOR_TX);
                nLeft = 0;
            }
        }
    }
}

static void SetStatusString(char* sStatus,bool bDisplayHex,bool bEchoOn,bool bInvertData) {
    sprintf(sStatus,"[%s | %s | %s | %s | %s ] ",
#if (LANGUAGE == LANG_ENGLISH)
            bDisplayHex ? "Hex" : "Ascii",
            bEchoOn ? "Echo On" : "Echo Off",
            bInvertData ? "Invert Rx":"Normal Rx",
            "Send: ^T", "Menu: ^A");
#elif (LANGUAGE == LANG_CHINESE)
            bDisplayHex ? "Hex":"Ascii", bEchoOn ? "回显Tx":"无回显", bInvertData ? "反显Rx":"正显Rx",
            "发送: ^T", "菜单: ^A");
#else
            bDisplayHex ? "Hex mode":"Ascii mode",
            bEchoOn ? "Echo On":"Echo Off",
            bInvertData ? "Invert Rx":"Normal Rx",
            "Send: ^T", "Menu: ^A");
#endif
}

static void ShowStatusBar(char* sText) {
    printf(VT100_GOTO(1,1) VT100_STYLE_BLUE);
    printf("%s\n",sText);
    VT100_LoadCursor(CURSOR_TX);
    printf(VT100_RESET VT100_CLR_EOL);    fflush(stdout);
}

#define VT100_STYLE_HOTKEY  VT100_COLOR(VT100_B_BLACK,VT100_F_RED) VT100_HIGHT_LIGHT VT100_UNDER_LINE
#define VT100_STYLE_ACTIVE  VT100_COLOR(VT100_B_BLUE,VT100_F_YELLOW) VT100_INVERT

#define MK1(v)  (v ? VT100_STYLE_ACTIVE : VT100_STYLE_HOTKEY)
#define MW1(v)  (v ? VT100_STYLE_ACTIVE : VT100_STYLE_MENU)
#define MK0(v)  MK1(!v)
#define MW0(v)  MW1(!v)
#define MENU_ITEM(l1, L, l2, r1, R, r2, v) \
    printf( VT100_STYLE_MENU "[%s" l1 "%s" L "%s" l2 \
            VT100_STYLE_MENU "/%s" r1 "%s" R "%s" r2 VT100_STYLE_MENU "] ",\
            MW1(v), MK1(v), MW1(v), MW0(v), MK0(v), MW0(v));

#define MENU_COMMAND(K, word) printf(VT100_STYLE_MENU "[" VT100_STYLE_KEY K VT100_STYLE_MENU word "] ");

static void ShowCommandLine(void) {
    printf(VT100_GOTO(1,1));
#if (LANGUAGE == LANG_ENGLISH)
    MENU_ITEM("", "H", "ex",        "", "A", "scii"     , bDisplayHex)
    MENU_ITEM("Echo ", "O", "n",    "o", "f", "f"       , bEchoOn)
    MENU_ITEM("In", "v", "ert",     "", "N", "ormal"    , bInvertData)
    MENU_COMMAND("S", "ave")
    MENU_COMMAND("C", "lear")
    MENU_COMMAND("Q", "quit")
#elif (LANGUAGE == LANG_CHINESE)
    MENU_ITEM("", "H", "ex",        "", "A", "scii"   , bDisplayHex)
    MENU_ITEM("回显", "o", "开",     "", "f", "关"      , bEchoOn)
    MENU_ITEM("反显", "v",  "",      "正常", "N", ""    , bInvertData)
    MENU_COMMAND("S", "保存")
    MENU_COMMAND("C", "清屏")
    MENU_COMMAND("Q", "退出")
#else
    MENU_ITEM("", "H", "ex",        "", "A", "scii"     , bDisplayHex)
    MENU_ITEM("Echo ", "O", "n",    "o", "f", "f"       , bEchoOn)
    MENU_ITEM("In", "v", "ert",     "", "N", "ormal"    , bInvertData)
    MENU_COMMAND("C", "lear")
    MENU_COMMAND("Q", "quit")
#endif
    printf(">" VT100_RESET);
    printf(VT100_CLR_EOL VT100_LOAD_CURSOR);
    fflush(stdout);
}

static char VT100_Key(char* cKeys, int nKeys) {
    if (nKeys >= 3) {//Check if a VT100 Key
        if (cKeys[0] == VT100_H0 && cKeys[1] == VT100_H1) {
            if (VT100_PickCursor(cKeys, nKeys)) return 0;
            else printf("%s", cKeys);  fflush(stdout);
            return 0;
        } else return cKeys[0];
    } else return cKeys[0];
}

static bool HandleMenuKey(int nKey,CKeyboard* keyb) {  //return false to exit Application
    char    sStatus[256];
    usleep(100000);
    switch((char)nKey) {
        //if switch to HEX mode then must set to ECHO ON mode
        case 'H':   case 'h':   bEchoOn = bDisplayHex = true;   break;
        case 'A':   case 'a':   bDisplayHex = false;    break;  //switch to ASCII mode
        case 'O':   case 'o':   bEchoOn = true;         break;  //switch to ECHO ON mode
        //if switch to ECHO OFF mode then must set to ASCII mode
        case 'F':   case 'f':   bDisplayHex = bEchoOn = false;  break;
        case 'V':   case 'v':   bInvertData = true;     break;
        case 'N':   case 'n':   bInvertData = false;    break;
        case 'S':   case 's':   RecordSave();           break;
        case 'C':   case 'c':   PRINT_AND_MOVE(VT100_GOTO(2,1) VT100_CLEAR);    break;
        case 'Q':   case 'q':   return false;
        case '\n':  case KEY_TABLE:     break;  //exit Menu mode
        default:                return true;    //do nothing
    }
    //KeySetEcho(keyb, bEchoOn, false);
    KeySetEcho(keyb, false, false);
    bMenuMode = false;   //switch to Transparent mode
    memset(sStatus,0x00,256);
    SetStatusString(sStatus,bDisplayHex,bEchoOn,bInvertData);
    ShowStatusBar(sStatus);
    return true;
}

static void SwitchMode(CKeyboard* keyb) {
    bMenuMode = !bMenuMode;
    if (bMenuMode) { //switch to Command mode
        KeySetEcho(keyb, false, false);
        ShowCommandLine();
    } else {            //switch to Transparent mode
        printf(VT100_CLR_EOL VT100_LOAD_CURSOR);
        fflush(stdout);
    }
}

int main(int argc, char* argv[]) {
    CKeyboard   keyb;
    SerialCom   comm;
    pthread_t   printer = 0;
    int     nPort = 1;
    int     nBaud = 9600;
    int     nBits = 8;
    int     nStop = 1;
    char    cParity = 'N';

    if (argc < 2) {
        int     nEchoOn = 0,nInvert = 1,nAscii = 1;
        char    sParamValue[MAX_VALUE_SIZE];
        memset( sParamValue,0x00,MAX_VALUE_SIZE );
        for( int i=1; i<=16; i++ ) {
            char sKey[13];
            memset( sKey,0x00,13);
            sprintf( sKey,"NameOfPort%d",i);
            if( 0 == read_cfg(CONFIG_FILE,sKey,sParamValue) ) {
                printf("\nSet COM%d device name : %s\n",i,sParamValue);
                ScSetDevName(&comm, i-1,sParamValue);
            }
        }
        if( 0 == read_cfg(CONFIG_FILE,"Port",sParamValue) ) sscanf(sParamValue,"%d",&nPort);
        if( 0 == read_cfg(CONFIG_FILE,"Baud",sParamValue) ) sscanf(sParamValue,"%d",&nBaud);
        if( 0 == read_cfg(CONFIG_FILE,"Byte",sParamValue) ) sscanf(sParamValue,"%d",&nBits);
        if( 0 == read_cfg(CONFIG_FILE,"Stop",sParamValue) ) sscanf(sParamValue,"%d",&nStop);
        if( 0 == read_cfg(CONFIG_FILE,"Parity",sParamValue) ) sscanf(sParamValue,"%c",&cParity);
        if( 0 == read_cfg(CONFIG_FILE,"Echo",sParamValue) ) sscanf(sParamValue,"%d",&nEchoOn);
        if( 0 == read_cfg(CONFIG_FILE,"Invert",sParamValue) ) sscanf(sParamValue,"%d",&nInvert);
        if( 0 == read_cfg(CONFIG_FILE,"Ascii",sParamValue) ) sscanf(sParamValue,"%d",&nAscii);
//        if( 0 == read_cfg(CONFIG_FILE,"Langauge",sParamValue)) {
//            if (0 == strcasecmp(sParamValue,"English")) LANGUAGE = LANG_ENGLISH;
//            else if (0 == strcasecmp(sParamValue,"Chinese")) LANGUAGE = LANG_CHINESE;
//            else LANGUAGE = LANG_CHINESE;
//        }
        if (nBaud != 1200 && nBaud != 2400 && nBaud != 4800 &&
            nBaud != 9600 && nBaud != 19200 && nBaud != 38400 &&
            nBaud != 57600 && nBaud != 115200) return PrintHelp();
        if (nBits != 7 && nBits != 8) return PrintHelp();
        if (nStop != 1 && nStop != 2) return PrintHelp();
        if (cParity != 'N' && cParity != 'n' && cParity != 'O' &&
            cParity != 'o' && cParity != 'E' && cParity != 'e') return PrintHelp();
        bEchoOn = !!(nEchoOn);
        bInvertData = !!(nInvert);
        bDisplayHex = !(nAscii);
    }
    for (int i = 1; i < argc; i++) {
        char* szArgv = argv[i];
        char* pIndex = NULL;
        if ((pIndex = strstr(szArgv, "-h")) != NULL ) {
            return PrintHelp();
        } else if ((pIndex = strstr(szArgv, "-d")) != NULL) {
            if (strstr(argv[i+1], "-") != NULL) return PrintHelp();
            for (int p=1; p<=16; p++ )
                ScSetDevName(&comm, p-1,argv[i+1]);
            i ++;
        } else if ((pIndex = strstr(szArgv, "-c")) != NULL) {
            sscanf(argv[i+1], "%d", &nPort);
            if (nPort < 1 || nPort > 8) return PrintHelp();
            else i ++;
        } else if ((pIndex = strstr(szArgv, "-r")) != NULL) {
            sscanf(argv[i+1], "%d", &nBaud);
            if (nBaud != 1200 && nBaud != 2400 && nBaud != 4800 &&
                nBaud != 9600 && nBaud != 19200 && nBaud != 38400 &&
                nBaud != 57600 && nBaud != 115200) return PrintHelp();
            else i ++;
        } else if ((pIndex = strstr(szArgv, "-b")) != NULL) {
            sscanf(argv[i+1], "%d", &nBits);
            if (nBits != 7 && nBits != 8) return PrintHelp();
            else i ++;
        } else if ((pIndex = strstr(szArgv, "-s")) != NULL) {
            sscanf(argv[i+1], "%d", &nStop);
            if (nStop != 1 && nStop != 2) return PrintHelp();
            else i ++;
        } else if ((pIndex = strstr(szArgv, "-p")) != NULL) {
            sscanf(argv[i+1], "%c", &cParity);
            if (cParity != 'N' && cParity != 'n' && cParity != 'O' &&
                cParity != 'o' && cParity != 'E' && cParity != 'e') return PrintHelp();
            else i ++;
        } else {
            printf("Invalid parameter %s\n", argv[i]);
            return PrintHelp();
        }
    }
//    nPort = 1;  nBaud = 115200; nBits = 8; nStop = 1; cParity = 'N';
    if (ScOpen(&comm, nPort, nBaud, nBits, nStop, cParity) == true && KeyOpen(&keyb, true)) {
        pthread_create(&printer, NULL, ThreadPrintData, &comm);
        char    sParams[256];
        char    sStatus[256];
        char    sMessag[1024];
        char    cKeys[16];
        int     nKeys = 0;
        bool    bRun = true;

        memset(cKeys,0x00,16);
        memset(sParams,0x00,256);
        memset(sStatus,0x00,256);
        memset(sMessag,0x00,1024);
        snprintf(sParams, 1023, "COM %d,%d,%c,%d,%d/", nPort, nBaud, cParity, nBits, nStop);
        SetStatusString(sStatus,bDisplayHex,bEchoOn,bInvertData);
        snprintf(sMessag, 1023, "%s%s", sParams, sStatus);
        RecordLoad();
        usleep(100000);
        PRINT_AND_MOVE(VT100_GOTO(2,1) VT100_CLEAR);
        ShowStatusBar(sStatus);
        //KeySetEcho(&keyb, bEchoOn, false);
        KeySetEcho(&keyb, false, false);
        while (bRun) {
            if (!bMenuMode && bEchoOn) {  //wait for datas to be send
                if (SendUserInput(&comm, &keyb) < 0)
                    SwitchMode(&keyb);   //directly return to the menu mode.
            } else if ((nKeys = KeyGetKeyBuff(&keyb, cKeys,8)) > 0) {  //Key triger mode
                char cKey = VT100_Key(cKeys, nKeys);
                //(menu mode or one arm shot mode).
                if (KEY_CTRL_A == cKey) {   //mode switching
                    SwitchMode(&keyb);
                } else if (bMenuMode) { //in Command mode
                    bRun = HandleMenuKey(cKey, &keyb);
                } else if (cKey && !bDisplayHex && !bEchoOn)  { //(Ascii+Eecho off) mode
                    SetStatusString(sStatus,bDisplayHex,bEchoOn,bInvertData);
                    char* sRecently = SendUserKey(&comm, cKeys, nKeys);
                    snprintf(sMessag, 1023, "%s" VT100_STYLE2 "%s", sStatus,sRecently);
                    ShowStatusBar(sMessag);
                } else {    //(Hex+Echo on) mode or (Ascii+Echo on) mode
                }
                memset(cKeys,0x00,16);
            }
        }
        KeyClose(&keyb);
        pthread_cancel(printer);
        pthread_join(printer, NULL);
    } else {
        KeyClose(&keyb);
        ScClose(&comm);
        return 0;
    }
COMMATE_EXIT:
    ScClose(&comm);
    printf(VT100_CLEAR VT100_GOTO(1,1));
    fflush(stdout);
    return 0;
}
