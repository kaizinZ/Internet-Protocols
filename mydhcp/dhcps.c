#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <ctype.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>

#define MAX_BUFSIZE 1024

#define INIT 1
#define WAIT_REQ 2
#define IN_USE 3
#define REQ_TIMEOUT 4
#define TERMINATE 5

#define RECV_DIS 1
#define RECV_REQ_OK 2
#define RECV_REQ_EXT_OK 3
#define RECV_REL_OK 4
#define RECV_TIMEOUT_OFFER 5
#define RECV_RELEASE_NG 6
#define RECV_UNKNOWN_MSG 7
#define RECV_REQ_NG 8
#define RECV_UNEXPECTED_MSG 9
#define RECV_REQ_EXT_NG 10
#define RECV_TIMEOUT_2 11
#define WAIT_MSG 12

void f_acts1(), f_acts2(), f_acts3(), f_acts4(), f_acts5(), f_acts6(), f_acts7();

struct proctable {
	int status;
	int event;
	void (*func)(struct sockaddr_in, int);
} ptab[] = {
	{INIT, RECV_DIS, f_acts1},
    {INIT, RECV_UNEXPECTED_MSG, f_acts4},
	{WAIT_REQ, RECV_REQ_OK, f_acts2},
    {WAIT_REQ, RECV_TIMEOUT_OFFER, f_acts3},
    {WAIT_REQ, RECV_UNKNOWN_MSG, f_acts4},
    {WAIT_REQ, RECV_UNEXPECTED_MSG, f_acts5},
	{WAIT_REQ, RECV_REQ_NG, f_acts5},
    {REQ_TIMEOUT, RECV_REQ_OK, f_acts2},
    {REQ_TIMEOUT, RECV_REQ_NG, f_acts5},
    {REQ_TIMEOUT, RECV_UNKNOWN_MSG, f_acts4},
    {REQ_TIMEOUT, RECV_UNEXPECTED_MSG, f_acts4},
    {REQ_TIMEOUT, RECV_TIMEOUT_2, f_acts4},
	{IN_USE, RECV_REQ_EXT_OK, f_acts6},
    {IN_USE, RECV_RELEASE_NG, f_acts7},
    {IN_USE, RECV_REL_OK, f_acts4},
    {IN_USE, RECV_REQ_EXT_NG, f_acts6},
    {IN_USE, RECV_UNKNOWN_MSG, f_acts4},
    {IN_USE, RECV_UNEXPECTED_MSG, f_acts4},
   	{0, 0, NULL}
};

struct dhcph {
	uint8_t    type;
	uint8_t    code;
	uint16_t   ttl;
	in_addr_t  address;
	in_addr_t  netmask;
}; 

struct client {
    struct client *fp;
    struct client *bp;
    short status;
    int ttl_flag;
    // below: network byte order
    struct in_addr id;
    struct in_addr addr;
    struct in_addr netmask;
    in_port_t port;
    uint16_t ttl;
    int ttlcounter;
};

struct client client_list, release_client, tmp;

struct address {
	struct address *fp;
	struct address *bp;
	struct in_addr addr;
	struct in_addr netmask;
};

struct address address_list;
struct address used_address_list;

int s, status, TTL, addrlen, request_flag, flag, ttlcounter, ttl_expire_flag, wait_msg_flag;
char lbuf[32];
struct sockaddr_in skt;
struct dhcph d;
char *vis;


char *status2char(int status){
    char *s;
    if(status == 0) {s = "NULL"; return s;} 
    if(status == 1) {s = "INIT"; return s;}
    if(status == 2) {s = "WAIT_REQ"; return s;}
    if(status == 3) {s = "IN_USE"; return s;}
    if(status == 4) {s = "REQ_TIMEOUT"; return s;}
    if(status == 5) {s = "TERMINATE"; return s;}
    printf("error: status not found\n");
    return "";
}
char *dhcp_type2char(int type){
    if(type == 1) return "DISCOVER";
    if(type == 2) return "OFFER";
    if(type == 3) return "REQUEST";
    if(type == 4) return "ACK";
    if(type == 5) return "RELEASE";
    printf("error: status not found\n");
    return "";
}
char *dhcp_code2char(int code){
    if(code == 0) return "SUCCESS";
    if(code == 1) return "FAILED OFFER";
    if(code == 2) return "alloc";
    if(code == 3) return "EXT";
    if(code == 4) return "FAILED REQUEST";
    printf("error: status not found\n");
    return "";
}


void insert_tail_address(struct address *h, struct address *p)
{
    p->fp = h;
    p->bp = h->bp;
    p->bp->fp = p;
    h->bp = p;
}

void remove_from_address(struct address *p)
{
    p->bp->fp = p->fp;
    p->fp->bp = p->bp;
    p->bp = p->fp = NULL;
}

void remove_from_client(struct client *p)
{
    p->bp->fp = p->fp;
    p->fp->bp = p->bp;
    p->bp = p->fp = NULL;
}

void insert_tail_client(struct client *h, struct client *p)
{
    p->fp = h;
    p->bp = h->bp;
    p->bp->fp = p;
    h->bp = p;
}

void signal_handler(int signal_num){
    struct client *c;
    if(signal_num == SIGALRM){
        for(c = client_list.fp; c != &client_list; c = c->fp){
            c->ttlcounter -= 1;
            if(c != &client_list) printf("(ID %s) ttlcounter: %d\n", inet_ntoa(c->id), c->ttlcounter);
            if(c->ttlcounter == 0){
                printf("## ID(%s): TTL is expired ##\n", inet_ntoa(c->id));
                c->ttl_flag = 1;
                ttl_expire_flag = 1;
                release_client = *c;
            }
        }
    }
}

int recvfromWithTimeout(int sockfd, socklen_t sktlen) {

    struct timeval tv;
    fd_set readfds;
    int ret_select;
    int ret_recv;

    /* タイムアウト時間を設定 */
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    /* 読み込みFD集合を空にする */
    FD_ZERO(&readfds);

    /* 読み込みFD集合にsockfdを追加 */
    FD_SET(sockfd, &readfds);

    /* selectでreadfdsのどれかが読み込み可能になるまでorタイムアウトまで待ち */
    ret_select = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    /* 戻り値をチェック */
    if (ret_select == -1) {
        /* select関数がエラー */
        perror("select");
        return -1;
    }

    if (ret_select == 0) {
        /* 読み込み可能になったFDの数が0なのでタイムアウトと判断 */
        return 0;
    }

    /* sockfdが読み込み可能なのでrecv実行 */
    ret_recv = recvfrom(sockfd, &d, sizeof(d), 0, (struct sockaddr *)&skt, &sktlen);
    return ret_recv;
}

int wait_event(char buf[MAX_BUFSIZE], int s){
    int count, ev, clientlen = 0, timeout_flag = 0, cli_count = 0, val;
    socklen_t sktlen;
    struct client *c;
    fd_set rfds;
    
    if(wait_msg_flag == 0) printf("--- wait event ---\n");

    memset(&d, 0, sizeof d);
    sktlen = sizeof skt;

    /*
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);
    if (select(s+1, &rfds, NULL, NULL, NULL) < 0) {
        printf("error: does not exist any discriptor\n");
    }
    if (FD_ISSET(s, &rfds)) {
    }
    */
    //signal(SIGALRM, SIG_IGN);
    if(status == WAIT_REQ || status == REQ_TIMEOUT){
        count = recvfromWithTimeout(s, sktlen);
        if(count == 0){
            printf("timeout\n");
            timeout_flag = 1;
        }else if(count > 0){    
            printf("## message received from %s (port %d)##\n", inet_ntoa(skt.sin_addr), skt.sin_port);
        }else if(count < 0){
            perror("recvfrom");
            exit(1); 
        }   
    }else{
        if(ttl_expire_flag == 0){
            if(wait_msg_flag == 0) printf("recieve from ...\n");
            if ((count = recvfrom(s, &d, sizeof(d), MSG_DONTWAIT,
            (struct sockaddr *)&skt, &sktlen)) < 0) { 
                if(errno == EAGAIN){
                    wait_msg_flag = 1;
                    sleep(1);
                    return WAIT_MSG;
                }else{
                    perror("recvfrom");
                    exit(1); 
                }
            }
            printf("## message received from %s (port %d)##\n", inet_ntoa(skt.sin_addr), skt.sin_port);
        }
    }
    //signal(SIGALRM, signal_handler);

    for(c=client_list.fp; c!=&client_list; c=c->fp){
        if(c->id.s_addr == skt.sin_addr.s_addr && c->port == skt.sin_port){
            status = c->status;
            flag = 1;
            break;
        }
        cli_count += 1;
    }
    if(timeout_flag == 0 && flag != 0 && c->ttl_flag == 0){
        tmp.netmask.s_addr = d.netmask;
        //printf("## message received from %s ##\n", inet_ntoa(skt.sin_addr));
        printf("type %d(%s), code %d(%s), ttl %d, IP %s, netmask %s\n", (int)(d.type), dhcp_type2char((int)(d.type)), (int)d.code, dhcp_code2char((int)(d.code)), d.ttl, inet_ntop(AF_INET, &(d.address), lbuf, sizeof(lbuf)), inet_ntoa(tmp.netmask));
    }

    for(c=client_list.fp; c!=&client_list; c=c->fp){
        clientlen ++;
    }

    for(c=client_list.fp; c!=&client_list; c=c->fp){
        if(c->id.s_addr == skt.sin_addr.s_addr && c->port == skt.sin_port) break;
    }
    
    if(cli_count == clientlen) {
        flag = 0;
    }

    if(flag == 0){
        printf("## status changed ID(%s): %s -> %s\n", inet_ntoa(skt.sin_addr), status2char(status), status2char(INIT));
        status = INIT;
        flag = 1;
    }

    if((status == WAIT_REQ || status == REQ_TIMEOUT) && ((d.code != 3 && d.code != 2) || d.ttl > TTL)){
        printf("unexpected message recieved\n");
        return RECV_UNEXPECTED_MSG;
    }else if((status == INIT) && !((d.type == 1 && d.code == 0 && d.address == inet_addr("0.0.0.0") && d.netmask == inet_addr("0.0.0.0")))){
        printf("unexpected message recieved\n");
                printf("type: %d, code: %d, status: %s\n", (int)d.type, (int)d.code, status2char(status));
        printf("a\n");
        return RECV_UNEXPECTED_MSG;
    }else if((status == IN_USE) && ((d.type != 5 && d.type != 3 && d.type != 0) || (d.code != 3 && d.code != 0))){
        printf("unexpected message recieved\n");
        return RECV_UNEXPECTED_MSG;
    }

    if(d.type == 3 && ((d.address != c->addr.s_addr) || (d.netmask != c->netmask.s_addr) || (TTL < d.ttl))){
        request_flag = 1;
    }else if((status != IN_USE) && (d.type > 5 || d.type < 0 || d.code > 4 || d.code < 0 ||(d.type != 5 && (d.address != c->addr.s_addr || d.netmask != c->netmask.s_addr || d.ttl > TTL)))){
        printf("unexpected message recieved\n");
        return RECV_UNEXPECTED_MSG;
    }

    //printf("type: %d, code: %d, status: %s\n", (int)d.type, (int)d.code, status2char(status));
    if(ttl_expire_flag == 1){
        c = &release_client;
        status = c->status;
    }
    switch (status)
    {
    case INIT:
        return RECV_DIS;
    case WAIT_REQ:
        if(timeout_flag == 1) {
            timeout_flag = 0;
            return RECV_TIMEOUT_OFFER; 
        }
        if(address_list.fp == &address_list) return RECV_REQ_NG;
        if(address_list.fp != &address_list) return RECV_REQ_OK;
        return RECV_UNKNOWN_MSG;
    case REQ_TIMEOUT:
        if(timeout_flag) {
            timeout_flag = 0;
            return RECV_TIMEOUT_2;
        }
        if(address_list.fp == &address_list) return RECV_REQ_NG;
        if(address_list.fp != &address_list) return RECV_REQ_OK;
        return RECV_UNKNOWN_MSG;
    case IN_USE:
        if(d.type == 5 || c->ttl_flag == 1) {
            return RECV_REL_OK;
        }else if(d.type == 5 && d.address != c->addr.s_addr) return RECV_RELEASE_NG;
        if(addrlen > 0 && d.code == 3) return RECV_REQ_EXT_OK;
        else if(d.address == c->addr.s_addr && d.type == 3 && d.code == 3) return RECV_REQ_EXT_NG;
        return RECV_UNKNOWN_MSG;
    default:
    printf("error: status not found\n");
    exit(1);
    return 0;
    }

}

int main(int argc, char *argv[]){
	struct proctable *pt;
	int event, i, fcounter=0, j=0;
	char *p;
	FILE *file;
	char *lbuf[128];
	struct address *q;
	char str[INET_ADDRSTRLEN];

    int count;
    in_port_t myport = 51230; //自ポート
    struct sockaddr_in myskt; //自ソケットアドレス構造体
    char buf[MAX_BUFSIZE];
    char a[32][128];
    request_flag = 0; ttl_expire_flag = 0, wait_msg_flag = 0;
    flag = 0; ttlcounter = 0;

    if(argv[1] == NULL){
        fprintf(stderr, "error: config_file not found\n");
        exit(1);
    }else if(argv[2] != NULL){
        fprintf(stderr, "error: too many arguments\n");
        exit(1);
    }

	file = fopen(argv[1], "r");
	if(file == NULL){
		printf("error : cannot open the file\n");
		return -1;
	}
    for(i = 0; fgets(a[i], sizeof(a[i]), file) != NULL; i++){
        fcounter += 1;
    }

    TTL = atoi((char *)a[0]);
    printf("--- config file: TTL %d ---\n", TTL);

    for(i=1; i < fcounter; i++){
        p = strchr(a[i], '\n');
        if ( p != NULL ) *p = '\0';
        p = strtok(a[i], " ");
        lbuf[j++] = p;
        p = strtok(NULL, " ");
        lbuf[j++] = p;
    }

  // printf("%s\n", argv[0]);
    for(i = 0; i < (fcounter-1)*2; i+=2){
        printf("--- config file: IP %s, netmask %s ---\n", lbuf[i], lbuf[i+1]);
    }

    //ソケットを作成
    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) { 
        perror("socket");
    }
    
    //バインド
    memset(&myskt, 0, sizeof(myskt));
    myskt.sin_family = AF_INET;
    myskt.sin_port = htons(myport);
    myskt.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&myskt, sizeof myskt) <0){
        perror("bind");
        exit(1); 
    }

    client_list.fp = &client_list;
    client_list.bp = &client_list;
    address_list.fp = &address_list;
    address_list.bp = &address_list;
    used_address_list.fp = &used_address_list;
    used_address_list.bp = &used_address_list;
    for(i = 0; i < fcounter+1; i+=2){
        q = (struct address *)malloc(sizeof(struct address));
        if(inet_aton(lbuf[i], &(q->addr)) == 0){
            perror("mydhcp");
        }
        if(inet_aton(lbuf[i+1], &(q->netmask)) == 0){
            perror("mydhcp");
        }
        insert_tail_address(&address_list, q);
    }
    addrlen = i;

	for (;;) {
		event = wait_event(buf, s);
        if(event == WAIT_MSG) continue;
		for (pt = ptab; pt->status; pt++) {
			if (pt->status == status && pt->event == event) {
				(*pt->func)(skt, s);
				break;
			}
		}
        if (pt->status == 0){
            fprintf(stderr, "error: status not found\n");
            //error processing;
        }
    }
}
	
void f_acts1(struct sockaddr_in skt, int s) //init
{
    int count;
    struct client *c = (struct client *)malloc(sizeof(struct client));
    struct dhcph dhcp_h;
    struct address *a = address_list.fp;
    printf("Create client, alloc IP, send OFFER\n");

    c->addr.s_addr = a->addr.s_addr;
    c->netmask.s_addr = a->netmask.s_addr;
    //printf("netmask: %s, %s\n", inet_ntoa(a->netmask), inet_ntoa(c->netmask));
    c->ttl = TTL;
    c->ttlcounter = TTL;
    c->port = skt.sin_port;
    c->id = skt.sin_addr;
    c->ttl_flag = 0;
    c->status = WAIT_REQ;
    status = WAIT_REQ;
    
    dhcp_h.type = 2;
    if(a == &address_list){
        dhcp_h.code = 1;
    }else{
        dhcp_h.code = 0;
        dhcp_h.ttl = TTL;
        dhcp_h.address = c->addr.s_addr;
        dhcp_h.netmask = c->netmask.s_addr;
    }
    insert_tail_client(&client_list, c);
    remove_from_address(a);
    insert_tail_address(&used_address_list, a);
    addrlen -= 1;

    int datalen = sizeof(dhcp_h);
    
    skt.sin_family = AF_INET;
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&skt, sizeof skt)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## OFFER (%s) sent to %s ##\n", dhcp_code2char(dhcp_h.code), inet_ntoa(c->id));
    printf("type 2(OFFER), code %d(%s), ttl %d, IP %s, netmask %s\n", dhcp_h.code, dhcp_code2char(dhcp_h.code), c->ttl, inet_ntop(AF_INET, &(c->addr), lbuf, sizeof(lbuf)), inet_ntoa(c->netmask));
    printf("stat changed (ID %s): %s -> %s ##\n", inet_ntoa(c->id), status2char(INIT), status2char(c->status));
}

void f_acts2(struct sockaddr_in skt, int s) //wait req -> in use
{
    struct client *c;
    struct dhcph dhcp_h;
    int count, datalen;
    struct itimerval value = {1, 0, 1, 0};
    struct itimerval ovalue = {0, 0, 0, 0};
    //printf("wait req -> in use\n");

    datalen = sizeof(dhcp_h);

    for(c=client_list.fp; c!=&client_list; c=c->fp){
        if(c->id.s_addr == skt.sin_addr.s_addr){
            break;
        }
    }
    c->status = IN_USE;
    status = 0;
    dhcp_h.type = 4;
    dhcp_h.address = c->addr.s_addr;
    dhcp_h.netmask = c->netmask.s_addr;
    dhcp_h.code = 0;
    dhcp_h.ttl = TTL;
    if(request_flag == 1){
        dhcp_h.code = 4;
    }
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&skt, sizeof skt)) < 0) { 
        perror("sendto");
        exit(1);
    }

    printf("## ACK (%s) sent to %s ##\n", dhcp_code2char(dhcp_h.code), inet_ntoa(c->id));
    printf("type 4(ACK), code %d(%s), ttl %d, IP %s, netmask %s\n", dhcp_h.code, dhcp_code2char(dhcp_h.code), c->ttl, inet_ntop(AF_INET, &(c->addr), lbuf, sizeof(lbuf)), inet_ntoa(c->netmask));
    printf("stat changed (ID %s): %s -> %s ##\n", inet_ntoa(c->id), status2char(WAIT_REQ), status2char(IN_USE));
    signal(SIGALRM, signal_handler);
    setitimer(ITIMER_REAL, &value, &ovalue);
    pause(); 
}

void f_acts3(struct sockaddr_in act, int s) //recv timeout
{
    struct client *c;
    struct dhcph dhcp_h;
    int count, datalen;
    printf("recv timeout\n");

    for(c=client_list.fp; c!=&client_list; c=c->fp){
        if(c->id.s_addr == skt.sin_addr.s_addr){
            break;
        }
    }

    c->status = REQ_TIMEOUT;

    dhcp_h.address = c->addr.s_addr;
    dhcp_h.netmask = c->netmask.s_addr;
    dhcp_h.type = 2;
    dhcp_h.code = 0;
    dhcp_h.ttl = TTL;

    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&skt, sizeof skt)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## OFFER (SUCCESS) sent to %s ##\n", inet_ntoa(c->id));
    printf("type 2(resend OFFER), code 0(SUCCESS), ttl %d, IP %s, netmask %s\n", c->ttl, inet_ntop(AF_INET, &(c->addr), lbuf, sizeof(lbuf)), inet_ntoa(c->netmask));
    printf("stat changed (ID %s): %s -> %s ##\n", inet_ntoa(c->id), status2char(WAIT_REQ), status2char(c->status));
}

void f_acts4(struct sockaddr_in skt, int s) //recv RELEASE OK
{
    struct client *c;
    struct dhcph dhcp_h;
    struct address *a = (struct address *)malloc(sizeof(struct address)), *d;
    int count, datalen;

    datalen = sizeof(dhcp_h);
    if(status != INIT){
        if(ttl_expire_flag == 1){
            a->netmask = release_client.netmask;
            a->addr = release_client.addr;
            c = &release_client;
        }else{
            for(c=client_list.fp; c!=&client_list; c=c->fp){ //クライアントを特定       
                if(c->id.s_addr == skt.sin_addr.s_addr){
                    a->netmask = c->netmask;
                    a->addr = c->addr;
                    break;
                }
            }
        }
        
        insert_tail_address(&address_list, a); //address_listにアドレスを戻す
        addrlen += 1;


        for(d = used_address_list.fp; d != &used_address_list; d = d->fp){
            if(d->addr.s_addr == c->addr.s_addr){
                break;
            }
        }

        if(d == &used_address_list) {
            printf("error: address not found\n");
            exit(0);
        }

        //printf("type (), code 0(SUCCESS), ttl %d, IP %s, netmask %s\n", c->ttl, inet_ntoa(c->addr), inet_ntoa(c->netmask));
        printf("stat changed (ID %s): %s -> TERMINATE ##\n", inet_ntoa(c->id), status2char(c->status));
        status = 0;
        remove_from_address(d); //used_address_listからアドレスを削除
        remove_from_client(c); //client_listからクライアントを削除
        wait_msg_flag = 0;
        ttl_expire_flag = 0;
        signal(SIGALRM, SIG_IGN);
    }else{
        flag = 0;
        wait_msg_flag = 0;
        printf("stat changed (ID %s): %s -> TERMINATE ##\n", inet_ntoa(skt.sin_addr), status2char(status));
        status = 0;
    }
}

void f_acts5(struct sockaddr_in skt, int s) //recv request NG 
{
    struct client *c;
    struct dhcph dhcp_h;
    struct address *a, *d;
    int count, datalen;

    datalen = sizeof(dhcp_h);

    for(c=client_list.fp; c!=&client_list; c=c->fp){ //クライアントを特定       
        if(c->id.s_addr == skt.sin_addr.s_addr){
            status = c->status;
            a->netmask.s_addr = c->netmask.s_addr;
            a->addr.s_addr = c->addr.s_addr;
            break;
        }
    }
    insert_tail_address(&address_list, a); //address_listにアドレスを戻す
    addrlen += 1;

    for(d = used_address_list.fp; d != &used_address_list; d = d->fp){
        if(d->addr.s_addr == c->addr.s_addr){
            break;
        }
    }
    if(d == &used_address_list) {
        printf("error: address not found\n");
        exit(0);
    }
    remove_from_address(d); //used_address_listからアドレスを削除
    remove_from_client(c); //client_listからクライアントを削除

    dhcp_h.type = 4;
    dhcp_h.address = c->addr.s_addr;
    dhcp_h.netmask = c->netmask.s_addr;
    dhcp_h.code = 4;
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&skt, sizeof skt)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## ACK (FAILED) sent to %s ##\n", inet_ntoa(c->id));
    printf("type 4(ACK), code 4(FAILED), ttl %d, IP %s, netmask %s\n", c->ttl, inet_ntop(AF_INET, &(c->addr), lbuf, sizeof(lbuf)), inet_ntoa(c->netmask));
    printf("stat changed (ID %s): %s -> TERMINATE ##\n", inet_ntoa(c->id), status2char(c->status));
    signal(SIGALRM, SIG_IGN);
    status = 0;
    wait_msg_flag = 0;

}

void f_acts6(struct sockaddr_in skt, int s) // recv request ext OK 
{
    struct client *c;
    struct dhcph dhcp_h;
    int count, datalen;

    datalen = sizeof(dhcp_h);

    for(c=client_list.fp; c!=&client_list; c=c->fp){
        if(c->id.s_addr == skt.sin_addr.s_addr){
            break;
        }
    }
    c->ttl = TTL;
    c->ttlcounter = TTL;
    dhcp_h.type = 4;
    dhcp_h.address = c->addr.s_addr;
    dhcp_h.netmask = c->netmask.s_addr;
    dhcp_h.code = 0;
    dhcp_h.ttl = TTL;
    if(request_flag == 1){
        dhcp_h.code = 4;
        request_flag = 0;
    }
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&skt, sizeof skt)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## ACK (SUCCESS) sent to %s ##\n", inet_ntoa(c->id));
    printf("type 4(ACK), code 0(SUCCESS), ttl %d, IP %s, netmask %s\n", dhcp_h.ttl, inet_ntop(AF_INET, &(c->addr), lbuf, sizeof(lbuf)), inet_ntoa(c->netmask));
    printf("## stat continue (ID %s): %s ##\n", inet_ntoa(c->id), status2char(c->status));

}

void f_acts7(struct sockaddr_in skt, int s) //RELEASE NG
{
    struct client *c;

    for(c=client_list.fp; c!=&client_list; c=c->fp){
        if(c->id.s_addr == skt.sin_addr.s_addr){
            break;
        }
    }

    printf("## RELEASE NG ##\n");
    printf("stat continue (ID %s) until TTL expire: %s ##\n", inet_ntoa(c->id), status2char(c->status));
}
