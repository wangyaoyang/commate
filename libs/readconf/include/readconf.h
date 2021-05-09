/* 
 * File:   readconf.h
 * Author: root
 *
 * Created on 2011年2月24日, 下午6:36
 */
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef READCONF_H
#define	READCONF_H

#define MAX_VALUE_SIZE 256

int read_cfg(const char *file_name,const char *key,char *value);

/**************Usage example:

int main(int argc,char *argv[]) {
    char *key="w";
    char *value=(char *)malloc(MAX_VALUE_SIZE);

    memset(value,0,MAX_VALUE_SIZE);

    int flag=read_cfg("cfg.txt",key,value);
    if (flag==1) printf("open file error");
    printf(" filename=%s key=%s, value=%s \n","cfg.txt","test",value);
    free(value);
    return 0;
}

*****************************/
#ifdef	__cplusplus
}
#endif
#endif	/* READCONF_H */

