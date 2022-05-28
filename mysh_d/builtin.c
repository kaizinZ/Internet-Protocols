#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define PWD_BUFSIZE 1024
#define MAX_BUFSIZE 1024

static int p_num = 0, init_num = 0, envnum = 0;
static char *env[MAX_BUFSIZE];
extern char **environ;

int shell_cd(int argc, char *argv[]);
int shell_pwd(int argc, char *argv[]);
int shell_exit(int argc, char *argv[]);

void signal_handler();

char *builtin_str[] = {
    "cd\0",
    "pwd\0",
    "exit\0"
};

int (*builtin_func[])(int, char *[]) = {
    &shell_cd,
    &shell_pwd,
    &shell_exit
};

int msh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

void redirect(int argc, char *argv[], int pipe_locate, int envnum, char *env[]){
    int fd, i, env_count = 0;
    char filename[MAX_BUFSIZE];

    argv[argc] = NULL;
    fd=open(argv[argc+1],O_WRONLY | O_CREAT,0664);//rwrwrのこと
    if((i = dup2(fd, 1)) < 0) perror("msh");
    close(fd);
    for (i = 0; i < envnum; i++) {
        memset(filename, 0, sizeof(filename));
        if (snprintf(filename, sizeof(filename), "%s/%s", env[i], argv[0]) < 0) {
            fprintf(stderr, "Too long PATH\n");
            return;
        }
        if (execve(filename, argv, environ) < 0)
            env_count++;
        }
    if (env_count == envnum) {
        fprintf(stderr, "command not found: %s\n", filename);
        perror("msh");
        return;
    }
}

void in_redirect(int argc, char *argv[], int envnum, char *env[]){
    int fd, i, env_count = 0;
    char filename[MAX_BUFSIZE];

    argv[argc] = NULL;
    fd=open(argv[argc+1],O_RDONLY | O_CREAT,0664);//rwrwrのこと
    dup2(fd, 0);
    close(fd);
    for (i = 0; i < envnum; i++) {
        memset(filename, 0, sizeof(filename));
        if (snprintf(filename, sizeof(filename), "%s/%s", env[i], argv[0]) < 0) {
            fprintf(stderr, "Too long PATH\n");
            return;
        }
        if (execve(filename, argv, environ) < 0)
            env_count++;
        }
    if (env_count == envnum) {
        fprintf(stderr, "command not found: %s\n", argv[0]);
        perror("msh");
        return;
    }
}


int shell_cd(int argc, char *argv[]) {
    char *home;
 
    if(argc > 2){
        if(argv[2] == NULL){
            argv[2] = "\0";
        }else{
            fprintf(stderr, "cd: string not in pwd: %s\n", argv[1]);
            return -1; 
        }
    }else if(argc == 2){
        if ((home = getenv("HOME")) == NULL) {
            perror("msh");
            return 1;
        }
        if (chdir(home) == -1) {
            perror("msh");
        }
        return 1;
    }
    if (chdir(argv[1]) != 0) {
        perror("msh");
        return -1;
    }
    return 1;
}

int shell_pwd(int argc, char *argv[]){

    char pwd_buf[PWD_BUFSIZE];
    memset(pwd_buf, '\0', PWD_BUFSIZE); 
    if(argc > 1){
        if(argv[1] == NULL){
            argv[1] = "\0";
        }else{
            fprintf(stderr, "pwd: too many arguments\n");
            return 1; 
        }
    }
    if (getcwd(pwd_buf, PWD_BUFSIZE) == NULL) {
        perror("msh");
    }else{
        fprintf(stdout,"current :%s\n", pwd_buf);
    }
    return 1;

}

int shell_exit(int argc, char *argv[]) {
    return 0;
}

int mygetenv(char *buf, char *env[])
{
    char temp[MAX_BUFSIZE];
    memset(temp, 0, sizeof(temp));

    if ((buf = getenv("PATH")) == NULL) {
        perror("getenv");
        return -1;
    }
    int i;
    for (i = 0; *buf != '\0'; buf++) {
        if (i == 0) {
            env[i] = buf;
            i++;
        }
        if (*buf == ':') {
            *buf = '\0';
            env[i] = (buf + 1);
            i++;
        }
    }
    return i;
}


int shell_launch(int argc, char *argv[]) {
    pid_t pid, wpid, cpid;
    char buf[MAX_BUFSIZE], buf_env[MAX_BUFSIZE], filename[MAX_BUFSIZE], cwd[MAX_BUFSIZE];
    int status, i, j, pipe_locate[10], pipe_count=0, pid_array[9], bg_flag = 0,  re_flag = 0, env_count = 0;
    pipe_locate[0] = -1;

    //init
    if(init_num == 0){
        memset(buf_env, 0, sizeof(buf_env));
        memset(env, 0, sizeof(env));
        memset(cwd, 0, sizeof(cwd));
        if ((envnum = mygetenv(buf_env, env)) == -1) return 1;
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd");
            return 1;
        }
        envnum++;
        env[envnum - 1] = cwd;
        init_num = 1;
    }

    for (i = 0; argv[i] != NULL; i++) {
        if (strncmp(argv[i], "|", 1) == 0) {
            pipe_count++;
            pipe_locate[pipe_count] = i;
            argv[i] = NULL;
        }
    }

    if((strncmp(argv[argc-2], "&", 1)) == 0){
        bg_flag = 1;
        argv[argc-2] = NULL;
        //printf("background init\n");
    }

    /*

    if(argc > 4 && pipe_count != 0){
        if((strncmp(argv[argc-3], ">", 1)) == 0){
            re_flag = 1;
            argv[argc-3] = NULL;
        }else if((strncmp(argv[argc-3], "<", 1)) == 0){
            re_flag = 2;
            argv[argc-3] = NULL;
        }
    }
    */

    int pfd[9][2];
    if(pipe_count == 0){
        if ((pid = fork()) < 0) {
            // フォークでエラー
            perror("msh");
            return 1;
        }else if (pid == 0) {
            // 子プロセス
            //printf("child pid: %d, pgid: %d\n", getpid(), getpgid(getpid()));
            for(i=0; i < argc - 1; i++){
                if(strncmp(argv[i],">", 1) == 0){
                    redirect(i, argv, 0, envnum, env);
                }else if(strncmp(argv[i],"<", 1) == 0){
                    in_redirect(i, argv, envnum, env);
                }
            }
            env_count = 0;
            for (i = 0; i < envnum; i++) {
                memset(filename, 0, sizeof(filename));
                if (snprintf(filename, sizeof(filename), "%s/%s", env[i], argv[0]) < 0) {
                    fprintf(stderr, "Too long PATH\n");
                    return 1;
                }
                if (execve(filename, argv, environ) < 0)
                    env_count++;
            }
            if (env_count == envnum) {
                fprintf(stderr, "command not found: %s\n", argv[0]);
                perror("msh");
                return 1;
            }
            exit(0);
        }  else {
            // 親プロセス
            //printf("parent pid: %d, pgid: %d, tcgetpgrp(): %d\n", getpid(), getpgid(getpid()), tcgetpgrp(1));
            cpid = pid;
            if(setpgid(pid, pid) < 0){
                perror("msh");
                return 1;
            }

            //printf("child pid: %d, pgid: %d, tcgetpgrp(): %d\n", pid, getpgid(pid), tcgetpgrp(1));

            if(bg_flag == 0){ 
                //printf("pid: %d\n", pid);
                tcsetpgrp(1, pid);
                //printf("foreground pgid: %d\n", tcgetpgrp(1));
            }else{
                tcsetpgrp(1, getpgid(getpid()));
                //printf("foreground pgid: %d\n", tcgetpgrp(1));
                //printf("[%d] %d\n", p_num, pid);
                signal(SIGCHLD, signal_handler);
                bg_flag = 0;
                return 1;
            }
            //printf("child pid: %d, pgid: %d, tcgetpgrp(): %d\n", pid, getpgid(pid), tcgetpgrp(1));
            //printf("wait\n");
            do {
                wpid = waitpid(cpid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            tcsetpgrp(1, getpgid(getpid())); 
            //printf("after foreground pgid: %d\n", tcgetpgrp(1));
            return 1;
        }
    }


    for (i = 0; i < pipe_count + 1 && pipe_count != 0; i++) {
        if (i != pipe_count) pipe(pfd[i]);

        if ((pid_array[i] = fork()) < 0) {
            // フォークでエラー
            perror("msh");
        }else if (pid_array[i] == 0) {
            if(i == pipe_count && re_flag > 0){
                if (re_flag == 1){
                    pfd[i-1][0] = open(argv[argc-2],O_WRONLY | O_CREAT,0664);//rwrwrのこと
                    //printf("redirect\n");
                    dup2(pfd[i-1][0], 1);
                    close(pfd[i - 1][0]); close(pfd[i - 1][1]);
                    
                    for (j = 0; j < envnum; j++) {
                        memset(filename, 0, sizeof(filename));
                        if (snprintf(filename, sizeof(filename), "%s/%s", env[j], argv[pipe_locate[i]+1]) < 0) {
                            fprintf(stderr, "Too long PATH\n");
                            return 1;
                        }
                        if (execve(filename, (argv+pipe_locate[i]+1), environ) < 0){
                            env_count++;
                        }
                    }
                    if (env_count == envnum) {
                        fprintf(stderr, "command not found: %s\n", argv[0]);
                        perror("msh");
                        return 1;
                    }
                }else if (re_flag == 2){
                    in_redirect(argc-3, argv, envnum, env);
                }
                exit(0);
                return 1;
            }

            if (i == 0) {
                if(dup2(pfd[i][1], 1) < 0){
                    perror("msh");
                }
                close(pfd[i][0]); close(pfd[i][1]);
            } else if (i == pipe_count) {
                dup2(pfd[i - 1][0], 0);
                close(pfd[i - 1][0]); close(pfd[i - 1][1]);
            } else {
                dup2(pfd[i - 1][0], 0);
                dup2(pfd[i][1], 1);
                close(pfd[i - 1][0]); close(pfd[i - 1][1]);
                close(pfd[i][0]); close(pfd[i][1]);
            }

            env_count = 0;
            for (j = 0; j < envnum; j++) {
                memset(filename, 0, sizeof(filename));
                if (snprintf(filename, sizeof(filename), "%s/%s", env[j], argv[pipe_locate[i]+1]) < 0) {
                    fprintf(stderr, "Too long PATH\n");
                    return 1;
                }
                if (execve(filename, (argv+pipe_locate[i]+1), environ) < 0){
                    env_count++;
                }
            }
            if (env_count == envnum) {
                fprintf(stderr, "command not found: %s\n", argv[0]);
                perror("msh");
                return 1;
            }
            exit(0);
        }else {
            if (i > 0) {
                close(pfd[i - 1][0]); 
                close(pfd[i - 1][1]);
            }
            do {
                wpid = waitpid(pid_array[i], &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
    /*
    for (i = 0; i < pipe_count + 1; i++) {
        wait(&status);
    }  
    */
    return 1;
}

void signal_handler(int signal_num){
    if(signal_num == SIGINT){
        write(0, "\n$ ", 3);
    }else if(signal_num == SIGTSTP){
       //fprintf(stderr, "\n"); 
    }else if(signal_num == SIGCHLD){
        pid_t pid = 0;
        int child_ret;
        do {
        pid = waitpid(-1, &child_ret, WNOHANG);
        } while(pid > 0);
        printf("\n[%d] + done\n$ ", p_num++);
        //signal(SIGCHLD, SIG_IGN);
    }
}

int shell_execute(int argc, char *argv[]) {
    int i;
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);

    if (argv[0] == NULL) {
        return 1;
    }
    for (i = 0; i < msh_num_builtins(); i++) {
        if (strcmp(argv[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(argc, argv);
        }
    }
    return shell_launch(argc, argv);
}