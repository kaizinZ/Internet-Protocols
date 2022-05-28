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

#define QUIT 0x01
#define PWD 0x02
#define CWD 0x03
#define LIST 0x04
#define RETR 0x05
#define STOR 0x06
#define OK 0x10
#define CMD_EER 0x11
#define FILE_EER 0x12
#define UNKWN_EER 0x13
#define DATA 0x20

#ifndef S_IXUGO
#define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif
#define PATH_MAX 4096

enum {
    FILTER_DEFAULT, /**< '.'から始まるもの以外を表示する */
    FILTER_ALMOST,  /**< '.'と'..'以外を表示する */
    FILTER_ALL,     /**< すべて表示する */
};
static int filter = FILTER_DEFAULT;

void f_act1(), f_act2(), f_act3(), f_act4(), f_act5(), f_act6(), f_act7(), f_act8(), f_act9(), f_act10();

struct sockaddr_in myskt, *skt;
char pwd_buf[256];
int s, s2, byte;

struct myftph{
    uint8_t type;
    uint8_t code;
    uint16_t length;
    char data[1];
};

struct proctable {
	int event;
	void (*func)(int, struct myftph *);
} ptab[] = {
    {QUIT, f_act1},
    {PWD, f_act2},
    {CWD, f_act3},
    {LIST, f_act4},
    {RETR, f_act5},
    {STOR, f_act6},
   	{0, NULL}
};

void signal_handler(int num){
    if(num == SIGINT){
        close(s);
        close(s2);
        printf("ctr-C\n");
        exit(0);
    }
}

void trim_null(char p[], struct myftph *f){
    int i = 0;
    while(strcmp(&p[i], "\0") != 0){
        f->data[i] = p[i];
        i++;
    } 
    return;
}

int shell_cd(int argc, char *a[]) {
    char *home;
    if(a[0] == NULL){
        if ((home = getenv("HOME")) == NULL) {
            perror("msh");
            return -1;
        }
        if (chdir(home) == -1) {
            perror("msh");
            return -1;
        }
    }else if(chdir(a[0]) != 0) {
        perror("myFTP");
        //errnoでリターン値を変える
        return -1;
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

static char *parse_cmd_args(int argc, char*argv){
    char *path = "./";
    if (argc > 1) {
        path = argv;
    }
    return path;
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

struct myftph *list_dir(const char *base_path, struct myftph *ftph){
    DIR *dir;
    struct dirent *dent;
    char path[PATH_MAX + 1];
    size_t path_len;
    char d[8192];
    char string[100];

    memset(d, '\0', sizeof(d));
    dir = opendir(base_path);
    if (dir == NULL) {
        base_path = "./";
        dir = opendir(base_path);
    }
    path_len = strlen(base_path);
    if (path_len >= PATH_MAX - 1) {
        fprintf(stderr, "too long path\n");
        return ftph;
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
            continue;
        }
        char buf[20];
        get_mode_string(dent_stat.st_mode, buf);
        //printf("%s ", buf);
        //printf("%3d ", (int)dent_stat.st_nlink);
        //printf("%4d %4d ", dent_stat.st_uid, dent_stat.st_gid);
        if (S_ISCHR(dent_stat.st_mode) || S_ISBLK(dent_stat.st_mode)) {
            //printf("%4d,%4d ", major(dent_stat.st_rdev), minor(dent_stat.st_rdev));
        } else {
            //printf("%9lld ", dent_stat.st_size);
        }
        //printf("%s", name);
        strcat(d, buf);
        sprintf(string, "%d", dent_stat.st_nlink);
        strcat(d, "  ");
        strcat(d, string);
        sprintf(string, "%d", dent_stat.st_uid);
        strcat(d, "  ");
        strcat(d, string);
        memset(string, '\0', sizeof(string));
        sprintf(string, "%d",dent_stat.st_gid);
        strcat(d, "  ");
        strcat(d, string);
        if (S_ISCHR(dent_stat.st_mode) || S_ISBLK(dent_stat.st_mode)) {
            memset(string, '\0', sizeof(string));
            sprintf(string, "%d", major(dent_stat.st_rdev));
            strcat(d, "  ");
            strcat(d, string);
            memset(string, '\0', sizeof(string));
            sprintf(string, "%d", minor(dent_stat.st_rdev));
            strcat(d, "  ");
            strcat(d, string);
        }else{
            memset(string, '\0', sizeof(string));
            sprintf(string, "%lld", dent_stat.st_size);
            strcat(d, "  ");
            strcat(d, string);
        }
        strcat(d, "  ");
        strcat(d, name);
        strcat(d, "\n");
    }
    int i = 0;
    char *p = d;
    while(strcmp(p, "\0") != 0){
        p++;
    }
    p--;
    *p = '\0';
    strcpy(ftph->data, d);
    byte = sizeof d;
   // printf("%s", ftph->data);
    closedir(dir);
    return ftph;
}

int event_handler(struct myftph *ftph, int s){
    if(ftph->type == QUIT) return QUIT;
    if(ftph->type == PWD) return PWD;
    if(ftph->type == CWD) return CWD;
    if(ftph->type == LIST) return LIST;
    if(ftph->type == RETR) return RETR;
    if(ftph->type == STOR) return STOR;
    return 0;
}

int main(int argc, char *argv[]){
	char lbuf[MAX_BUFSIZE];
	char *buf[16];
    int i=0, err, pid, wpid, status;
    char *p, *serv;
    struct proctable *pt;
    struct addrinfo hints, *res;
    in_port_t myport = 51230; 
    socklen_t sktlen;
    struct myftph *ftph;
    uint8_t event;

    signal(SIGINT, signal_handler);

    if(argc > 1){
        printf("cd\n");
        shell_cd(argc, &argv[1]);
    }

    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) { 
        perror("socket");
        exit(1);
    }

    memset(&myskt, 0, sizeof(myskt));
    myskt.sin_family = AF_INET;
    myskt.sin_port = htons(myport);
    myskt.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&myskt, sizeof myskt) <0){
        perror("bind");
        exit(1); 
    }
    ftph = (struct myftph *)malloc(sizeof(struct myftph)+1024);
    for(;;){
        printf("listen...\n");
        if(listen(s, 5) < 0){
            perror("listen");
            exit(1);
        }

        if ((pid = fork()) < 0) {
            // フォークでエラー
            perror("msh");
            return 1;
        }else if (pid == 0) { //子プロセス
            sktlen = sizeof *skt;
            if((s2 = accept(s, (struct sockaddr *)skt, &sktlen)) < 0){
                perror("accept");
                exit(1);
            }
            for (;;) {
                //sleep(3);
                memset(ftph, 0, sizeof *ftph);
                if(recv(s2, ftph, 1024, 0) < 0){
                    perror("recv");
                    exit(1);
                }
               // printf("recieved: type(%04x), code(%04x), data(%s), length(%d)\n", ftph->type, ftph->code, ftph->data, ftph->length);
                event = event_handler(ftph, s2);
                for (pt = ptab; pt->event; pt++) {
                    if (pt->event == event) {
                        (*pt->func)(s2, ftph);
                        break;
                    }
                }
                if (pt->event == 0){
                    fprintf(stderr, "error: command not found\n");
                    //error processing;
                }
            }
            exit(0);
        }else{ //親プロセス
            do {
                wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
}

void f_act1(int s2, struct myftph *ftph){ //quit
    struct myftph *f;
    f->type = OK;
    f->code = 0x00;
    f->length = 0;
    //f->data[0] = NULL;
    if(send(s2, f, sizeof *f, 0) < 0){
        perror("send");
        close(s2);
        return;
    }
    printf("send QUIT ACK\n");
    exit(0);
}

void f_act2(int s2, struct myftph *ftph){ //pwd
    struct myftph *f = (struct myftph *)malloc(sizeof(struct myftph) + 1024);

    f->type = OK;
    f->code = 0x00;
    shell_pwd(f);
    trim_null(pwd_buf, f);
    f->length = strlen(f->data);
    //printf("send PWD\n");
   // printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f->type, f->code, f->data, f->length);
    if(send(s2, f, 1024, 0) < 0){
        perror("send");
        close(s2);
        exit(1);
    }
}

void f_act3(int s2, struct myftph *ftph){ //cwd
    struct myftph *f;
    char *a[256];
    int c;

    f = (struct myftph *)malloc(sizeof *f + 1024);
    //printf("cwd\n");
    a[0] = &(ftph->data[0]);
    c = shell_cd(1, a);
   // printf("c: %d\n", c);
    if(c == 1){
        f->type = OK;
        f->code = 0x00;
        f->length = 0;
        //f->data[0] = NULL;
        if(send(s2, f, 1024, 0) < 0){
            perror("send"); 
            close(s2);
            return;
        }
    }else{
        f->type = 0x11;
        f->code = 0x01;
        f->length = 0;
        if(send(s2, f, 1024, 0) < 0){
            perror("send"); 
            close(s2);
            return;
        }
    }
}

void f_act4(int s2, struct myftph *ftph){ //list
    struct myftph *f = (struct myftph *)malloc(sizeof(struct myftph) + 1024);
    struct myftph *f2 = (struct myftph *)malloc(sizeof(struct myftph) + 2048);
    char *d;
    int i;
    FILE *fp;
    char *path;

    //printf("dir\n");

    memset(f, 0, sizeof(*f)+1024);
    memset(f2, 0, sizeof(*f2)+2048);

    if(&(ftph->data) != NULL){
        path = parse_cmd_args(2, ftph->data);
    }else{
        path = parse_cmd_args(1, ftph->data);
    }
    if (path == NULL) {
        printf("error: no such file or directory\n");
        return;
    }

    f->type = OK;
    f->code = 0x01;
    f->length = 0;
   //f->data[0] = NULL;
   // printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f->type, f->code, f->data, f->length);
    if(send(s2, f, sizeof(*f)+1024, 0) < 0){
        perror("send");
        close(s2);
        return;
    }

    f2 = list_dir(path, f);
    //printf("%s\n", f->data);
    f2->type = DATA;
    f2->code = 0x00;
    f2->length = strlen(f2->data);
   // printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f2->type, f2->code, f2->data, f2->length);
    usleep(10000);
    if(send(s2, f2, sizeof(*f2)+2048, 0) < 0){
        perror("send");
        close(s2);
    }
    /*
    for(i = 0; i < sizeof(mybuf); i += 512){
        if(i+512 > sizeof(mybuf)){
            f->code = 0x00;
        }
        strncpy(f->data[0], &d[i], 512);
        if(send(s2, f, sizeof f, 0) < 0){
            perror("send");
            close(s2);
            return;
        }
    }
    */
}

void f_act5(int s2, struct myftph *ftph){ //retr
    struct myftph *f, *f2;
    int fd, err_flag = 0, byte = 0, flag=0;
    FILE *fp;

   // printf("retr\n");
    f = (struct myftph *)malloc(sizeof(*f) + 1024);
    memset(f, 0, sizeof(*f) + 1024);
    chmod(ftph->data, 0644);
    if((fp = fopen(ftph->data, "r")) == NULL){
        f->type = 0x12;
        f->code = 0x00;
        f->length = 0;
        err_flag = 1;
    }else{
        fclose(fp);
        if((fd = open(ftph->data, O_RDONLY)) < 0){
            perror("open");
            if(errno == EACCES){
                f->type = 0x12;
                f->code = 0x01;
                f->length = 0;
                err_flag = 1;
            }
        }else{
            f->type = 0x10;
            f->code = 0x01;
            f->length = 0;
        }
    }
   // printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f->type, f->code, f->data, f->length);
    if(send(s2, f, 1024, 0) < 0){
        perror("send");
        close(s2);
        return;
    }

    if(err_flag == 0){
        f2 = (struct myftph *)malloc(sizeof(*f2));
        while(1){
            memset(f2, 0, sizeof(*f2)+2048);
            byte = read(fd, f2->data, 2048);
           // printf("byte: %d\n", byte);
            if(byte > 0){
                if(byte < 2048){
                    f2->code = 0x00;
                    flag = 0;
                }else{
                    f2->code = 0x01;
                    flag = 1;
                }
                f2->type = 0x20;
                f2->length = byte;
              //  printf("send: type(%04x), code(%04x) length(%d)\n", f2->type, f2->code, f2->length);
                usleep(10000);
                if(send(s2, f2, sizeof(*f2)+2048, 0) < 0){
                    perror("send");
                    close(s2);
                    return;
                }
            }else{
                if(flag == 1){
                    f2->type = 0x20;
                    f2->code = 0x00;
                    f2->length = 0;
                  //  printf("send: type(%04x), code(%04x), length(%d)\n", f2->type, f2->code, f2->length);
                    if(send(s2, f2, sizeof(*f2)+2048, 0) < 0){
                        perror("send");
                        close(s);
                        return;
                    }   
                }
                break;
            }
        }
    }
    close(fd);
}

void f_act6(int s2, struct myftph *ftph){
    struct myftph *f, *f2;
    int fd, err_flag = 0, byte = 0;
    char *filename;

    f = (struct myftph *)malloc(sizeof(struct myftph)+1024);
    memset(f, 0, sizeof(*f)+1024);
    //printf("stor\n");
    filename = ftph->data;
    chmod(filename, 0644);
    //printf("filename: %s\n", filename);
    if((fd = open(filename, O_CREAT | O_WRONLY)) < 0){
        if(errno == EACCES){
            f->type = 0x10;
            f->code = 0x02;
            f->length = 0;
        }else{
            perror("open");
            f->type = 0x12;
            f->code = 0x01;
            f->length = 0;
            err_flag = 1;
        }
    }else{
        f->type = 0x10;
        f->code = 0x02;
        f->length = 0;
    }
   // printf("send: type(%04x), code(%04x), data(%s), length(%d)\n", f->type, f->code, f->data, f->length);
    if(send(s2, f, 1024, 0) < 0){
        perror("send");
        close(s2);
        return;
    }

    if(err_flag == 0){
        while(1){
            f2 = (struct myftph *)malloc(sizeof(*f2)+1024);
            memset(f2, 0, sizeof(*f2)+1024);
            if(recv(s2, f2, sizeof(*f2)+1024, 0) < 0){
                perror("send");
                close(s2);
                return;
            }
            //printf("data: %s\n", f2->data);
            //printf("recieved: type(%04x), code(%04x), length(%d)\n", f2->type, f2->code, f2->length);

            if(f2->type == 0x20 && (f2->code == 0x01 || f2->code == 0x00)){
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
    }
    close(fd);
}