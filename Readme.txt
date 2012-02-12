/*
 NicoJK
  TVTest ニコニコ実況プラグイン
*/

■なにこれ？
ニコニコ実況を表示するプラグインです。

■必要なもの
ニコニコ実況のSDKが必要です。
http://help.nicovideo.jp/jksdk/
からダウンロードして、インストールしてください。

■使い方
NicoJK.ini、NicoJK.tvtpをTVTestのPluginsフォルダに入れて、
TVTestを起動して、プラグインを有効にしてください。

■設定
NicoJK.iniで、
　「TVTestでのチャンネル名」
　と
　「ニコニコ実況の番号 （jk?の数字部分）」
を設定します。
デフォルトでは関東地域と難視聴チャンネルが設定されています。

■制限
コメントはできません。
BSはSDKが対応していないので表示できません。

■テスト環境
Win7 sp1 + PT2 + ptTimer + BonDriver_ptmr.dll + TVTest 0.7.19(x86)

■配布
https://github.com/rutice/NicoJK/downloads

■ソースコード
https://github.com/rutice/NicoJK

■更新履歴
rev.5
コメントの勢いを見れるウィンドウを実装

rev.4
プラグインを切っても起動時やチャンネル切替時にコメントが表示されたのを修正
ドライバ切替時にはすぐコメントを消すようにした

rev.3
難視聴のチャンネル名、通常のサービス名をNicoJK.iniに追加
TVTestへの追尾方法を変更（全画面表示・最前面表示だとまだ微妙におかしいけど）
サービス切替時にコメントが変更されないのを修正
実況のないチャンネルに切替してもコメントが消えなかったのを修正

rev.2
チャンネル設定対応
全画面対応

rev.1
初期リリース