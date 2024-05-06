FunctionDecoderLED

DCC用LEDファンクションデコーダで銀座線2000形用消える室内灯ファンクションです

 F0 ヘッドライト・テールライト点灯(FWD/REV切り替えあり）
 F1 室内灯（予備灯）ON/OFF
 F2 サイドレール断トリガ
 CV50 n：FWD F2 変化からのディレイ n x 100msで消灯、予備灯点灯　　０設定でサイドレール断トリガ無効
 CV51 n：FWD F2 変化からのディレイ n x 100msで点灯、予備灯消灯
 CV52 n：REV F2 変化からのディレイ n x 100msで消灯、予備灯点灯
 CV53 n：REV F2 変化からのディレイ n x 100msで点灯、予備灯消灯
 
 2桁/4桁アドレス対応
 CV8対応（初期化）
 CV ACK対応ですが負荷が軽いので読み出し不可


DCCドライバはNmraDccを使用しています。
https://github.com/mrrwa/NmraDcc

詳細は[ArduinoNANOで作るDCC Decoder]を参照してください。

http://ayanos.cart.fc2.com/ http://dcc.client.jp/ http://maison-dcc.sblo.jp/ https://twitter.com/masashi_214

DCC電子工作連合のメンバーです
https://desktopstation.net/tmi/ https://desktopstation.net/bb/index.php

This software is released under the MIT License, see LICENSE.txt.
（このソフトウェアは、MITライセンスのもとで公開されています。LICENSE.txtを見てください）
