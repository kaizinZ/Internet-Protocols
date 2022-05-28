# バッファキャッシュの実装
HDDアクセスは時間がかかるため、一度HDDから読み込んだデータをカーネル内部にキャッシュすることで、処理時間の削減につなげる。
本プログラムではカーネル内部のバッファキャッシュの管理方法を擬似的に実装している。

それぞれのバッファには以下の状態が設定されており、やや複雑なステートマシンを設計。
<img width="725" alt="スクリーンショット 2022-05-28 17 16 04" src="https://user-images.githubusercontent.com/78332175/170816996-cdf3ed85-484f-4ff2-83f4-90d69da1f3dc.png">