//61812933 tamura yuki
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MAXSIZE 100
#define NHASH 4
#define BUFSIZE 12

#define STAT_LOCKED 0x00000001 /* ロックされている */
#define STAT_VALID 0x00000002 /* 有効なデータを保持 */
#define STAT_DWR 0x00000004 /* 遅延書込み */
#define STAT_KRDWR 0x00000008 /* カーネルがread/write */
#define STAT_WAITED 0x00000010 /* 待っているプロセスあり */
#define STAT_OLD 0x00000020 /* バッファのデータは古い */

void print_check_bit();

struct buf_header{
    int bufnum;
    int blkno; /* 論理ブロック番号 */
    struct buf_header *hash_fp; /* ハッシュの順方向ポインタ */
    struct buf_header *hash_bp; /* ハッシュの逆方向ポインタ */
    unsigned int stat; /* バッファの状態 */
    struct buf_header *free_fp; /* フリーリスト順方向ポインタ */
    struct buf_header *free_bp; /* フリーリスト逆方向ポインタ */
    char *chache_data; /* データ領域へのポインタ */
};


struct buf_header hash_head[NHASH];
struct buf_header freelist; /* フリーリストのリストヘッド */

int hash(int blkno){
    return blkno % NHASH;
}

int myatoi(char *str) {
    int num = 0;
    // 先頭に+付いてたら無視する
    if ( *str == '+' ) {
        str++;
    }
    // 先頭に-付いてたらtypeフラグを立てておく
    else if ( *str == '-' ) {
        fprintf(stderr, "iligal input error : minus not supported\n");
        return -1;
    }
    while(*str != '\0'){
        // 0〜9以外の文字列ならそこで終了
        if ( *str < 48 || *str > 57 ) {
            fprintf(stderr, "iligal input error : invalid character\n");
            return -1;
        }
        num += *str - 48;
        num *= 10;
        str++;
    }
    num /= 10;
    // -符号が付いていたら0から引くことで負の値に変換する
    return num;
}

void insert_head(struct buf_header *h, struct buf_header *p)
{
    p->hash_bp = h;
    p->hash_fp = h->hash_fp;
    h->hash_fp->hash_bp = p;
    h->hash_fp = p;
}

void insert_tail(struct buf_header *h, struct buf_header *p)
{
    p->hash_fp = h;
    p->hash_bp = h->hash_bp;
    p->hash_bp->hash_fp = p;
    h->hash_bp = p;
}

void insert_free_head(struct buf_header *h, struct buf_header *p)
{
    p->free_bp = h;
    p->free_fp = h->free_fp;
    h->free_fp->free_bp = p;
    h->free_fp = p;
}

void insert_free_tail(struct buf_header *h, struct buf_header *p)
{
    p->free_fp = h;
    p->free_bp = h->free_bp;
    p->free_bp->free_fp = p;
    h->free_bp = p;
}

void remove_from_hash(struct buf_header *p)
{
    p->hash_bp->hash_fp = p->hash_fp;
    p->hash_fp->hash_bp = p->hash_bp;
    p->hash_bp = p->hash_fp = NULL;
}

void remove_from_free(struct buf_header *p)
{
    p->free_bp->free_fp = p->free_fp;
    p->free_fp->free_bp = p->free_bp;
    p->free_bp = p->free_fp = NULL;
}

struct buf_header *
hash_search(int blkno)
{
    int h;
    struct buf_header *p;
    h = hash(blkno);
    for(p = hash_head[h].hash_fp; p != &hash_head[h];p = p->hash_fp){
        if (p->blkno == blkno)
            return p;
    }
    return NULL;
}

struct buf_header *
free_search(int blkno)
{
    struct buf_header *p;
    for(p = freelist.free_fp; p != &freelist;p = p->free_fp){
        if (p->blkno == blkno)
            return p;
    }
    return NULL;
}

struct buf_header *
getblk(int blkno)
{
    struct buf_header *p = hash_search(blkno);
    struct buf_header *q = NULL;
    freelist.blkno = -1;
    while (1) { 
        if (p != NULL) {
            if ((p->stat & STAT_LOCKED) == STAT_LOCKED) {
            /* scenario 5 */ 
                printf("scenario 5\n");
                //sleep(event buffer becomes remove buffer from free list);
                printf("Process goes to sleep\n");//continue;
                p->stat |= STAT_WAITED;
                return p;
            }
            /* scenario 1 */
            printf("scenario 1\n");
            p->stat |= STAT_LOCKED;
            remove_from_free(p);
            return p;
        }else{
            if (freelist.free_fp->blkno == -1) {
                /* scenario 4 */ 
                //sleep(event any buffer become free);
                printf("scenario 4\n");
                printf("Process goes to sleep\n");
                //continue;
                return p;
            }
            //remove buffer from free list; 
            if(q == NULL) q = freelist.free_fp;
            p = freelist.free_fp;
            remove_from_free(p);
            if ((q->stat & STAT_DWR) == STAT_DWR){
                /* scenario 3 */ 
                printf("scenario 3\n");
                q->stat |= STAT_LOCKED;
                q->stat &= ~STAT_DWR;
                q->stat |= STAT_KRDWR;
                q->stat |= STAT_OLD;
                //q->stat &= ~STAT_KRDWR;
                print_check_bit(p);
                printf("\n");
                p = NULL;
                continue;
            }
            /* scenario 2 */
            printf("scenario 2\n");
            p->stat |= STAT_LOCKED;
            p->stat &= ~STAT_VALID;
            p->blkno = blkno;
            remove_from_hash(p);
            insert_tail(&hash_head[hash(blkno)], p); 
            /*
            if((q->stat & STAT_OLD) == STAT_OLD){
                printf("insert old buffer to freehead\n");
                insert_free_head(&freelist, q);
                q->stat &= ~STAT_OLD;
                q->stat &= ~STAT_LOCKED;
            }
            */
            return p;
         } /* end of “else” */
    } /* end of while() */
    return NULL;
}

void brelse(struct buf_header *buffer) {
    //wakeup() 
    printf("Wakeup processes waiting for any buffer\n");
    if (STAT_WAITED == (buffer->stat & STAT_WAITED)) 
        //wake up()
        printf("Wakeup processes waiting for buffer of blkno %d\n", buffer->blkno);
    // raise_cpu_level();
    if (((buffer->stat & STAT_VALID) == STAT_VALID) && ((buffer->stat & STAT_OLD) != STAT_OLD)){
        insert_free_tail(&freelist, buffer);
    }else{
        buffer->stat &= ~STAT_OLD;
        insert_free_head(&freelist, buffer);
    }
    // lower_cpu_level();
    buffer->stat &= ~STAT_LOCKED;
    buffer->stat &= ~STAT_WAITED;
}

void brelse_proc(int n, char *argv[]){
    struct buf_header *p;
    int buf = myatoi(argv[1]);
    int hashnum = hash(buf);
    
    if(buf == -1) return;
    if(n != 2) {
        fprintf(stderr, "invalid argument error : number of argument is invalid\n");
        return;
    }
    p = free_search(buf);
    if(p != NULL){ 
        printf("this buffer is already in freelist\n");
        return;
    }
    p = hash_search(buf);
    if(p != NULL) brelse(p); 
    else fprintf(stderr, "not found error : any buffer has no block number %d\n", buf);
    return;
}

void getblk_proc(int n, char *argv[]){
    int buf = myatoi(argv[1]);
    int hashnum = hash(buf);

    if(buf == -1) return;
    if(n != 2) {
        fprintf(stderr, "invalid argument error : number of argument is invalid\n");
        return;
    }
    getblk(buf);
    return;
}

void help_proc(int, char *[]);
void init_proc(int, char *[]);
void buf_proc(int, char *[]);
void hash_proc(int, char *[]);
void free_proc(int, char *[]);
void set_proc(int, char *[]);
void reset_proc(int, char*[]);
void quit_proc(int, char *[]);
void getblk_proc(int, char *[]);
void brelse_proc(int, char *[]);

struct command_table
{
    char *cmd;
    void (*func)(int, char *[]);
    /* data */
} cmd_tbl[] = {
    {"help", help_proc},
    {"init", init_proc},
    {"buf", buf_proc},
    {"hash", hash_proc},
    {"free", free_proc},
    {"set", set_proc},
    {"reset", reset_proc},
    {"getblk", getblk_proc},
    {"brelse", brelse_proc},
    {"quit", quit_proc},
    {NULL, NULL}
};

void help_proc(int n, char *argv[]){
    if(n != 1) printf("help takes no argument\n");
    printf("HELP : here shows description of each commands\n\n");
    printf("COMMAND : init\n\tinitialize the array of buffer\n\n");
    printf("COMMAND : buf [n...]\n\tdisplay state of each buffer\n\targument:\n\t\t[n...] block number(s) (you can choose)\n\n");
    printf("COMMAND : hash [n...]\n\tdisplay the hashlist\n\targument:\n\t\t[n...] hash number. display the hashlist which has n hash\n\n");
    printf("COMMAND : free\n\tdisplay the freelist\n\n");
    printf("COMMAND : getblk [n]\n\texcute function getblk(n)\n\targument:\n\t\tlogical block number n\n\n");
    printf("COMMAND : brelse [n]\n\texcute function brelse(bp)\n\targument:\n\t\tpointer bp to buffer header corresponding logical block number n\n\n");
    printf("COMMAND : set [n] [stat...]\n\tset states to buffer having logical block number n\n\targument:\n\t\t[n] a block number\n\t\t[stat...] state of buffer. all states are below\n\t\tdata is old:O\tprocess is waiting:W\tkernel read/write:K\tdelayed write:D\tcontain valid data:V\tlocked:L\n\n");
    printf("COMMAND : reset [n] [stat...]\n\treset states to buffer having logical block number n\n\targument:\n\t\t the same as argument of command : set\n\n");
    printf("COMMAND : quit\n\tterminate the all processes\n\n");
    
}

void init(struct buf_header *p){
    struct buf_header *q = (struct buf_header *)malloc(sizeof(struct buf_header));
    printf("b\n");
    insert_tail(p, q);
}

void init_proc(int n, char *argv[]){
    struct buf_header *p1 = (struct buf_header *)malloc(sizeof(struct buf_header));
    hash_head[0].hash_fp = &hash_head[0];
    hash_head[0].hash_bp = &hash_head[0];
    hash_head[1].hash_fp = &hash_head[1];
    hash_head[1].hash_bp = &hash_head[1];
    hash_head[2].hash_fp = &hash_head[2];
    hash_head[2].hash_bp = &hash_head[2];
    hash_head[3].hash_fp = &hash_head[3];
    hash_head[3].hash_bp = &hash_head[3];
    p1->blkno=28;p1->bufnum=0;p1->stat=STAT_VALID;
    insert_tail(&hash_head[0], p1); p1 = (struct buf_header *)malloc(sizeof(struct buf_header)); 
    p1->blkno=4;p1->bufnum=1;p1->stat=STAT_VALID;
    insert_tail(&hash_head[0], p1); p1 = (struct buf_header *)malloc(sizeof(struct buf_header)); 
    p1->blkno=64;p1->bufnum=2;p1->stat=(STAT_VALID|STAT_LOCKED);
    insert_tail(&hash_head[0], p1); 

    struct buf_header *p2 = (struct buf_header *)malloc(sizeof(struct buf_header));
    p2->blkno = 17; p2->bufnum = 3; p2->stat = (STAT_VALID|STAT_LOCKED);
    insert_tail(&hash_head[1], p2); p2 = (struct buf_header *)malloc(sizeof(struct buf_header));  
    p2->blkno = 5; p2->bufnum = 4; p2->stat = STAT_VALID;
    insert_tail(&hash_head[1], p2); p2 = (struct buf_header *)malloc(sizeof(struct buf_header));  
    p2->blkno = 97; p2->bufnum = 5; p2->stat = STAT_VALID;
    insert_tail(&hash_head[1], p2); 

    struct buf_header *p3 = (struct buf_header *)malloc(sizeof(struct buf_header));
    p3->blkno = 98; p3->bufnum = 6; p3->stat = (STAT_VALID|STAT_LOCKED);
    insert_tail(&hash_head[2], p3); p3 = (struct buf_header *)malloc(sizeof(struct buf_header));
    p3->blkno = 50; p3->bufnum = 7; p3->stat = (STAT_VALID|STAT_LOCKED);
    insert_tail(&hash_head[2], p3); p3 = (struct buf_header *)malloc(sizeof(struct buf_header));
    p3->blkno = 10; p3->bufnum = 8; p3->stat = STAT_VALID;
    insert_tail(&hash_head[2], p3); 

    struct buf_header *p4 = (struct buf_header *)malloc(sizeof(struct buf_header));
    p4->blkno = 3; p4->bufnum = 9; p4->stat = STAT_VALID;
    insert_tail(&hash_head[3], p4); p4 = (struct buf_header *)malloc(sizeof(struct buf_header));
    p4->blkno = 35; p4->bufnum = 10; p4->stat = (STAT_VALID|STAT_LOCKED);
    insert_tail(&hash_head[3], p4); p4 = (struct buf_header *)malloc(sizeof(struct buf_header));
    p4->blkno = 99; p4->bufnum = 11; p4->stat = (STAT_VALID|STAT_LOCKED);
    insert_tail(&hash_head[3], p4); 

    freelist.free_bp = &freelist;
    freelist.free_fp = &freelist;
    struct buf_header *f = (struct buf_header *)malloc(sizeof(struct buf_header));
    f = hash_head[3].hash_fp;
    insert_free_tail(&freelist, f);
    f = hash_head[1].hash_fp->hash_fp;
    insert_free_tail(&freelist, f);
    f = hash_head[0].hash_fp->hash_fp;
    insert_free_tail(&freelist, f);
    f = hash_head[0].hash_fp;
    insert_free_tail(&freelist, f);
    f = hash_head[1].hash_fp->hash_fp->hash_fp;
    insert_free_tail(&freelist, f);
    f = hash_head[2].hash_fp->hash_fp->hash_fp;
    insert_free_tail(&freelist, f);
}


void print_check_bit(struct buf_header *p){ //helper for free_proc, hash_proc
    printf("[ %2d:%3d ", p->bufnum, p->blkno);
    if((p->stat & STAT_OLD) == STAT_OLD){printf("O");}else{printf("-");}
    if((p->stat & STAT_WAITED) == STAT_WAITED) printf("W");else printf("-");
    if((p->stat & STAT_DWR) == STAT_DWR) printf("D");else printf("-");
    if((p->stat & STAT_KRDWR) == STAT_KRDWR) printf("K");else printf("-");
    if((p->stat & STAT_VALID) == STAT_VALID) printf("V");else printf("-");
    if((p->stat & STAT_LOCKED) == STAT_LOCKED) printf("L");else printf("-");
    printf("] ");
}

void free_proc(int n, char *argv[]){
    struct buf_header *p;
    if(n != 1){
        fprintf(stderr, "argument error : free takes no argument\n");
        return;
    }
    p = freelist.free_fp;
    if(p == &freelist){
        printf("freelist is empty\n");
        return;
    }
    for(;p != &freelist;p = p->free_fp){
        print_check_bit(p);
    }
    printf("\n");
    return;
}

void hash_proc(int n, char *argv[]){
    int i, j, buf;
    struct buf_header *p;
    if(n > 1 && n <= NHASH+1){
        for(i=1; i < n; i++){
            buf = myatoi(argv[i]);
            if(buf == -1) return;
            if(buf > NHASH-1){
                fprintf(stderr, "iligal input error : out of range\n");
                return;
            }
        }
        for(j = 1; j < n; j++){
            buf = myatoi(argv[j]);
            for(i = 0;i < NHASH;i++){
                if(i != buf) continue;
                printf("%d: ", i);
                for(p = hash_head[i].hash_fp; p != &hash_head[i];p = p->hash_fp){
                    print_check_bit(p);
                }
                printf("\n");
            }
        }
    }else if(n == 1){
        for(i = 0;i < NHASH;i++){
            printf("%d: ", i);
            for(p = hash_head[i].hash_fp; p != &hash_head[i];p = p->hash_fp){
                print_check_bit(p);
            }
            printf("\n");
        }
    }else{
        fprintf(stderr, "argument error : too many arguments\n");
    }
}

void buf_proc(int n, char *argv[]){
    int i, j; int buf, hashnum;struct buf_header *p;
    if (n > 1 && n <= BUFSIZE+1){
        for (i = 1;i<n;i++){
            buf = myatoi(argv[i]);
            if(buf == -1){
                return;
            }
            if (buf >= 0 && buf <= BUFSIZE){
                for(j = 0; j < NHASH; j++){
                    for(p = hash_head[j].hash_fp; p != &hash_head[j];p = p->hash_fp){
                        if (p->bufnum == buf){
                            print_check_bit(p);printf("\n");
                        }
                    }
                }
            }else{
                fprintf(stderr, "iligal input error : numbers of inputs are out of buffer size\n"); 
                return;
            }
        }
    }else if(n > BUFSIZE+1){
        fprintf(stderr, "iligal input error : out of range error\n");
        return;
    }else if(n == 1){
        for(i = 0;i < NHASH;i++){
            for(p = hash_head[i].hash_fp; p != &hash_head[i];p = p->hash_fp){
                print_check_bit(p);printf("\n");
            }
        }
    }
}

void set_proc(int n, char *argv[]){
    struct buf_header *p; int i, hashnum, buf;int flag = 0;
    if (n < 3){
        fprintf(stderr, "short of input error\n");
        return;
    }
    buf = myatoi(argv[1]);
    if(buf == -1) return;
    hashnum = hash(buf);

    for(p = hash_head[hashnum].hash_fp; p != &hash_head[hashnum];p = p->hash_fp){
        if (p->blkno == buf){
            flag = 1;
            for (i = 2; i < n; i++){
                if (strcmp(argv[i], "L") == 0){ p->stat = (p->stat | STAT_LOCKED);
                }else if (strcmp(argv[i], "V") == 0) {p->stat = (p->stat | STAT_VALID);
                }else if (strcmp(argv[i], "D") == 0) {p->stat = (p->stat | STAT_DWR);
                }else if (strcmp(argv[i], "K") == 0) {p->stat = (p->stat | STAT_KRDWR);
                }else if (strcmp(argv[i], "W") == 0) {p->stat = (p->stat | STAT_WAITED);
                }else if (strcmp(argv[i], "O") == 0) {p->stat = (p->stat | STAT_OLD);
                }else{ fprintf(stderr, "invalid argument error : %s\n", argv[i]);}
            }
            break;
        }
    }
    if(flag == 0){
        fprintf(stderr,"invalid block number error\n");
        return;
    }
    printf("current state of %d buffer : ", buf);
    print_check_bit(p);
    printf("\n");
    return;
}

void reset_proc(int n, char *argv[]){
    struct buf_header *p; int i, hashnum, buf;int flag = 0;
    if (n < 3){
        fprintf(stderr, "short of input error\n");
        return;
    }
    buf = myatoi(argv[1]);
    if(buf == -1) return;
    hashnum = hash(buf);

    for(p = hash_head[hashnum].hash_fp; p != &hash_head[hashnum];p = p->hash_fp){
        if (p->blkno == buf){
            flag = 1;
            for (i = 2; i < n; i++){
                if (strcmp(argv[i], "L") == 0){ p->stat = (p->stat & ~STAT_VALID);
                }else if (strcmp(argv[i], "V") == 0) {p->stat = (p->stat & ~STAT_VALID);
                }else if (strcmp(argv[i], "D") == 0) {p->stat = (p->stat & ~STAT_DWR);
                }else if (strcmp(argv[i], "K") == 0) {p->stat = (p->stat & ~STAT_KRDWR);
                }else if (strcmp(argv[i], "W") == 0) {p->stat = (p->stat & ~STAT_WAITED);
                }else if (strcmp(argv[i], "O") == 0) {p->stat = (p->stat & ~STAT_OLD);
                }else{ fprintf(stderr, "invalid argument error : %s\n", argv[i]);}
            }
            break;
        }
    }
    if(flag == 0){
        fprintf(stderr,"invalid block number error\n");
    }
    printf("current state of %d buffer : ", buf);
    print_check_bit(p);
    printf("\n");
    return;
}

void quit_proc(int n, char *argv[]){
    printf("This program is to be end;;\n");
    exit(0);
}


void getargs(int *argc, char *argv[], char *p){
    *argc = 0;
    argv[0] = NULL;
    
    for(;;){
        while(isblank(*p)) p++;
        if(*p == '\0') return;
        argv[(*argc)++] = p;
        while(*p && !isblank(*p)) p++;
        if(*p == '\0') return;
        *p++ = '\0';
    }
    //printf("%s", argv[0]);
}

int main(){
    int argc; char lbuf[MAXSIZE];
    struct command_table *pct;
    char *argv[16];

    init_proc(argc, argv);
    for(;;){
        fprintf(stderr, "input a line: ");
        if(fgets(lbuf, sizeof(lbuf), stdin) == NULL){
            putchar('\n');
            return 0;
        }
        
        lbuf[strlen(lbuf)-1] = '\0';
        if(*lbuf == '\0') continue;
        //printf("%s", lbuf);
        getargs(&argc, argv, lbuf);

        for(pct = cmd_tbl; pct->cmd; pct++){
            //printf("%s\n", argv[1]);
            if (strcmp(argv[0], pct->cmd) == 0){
                //printf("%s", argv[0]);
                (*pct->func)(argc, argv);
                break;
            }
        }
        if (pct->cmd == NULL)
            fprintf(stderr, "unknown command\n");
    }
}