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

#define MAX_BUFSIZE 1024
#define DESIRED_TTL 20

#define INIT 1
#define WAIT_OFFER 2
#define WAIT_ACK 3
#define IN_USE 4
#define WAIT_EXT_ACK 5
#define OFFER_TIMEOUT 6
#define ACK_TIMEOUT 7
#define EXT_ACK_TIMEOUT 8
#define EXIT 9

#define RECV_OFFER_OK 1
#define RECV_ACK_OK 2
#define TTL2_PASSED 3
#define RECV_OFFER_TIMEOUT 4
#define RECV_ACK_TIMEOUT 5
#define RECV_EXT_ACK_TIMEOUT 6
#define RECV_OFFER_NG 7
#define RECV_UNKNOWN_MSG 8
#define RECV_UNEXPECTED_MSG 9
#define RECV_TIMEOUT 10
#define RECV_ACK_NG 11
#define RECV_SIGHUP 12

#define USE 13

void f_act1(), f_act2(), f_act3(), f_act4(), f_act5(), f_act6(), f_act7(), f_act8(), f_act9();

int status, TTL, flag, ttlcounter, extflag, sighup_flag;
char lbuf[32];
struct sockaddr_in skt, myskt;
struct in_addr ipaddr;

struct proctable {
	int status;
	int event;
	void (*func)(struct sockaddr_in, int, int);
} ptab[] = {
	{WAIT_OFFER, RECV_OFFER_OK, f_act1},
    {WAIT_OFFER, RECV_OFFER_TIMEOUT, f_act2},
    {WAIT_OFFER, RECV_OFFER_NG, f_act3},
    {WAIT_OFFER, RECV_UNKNOWN_MSG, f_act3},
    {WAIT_OFFER, RECV_UNEXPECTED_MSG, f_act3},
    {WAIT_OFFER, RECV_SIGHUP, f_act7},
    {WAIT_ACK, RECV_ACK_OK, f_act4},
    {WAIT_ACK, RECV_ACK_NG, f_act3},
    {WAIT_ACK, RECV_ACK_TIMEOUT, f_act5},
    {WAIT_ACK, RECV_UNEXPECTED_MSG, f_act3},
    {WAIT_ACK, RECV_UNKNOWN_MSG, f_act3},
    {WAIT_ACK, RECV_SIGHUP, f_act7},
    {IN_USE, TTL2_PASSED, f_act6},
    {IN_USE, RECV_SIGHUP, f_act7},
    {WAIT_EXT_ACK, RECV_ACK_OK, f_act4},
    {WAIT_EXT_ACK, RECV_ACK_NG, f_act3},
    {WAIT_EXT_ACK, RECV_EXT_ACK_TIMEOUT, f_act8},
    {WAIT_EXT_ACK, RECV_UNKNOWN_MSG, f_act3},
    {WAIT_EXT_ACK, RECV_UNEXPECTED_MSG, f_act3},
    {WAIT_EXT_ACK, RECV_SIGHUP, f_act7},
    {OFFER_TIMEOUT, RECV_OFFER_OK, f_act1},
    {OFFER_TIMEOUT, RECV_OFFER_NG, f_act3},
    {OFFER_TIMEOUT, RECV_OFFER_TIMEOUT, f_act3},
    {OFFER_TIMEOUT, RECV_UNKNOWN_MSG, f_act3},
    {OFFER_TIMEOUT, RECV_UNEXPECTED_MSG, f_act3},
    {OFFER_TIMEOUT, RECV_SIGHUP, f_act7},
    {ACK_TIMEOUT, RECV_ACK_OK, f_act4},
    {ACK_TIMEOUT, RECV_ACK_NG, f_act3},
    {ACK_TIMEOUT, RECV_ACK_TIMEOUT, f_act3},
    {ACK_TIMEOUT, RECV_UNKNOWN_MSG, f_act3},
    {ACK_TIMEOUT, RECV_UNEXPECTED_MSG, f_act3},
    {ACK_TIMEOUT, RECV_SIGHUP, f_act7},
    {EXT_ACK_TIMEOUT, RECV_ACK_OK, f_act4},
    {EXT_ACK_TIMEOUT, RECV_ACK_NG, f_act3},
    {EXT_ACK_TIMEOUT, RECV_EXT_ACK_TIMEOUT, f_act3},
    {EXT_ACK_TIMEOUT, RECV_UNKNOWN_MSG, f_act3},
    {EXT_ACK_TIMEOUT, RECV_UNEXPECTED_MSG, f_act3},
    {EXT_ACK_TIMEOUT, RECV_SIGHUP, f_act7},
    {IN_USE, USE, f_act9},
   	{0, 0, NULL}
};

struct dhcph {
	uint8_t    type;
	uint8_t    code;
	uint16_t   ttl;
	in_addr_t  address;
	in_addr_t  netmask;
}; 
struct dhcph d;

struct client {
    struct in_addr address;
    struct in_addr netmask;
};
struct client c;

char *dhcp_type2char(int type){
    if(type == 1) return "DISCOVER";
    if(type == 2) return "OFFER";
    if(type == 3) return "REQUEST";
    if(type == 4) return "ACK";
    if(type == 5) return "RELEASE";
    printf("error: status not found\n");
    exit(1);
    return "";
}

char *event2char(int ev){
    if(ev == 1) return "RECV_OFFER_OK";
    if(ev == 2) return "RECV_ACK_OK";
    if(ev == 3) return "TTL2_PASSED";
    if(ev == 4) return "RECV_OFFER_TIMEOUT";
    if(ev == 5) return "RECV_ACK_TIMEOUT";
    if(ev == 6) return "RECV_EXT_ACK_TIMEOUT";
    if(ev == 7) return "RECV_OFFER_NG";
    if(ev == 8) return "RECV_UNKNOWN_MSG";
    if(ev == 9) return "RECV_UNEXPECTED_MSG";
    if(ev == 10) return "RECV_TIMEOUT";
    if(ev == 11) return "RECV_ACK_NG";
    if(ev == 12) return "RECV_SIGHUP";
    printf("error: status not found\n");
    exit(1);
    return "";
}

char *status2char(int st){
    if(st == 1) return "INIT";
    if(st == 2) return "WAIT_OFFER";
    if(st == 3) return "WAIT_ACK";
    if(st == 4) return "IN_USE";
    if(st == 5) return "WAIT_EXT_ACK";
    if(st == 6) return "OFFER_TIMEOUT";
    if(st == 7) return "ACK_TIMEOUT";
    if(st == 8) return "EXT_ACK_TIMEOUT";
    if(st == 9) return "EXIT";
    exit(1);
    return "";
}

char *dhcp_code2char(int code){
    if(code == 0) return "SUCCESS";
    if(code == 1) return "FAILED OFFER";
    if(code == 2) return "REQUEST";
    if(code == 3) return "REQUEST_EXT";
    if(code == 4) return "FAILED REQUEST";
    printf("error: status not found\n");
    exit(1);
    return "";
}



void signal_handler(int signal_num){
    if(signal_num == SIGHUP){
        printf("\nSIG_HUP recieved\n");
        printf("please push enter key to send RELEASE\n");
        sighup_flag = 1;
    }
    if(signal_num == SIGALRM){
        //printf("ttlcounter: %d\n", ttlcounter);
        ttlcounter -= 1;
        if(ttlcounter < -(TTL/2)){
            printf("\nTTL is over. dhcp end.\n");
            printf("stat changed (ID %s): %s -> EXIT ##\n", inet_ntoa(ipaddr), status2char(status));
            exit(0);
        }
    }
}

int recvfromWithTimeout(int sockfd, struct dhcph *d, size_t len, socklen_t sktlen) {

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

    signal(SIGALRM, SIG_IGN);

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
    ret_recv = recvfrom(sockfd, d, len, 0, (struct sockaddr *)&myskt, &sktlen);

    return ret_recv;
}

int wait_event(int s){
    int count, ev;
    socklen_t sktlen;

    if(extflag == 0) printf("--- wait event ---\n");

    //memset(&skt, 0, sizeof skt);
    sktlen = sizeof skt;

    if(status != IN_USE){
        count = recvfromWithTimeout(s, &d, sizeof(d), sktlen);
        if(count == 0){
            printf("timeout\n");
            d.type = 0;
        }else if(count > 0){    
            printf("## message received from %s ##\n", inet_ntoa(myskt.sin_addr));
            //printf("type: %d, code: %d, status: %s\n", (int)d.type, (int)d.code, status2char(status));
            
        }else if(count < 0){
            perror("recvfrom");
            exit(1); 
        }   
    }
    if((status == WAIT_OFFER || status == OFFER_TIMEOUT) && (d.code != 0 && d.code != 1)){
        printf("unexpected message recieved\n");
        return RECV_UNEXPECTED_MSG;
    }
    if((status == WAIT_ACK || status == ACK_TIMEOUT) && (d.code != 0 && d.code != 4)){
        printf("unexpected message recieved\n");
        return RECV_UNEXPECTED_MSG;
    } 

    //printf("## type %d(%s), code %d(%s) send OFFER to %s ##\n", (int)d.type, dhcp_type2char((int)d.type), (int)d.code, dhcp_code2char((int)d.code), inet_ntoa(ipaddr));
    //printf("%s\n", buf);

    TTL = d.ttl;
    c.address.s_addr = d.address;
    c.netmask.s_addr = d.netmask;
    //printf("type: %d, code: %d, status: %s\n", (int)d.type, (int)d.code, status2char(status));
    if(sighup_flag == 1) return RECV_SIGHUP;
    switch (status)
    {
        case WAIT_OFFER:
            if(d.type == 2 && d.code == 0) return RECV_OFFER_OK;
            if(d.type == 2 && d.code == 1) return RECV_OFFER_NG;
            if(count == 0) return RECV_OFFER_TIMEOUT;
            return RECV_UNKNOWN_MSG;
        case WAIT_ACK:
            if(d.type == 4 && d.code == 0) return RECV_ACK_OK;
            if(d.type == 4 && d.code == 4) return RECV_ACK_NG;
            if(count == 0) return RECV_ACK_TIMEOUT;
            return RECV_UNKNOWN_MSG;
        case IN_USE:
            if(ttlcounter < 0 && extflag == 1) {
                extflag = 0;
                return TTL2_PASSED;
            }
            return USE;
        case WAIT_EXT_ACK:
            if(d.type == 4 && d.code == 0) return RECV_ACK_OK;
            if(d.type == 4 && d.code == 4) return RECV_ACK_NG;
            if(count == 0) return RECV_EXT_ACK_TIMEOUT;
            return RECV_UNKNOWN_MSG;
        case OFFER_TIMEOUT:
            if(d.type == 2 && d.code == 0) return RECV_OFFER_OK;
            if(d.type == 2 && d.code == 1) return RECV_OFFER_NG;
            if(count == 0) return RECV_OFFER_TIMEOUT;
            return RECV_UNKNOWN_MSG;
        case ACK_TIMEOUT:
            if(d.type == 4 && d.code ==0) return RECV_ACK_OK;
            if(d.type == 4 && d.code == 4) return RECV_ACK_NG;
            if(count == 0) return RECV_ACK_TIMEOUT;
            return RECV_UNKNOWN_MSG;
        case EXT_ACK_TIMEOUT:
            if(d.type == 4 && d.code ==0) return RECV_ACK_OK;
            if(d.type == 4 && d.code == 4) return RECV_ACK_NG;
            if(count == 0) return RECV_EXT_ACK_TIMEOUT;
            return RECV_UNKNOWN_MSG;
        default:
        printf("error: status not found\n");
        exit(0);
        return 0;
    }
}

int main(int argc, char* argv[]){
    struct proctable *pt;
	int event, i = 0;
    int s, s2, count;
    in_port_t port = 51230;
    struct dhcph dhcp_h;
    status = 0; ttlcounter = 0; extflag = 0; sighup_flag = 0;

    if(argv[1] == NULL){
        fprintf(stderr, "error: server-IP-address not found\n");
        exit(1);
    }else if(argv[2] != NULL){
        fprintf(stderr, "error: too many arguments\n");
        exit(1);
    }

    //ソケットを作成
    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) { 
        perror("socket");
        exit(1);
    }

    dhcp_h.type = 1;
    dhcp_h.code = 0;
    dhcp_h.address = inet_addr("0.0.0.0");
    dhcp_h.netmask = inet_addr("0.0.0.0");
    dhcp_h.ttl = 0;
    
    memset(&skt, 0, sizeof(skt));
    skt.sin_family = AF_INET;
    skt.sin_port = htons(port);
    skt.sin_addr.s_addr = inet_addr(argv[1]);
    ipaddr.s_addr = inet_addr(argv[1]);

    if ((count = sendto(s, &dhcp_h, sizeof dhcp_h, 0, (struct sockaddr *)&skt, sizeof skt)) < 0){
        perror("sendto");
        exit(1); 
    }
    printf("## type %d(%s), code %d(%s) send OFFER to %s ##\n", dhcp_h.type, dhcp_type2char(dhcp_h.type), dhcp_h.code, dhcp_code2char(dhcp_h.code), argv[1]);
    signal(SIGHUP, signal_handler);

    printf("## status changed (ID %s): %s -> %s\n", argv[1], status2char(INIT), status2char(WAIT_OFFER));
    status = WAIT_OFFER;
    
    for (;;) {
		event = wait_event(s);
		for (pt = ptab; pt->status; pt++) {
			if (pt->status == status && pt->event == event) {
				(*pt->func)(skt, s, event);
				break;
			}
		}
        if (pt->status == 0){
            fprintf(stderr, "error: status not found\n");
            //error processing;
        }
    }
}

void f_act1(struct sockaddr_in sk, int s, int ev)
{
    int count;
    struct dhcph dhcp_h;
    
    dhcp_h.type = 3;
    dhcp_h.code = 2;
    dhcp_h.address = c.address.s_addr;
    dhcp_h.netmask = c.netmask.s_addr;
    dhcp_h.ttl = DESIRED_TTL;
    struct in_addr in;
    in.s_addr = d.address;

    int datalen = sizeof(dhcp_h);
    /*
    if(TTL < DESIRED_TTL){
        printf("error: ttl is over\n");
        exit(1);
    }
    */
    
    sleep(2);
    sk.sin_family = AF_INET;
    //sk.sin_addr.s_addr = ipaddr.s_addr;
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&sk, sizeof sk)) < 0) { 
        perror("sendto");
        exit(1);
    }
    //printf("IP %s, d.type(%d), port(%d)\n", inet_ntoa(sk.sin_addr), d.type, (int)sk.sin_port);
    printf("## REQUEST (alloc) sent to %s ##\n", inet_ntoa(myskt.sin_addr));
    printf("type 3(REQUEST), code 2(alloc), ttl %d, IP %s, netmask %s\n", d.ttl, inet_ntoa(c.address), inet_ntop(AF_INET, &(c.netmask), lbuf, sizeof(lbuf)));
    printf("stat changed (ID %s): %s -> WAIT_ACK ##\n", inet_ntoa(skt.sin_addr), status2char(status));
    status = WAIT_ACK;
}

void f_act2(struct sockaddr_in sk, int s, int ev)
{
    struct dhcph dhcp_h;
    int c;
    status = OFFER_TIMEOUT;

    dhcp_h.type = 1;
    dhcp_h.code = 0;
    dhcp_h.address = 0;
    dhcp_h.netmask = 0;
    dhcp_h.ttl = 0;    

    if ((c = sendto(s, &dhcp_h, sizeof dhcp_h, 0, (struct sockaddr *)&skt, sizeof skt)) < 0){
        perror("sendto");
        exit(1); 
    }
    printf("## type %d(%s), code %d(%s) resend OFFER to %s ##\n", dhcp_h.type, dhcp_type2char(dhcp_h.type), dhcp_h.code, dhcp_code2char(dhcp_h.code), inet_ntoa(ipaddr));
    printf("stat changed (ID %s): WAIT_OFFER -> OFFER_TIMEOUT ##\n", inet_ntoa(ipaddr));
}

void f_act3(struct sockaddr_in sk, int s, int ev)
{
    fprintf(stderr, "error: %s occured\n", event2char(ev));
    printf("stat changed (ID %s): %s -> EXIT ##\n", inet_ntoa(skt.sin_addr), status2char(status));
    exit(1);
}

void f_act4(struct sockaddr_in sk, int s, int ev)
{
    struct itimerval value = {1, 0, 1, 0};
    struct itimerval ovalue = {0, 0, 0, 0};

    c.address.s_addr = d.address;    
    c.netmask.s_addr = d.netmask;

    ttlcounter = TTL/2;
    printf("type 4(ACK), code 0(SUCCESS), ttl %d, IP %s, netmask %s\n", d.ttl, inet_ntoa(c.address), inet_ntop(AF_INET, &(c.netmask), lbuf, sizeof(lbuf)));
    printf("stat changed (ID %s): %s -> IN_USE ##\n", inet_ntoa(myskt.sin_addr), status2char(status));
    status = IN_USE;
    signal(SIGALRM, signal_handler);
    setitimer(ITIMER_REAL, &value, &ovalue);
    pause();
     
}

void f_act5(struct sockaddr_in sk, int s, int ev)
{
    int count, desired_ttl = DESIRED_TTL;
    struct dhcph dhcp_h;
    status = ACK_TIMEOUT;
    
    dhcp_h.type = 3;
    dhcp_h.code = 2;
    dhcp_h.address = d.address;
    dhcp_h.netmask = d.netmask;
    dhcp_h.ttl = desired_ttl;
    int datalen = sizeof(dhcp_h);

    if(desired_ttl > TTL){
        printf("error: ttl is over\n");
        exit(0);
    }
    
    sk.sin_family = AF_INET;
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&sk, sizeof sk)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## REQUEST (alloc) resent to %s ##\n", inet_ntoa(ipaddr));
    printf("type 3(REQUEST), code 2(alloc), ttl %d, IP %s, netmask %s\n", d.ttl, inet_ntoa(c.address), inet_ntop(AF_INET, &(c.netmask), lbuf, sizeof(lbuf)));
    printf("stat changed (ID %s): WAIT_ACK -> ACK_TIMEOUT ##\n", inet_ntoa(ipaddr));
}

void f_act6(struct sockaddr_in sk, int s, int ev)
{
    int count, desired_ttl = DESIRED_TTL;
    struct dhcph dhcp_h;
    status = WAIT_EXT_ACK;
    
    dhcp_h.type = 3;
    dhcp_h.code = 3;
    dhcp_h.address = d.address;
    dhcp_h.netmask = d.netmask;
    dhcp_h.ttl = desired_ttl;

    int datalen = sizeof(dhcp_h);

    if(desired_ttl > TTL){
        printf("error: ttl is over\n");
        exit(0);
    }
    
    sk.sin_family = AF_INET;
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&sk, sizeof sk)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## REQUEST (EXT) sent to %s ##\n", inet_ntoa(ipaddr));
    printf("type 3(REQUEST), code 3(EXT), ttl %d, IP %s, netmask %s\n", d.ttl, inet_ntoa(c.address), inet_ntop(AF_INET, &(c.netmask), lbuf, sizeof(lbuf)));
    printf("stat changed (ID %s): IN_USE -> WAIT_EXT_ACK ##\n", inet_ntoa(myskt.sin_addr));
}

void f_act7(struct sockaddr_in sk, int s, int ev)
{
    int count, desired_ttl = 20;
    struct dhcph dhcp_h;
    
    dhcp_h.type = 5;
    dhcp_h.code = 0;
    dhcp_h.address = d.address;
    dhcp_h.netmask = 0;
    dhcp_h.ttl = 0;

    int datalen = sizeof(dhcp_h);

    if(desired_ttl > TTL){
        printf("error: ttl is over\n");
        exit(0);
    }
    
    sk.sin_family = AF_INET;
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&sk, sizeof sk)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## RELEASE sent to %s ##\n", inet_ntoa(ipaddr));
    printf("type 5(RELEASE), ttl %d, IP %s, netmask %s\n", dhcp_h.ttl, inet_ntoa(c.address), inet_ntop(AF_INET, &(dhcp_h.netmask), lbuf, sizeof(lbuf)));
    printf("stat changed (ID %s): %s -> EXIT ##\n", inet_ntoa(ipaddr), status2char(status));
    status = EXIT;
    exit(0);
}

void f_act8(struct sockaddr_in sk, int s, int ev)
{
    int count, desired_ttl = 20;
    struct dhcph dhcp_h;
    status = EXT_ACK_TIMEOUT;
    
    dhcp_h.type = 3;
    dhcp_h.code = 3;
    dhcp_h.address = d.address;
    dhcp_h.netmask = d.netmask;

    int datalen = sizeof(dhcp_h);

    if(desired_ttl > TTL){
        printf("error: ttl is over\n");
        exit(0);
    }
    
    sk.sin_family = AF_INET;
    if ((count = sendto(s, &dhcp_h, datalen, 0, (struct sockaddr *)&sk, sizeof sk)) < 0) { 
        perror("sendto");
        exit(1);
    }
    printf("## REQUEST (EXT) resent to %s ##\n", inet_ntoa(ipaddr));
    printf("type 3(REQUEST), code 3(EXT), ttl %d, IP %s, netmask %s\n", d.ttl, inet_ntoa(c.address), inet_ntop(AF_INET, &(c.netmask), lbuf, sizeof(lbuf)));
    printf("stat changed (ID %s): WAIT_EXT_ACK -> EXT_ACK_TIMEOUT ##\n", inet_ntoa(ipaddr));
}

void f_act9(struct sockaddr_in sk, int s, int ev){
    char buffer[256];

    if(extflag == 1) {
        pause();
        return;
    }
    for(;;){
        if(sighup_flag == 1) return;
        printf("if you want to extend using IP, please enter 'y': ");
        fgets(buffer, sizeof(buffer), stdin);
        if(strncmp(buffer, "y", 1) != 0){
            if(sighup_flag == 1) return;
            continue;
        }else{
            if(sighup_flag == 1) return;
            status = IN_USE;
            extflag = 1;
            break;
        }
    }
    return;
}