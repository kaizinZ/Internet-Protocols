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

void calc_dijkstra();
void print_result();

int main(){
    int i = 0;
    for(i=0;i<NNODE;i++){
        calc_dijkstra(i);
        print_result(i);   
    }
}

void calc_dijkstra(int node_num){
    int n = 0;int i = 0;int flag = 0;int loop_flag = 0;int j = 0;
    int N[NNODE];for(i=0;i<NNODE;i++) N[i]=i; //変数の宣言，初期化
    int N_[NNODE];for(i=0;i<NNODE;i++) N_[i]=NNODE+1;
    
    N_[n] = node_num;

    for(i=0;i<NNODE;i++){ //initialization
        if(cost[node_num][i] != INF){
            dist[i] = cost[node_num][i];
            prev[i] = node_num;
        }else{dist[i] = INF;prev[i] = node_num;}
    }

    while (loop_flag == 0){ //loop
        int w = INF; int min = INF;
        for(i=0;i<NNODE;i++){ //N_に含まれないノードを探索 最小のwを見つける wはdist[]が最も小さいノード
            for(j=0;j<NNODE;j++){if(N_[j] == i) flag = 1;}
            if(dist[i] != INF && dist[i] != 0 && flag == 0){
                if(min > dist[i]){w = i; min = dist[i];}
            }
            flag = 0;
        }

        N_[++n] = w; //N_にwを追加する

        for(i=0;i<NNODE;i++){ //dist[i]とprev[i]を更新する
            if(dist[i] > (dist[w] + cost[w][i])){
                dist[i] = dist[w] + cost[w][i];
                prev[i] = w;
            }
        }
        
        for(i=0;i<NNODE;i++){ //N_の要素とNの要素が同じであれば終了
            for(j=0;j<NNODE;j++){if(N_[i] == N[j]) flag = 1;}
            if(flag == 0){loop_flag = 0;break;}
            flag = 0; loop_flag = 1;
        }
    }
}

void print_result(int node_num){
    int i;
    printf("root node %c:\n\t", Nodes_Name[node_num]);
    for(i=0; i<NNODE; i++){
        printf("[%c, %c, %d] ", Nodes_Name[i], Nodes_Name[prev[i]], dist[i]);
    }
    printf("\n");
}