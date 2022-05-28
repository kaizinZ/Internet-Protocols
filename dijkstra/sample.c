#include <stdio.h>
#define NNODE 6
#define INF 100   //infinity

int cost[NNODE][NNODE] = {
    {0, 2, 5, 1, INF, INF},
    {2, 0, 3, 2, INF, INF},
    {5, 3, 0, 3, 1, 5},
    {1, 2, 3, 0, 1, INF},
    {INF, INF, 1, 1, 0, 2},
    {INF, INF, 5, INF, 2, 0}
};

static char Nodes_Name[NNODE] = {
    'A', 'B', 'C', 'D', 'E', 'F'
};
    
int dist[NNODE];
int prev[NNODE];
int loop_flag = 0;

void calc_dijkstra();
void print_result();

int main(){
    for(int i=0;i<1;i++){
        calc_dijkstra(i);
        print_result(i);   
    }
}

void calc_dijkstra(int node_num){
    int N[NNODE];for(int i=0;i<NNODE;i++) N[i]=i;
    int N_[NNODE];
    int n = 0;int i = 0;int flag = 0;
    N_[n] = node_num;

    for(i=0;i<NNODE;i++){ //initialization
        if(cost[node_num][i] != INF){
            dist[i] = cost[node_num][i];
            prev[i] = node_num;
        }else{
            dist[i] = INF;
        }
    }
    for(i=0;i<NNODE;i++){ 
        printf("dist[%d]:%d\n", i, dist[i]);
    }

    while (loop_flag == 0) //loop
    {
        //最小のwを見つける wはdist[]で最も小さいノード
        int w = INF;
        for(i=0;i<NNODE;i++){ //N_に含まれないノードを探索
            for(int j=0;j<NNODE;j++){
                if(N_[j] == i)flag = 1;
            }
            if(i == 0){ //最初はw=i
                w = i;printf("brefore:%d\n", w);
            }else{ //比較して小さい方のdistを持つノードをw
                printf("dist[w]:%d dist[i]:%d\n", dist[w], dist[i]);
                if(flag == 0 && dist[w] > dist[i]) {w = i;printf("update:%d", w);}
            }
            flag = 0;
        }
        
        N_[++n] = w; //N_にwを追加する

        //dist[i]とprev[i]を更新する
        for(i=0;i<NNODE;i++){
            //if(N_[0] == i);continue;
            if(dist[i] > (dist[w] + cost[w][i])){
                dist[i] = dist[w] + cost[w][i];
                prev[i] = w;
            }
        }
        for(i=0;i<NNODE;i++){ //N_の要素とNの要素が同じであれば終了
            for(int j=0;j<NNODE;j++){
                if(N_[i] == N[j]){
                    flag = 1;
                }
            }
            if(flag == 0){loop_flag = 0;break;}
            flag = 0;loop_flag = 1;
        }
    }
}

void print_result(int node_num){
    printf("root node %c:\n\t", Nodes_Name[node_num]);
    for(int i=0; i<NNODE; i++){
        printf("[%c, %c, %d] ", Nodes_Name[i], Nodes_Name[prev[i]], dist[i]);
    }
    printf("\n");
}

printf("node_num:%d, dist[i]:%d, prev[i]:%d\n", node_num, dist[i], prev[i]); 