
// ==================== ライブラリ読み込み ====================
#include <IRremote.hpp>      // 赤外線リモコン受信ライブラリ
#include <Servo.h>           // サーボモーター制御ライブラリ
#include <string.h>          // 文字列操作用標準ライブラリ

// 4桁7セグ（TM1637）を未使用にする場合は 0。
#define USE_TM1637 0
#if USE_TM1637
#include <TM1637Display.h>   // 7セグメントLED表示ライブラリ
#endif

// ==================== デバッグ用シリアル出力制御 ====================
#define DEBUG_LOG 0
#define MIN_IR_LOG 1
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
#if USE_TM1637
const uint8_t PIN_TM1637_CLK  = 3;   // 4桁7セグ CLK（D3）
const uint8_t PIN_TM1637_DIO  = 4;   // 4桁7セグ DIO（D4）
#endif
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
const uint8_t IR_FLAG_REPEAT = 0x01;
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
#if USE_TM1637
TM1637Display gDisplay(PIN_TM1637_CLK, PIN_TM1637_DIO);
#endif
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
#if USE_TM1637
unsigned long last7SegRefreshMs = 0; // 7セグ更新時刻
#endif
unsigned long lastLockCheckMs = 0;   // 警告解除判定時刻
unsigned long stateEnteredMs = 0;    // 状態遷移時刻
unsigned long alertBuzzerToggleMs = 0; // 警告ブザー切替時刻

unsigned long lastIrCode = 0;        // 最後のIRコード
int lastLogicalKey = KEY_NONE;       // 最後の論理キー
bool blinkOn = false;                // LED点滅状態
bool alertBuzzerOn = false;          // ブザー断続状態
int pendingDigitAfterError = KEY_NONE; // ERROR中に受けた数字キーを一時保持

// ==================== 7セグメント表示用セグメントデータ ====================
// 各種文字表示用のセグメントパターン。
#if USE_TM1637
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
#endif

// 状態を文字列で返す（デバッグ用）
const char* stateToString(LockState state) {
  switch (state) {
    case STATE_IDLE: return "IDLE";
    case STATE_INPUT: return "INPUT";
    case STATE_VERIFY: return "VERIFY";
    case STATE_UNLOCKED: return "UNLOCKED";
    case STATE_ERROR: return "ERROR";
    case STATE_ALERT: return "ALERT";
    default: return "UNKNOWN";
  }
}

// 指定時刻を過ぎたか判定（境界値テスト対応）
bool isTimeReached(unsigned long now, unsigned long targetMs) {
  const bool reached = (long)(now - targetMs) >= 0;
  if (reached) {
    LOG("[BRANCH] isTimeReached: reached=true");
  } else {
    LOG("[BRANCH] isTimeReached: reached=false");
  }
  return reached;
}

// 指定間隔が経過したか判定（非ブロッキングタイマー）
bool isElapsed(unsigned long now, unsigned long lastMs, unsigned long intervalMs) {
  const bool elapsed = (now - lastMs) >= intervalMs;
  if (elapsed) {
    LOG("[BRANCH] isElapsed: elapsed=true");
  }
  return elapsed;
}

// RGB LEDの各色を制御する
void setRgb(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
  digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}

// 入力バッファをクリアする
void clearInputBuffer() {
  inputLength = 0;
  inputBuffer[0] = '\0';
  LOG("[BRANCH] clearInputBuffer: cleared");
}

// ERROR中に押された数字キーを保持し、復帰後の最初の入力として使う。
void rememberDigitDuringError(int key) {
  if (key >= 0 && key <= 9 && pendingDigitAfterError == KEY_NONE) {
    pendingDigitAfterError = key;
  }
}

// 入力バッファに1桁追加（4桁まで）
void appendInputDigit(uint8_t digit) {
  if (digit > 9 || inputLength >= 4) {
    LOG("[BRANCH] appendInputDigit: rejected");
    return;
  }
  inputBuffer[inputLength] = (char)('0' + digit);
  inputLength++;
  inputBuffer[inputLength] = '\0';
  LOG("[BRANCH] appendInputDigit: appended");
}

// 入力バッファの末尾を1桁削除
void removeLastDigit() {
  if (inputLength == 0) {
    LOG("[BRANCH] removeLastDigit: no-op");
    return;
  }
  inputLength--;
  inputBuffer[inputLength] = '\0';
  LOG("[BRANCH] removeLastDigit: removed");
}

// 入力4桁と登録パスコードを照合する
bool verifyPasscode(const char* input, const char* currentPasscode) {
  if (input == nullptr || currentPasscode == nullptr) {
    LOG("[BRANCH] verifyPasscode: null input");
    return false;
  }
  if (strlen(input) != 4 || strlen(currentPasscode) != 4) {
    LOG("[BRANCH] verifyPasscode: invalid length");
    return false;
  }
  const bool matched = strncmp(input, currentPasscode, 4) == 0;
  if (matched) {
    LOG("[BRANCH] verifyPasscode: matched");
  } else {
    LOG("[BRANCH] verifyPasscode: mismatch");
  }
  return matched;
}

// 状態遷移処理。状態と遷移時刻を記録し、デバッグ出力。
void setState(LockState nextState, unsigned long now) {
  const LockState prevState = currentState;
  currentState = nextState;
  stateEnteredMs = now;

  // IDLE復帰時に重複入力抑制の履歴をクリアし、再入力開始を確実にする。
  if (nextState == STATE_IDLE) {
    lastLogicalKey = KEY_NONE;
    lastIrCode = 0;
    lastIrReceivedMs = 0;
  }

  if (prevState != nextState) {
    Serial.print(F("[STATE] "));
    Serial.print(stateToString(prevState));
    Serial.print(F(" -> "));
    Serial.print(stateToString(nextState));
    Serial.print(F(" @ms="));
    Serial.println(now);
  } else {
    Serial.print(F("[STATE] stay "));
    Serial.print(stateToString(nextState));
    Serial.print(F(" @ms="));
    Serial.println(now);
  }
}

// サーボを施錠/解錠位置に移動。安全側（施錠）優先。
void setLockState(bool unlock, unsigned long now) {
  if (unlock) {
    LOG("[BRANCH] setLockState: unlock");
    gServo.write(SERVO_UNLOCKED_ANGLE); // 解錠角度へ
    unlockStartMs = now;
  } else {
    LOG("[BRANCH] setLockState: lock");
    gServo.write(SERVO_LOCKED_ANGLE);   // 施錠角度へ
  }
  clearInputBuffer();
}

// 7セグメントLEDに任意のセグメントデータを表示
#if USE_TM1637
void showSegments(const uint8_t* segs) {
  gDisplay.setSegments(segs, 4, 0);
}

// 待機表示（----）
void showIdleDisplay() {
  uint8_t segs[4] = {SEG_DASH, SEG_DASH, SEG_DASH, SEG_DASH};
  showSegments(segs);
}

// 入力中表示（入力済み桁のみ数字表示）
void showInputDisplay() {
  uint8_t segs[4] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK};
  for (uint8_t i = 0; i < inputLength && i < 4; i++) {
    segs[i] = gDisplay.encodeDigit((uint8_t)(inputBuffer[i] - '0'));
  }
  showSegments(segs);
}

// 解錠中表示（OPEN）
void showOpenDisplay() {
  uint8_t segs[4] = {SEG_O, SEG_P, SEG_E, SEG_N};
  showSegments(segs);
}

// エラー表示（ERR）
void showErrorDisplay() {
  uint8_t segs[4] = {SEG_E, SEG_R, SEG_R, SEG_BLANK};
  showSegments(segs);
}

// 警告表示（LOCK）
void showLockDisplay() {
  uint8_t segs[4] = {SEG_L, SEG_O, SEG_C, SEG_K};
  showSegments(segs);
}

// 定義外状態時の安全表示（待機表示）
void showDefaultSafeDisplay() {
  showIdleDisplay();
}
#endif

// 状態に応じて7セグメントLED表示を更新
void update7SegDisplay(unsigned long now) {
#if !USE_TM1637
  (void)now;
  LOG("[BRANCH] update7SegDisplay: disabled");
  return;
#else
  if (!isElapsed(now, last7SegRefreshMs, SEG_REFRESH_MS)) {
    LOG("[BRANCH] update7SegDisplay: skip refresh");
    return;
  }
  last7SegRefreshMs = now;

  if (currentState == STATE_INPUT) {
    LOG("[BRANCH] update7SegDisplay: STATE_INPUT");
    showInputDisplay();
    return;
  }

  if (currentState == STATE_UNLOCKED && isPassChangeMode) {
    LOG("[BRANCH] update7SegDisplay: STATE_UNLOCKED + change mode");
    showInputDisplay();
    return;
  }

  switch (currentState) {
    case STATE_IDLE:
      LOG("[BRANCH] update7SegDisplay: STATE_IDLE");
      showIdleDisplay();
      break;
    case STATE_VERIFY:
      LOG("[BRANCH] update7SegDisplay: STATE_VERIFY");
      showIdleDisplay();
      break;
    case STATE_UNLOCKED:
      LOG("[BRANCH] update7SegDisplay: STATE_UNLOCKED");
      showOpenDisplay();
      break;
    case STATE_ERROR:
      LOG("[BRANCH] update7SegDisplay: STATE_ERROR");
      showErrorDisplay();
      break;
    case STATE_ALERT:
      LOG("[BRANCH] update7SegDisplay: STATE_ALERT");
      showLockDisplay();
      break;
    default:
      LOG("[BRANCH] update7SegDisplay: default");
      showDefaultSafeDisplay();
      break;
  }
#endif
}

// 状態に応じてRGB LEDを制御
void updateLedIndicator(unsigned long now) {
  switch (currentState) {
    case STATE_IDLE:
      LOG("[BRANCH] updateLedIndicator: STATE_IDLE");
      setRgb(false, false, false);
      break;
    case STATE_INPUT:
      LOG("[BRANCH] updateLedIndicator: STATE_INPUT");
      setRgb(false, false, true);
      break;
    case STATE_VERIFY:
      LOG("[BRANCH] updateLedIndicator: STATE_VERIFY");
      setRgb(false, true, true);
      break;
    case STATE_UNLOCKED:
      LOG("[BRANCH] updateLedIndicator: STATE_UNLOCKED");
      setRgb(false, true, false);
      break;
    case STATE_ERROR:
    case STATE_ALERT:
      LOG("[BRANCH] updateLedIndicator: STATE_ERROR/ALERT");
      if (isElapsed(now, lastErrorBlinkMs, LED_BLINK_INTERVAL_MS)) {
        LOG("[BRANCH] updateLedIndicator: blink toggle");
        lastErrorBlinkMs = now;
        blinkOn = !blinkOn;
      }
      setRgb(blinkOn, false, false);
      break;
    default:
      LOG("[BRANCH] updateLedIndicator: default");
      setRgb(false, false, false);
      break;
  }
}

// 状態に応じてブザーを制御
void playBuzzerPattern(unsigned long now) {
  if (currentState == STATE_ERROR) {
    LOG("[BRANCH] playBuzzerPattern: STATE_ERROR");
    if (isElapsed(now, stateEnteredMs, 20) && (now - stateEnteredMs) < 40) {
      LOG("[BRANCH] playBuzzerPattern: short error tone");
      tone(PIN_BUZZER, 2000, 120);
    }
    return;
  }

  if (currentState == STATE_ALERT) {
    LOG("[BRANCH] playBuzzerPattern: STATE_ALERT");
    if (isElapsed(now, alertBuzzerToggleMs, ALERT_BUZZER_TOGGLE_MS)) {
      LOG("[BRANCH] playBuzzerPattern: toggle alert buzzer");
      alertBuzzerToggleMs = now;
      alertBuzzerOn = !alertBuzzerOn;
      if (alertBuzzerOn) {
        LOG("[BRANCH] playBuzzerPattern: buzzer ON");
        tone(PIN_BUZZER, 1600);
      } else {
        LOG("[BRANCH] playBuzzerPattern: buzzer OFF");
        noTone(PIN_BUZZER);
      }
    }
    return;
  }

  LOG("[BRANCH] playBuzzerPattern: normal state (buzzer off)");
  alertBuzzerOn = false;
  noTone(PIN_BUZZER);
}

// 状態に応じてLED・ブザー・7セグ表示を一括更新
void updateOutputByState(unsigned long now) {
  updateLedIndicator(now);
  playBuzzerPattern(now);
  update7SegDisplay(now);
}

// Command-based mapping for common ELEGOO IR remote.
// IRリモコンのコマンド値を論理キー値に変換
int mapIrCommandToKey(uint8_t command) {
  switch (command) {
    // 0
    case 0x16:
    case 0x68:
      LOG("[BRANCH] mapIrCommandToKey: 0");
      return 0;
    // 1
    case 0x0C:
    case 0x45:
    case 0xA2:
      LOG("[BRANCH] mapIrCommandToKey: 1");
      return 1;
    // 2
    case 0x18:
    case 0x46:
    case 0x62:
      LOG("[BRANCH] mapIrCommandToKey: 2");
      return 2;
    // 3
    case 0x5E:
    case 0x47:
    case 0xE2:
      LOG("[BRANCH] mapIrCommandToKey: 3");
      return 3;
    // 4
    case 0x08:
    case 0x44:
    case 0x22:
      LOG("[BRANCH] mapIrCommandToKey: 4");
      return 4;
    // 5
    case 0x1C:
    case 0x40:
    case 0x02:
      LOG("[BRANCH] mapIrCommandToKey: 5");
      return 5;
    // 6
    case 0x5A:
    case 0x43:
    case 0xC2:
      LOG("[BRANCH] mapIrCommandToKey: 6");
      return 6;
    // 7
    case 0x42:
    case 0x07:
    case 0xE0:
      LOG("[BRANCH] mapIrCommandToKey: 7");
      return 7;
    // 8
    case 0x52:
    case 0x15:
    case 0xA8:
      LOG("[BRANCH] mapIrCommandToKey: 8");
      return 8;
    // 9
    case 0x4A:
    case 0x09:
    case 0x90:
      LOG("[BRANCH] mapIrCommandToKey: 9");
      return 9;

    // 制御キーは衝突しやすいため、rawマッピング側を優先する。
    case 0x19:
    case 0x4C:
      LOG("[BRANCH] mapIrCommandToKey: KEY_CONFIRM");
      return KEY_CONFIRM;
    case 0x0D:
      LOG("[BRANCH] mapIrCommandToKey: KEY_CANCEL");
      return KEY_CANCEL;
    default:
      LOG("[BRANCH] mapIrCommandToKey: KEY_NONE");
      return KEY_NONE;
  }
}

// Raw-code fallback for remotes that return only raw NEC frames.
// NECプロトコルの生データを論理キー値に変換
int mapIrRawToKey(unsigned long raw) {
  switch (raw) {
    case 0xFF6897: LOG("[BRANCH] mapIrRawToKey: 0"); return 0;
    case 0xFF30CF: LOG("[BRANCH] mapIrRawToKey: 1"); return 1;
    case 0xFF18E7: LOG("[BRANCH] mapIrRawToKey: 2"); return 2;
    case 0xFF7A85: LOG("[BRANCH] mapIrRawToKey: 3"); return 3;
    case 0xFF10EF: LOG("[BRANCH] mapIrRawToKey: 4"); return 4;
    case 0xFF38C7: LOG("[BRANCH] mapIrRawToKey: 5"); return 5;
    case 0xFF5AA5: LOG("[BRANCH] mapIrRawToKey: 6"); return 6;
    case 0xFF42BD: LOG("[BRANCH] mapIrRawToKey: 7"); return 7;
    case 0xFF4AB5: LOG("[BRANCH] mapIrRawToKey: 8"); return 8;
    case 0xFF52AD: LOG("[BRANCH] mapIrRawToKey: 9"); return 9;

    case 0xFF02FD: LOG("[BRANCH] mapIrRawToKey: KEY_CONFIRM"); return KEY_CONFIRM;
    case 0xFF22DD: LOG("[BRANCH] mapIrRawToKey: KEY_CANCEL"); return KEY_CANCEL;
    case 0xFFA857: LOG("[BRANCH] mapIrRawToKey: KEY_CHANGE"); return KEY_CHANGE;
    case 0xFF906F: LOG("[BRANCH] mapIrRawToKey: KEY_LOCK"); return KEY_LOCK;
    default:
      LOG("[BRANCH] mapIrRawToKey: KEY_NONE");
      return KEY_NONE;
  }
}

// IR受信値を読み取り、論理キー値に変換
int readIrInput(unsigned long now) {
  if (!IrReceiver.decode()) {
    LOG("[BRANCH] readIrInput: decode=false");
    return KEY_NONE;
  }

  const uint8_t command = IrReceiver.decodedIRData.command;
  const unsigned long rawCode = IrReceiver.decodedIRData.decodedRawData;
  const uint8_t flags = IrReceiver.decodedIRData.flags;
  int logicalKey = mapIrCommandToKey(command);
  if (logicalKey == KEY_NONE) {
    LOG("[BRANCH] readIrInput: command map none -> raw fallback");
    logicalKey = mapIrRawToKey(rawCode);
  }

  // decodedRawDataの中間バイトにcommand相当値が入る実装差を吸収。
  if (logicalKey == KEY_NONE && rawCode != 0) {
    const uint8_t commandFromRaw = (uint8_t)((rawCode >> 16) & 0xFF);
    logicalKey = mapIrCommandToKey(commandFromRaw);
  }

  // リピートフレーム時にキー値が空なら、直前キーを再利用する。
  if (logicalKey == KEY_NONE && (flags & IR_FLAG_REPEAT) != 0 && lastLogicalKey != KEY_NONE) {
    logicalKey = lastLogicalKey;
  }

#if MIN_IR_LOG
  Serial.print(F("[IRMIN] c=0x"));
  Serial.print(command, HEX);
  Serial.print(F(" f=0x"));
  Serial.print(flags, HEX);
  Serial.print(F(" k="));
  Serial.println(logicalKey);
#endif

  IrReceiver.resume();

  if (logicalKey == KEY_NONE) {
    LOG("[BRANCH] readIrInput: key none after fallback");
    return KEY_NONE;
  }

  // Optimization: removed redundant isElapsed(..., 0) check.
  if (logicalKey == lastLogicalKey && (now - lastIrReceivedMs) < IR_REPEAT_WINDOW_MS) {
    LOG("[BRANCH] readIrInput: repeat filtered");
    return KEY_NONE;
  }

  lastLogicalKey = logicalKey;
  lastIrCode = rawCode;
  lastIrReceivedMs = now;
  LOG_VAL("[BRANCH] readIrInput: accepted key=", logicalKey);
  return logicalKey;
}

// パスコード変更モード時の入力処理
bool handlePasscodeChangeMode(int key) {
  if (!isPassChangeMode) {
    LOG("[BRANCH] handlePasscodeChangeMode: disabled");
    return false;
  }

  if (key >= 0 && key <= 9) {
    LOG("[BRANCH] handlePasscodeChangeMode: digit");
    appendInputDigit((uint8_t)key);
    return false;
  }

  if (key == KEY_CANCEL) {
    LOG("[BRANCH] handlePasscodeChangeMode: cancel");
    clearInputBuffer();
    isPassChangeMode = false;
    return false;
  }

  if (key == KEY_CONFIRM && inputLength == 4) {
    LOG("[BRANCH] handlePasscodeChangeMode: confirm and save");
    strncpy(passcode, inputBuffer, 4);
    passcode[4] = '\0';
    clearInputBuffer();
    isPassChangeMode = false;
    return true;
  }

  LOG("[BRANCH] handlePasscodeChangeMode: ignored key");
  return false;
}

// ==================== 初期化処理 ====================
// ピン・ライブラリ・変数・出力の初期化。安全側で起動。
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    // Needed for boards with native USB, harmless on UNO.
  }
  LOG("[BOOT] setup start");

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  IrReceiver.begin(PIN_IR_RECEIVER, DISABLE_LED_FEEDBACK);

  gServo.attach(PIN_SERVO);

  clearInputBuffer();
  failedCount = 0;
  isPassChangeMode = false;
  pendingDigitAfterError = KEY_NONE;
  lockUntilMs = 0;
  unlockStartMs = 0;
  lastIrReceivedMs = 0;
  lastErrorBlinkMs = 0;
#if USE_TM1637
  last7SegRefreshMs = 0;
#endif
  lastLockCheckMs = 0;
  stateEnteredMs = millis();
  alertBuzzerToggleMs = 0;
  lastIrCode = 0;
  lastLogicalKey = KEY_NONE;
  blinkOn = false;
  alertBuzzerOn = false;

  setLockState(false, millis());
  setRgb(false, false, false);
  noTone(PIN_BUZZER);
#if USE_TM1637
  gDisplay.setBrightness(7, true);
  showIdleDisplay();
#endif
  setState(STATE_IDLE, millis());

  LOG("[BOOT] setup done");
}

// ==================== メインループ ====================
// 状態ごとに分岐し、状態遷移・タイミング判定・安全側動作を徹底。
void loop() {
  const unsigned long now = millis();
  const int key = readIrInput(now);

  updateOutputByState(now);

  // 警告状態の解除判定（LOCK_CHECK_MSごと）
  if (currentState == STATE_ALERT && isElapsed(now, lastLockCheckMs, LOCK_CHECK_MS)) {
    lastLockCheckMs = now;
    if ((long)(now - lockUntilMs) >= 0) { // 境界値テストしやすいよう明示
      failedCount = 0;
      clearInputBuffer();
      setState(STATE_IDLE, now);
    }
  }

  switch (currentState) {
    case STATE_IDLE:
      // 【待機中】
      // 数字キーが押されたら入力バッファに追加し、入力状態へ遷移。
      if (key >= 0 && key <= 9) {
        appendInputDigit((uint8_t)key);
        setState(STATE_INPUT, now);
      }
      break;

    case STATE_INPUT:
      // 【入力中】
      // 数字キーならバッファに追加。
      if (key >= 0 && key <= 9) {
        appendInputDigit((uint8_t)key);
      } else if (key == KEY_CANCEL) {
        // 取消キーなら1桁削除。バッファが空になったら待機状態へ戻る。
        removeLastDigit();
        if (inputLength == 0) {
          setState(STATE_IDLE, now);
        }
      }
      // 4桁揃ったら認証状態へ遷移。
      if (inputLength == 4) {
        setState(STATE_VERIFY, now);
      }
      break;

    case STATE_VERIFY:
      // 【認証中】
      // 入力とパスコードが一致すれば解錠。
      if (verifyPasscode(inputBuffer, passcode)) {
        failedCount = 0;           // 失敗回数リセット
        isPassChangeMode = false;  // パス変更モード解除
        setLockState(true, now);   // サーボを解錠位置へ
        setState(STATE_UNLOCKED, now); // 解錠状態へ
      } else {
        // 不一致なら失敗回数加算、バッファクリア
        failedCount++;
        clearInputBuffer();
        if (failedCount >= 3) {
          // 3回連続失敗で警告状態（一定時間ロック）
          lockUntilMs = now + LOCK_DURATION_MS;
          isPassChangeMode = false;
          setState(STATE_ALERT, now);
        } else {
          // 3回未満ならエラー状態（短時間表示）
          setState(STATE_ERROR, now);
        }
      }
      break;

    case STATE_UNLOCKED:
      // 【解錠中】
      // パス変更モード中は専用処理。
      if (isPassChangeMode) {
        handlePasscodeChangeMode(key);
      } else {
        // 施錠キーで施錠・待機へ遷移。
        if (key == KEY_LOCK) {
          setLockState(false, now);
          setState(STATE_IDLE, now);
          break;
        }
        // 変更キーでパス変更モード開始。
        if (key == KEY_CHANGE) {
          isPassChangeMode = true;
          clearInputBuffer();
          break;
        }
      }
      // 一定時間経過で自動施錠。
      if ((long)(now - unlockStartMs) >= (long)AUTO_LOCK_DURATION_MS) {
        setLockState(false, now);
        isPassChangeMode = false;
        setState(STATE_IDLE, now);
      }
      break;

    case STATE_ERROR:
      // 【エラー中】
      rememberDigitDuringError(key);

      // 一定時間経過でバッファクリアし待機へ戻る。
      if (isElapsed(now, stateEnteredMs, ERROR_DISPLAY_MS)) {
        clearInputBuffer();
        setState(STATE_IDLE, now);

        // ERROR中に受けた最初の数字キーを復帰直後に取り込む。
        if (pendingDigitAfterError >= 0 && pendingDigitAfterError <= 9) {
          appendInputDigit((uint8_t)pendingDigitAfterError);
          pendingDigitAfterError = KEY_NONE;
          setState(STATE_INPUT, now);
        } else if (key >= 0 && key <= 9) {
          // 復帰境界で受けたキーも保険として取り込む。
          appendInputDigit((uint8_t)key);
          setState(STATE_INPUT, now);
        }
      }
      break;

    case STATE_ALERT:
      // 【警告中】
      // 入力は全て無視。警告解除判定はループ先頭で実施。
      break;

    default:
      // 【異常時の安全側動作】
      // 施錠・出力停止・状態初期化。
      setLockState(false, now);
      isPassChangeMode = false;
      failedCount = 0;
      setRgb(false, false, false);
      noTone(PIN_BUZZER);
      setState(STATE_IDLE, now);
      break;
  }
}
