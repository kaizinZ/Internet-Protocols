# DHCPの実装
DHCP（Dynamic Host Configuration Protocol）とはホストマシンのIPアドレスを動的に割り当てたり、一定時間でIPを取り上げたりする、IPの管理のためのプロトコルである。

クライアントとサーバで実装が異なり、それぞれdhcpc.c、dhcps.cである。
クライアントとサーバはそれぞれ状態を持っており、それらが変化することをお互い把握している。

例えば、サーバの状態遷移図は以下のようになっている。
<img width="953" alt="スクリーンショット 2022-05-28 17 42 45" src="https://user-images.githubusercontent.com/78332175/170818047-c3535e35-c3b6-4c9a-bba8-dea6d9249c30.png">
