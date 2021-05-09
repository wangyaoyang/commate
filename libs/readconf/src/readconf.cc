#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include "readconf.h"

int read_cfg(const char *file_name,const char *key,char *value) {
    FILE *fd = fopen(file_name,"r");
    if (fd==NULL) return 1;

    char    item[MAX_VALUE_SIZE];
    int     index=0;
    
    memset(item,0,MAX_VALUE_SIZE);

    while(fgets(item,MAX_VALUE_SIZE,fd)!=NULL) {
        char *temp = item;
        char *analyze = temp;

        if (*temp=='#') continue;

        int key_start=0,key_end=0,val_start=0,val_end=0,parse=0,parse_locate=0;
        int flag=1;

        while(temp!=NULL&&*temp!='\0'&&*temp!='\n') {
            char find_char=*(temp++);
            if (find_char=='=') {
                parse++;
                if (parse==1) parse_locate=flag;
            }
            if (find_char!=' '&&!parse) {
                if (key_start==0) {
                    key_start=flag;
                    if (*temp=='=') key_end=key_start;
                }
            }
            if (find_char!=' '&&!parse&&key_start) {
                key_end=flag;
            }
            if (find_char!=' '&&parse==1&&parse_locate!=flag&&!val_start) {
                val_start=flag;
            }
            if (find_char!=' '&&val_start) {
                val_end=flag;
            }
            flag++;
        }
        /**************************************************/
        if (key_start*key_end*val_start*val_end) {
            int index_tmp;
            char key_buf[MAX_VALUE_SIZE];
            char val_buf[MAX_VALUE_SIZE];
            char *key_tmp = key_buf;
            char *val_tmp = val_buf;
            char *key_result = NULL;
            char *val_result = NULL;
            int key_len = key_end-key_start+1;
            int val_len = val_end-val_start+1;

            memset(key_buf,0,MAX_VALUE_SIZE);
            memset(val_buf,0,MAX_VALUE_SIZE);
            if( key_len > 0 ) key_result = key_tmp;
            if( val_len > 0 ) val_result = val_tmp;
            for (index_tmp = 1; analyze != NULL && (*analyze) != '\r' &&
                (*analyze) != '\n' && (*analyze) != '\0'; index_tmp ++) {
                if (index_tmp>=key_start&&index_tmp<=key_end) {
                    *(key_tmp++) = *analyze;
                }
                if (index_tmp>=val_start&&index_tmp<=val_end) {
                    *(val_tmp++) = *analyze;
                }
                analyze++;
            }
            if (!strcasecmp(key,key_result)) {
                while(val_len--) {
                    *(value++) = *(val_result++);
                }
                *val_result = '\0';
                *value = '\0';
                fclose(fd);
                return 0;
            }
        }
    }
    fclose(fd);
    return 3;
}
