#if __has_include(<IRremote.hpp>)
#include <IRremote.hpp>
#elif __has_include(<IRremote.h>)
#include <IRremote.h>

#endif

// Pin definitions
// 各モジュールを接続するArduinoのピン番号。
const uint8_t PIN_IR_RECEIVER = 3;
const uint8_t PIN_LED_RED = 9;
const uint8_t PIN_BUZZER = 8;

// State definitions
// 待機状態と警報状態を定数で定義する。
const int STATE_STANDBY = 0;
const int STATE_ALARM = 1;

// Timing (ms)
// millis()を使った点滅周期（待機:1秒 / 警報:0.2秒）。
const unsigned long STANDBY_BLINK_INTERVAL_MS = 1000;
const unsigned long ALARM_BLINK_INTERVAL_MS = 200;

// Set to false if your buzzer module is active-low.
const bool BUZZER_ACTIVE_HIGH = true;

// Replace these with measured IR codes
// 実機で測定したSOS/解除ボタンのIRコードを設定する。
const unsigned long DEFAULT_SOS_CODE = 0xBA45FF00;
const unsigned long DEFAULT_CANCEL_CODE = 0x00FF4AB5;

// State management
// 現在状態と、点滅周期制御用の最終時刻。
int currentState = STATE_STANDBY;
unsigned long lastMillis = 0;

// IR code management
// 最新受信コードと、判定用コードを保持する。
unsigned long irCode = 0;
unsigned long sosCode = 0;
unsigned long cancelCode = 0;

// Output states
// LED/ブザーの現在状態を管理するフラグ。
bool ledState = false;
bool buzzerState = false;

// Error flag (reserved for future error handling)
bool errorFlag = false;

unsigned long readIR();
void handleSerialCommand();
void setBuzzer(bool on);
void updateOutput(int state);
void startAlarm();
void stopAlarm();
void showStandby();

void setup() {
  // 入出力ピンを初期化する。
  pinMode(PIN_IR_RECEIVER, INPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // IR受信を開始する。
  IrReceiver.begin(PIN_IR_RECEIVER, DISABLE_LED_FEEDBACK);

  // 初期IRコードを定数から読み込む。
  sosCode = DEFAULT_SOS_CODE;
  cancelCode = DEFAULT_CANCEL_CODE;

  // デバッグ用シリアル通信を開始する。
  Serial.begin(9600);
  Serial.println("[INFO] System boot");
  Serial.print("[INFO] SOS code: 0x");
  Serial.println(sosCode, HEX);
  Serial.print("[INFO] CANCEL code: 0x");
  Serial.println(cancelCode, HEX);
  Serial.println("[INFO] Serial command: s=start alarm, c=cancel alarm");

  // Power-on check: keep LED ON for 1 second.
  digitalWrite(PIN_LED_RED, HIGH);
  setBuzzer(false);
  delay(1000);
  digitalWrite(PIN_LED_RED, LOW);

  lastMillis = millis();
}

void loop() {
  // シリアル入力で強制テスト（s:警報開始 / c:解除）を受け付ける。
  handleSerialCommand();

  // IR信号を読み取る（受信なしなら0）。
  irCode = readIR();

  if (currentState == STATE_STANDBY) {
    // Start alarm on any valid IR key press while in standby.
    if (irCode != 0) {
      // 待機中に何かボタンが押されたら警報状態へ遷移する。
      startAlarm();
      currentState = STATE_ALARM;
      Serial.println("[STATE] -> ALARM");
      Serial.print("[INFO] Trigger code: 0x");
      Serial.println(irCode, HEX);
    }
    // 待機中のゆっくり点滅を実行する。
    showStandby();
  } else if (currentState == STATE_ALARM) {
    // 警報中は解除コードのみ受け付ける。
    if (irCode == cancelCode) {
      stopAlarm();
      currentState = STATE_STANDBY;
      Serial.println("[STATE] -> STANDBY");
    } else if (irCode != 0) {
      Serial.println("[INFO] Not CANCEL code");
    } else {
      updateOutput(STATE_ALARM);
    }
  } else {
    // 想定外の状態値が入った場合は安全側（待機）へ戻す。
    errorFlag = true;
    stopAlarm();
    currentState = STATE_STANDBY;
  }
}

unsigned long readIR() {
  // IR受信できたときだけコードを取り出す。
  if (IrReceiver.decode()) {
    unsigned long code = IrReceiver.decodedIRData.decodedRawData;

    // 長押し時のリピートコードは無視する。
    if (code == 0xFFFFFFFFUL) {
      IrReceiver.resume();
      return 0;
    }

    // 受信コードをシリアル表示してデバッグしやすくする。
    Serial.print("[IR] raw=0x");
    Serial.println(code, HEX);

    // 次の受信に備えて受信機を再開する。
    IrReceiver.resume();

    return code;
  }

  return 0;
}

void handleSerialCommand() {
  // シリアル入力が無ければ何もしない。
  if (!Serial.available()) {
    return;
  }

  char ch = static_cast<char>(Serial.read());
  // 手動テスト用コマンドで状態を強制切替する。
  if (ch == 's' || ch == 'S') {
    startAlarm();
    currentState = STATE_ALARM;
    Serial.println("[CMD] Force alarm ON");
  } else if (ch == 'c' || ch == 'C') {
    stopAlarm();
    currentState = STATE_STANDBY;
    Serial.println("[CMD] Force alarm OFF");
  }
}

void setBuzzer(bool on) {
  // ブザーがアクティブHigh/Lowのどちらでも動くように吸収する。
  bool outputHigh = BUZZER_ACTIVE_HIGH ? on : !on;
  digitalWrite(PIN_BUZZER, outputHigh ? HIGH : LOW);
}

void updateOutput(int state) {
  // 現在時刻を基準に、delayなしで周期制御する。
  unsigned long now = millis();

  if (state == STATE_STANDBY) {
    // 待機中はブザーOFF、LEDをゆっくり点滅。
    buzzerState = false;
    setBuzzer(false);

    if (now - lastMillis >= STANDBY_BLINK_INTERVAL_MS) {
      lastMillis = now;
      ledState = !ledState;
      digitalWrite(PIN_LED_RED, ledState ? HIGH : LOW);
    }
  } else if (state == STATE_ALARM) {
    // 警報中はブザーON、LEDを高速点滅。
    buzzerState = true;
    setBuzzer(true);

    if (now - lastMillis >= ALARM_BLINK_INTERVAL_MS) {
      lastMillis = now;
      ledState = !ledState;
      digitalWrite(PIN_LED_RED, ledState ? HIGH : LOW);
    }
  }
}

void startAlarm() {
  // 警報開始時の初期状態をセットする。
  ledState = true;
  buzzerState = true;
  lastMillis = millis();

  digitalWrite(PIN_LED_RED, HIGH);
  setBuzzer(true);

  updateOutput(STATE_ALARM);
}

void stopAlarm() {
  // 警報停止時に出力をOFFへ戻す。
  ledState = false;
  buzzerState = false;
  lastMillis = millis();

  digitalWrite(PIN_LED_RED, LOW);
  setBuzzer(false);

  updateOutput(STATE_STANDBY);
}

void showStandby() {
  // 待機中のLEDゆっくり点滅を実行する。
  unsigned long now = millis();

  if (now - lastMillis >= STANDああY_BLINK_INTERVAL_MS) {
    lastMillis = now;
    ledState = !ledState;
    digitalWrite(PIN_LED_RED, ledState ? HIGH : LOW);
    Serial.println(ledState);
  }

  // 待機中は常にブザーOFFを維持する。
  buzzerState = false;
  setBuzzer(false);
}
