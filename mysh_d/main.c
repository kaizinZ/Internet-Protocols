#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "builtin.c"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define MAX_BUFSIZE 1024
#define TKN_NORMAL 1
#define TKN_REDIR_IN 2
#define TKN_REDIR_OUT 3
#define TKN_PIPE 4
#define TKN_BG 5
#define TKN_EOL 6
#define TKN_EOF 7

int gettoken(char *token){
    if(*token == '<'){
        return TKN_REDIR_OUT;
    }else if (*token =='>'){
        return TKN_REDIR_IN;
    }else if(*token == '|'){
        return TKN_PIPE;
    }else if(*token == '&'){
        return TKN_BG;
    }else{
        return TKN_NORMAL;
    }
}

void getargs(int *argc, char *argv[], char *p){
    *argc = 0;
    argv[0] = NULL;
    char *buf[7] = {"", ">", "<", "|", "&", "", ""};
    //printf("a\n");
    for(;;){
        while(isblank(*p)) p++;
        if(*p == '\0') {
            argv[(*argc)++] = NULL;
            return;
        }

        if(strncmp(p, "\"", 1) == 0){ //echo "this message"
            p += 1;
            argv[(*argc)++] = p;
            while((strncmp(p, "\"", 1)!=0) && (strncmp(p, "\0", 1)!=0)) p++;
            *p = '\0';//todo もし""の後に文字があるならここで終了してはいけない．
            continue;
        }

        if(*p == '\0') {
            argv[(*argc)++] = NULL;
            return;
        }

        if(gettoken(p) > 1){
            argv[(*argc)++] = buf[gettoken(p)-1];
            *p++ = '\0';
            continue;
        }
        argv[(*argc)++] = p;
        do{
            p+=1;
        }while(*p && (gettoken(p)==TKN_NORMAL) && !isblank(*p));

        if(!isblank(*p) && gettoken(p) > 1) {
            switch (gettoken(p))
                {
                case TKN_REDIR_IN:
                    argv[(*argc)++] = buf[1];
                    *p++ = '\0';
                    break;
                
                case TKN_REDIR_OUT:
                    argv[(*argc)++] = buf[2];
                    *p++ = '\0';
                    break;
                
                case TKN_PIPE:
                    argv[(*argc)++] = buf[3];
                    *p++ = '\0';
                    break;
                
                case TKN_BG:
                   argv[(*argc)++] = buf[4];
                    *p++ = '\0';
                    break;

                default:
                    break;
                }
            continue;
        }else if(isblank(*p)){
            *p++ = '\0';
        }
    }
}

void shell(){
    int argc; char lbuf[MAX_BUFSIZE];
    char *argv[16];
    int i=0, status = 1;
    char *p;
    signal(SIGTTOU, SIG_IGN);

    while(status){
        signal(SIGCHLD, SIG_IGN);

        argc = 0;
        memset(argv, '\0', sizeof(argv));
        memset(lbuf, '\0', MAX_BUFSIZE);
        fprintf(stdout, "$ ");
        if(fgets(lbuf, sizeof(lbuf), stdin) == NULL){
            perror("msh");
            return;
        }

        p = strchr(lbuf, '\n');
        if ( p != NULL ) *p = '\0';
        getargs(&argc, argv, lbuf);
        status = shell_execute(argc, argv);
    }
}

int main(int argc, char *argv[]){
    //signal(SIGINT,  SIG_IGN);
    signal(SIGTSTP,  SIG_IGN);
    shell();
    return EXIT_SUCCESS;
}