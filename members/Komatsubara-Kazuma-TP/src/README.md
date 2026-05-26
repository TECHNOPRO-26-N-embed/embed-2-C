# リモコン式デジタルロック Arduinoコード

このフォルダには、リモコン式デジタルロックのArduinoスケッチ（ard_code.ino）が含まれています。

## 概要
- IRリモコンでパスコード入力・施錠/解錠操作
- 7セグメントLEDで状態表示
- サーボモーターで物理ロック制御
- RGB LED・ブザーで状態通知
- 安全側設計（異常時は必ず施錠・出力停止）
- 詳細な日本語コメント・デバッグログ付き

---

## ソースコード全文

```cpp
// ==================== ライブラリ読み込み ====================
#include <IRremote.hpp>      // 赤外線リモコン受信ライブラリ
#include <Servo.h>           // サーボモーター制御ライブラリ
#include <TM1637Display.h>   // 7セグメントLED表示ライブラリ
#include <string.h>          // 文字列操作用標準ライブラリ

// ==================== デバッグ用シリアル出力制御 ====================
#define DEBUG_LOG 1
#if DEBUG_LOG
#define LOG(msg) Serial.println(F(msg))
#define LOG_VAL(msg, val) do { Serial.print(F(msg)); Serial.println(val); } while (0)
#else
#define LOG(msg) do {} while (0)
#define LOG_VAL(msg, val) do {} while (0)
#endif

// ==================== ピン定義（設計書準拠） ====================
// Arduino UNO R3 のデジタルピン割り当て。D0/D1は未使用（シリアル専用）。
const uint8_t PIN_IR_RECEIVER = 2;   // IR受信モジュール OUT（D2）
const uint8_t PIN_TM1637_CLK  = 3;   // 4桁7セグ CLK（D3）
const uint8_t PIN_TM1637_DIO  = 4;   // 4桁7セグ DIO（D4）
const uint8_t PIN_BUZZER      = 8;   // パッシブブザー（D8）
const uint8_t PIN_SERVO       = 9;   // サーボ SG90 信号（D9）
const uint8_t PIN_LED_R       = 10;  // RGB LED 赤（D10）
const uint8_t PIN_LED_G       = 11;  // RGB LED 緑（D11）
const uint8_t PIN_LED_B       = 12;  // RGB LED 青（D12）

// ==================== 状態定義 ====================
// ロックの状態を表す。状態遷移は設計書の状態遷移図に準拠。
enum LockState : uint8_t {
  STATE_IDLE      = 0, // 待機中
  STATE_INPUT     = 1, // 入力中
  STATE_VERIFY    = 2, // 認証中
  STATE_UNLOCKED  = 3, // 解錠中
  STATE_ERROR     = 4, // エラー中
  STATE_ALERT     = 5  // 警告中
};

// ==================== キー定義 ====================
// IRリモコンの論理キー値。数字キー0-9と制御キー。
const int KEY_NONE = -1;
const int KEY_CONFIRM = 10;
const int KEY_CANCEL = 11;
const int KEY_CHANGE = 12;
const int KEY_LOCK = 13;

// ==================== タイミング定数 ====================
// 各種タイマー値（ms単位）。設計書の境界値テストに対応。
const unsigned long LOCK_DURATION_MS = 30000UL;
const unsigned long AUTO_LOCK_DURATION_MS = 180000UL;
const unsigned long IR_REPEAT_WINDOW_MS = 150UL;
const unsigned long LED_BLINK_INTERVAL_MS = 300UL;
const unsigned long SEG_REFRESH_MS = 5UL;
const unsigned long LOCK_CHECK_MS = 50UL;
const unsigned long ERROR_DISPLAY_MS = 700UL;
const unsigned long ALERT_BUZZER_TOGGLE_MS = 200UL;

// ==================== サーボ角度定義 ====================
// 施錠・解錠時のサーボ角度。SG90基準。
const uint8_t SERVO_LOCKED_ANGLE = 10;
const uint8_t SERVO_UNLOCKED_ANGLE = 95;

// ==================== グローバルオブジェクト ====================
// 7セグメントディスプレイとサーボのインスタンス。
TM1637Display gDisplay(PIN_TM1637_CLK, PIN_TM1637_DIO);
Servo gServo;

// ==================== グローバル変数（設計書準拠） ====================
// 設計書の「グローバル変数・定数の設計」セクションに準拠。
LockState currentState = STATE_IDLE; // 現在の状態
char inputBuffer[5] = "";           // 入力バッファ（4桁+終端）
uint8_t inputLength = 0;             // 入力長
char passcode[5] = "1234";          // パスコード
uint8_t failedCount = 0;             // 認証失敗回数
bool isPassChangeMode = false;       // パス変更モード

unsigned long lastIrReceivedMs = 0;  // 最後のIR受信時刻
unsigned long unlockStartMs = 0;     // 解錠開始時刻
unsigned long lockUntilMs = 0;       // 警告解除時刻
unsigned long lastErrorBlinkMs = 0;  // エラーLED点滅時刻
unsigned long last7SegRefreshMs = 0; // 7セグ更新時刻
unsigned long lastLockCheckMs = 0;   // 警告解除判定時刻
unsigned long stateEnteredMs = 0;    // 状態遷移時刻
unsigned long alertBuzzerToggleMs = 0; // 警告ブザー切替時刻

unsigned long lastIrCode = 0;        // 最後のIRコード
int lastLogicalKey = KEY_NONE;       // 最後の論理キー
bool blinkOn = false;                // LED点滅状態
bool alertBuzzerOn = false;          // ブザー断続状態

// ==================== 7セグメント表示用セグメントデータ ====================
// 各種文字表示用のセグメントパターン。
const uint8_t SEG_BLANK = 0x00;
const uint8_t SEG_DASH = 0x40;
const uint8_t SEG_E = 0x79;
const uint8_t SEG_R = 0x50;
const uint8_t SEG_L = 0x38;
const uint8_t SEG_O = 0x3F;
const uint8_t SEG_C = 0x39;
const uint8_t SEG_K = 0x76;
const uint8_t SEG_P = 0x73;
const uint8_t SEG_N = 0x54;

// ...（以降も全コードを貼り付け）...
```

---

> 詳細な設計意図・動作仕様・安全側設計・状態遷移・ピン配線・テスト観点は docs/ 配下の設計書を参照してください。
```
