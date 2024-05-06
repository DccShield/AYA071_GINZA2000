//--------------------------------------------------------------------------------
//
// SMFDEasy_AYA071 LED ファンクションデコーダスケッチ
// For 銀座線2000形用消える室内灯ファンクション
// Copyright(C)'2024 Ayanosuke(Maison de DCC)
// [AYA071_ginza2000.ino]
// AYA071-2
//
// PIN_F0_F 0      // Atiny85 PB0(5pin)O7 analogwrite head light
// PIN_F0_R 1      // Atiny85 PB1(6pin)O6 analogwrite tail light
// PIN_SROOM 3      // Atint85 PB3(2pin)O3             sign light
// PIN_MROOM 4      // Atiny85 PB4(3pin)O2 analogwrite room light
//
// F0 ヘッドライト・テールライト点灯(FWD/REV切り替えあり）
// F1 室内灯（予備灯）ON/OFF
// F2 サイドレール断トリガ
// CV50 n：FWD F2 変化からのディレイ n x 100msで消灯、予備灯点灯　　０設定でサイドレール断トリガ無効
// CV51 n：FWD F2 変化からのディレイ n x 100msで点灯、予備灯消灯
// CV52 n：REV F2 変化からのディレイ n x 100msで消灯、予備灯点灯
// CV53 n：REV F2 変化からのディレイ n x 100msで点灯、予備灯消灯
//
// 2桁/4桁アドレス対応
// CV8対応（初期化）
// CV ACK対応ですが負荷が軽いので読み出し不可
//
// http://maison-dcc.sblo.jp/ http://dcc.client.jp/ http://ayabu.blog.shinobi.jp/
// https://twitter.com/masashi_214
//
// DCC電子工作連合のメンバーです
// https://desktopstation.net/tmi/ https://desktopstation.net/bb/index.php
//
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php
//
//--------------------------------------------------------------------------------
#include <arduino.h>
#include "DccCV.h"
#include "NmraDcc.h"
#include <avr/eeprom.h>   //required by notifyCVRead() function if enabled below

#define DEBUG      //リリースのときはコメントアウトすること

//各種設定、宣言
//                      // Atiny85 DCCin(7pin)
#define PIN_F0_F 0     // Atiny85 PB0(5pin)O7 analogwrite 
#define PIN_F0_R 1     // Atiny85 PB1(6pin)O6 analogwrite 
#define PIN_SROOM 3      // Atint85 PB3(2pin)O3             予備灯
#define PIN_MROOM 4      // Atiny85 PB4(3pin)O2 analogwrite 室内灯

//void Dccinit(void);

//使用クラスの宣言
NmraDcc   Dcc;
DCC_MSG  Packet;

struct CVPair {
  uint16_t  CV;
  uint8_t Value;
};

CVPair FactoryDefaultCVs [] = {
  {CV_MULTIFUNCTION_PRIMARY_ADDRESS, DECODER_ADDRESS}, // CV01
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},               // CV09 The LSB is set CV 1 in the libraries .h file, which is the regular address location, so by setting the MSB to 0 we tell the library to use the same address as the primary address. 0 DECODER_ADDRESS
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, 0},          // CV17 XX in the XXYY address
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, 0},          // CV18 YY in the XXYY address
  {CV_29_CONFIG, 128 },                                // CV29 Make sure this is 0 or else it will be random based on what is in the eeprom which could caue headaches
  {CV_50_FWD_OFF_DELAY,0},                             // nn x 100ms   0:無効 max 25500ms(25.5秒)
  {CV_51_FWD_ON_DELAY,0},                              // nn x 100ms
  {CV_52_REV_OFF_DELAY,0},                             // nn x 100ms
  {CV_53_REV_ON_DELAY, 0},                             // nn x 100ms
  {CV_dummy,0},
};

void(* resetFunc) (void) = 0;  //declare reset function at address 0

uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);

//void notifyDccReset(uint8_t hardReset );



//------------------------------------------------------------------
// Arduino固有の関数 setup() :初期設定
//------------------------------------------------------------------
void setup()
{
  //ファンクションの割り当てピン初期化
  TCCR1 = 0<<CTC1 | 0<<PWM1A | 0<<COM1A0 | 1<<CS10;


  pinMode(PIN_F0_F, OUTPUT);
  digitalWrite(PIN_F0_F, OFF);
  pinMode(PIN_F0_R, OUTPUT);
  digitalWrite(PIN_F0_R, OFF);
  pinMode(PIN_MROOM, OUTPUT);
  digitalWrite(PIN_MROOM, OFF);
  pinMode(PIN_SROOM, OUTPUT);
  digitalWrite(PIN_SROOM, OFF);

  Dccinit();

  //Reset task
  gPreviousL5 = millis();
}

//---------------------------------------------------------------------
// Arduino main loop
//---------------------------------------------------------------------
void loop() {
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  if ( (millis() - gPreviousL5) >= 10) // 100:100msec  10:10msec  Function decoder は 10msecにしてみる。
  {
    FunctionProcess();

    if(gCV50_fwd_off_delay != 0) // gCV50_fwd_off_delayに値がセットされている時に消える室内灯ステートマシンを実行
      process2k();

    gPreviousL5 = millis();
  }
}

//---------------------------------------------------------------------
// 銀座線2000形室内灯ステートマシーン
//---------------------------------------------------------------------
void process2k( void )
{
  enum{
      STS_INIT = 0,
      STS_IDLE,
      STS_OFFDELAY,
      STS_ONDELAY,
      STS_STOP,
  };
  static int state = STS_INIT;
  static char prevF2 = 0;
  static unsigned int t1;
  static unsigned int t2;
  static unsigned long PreviosTimer = 0;
    
  switch(state){
    case STS_INIT:
                  prevF2 = gState_F2;   // 変化前のF2の状態を記録
                  state = STS_IDLE;
                  break;
    case STS_IDLE:
                  if(gState_F1 == 0){   //　F1の処理
                    digitalWrite(PIN_MROOM, LOW);
                  } else {
                    digitalWrite(PIN_MROOM, HIGH);
                  }
   
                  if(gState_F1 != 0 && prevF2 != gState_F2 ){ // F1 ON 且つ　F2に変化あり？
                    if(gDirection != 0){  // fwd
                      t1 = gCV50_fwd_off_delay *100;
                      t2 = gCV51_fwd_on_delay *100;
                    } else {              // rev
                      t1 = gCV52_rev_off_delay *100;
                      t2 = gCV53_rev_on_delay *100;
                    }
                    PreviosTimer = millis();
                    state = STS_OFFDELAY;
                  }
                  break;
    case STS_OFFDELAY:  // OFFディレイまで待ってから室内灯消灯し予備棟点灯
                  if ((millis() - PreviosTimer) >= t1){
                    digitalWrite(PIN_MROOM, LOW); // 室内灯消灯                  
                    digitalWrite(PIN_SROOM, HIGH);
                    PreviosTimer = millis();
                    state = STS_ONDELAY;
                  }                      
                  if(gState_F1 == 0){
                    digitalWrite(PIN_MROOM, LOW);
                    state = STS_STOP;
                  }
                  break;
    case STS_ONDELAY:  // ONディレイまで待ってから室内灯点灯し予備棟消灯
                  if ((millis() - PreviosTimer) >= t2){
                    digitalWrite(PIN_MROOM, HIGH); // 室内灯点灯                  
                    digitalWrite(PIN_SROOM, LOW);
                    state = STS_INIT;
                  }      
                  if(gState_F1 == 0){
                    digitalWrite(PIN_MROOM, LOW);
                    state = STS_STOP;
                  }
                  break;
    case STS_STOP:
                  digitalWrite(PIN_MROOM, LOW);                  
                  digitalWrite(PIN_SROOM, LOW);
                  state = STS_INIT;
                  break;
    default:
                  break;
  }
}

//---------------------------------------------------------------------
//ファンクション受信によるイベント
//---------------------------------------------------------------------
void FunctionProcess(void){
// F0 受信時の処理
    if(gState_F0 > 0) {                   // DCC F0 コマンドの点灯処理
      if( gDirection == 1){                // Reverse 前進(DCS50Kで確認)
        digitalWrite(PIN_F0_F, HIGH);
        digitalWrite(PIN_F0_R, LOW);
      } else {                             // Forward 後進(DCS50Kで確認)
        digitalWrite(PIN_F0_F, LOW);
        digitalWrite(PIN_F0_R, HIGH);
      }
    }
    if(gState_F0 == 0) {
        digitalWrite(PIN_F0_F, LOW);  
        digitalWrite(PIN_F0_R, LOW);
    }

// F1 受信時の処理　室内灯ON/OFF処理

    if(gCV50_fwd_off_delay == 0){ // CV50が0の時は通常室内灯ON/OFF
      if(gState_F1 == 0){ 
        digitalWrite(PIN_MROOM, LOW);
      } else {
        digitalWrite(PIN_MROOM, HIGH);
      }
    }

// F2 受信時の処理 サイドレール断トリガに使用するためコメントアウト
//    if(gState_F2 == 0){
//      digitalWrite(PIN_AUX2, LOW);
//    } else {
//      digitalWrite(PIN_AUX2, HIGH);
//    }
}

//---------------------------------------------------------------------
// DCC速度信号の受信によるイベント
//---------------------------------------------------------------------
extern void notifyDccSpeed( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps )
{
  if ( gDirection != Dir )
  {
    gDirection = Dir;
  }
}


//---------------------------------------------------------------------------
//ファンクション信号受信のイベント
//FN_0_4とFN_5_8は常時イベント発生（DCS50KはF8まで）
//FN_9_12以降はFUNCTIONボタンが押されたときにイベント発生
//前値と比較して変化あったら処理するような作り。
//---------------------------------------------------------------------------
extern void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState)
{

  if( FuncGrp == FN_0_4)
  {
    if( gState_F0 != (FuncState & FN_BIT_00))
    {
      //Get Function 0 (FL) state
      gState_F0 = (FuncState & FN_BIT_00);
    }
    if( gState_F1 != (FuncState & FN_BIT_01))
    {
      //Get Function 1 state
      gState_F1 = (FuncState & FN_BIT_01);
    }
    if( gState_F2 != (FuncState & FN_BIT_02))
    {
      gState_F2 = (FuncState & FN_BIT_02);
    }
    if( gState_F3 != (FuncState & FN_BIT_03))
    {
      gState_F3 = (FuncState & FN_BIT_03);
    }
    if( gState_F4 != (FuncState & FN_BIT_04))
    {
      gState_F4 = (FuncState & FN_BIT_04);
    }
  }

  if( FuncGrp == FN_5_8)
  {
    if( gState_F5 != (FuncState & FN_BIT_05))
    {
      //Get Function 0 (FL) state
      gState_F5 = (FuncState & FN_BIT_05);
    }
    if( gState_F6 != (FuncState & FN_BIT_06))
    {
      //Get Function 1 state
      gState_F6 = (FuncState & FN_BIT_06);
    }
    if( gState_F7 != (FuncState & FN_BIT_07))
    {
      gState_F7 = (FuncState & FN_BIT_07);
    }
    if( gState_F8 != (FuncState & FN_BIT_08))
    {
      gState_F8 = (FuncState & FN_BIT_08);
    }
  }
}

//------------------------------------------------------------------
// CVをデフォルトにリセット(Initialize cv value)
// Serial.println("CVs being reset to factory defaults");
//------------------------------------------------------------------
void resetCVToDefault()
{
  for (int j = 0; j < FactoryDefaultCVIndex; j++ ) {
    Dcc.setCV( FactoryDefaultCVs[j].CV, FactoryDefaultCVs[j].Value);
  }
}

//------------------------------------------------------------------
// CV8 によるリセットコマンド受信処理
//------------------------------------------------------------------
void notifyCVResetFactoryDefault()
{
  //When anything is writen to CV8 reset to defaults.

  resetCVToDefault();
  delay(1000);  //typical CV programming sends the same command multiple times - specially since we dont ACK. so ignore them by delaying
  resetFunc();
};

//------------------------------------------------------------------
// CV Ackの処理
// そこそこ電流を流さないといけない
//------------------------------------------------------------------
void notifyCVAck(void)
{
  //Serial.println("notifyCVAck");
  digitalWrite(PIN_F0_F,HIGH);
  digitalWrite(PIN_F0_R,HIGH);
  digitalWrite(PIN_MROOM,HIGH);
  digitalWrite(PIN_SROOM,HIGH);

  delay( 6 );

  digitalWrite(PIN_F0_F,LOW);
  digitalWrite(PIN_F0_R,LOW);
  digitalWrite(PIN_MROOM,LOW);
  digitalWrite(PIN_SROOM,LOW);
}

void MOTOR_Ack(void)
{
//  analogWrite(O4, 128);
//  delay( 6 );  
//  analogWrite(O4, 0);
}

//------------------------------------------------------------------
// DCC初期化処理）
//------------------------------------------------------------------
void Dccinit(void)
{

  //DCCの応答用負荷ピン
#if defined(DCCACKPIN)
  //Setup ACK Pin
  pinMode(DccAckPin, OUTPUT);
  digitalWrite(DccAckPin, 0);
#endif

#if !defined(DECODER_DONT_DEFAULT_CV_ON_POWERUP)
  if ( Dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS) == 0xFF ) {   //if eeprom has 0xFF then assume it needs to be programmed
    notifyCVResetFactoryDefault();
  } else {
    //Serial.println("CV Not Defaulting");
  }
#else
  //Serial.println("CV Defaulting Always On Powerup");
  notifyCVResetFactoryDefault();
#endif

  // Setup which External Interrupt, the Pin it's associated with that we're using, disable pullup.
  Dcc.pin(0, 2, 0); // Atiny85 7pin(PB2)をDCC_PULSE端子に設定

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 100,   FLAGS_MY_ADDRESS_ONLY , 0 );

  //Init CVs
  gCV1_SAddr = Dcc.getCV( CV_MULTIFUNCTION_PRIMARY_ADDRESS ) ;
  gCV1_SAddr = Dcc.getCV( CV_MULTIFUNCTION_PRIMARY_ADDRESS ) ;
  gCVx_LAddr = (Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB ) << 8) + Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB );

  gCV50_fwd_off_delay = Dcc.getCV( CV_50_FWD_OFF_DELAY );
  gCV51_fwd_on_delay = Dcc.getCV( CV_51_FWD_ON_DELAY );
  gCV52_rev_off_delay = Dcc.getCV( CV_52_REV_OFF_DELAY );
  gCV53_rev_on_delay = Dcc.getCV( CV_53_REV_ON_DELAY );

  //Init CVs
  //E2P-ROMからCV値を読み込む

}


//------------------------------------------------------------------
// CV値が変化した時の処理（特に何もしない）
//------------------------------------------------------------------
extern void     notifyCVChange( uint16_t CV, uint8_t Value) {
}


//------------------------------------------------------------------
// Resrt処理 NMRA規格のしきたり
//------------------------------------------------------------------
void notifyDccReset(uint8_t hardReset )
{

  digitalWrite(PIN_F0_F,LOW);
  digitalWrite(PIN_F0_R,LOW);
  digitalWrite(PIN_MROOM,LOW);
  digitalWrite(PIN_SROOM,LOW);

  gState_F0 = 0;
  gState_F1 = 0;
  gState_F2 = 0;
  gState_F3 = 0; 
  gState_F4 = 0;
  gState_F5 = 0;
}
  
