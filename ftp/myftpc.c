#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_BUFSIZE 1024
#define QUIT 1
#define PWD 2
#define CD 3
#define SDIR 4
#define LPWD 5
#define LCD 6
#define LDIR 7
#define GET 8
#define PUT 9
#define HELP 10

#ifndef S_IXUGO
#define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif
#define PATH_MAX 4096
#define MAX 1024

enum {
    FILTER_DEFAULT, /**< '.'から始まるもの以外を表示する */
    FILTER_ALMOST,  /**< '.'と'..'以外を表示する */
    FILTER_ALL,     /**< すべて表示する */
};
static int filter = FILTER_DEFAULT;
char pwd_buf[256];

void f_act1(), f_act2(), f_act3(), f_act4(), f_act5(), f_act6(), f_act7(), f_act8(), f_act9(), f_act10();

struct myftph{
    uint8_t type;
    uint8_t code;
    uint16_t length;
    char data[1];
};

struct proctable {
	int event;
	void (*func)(char *[], int, struct myftph *);
} ptab[] = {
    {QUIT, f_act1},
    {PWD, f_act2},
    {CD, f_act3},
    {SDIR, f_act4},
    {LPWD, f_act5},
    {LCD, f_act6},
    {LDIR, f_act7},
    {GET, f_act8},
    {PUT, f_act9},
    {HELP, f_act10},
   	{0, NULL}
};

int shell_cd(int argc, char *a[]) {
    char *home;

    if(a[2] != NULL){
        printf("error: too many argument\n");
        return -1;
    }else if(a[1] == NULL){
        if ((home = getenv("HOME")) == NULL) {
                perror("msh");
                return 1;
            }
            if (chdir(home) == -1) {
                perror("msh");
            }
    }else{
        if (chdir(a[1]) != 0) {
            perror("myFTP");
            //errnoでリターン値を変える
            return -1;
        }
    }
    return 1;
}

void shell_pwd(struct myftph *f){
    memset(pwd_buf, '\0', 256); 
    if (getcwd(pwd_buf, 256) == NULL) {
        perror("myFTP");
        return;
    }
    f = malloc(sizeof(*f) + sizeof(pwd_buf));
}

void getargs(int *argc, char *argv[], char *p){
    *argc = 0;
    argv[0] = NULL;
    //printf("a\n");
    for(;;){
        while(isblank(*p)) p++;
        if(*p == '\0') {
            argv[(*argc)++] = NULL;
            return;
        }

        if(*p == '\0') {
            argv[(*argc)++] = NULL;
            return;
        }

        argv[(*argc)++] = p;
        do{
            p+=1;
        }while(*p  && !isblank(*p));

        if(isblank(*p)){
            *p++ = '\0';
        }
    }
}

void trim_null(char *p, struct myftph *f){
    int i = 0;
    while(strcmp(p, "\0") != 0){
        f->data[i++] = *p;
        p++;
    } 
    return;
}

static char *parse_cmd_args(int argc, char**argv){
    char *path = "./";
    if (argc > 1) {
        path = argv[1];
    }
    return path;
}

static void print_type_indicator(mode_t mode) {
    if (S_ISREG(mode)) {
        if (mode & S_IXUGO) {
        putchar('*');
        }
    } else {
        if (S_ISDIR(mode)) {
        putchar('/');
        } else if (S_ISLNK(mode)) {
        putchar('@');
        } else if (S_ISFIFO(mode)) {
        putchar('|');
        } else if (S_ISSOCK(mode)) {
        putchar('=');
        }
    }
}

static void get_mode_string(mode_t mode, char *str) {
    str[0] = (S_ISBLK(mode))  ? 'b' :
            (S_ISCHR(mode))  ? 'c' :
            (S_ISDIR(mode))  ? 'd' :
            (S_ISREG(mode))  ? '-' :
            (S_ISFIFO(mode)) ? 'p' :
            (S_ISLNK(mode))  ? 'l' :
            (S_ISSOCK(mode)) ? 's' : '?';
    str[1] = mode & S_IRUSR ? 'r' : '-';
    str[2] = mode & S_IWUSR ? 'w' : '-';
    str[3] = mode & S_ISUID ? (mode & S_IXUSR ? 's' : 'S')
                            : (mode & S_IXUSR ? 'x' : '-');
    str[4] = mode & S_IRGRP ? 'r' : '-';
    str[5] = mode & S_IWGRP ? 'w' : '-';
    str[6] = mode & S_ISGID ? (mode & S_IXGRP ? 's' : 'S')
                            : (mode & S_IXGRP ? 'x' : '-');
    str[7] = mode & S_IROTH ? 'r' : '-';
    str[8] = mode & S_IWOTH ? 'w' : '-';
    str[9] = mode & S_ISVTX ? (mode & S_IXOTH ? 't' : 'T')
                            : (mode & S_IXOTH ? 'x' : '-');
    str[10] = '\0';
}

static void list_dir(const char *base_path) {
    DIR *dir;
    struct dirent *dent;
    char path[PATH_MAX + 1];
    size_t path_len;
    dir = opendir(base_path);
    if (dir == NULL) {
        perror(base_path);
        return;
    }
    path_len = strlen(base_path);
    if (path_len >= PATH_MAX - 1) {
        fprintf(stderr, "too long path\n");
        return;
    }
    strncpy(path, base_path, PATH_MAX);
    if (path[path_len - 1] != '/') {
        path[path_len] = '/';
        path_len++;
        path[path_len] = '\0';
    }
    while ((dent = readdir(dir)) != NULL) {
        struct stat dent_stat;
        const char *name = dent->d_name;
        if (filter != FILTER_ALL
            && name[0] == '.'
            && (filter == FILTER_DEFAULT
                || name[1 + (name[1] == '.')] == '\0')) {
        continue;
        }
        strncpy(&path[path_len], dent->d_name, PATH_MAX - path_len);
        if (lstat(path, &dent_stat) != 0) {
        perror(path);
        continue;
        }
        char buf[20];
        get_mode_string(dent_stat.st_mode, buf);
        printf("%s ", buf);
        printf("%3d ", (int)dent_stat.st_nlink);
        printf("%4d %4d ", dent_stat.st_uid, dent_stat.st_gid);
        if (S_ISCHR(dent_stat.st_mode) || S_ISBLK(dent_stat.st_mode)) {
            printf("%4d,%4d ", major(dent_stat.st_rdev), minor(dent_stat.st_rdev));
        } else {
            printf("%9lld ", dent_stat.st_size);
        }
        //strftime(buf, sizeof(buf), "%F %T", localtime(&dent_stat.st_mtimespec.tv_sec));
        //printf("%s ", buf);
        printf("%s", name);
        if (1) {
        print_type_indicator(dent_stat.st_mode);
        }
        putchar('\n');
    }
    closedir(dir);
}

int determine_event(char *argv[]){
    if(strcmp(argv[0], "quit") == 0) return QUIT;
    if(strcmp(argv[0], "pwd") == 0) return PWD;
    if(strcmp(argv[0], "cd") == 0) return CD;
    if(strcmp(argv[0], "dir") == 0) return SDIR;
    if(strcmp(argv[0], "lpwd") == 0) return LPWD;
    if(strcmp(argv[0], "lcd") == 0) return LCD;
    if(strcmp(argv[0], "ldir") == 0) return LDIR;
    if(strcmp(argv[0], "get") == 0) return GET;
    if(strcmp(argv[0], "put") == 0) return PUT;
    if(strcmp(argv[0], "help") == 0) return HELP;
    return 0;
}

int main(int argc, char *argv[]){
    char lbuf[MAX_BUFSIZE];
	char *buf[16];
    int i=0, event = 0, s, err;
    char *p, *serv;
    struct proctable *pt;
    struct addrinfo hints, *res;
    struct myftph *ftph;

    ftph = (struct myftph *)malloc(sizeof(struct myftph) + 2048);
    
    if(argv[1] == NULL){
        fprintf(stderr, "error: hostname not found\n");
        exit(1);
    }else if(argv[2] != NULL){
        fprintf(stderr, "error: too many arguments\n");
        exit(1);
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    serv = "51230"; 
    if ((err = getaddrinfo(argv[1], serv, &hints, &res)) < 0) {
        fprintf(stderr, "getaddrinfo: %s¥n", gai_strerror(err));
        exit(1); 
    }
    freeaddrinfo(res);
    
    //ソケットを作成
    if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) { 
        perror("socket");
        exit(1);
    }
    if(connect(s, res->ai_addr, res->ai_addrlen) < 0){
        perror("connect");
        exit(1);
    }

    for(;;){
        argc = 0;
        memset(buf, '\0', sizeof(buf));
        memset(lbuf, '\0', MAX_BUFSIZE);
        fprintf(stdout, "myFTP%% ");
        if(fgets(lbuf, sizeof(lbuf), stdin) == NULL){
            perror("myFTP");
            continue;
        }
        p = strchr(lbuf, '\n');
        if ( p != NULL ) *p = '\0';
        getargs(&argc, buf, lbuf);
        if(buf[0] == NULL) continue; 
        event = determine_event(buf);
        for (pt = ptab; pt->event; pt++) {
            if (pt->event == event) {
                (*pt->func)(buf, s, ftph);
                break;
            }
        }
        if (pt->event == 0){
            fprintf(stderr, "error: command not found\n");
            //error processing;
        }
    }
}

void f_act1(char *buf[], int s, struct myftph *ftph){ //quit
    struct myftph *f = (struct myftph *)malloc(sizeof(struct myftph));

    f->type = 0x01;
    f->length = 0;
    //f->data[0] = NULL;
    if(send(s, f, sizeof f, 0) < 0){
        perror("send");
        close(s);
        return;
    }
    //printf("send QUIT\n");
    memset(ftph, 0, sizeof(*ftph));
    if(recv(s, ftph, sizeof(*ftph), 0) < 0){
        perror("recv");
        exit(1);
    }
    //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
    if(ftph->type == 0x10 && ftph->code == 0x00 && ftph->length == 0){
        printf("QUIT ACK recieved\n");
    }else{
        printf("error: bad ftph\n");
    }
    close(s);
    exit(0);
}

void f_act2(char *buf[], int s, struct myftph *ftph){ // pwd
    struct myftph *f = (struct myftph *)malloc(sizeof(struct myftph));
    if(buf[1] != NULL){
        printf("error: too many arguments");
        return;
    }
    f->type = 0x02;
    f->length = 0;
    //f->data[0] = NULL;
    if(send(s, f, 1024, 0) < 0){
        perror("send");
        close(s);
        return;
    }
    //printf("send PWD\n");
    memset(ftph, 0, sizeof(struct myftph));
    if(recv(s, ftph, 1024, 0) < 0){
        perror("recv");
        close(s);
        exit(1);
    }
    //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
    if(ftph->type == 0x10){
        printf("%s\n", ftph->data);
    }
}

void f_act3(char *buf[], int s, struct myftph *ftph){ //cwd
    struct myftph *f = (struct myftph *)malloc(sizeof(struct myftph)+256);

    if(buf[2] != NULL){
        printf("error: too many argument\n");
        return;
    }
    f->type = 0x03;
    //printf("buf[1]: %s\n", buf[1]);
    if(buf[1] != NULL){
        trim_null(buf[1], f);
        f->length = strlen(f->data);
    }else{
        //f->data[0] = NULL;
        f->length = 0;
    }
    //printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f->type, f->code, f->data, f->length);
    if(send(s, f, 1024, 0) < 0){
        perror("send");
        close(s);
        return;
    }
    //printf("send CWD\n");
    memset(ftph, 0, sizeof(*ftph));
    if(recv(s, ftph, 1024, 0) < 0){
        perror("recv");
        close(s);
    }
    //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
    if(ftph->type == 0x12 && ftph->code == 0x00){
        printf("error: file or directory does not exist\n");
    }else if(ftph->type == 0x12 && ftph->code == 0x01){
        printf("error: permission denied\n");
    }else if(ftph->type == 0x11 && ftph->code == 0x01){
        printf("myFTP: No such file or directory\n");
    }
}

void f_act4(char *buf[], int s, struct myftph *ftph){ //dir
    struct myftph *f = (struct myftph *)malloc(sizeof(struct myftph)+256);
    int i=0, j=0;

    f->type = 0x04;
    if(buf[1] != NULL){
        f->data[0] = *buf[1];
        f->length = strlen(f->data);
    }else{
        //f->data[0] = '\0';
        f->length = 0;
    }

    if(send(s, f, sizeof *f, 0) < 0){
        perror("send");
        close(s);
        return;
    }
    //printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f->type, f->code, f->data, f->length);
    memset(ftph, 0, sizeof(*ftph)+2048);
    if(recv(s, ftph, 4096, 0) < 0){
        perror("recv");
        exit(1);
    }
    //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
    if(ftph->type == 0x10 && ftph->code == 0x01 && ftph->length == 0){
        memset(ftph, 0, sizeof(*ftph)+2048);
        if(recv(s, ftph, 4096, 0) < 0){
            perror("recv");
            close(s);
            exit(1);
        }
        //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
        printf("%s\n", ftph->data);
        /*
        for(;ftph->code != 0x00;){
            memset(ftph, 0, sizeof *ftph);
            if(recv(s, ftph, 1024, 0) < 0){
                perror("recv");
                exit(1);
            }
            b[i++] = *(ftph->data[0]);
        }
        for(;j < i; j++){
            printf("%c", b[j]);
        }
        */
    }else if(ftph->type == 0x12 && ftph->code == 0x00){
        printf("error: file or directory does not exist\n");
    }else if(ftph->type == 0x12 && ftph->code == 0x01){
        printf("error: permission denied\n");
    }else{
        printf("error: unknown\n");
    }
}

void f_act5(char *buf[], int s, struct myftph *ftph){ //lpwd
    struct myftph *f;

    if(buf[1] != NULL){
        printf("error: pwd takes no argument\n");
    }else{
        shell_pwd(f);
        printf("%s\n", pwd_buf);
    }
}

void f_act6(char *buf[], int s, struct myftph *ftph){ //lcd
    struct myftph *f;
    int c;

    c = shell_cd(1, buf);
}

void f_act7(char *buf[], int s, struct myftph *ftph){ //ldir
    char *path;
    if(buf[1] == NULL){
        path = parse_cmd_args(1, buf);
    }else if(buf[2] == NULL){
        path = parse_cmd_args(2, buf);
    }else{
        printf("error: too many arguments\n");
        return;
    }
    if (path == NULL) {
        printf("error: no file or directory\n");
        return;
    }
    list_dir(path);

}

void f_act8(char *buf[], int s, struct myftph *ftph){
    struct myftph *f, *f2;
    int fd, byte;
    
    if(buf[2] == NULL){
        f = malloc(sizeof(*f) + 1024);
        memset(f, 0, sizeof *f);
        trim_null(buf[1], f);
        f->length = strlen(buf[1]);
    }else{
        f = malloc(sizeof(*f) + 1024);
        memset(f, 0, sizeof *f);
        trim_null(buf[2], f);
        f->length = strlen(buf[2]);
    }
    f->type = 0x05;

    if((fd = open(f->data, O_CREAT | O_WRONLY)) < 0){
        perror("open");
        return;
    }

    if(send(s, f, 1024, 0) < 0){
        perror("send");
        close(s);
        return;
    }
    memset(ftph, 0, sizeof(*ftph)+2048);
    if(recv(s, ftph, 1024, 0) < 0){
        perror("recv");
        exit(1);
    }
    f2 = malloc(sizeof(*f2) + 2048);
    //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
    if(ftph->type == 0x10 && ftph->code == 0x01 && ftph->length == 0){
        while(1){
            memset(f2, 0, sizeof(*f2)+2048);
            if(recv(s, f2, sizeof(*f2)+2048, 0) < 0){
                perror("recv");
                return;
            } 
           // usleep(10000);
            //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", f2->type, f2->code, f2->data, f2->length);
            if(f2->type == 0x20){
                byte = write(fd, f2->data, f2->length);
                if(f2->code == 0x00){
                    //printf("a\n");
                    break;
                }
            }else{
                printf("error: type is not DATA\n");
                close(fd);
                return;
            }
        }
    }else if(f2->code == 0x00 && f2->type == 0x12){
        printf("error: file or directory does not exist\n");
    }else if(f2->type == 0x12 && f2->code == 0x01){
        printf("error: permission denied\n");
    }else{
        printf("error: CMD-ERR\n");
    }

}
void f_act9(char *buf[], int s, struct myftph *ftph){ //put
    struct myftph *f, *f2;
    int fd, byte, flag = 0;

    if(buf[1] == NULL){
        printf("argument error: put takes 1 or 2 positional argument(s)\n");
    }
    if((fd = open(buf[1], O_RDONLY)) < 0){
        perror("open");
        return;
    }else{
        if(buf[2] == NULL){
            f = (struct myftph *)malloc(sizeof(*f) + 256);
            trim_null(buf[1], f);
            f->length = strlen(buf[1]);
        }else{
            f = (struct myftph *)malloc(sizeof(*f) + 256);
            trim_null(buf[2], f);
            f->length = strlen(buf[2]);
        }
        f->type = 0x06;
       // printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f->type, f->code, f->data, f->length);
        if(send(s, f, 1024, 0) < 0){
            perror("send");
            close(s);
            return;
        }
        memset(ftph, 0, sizeof(*ftph)+2048);
        if(recv(s, ftph, 1024, 0) < 0){
            perror("recv");
            exit(1);
        }
        
        //printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
        f2 = (struct myftph *)malloc(sizeof(*f2)+1024);
        if(ftph->type == 0x10 && ftph->code == 0x02 && ftph->length == 0){
            while(1){
                usleep(10000);
                memset(f2, 0, sizeof(*f2)+1024);
                byte = read(fd, f2->data, MAX);
                //printf("byte: %d\n", byte);
                if(byte > 0){
                    if(byte < MAX){
                        f2->code = 0x00;
                        flag = 0;
                    }else{
                        f2->code = 0x01;
                        flag = 1;
                    }
                    f2->type = 0x20;
                    f2->length = byte;
                    //printf("%s\n", f2->data);
                  //  printf("send: type(%04x), code(%04x), length(%d)\n", f2->type, f2->code, f2->length);
                    if(send(s, f2, sizeof(*f2)+1024, 0) < 0){
                        perror("send");
                        close(s);
                        return;
                    }
                }else{
                    if(flag == 1){
                        f2->type = 0x20;
                        f2->code = 0x00;
                        f2->length = 0;
                       // printf("send: type(%04x), code(%04x), length(%d)\n", f2->type, f2->code, f2->length);
                        if(send(s, f2, sizeof(*f2)+1024, 0) < 0){
                            perror("send");
                            close(s);
                            return;
                        }   
                    }
                    break;
                }
            }
        }else if(ftph->code == 0x00 && ftph->type == 0x12){
            printf("error: file or directory does not exist\n");
        }else if(ftph->type == 0x12 && ftph->code == 0x01){
            printf("error: permission denied\n");
        }else{
            printf("error: CMD-ERR\n");
        }
    }

}

void f_act10(char *buf[], int s){
    printf("HELP\n");
    printf("command: quit\n\texit this proglam\n");
    printf("command: pwd\n\tdisplay server's current directory\n");
    printf("command: cd path\n\tchange directory on the server\n\tpath is name of directory\n");
    printf("command: dir [path]\n\tget the information of files on the server\n");
    printf("command: lpwd\n\tdisplay the your current directory\n");
    printf("command: lcd path\n\tchange directory on your machine\n\tpath is name of directory\n");
    printf("command: ldir path\n\tget the information of files on your machine\n");
    printf("command: get path1 [path2]\n\ttransmit path1 on the server to path2 on your machine\n");
    printf("command: put path1 [path2]\n\ttransmit path1 on your machine to path2 on the server\n");
}