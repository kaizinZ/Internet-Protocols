# FTPの実装
FTP(File Transfer Protocol)とは、ネットワーク上のコンピュータ間でファイルを転送するために使用されるネットワークプロトコルである。

実装に当たってはクライアントサーバモデルを使用し、それぞれ異なる状態を遷移しながらファイルの送受信を行う。