#include <IRremote.h>

/* ===== ピン定義 ===== */
#define IR_PIN      2
#define LED_PIN     9
#define BUZZER_PIN  8

/* ===== リモコンコード ===== */
/* 1ボタン = LED、2ボタン = ブザー、3ボタン = 停止、警告ボタン = 警告モード */
#define CODE_LED      0xFF30CF
#define CODE_BUZZER   0xFF18E7
#define CODE_STOP     0xFF7A85
#define CODE_WARNING  0xFFA25D

/* ===== 状態定義 ===== */
#define STATE_WAIT     0
#define STATE_LED      1
#define STATE_BUZZER   2
#define STATE_STOP     3
#define STATE_WARNING  4

/* ===== 赤外線受信 ===== */
IRrecv irrecv(IR_PIN);
decode_results results;

/* ===== グローバル変数 ===== */
int currentState = STATE_WAIT;

unsigned long receivedCode = 0;
unsigned long lastReceiveMillis = 0;
unsigned long lastBlinkMillis = 0;
unsigned long lastBuzzerMillis = 0;

int ledState = LOW;
int buzzerState = LOW;

/* ===== 定数 ===== */
#define RECEIVE_INTERVAL         200
#define BLINK_INTERVAL           500
#define WARNING_BUZZER_INTERVAL  300

unsigned long blinkInterval = BLINK_INTERVAL;

/* ===== 関数プロトタイプ ===== */
unsigned long readRemote(void);
void updateState(unsigned long code);
void controlLed(int state);
void controlBuzzer(int state);
void warningMode(int state);
void stopAll(void);

/* ===== 初期化 ===== */
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  ledState = LOW;
  buzzerState = LOW;
  currentState = STATE_WAIT;

  Serial.begin(9600);
  irrecv.enableIRIn();

  Serial.println("System Start");
  Serial.println("1: LED / 2: BUZZER / 3: STOP / WARNING");
}

/* ===== メイン処理 ===== */
void loop() {
  unsigned long code = readRemote();

  if (code != 0) {
    receivedCode = code;
    updateState(receivedCode);
  }

  if (currentState == STATE_WAIT) {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    ledState = LOW;
    buzzerState = LOW;
  }
  else if (currentState == STATE_LED) {
    controlLed(currentState);

    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = LOW;
  }
  else if (currentState == STATE_BUZZER) {
    digitalWrite(LED_PIN, LOW);
    ledState = LOW;

    controlBuzzer(currentState);
  }
  else if (currentState == STATE_WARNING) {
    warningMode(currentState);
  }
  else if (currentState == STATE_STOP) {
    stopAll();
  }
}

/* ===== 赤外線リモコン入力受信 ===== */
unsigned long readRemote(void) {
  unsigned long now;
  unsigned long code;

  if (!irrecv.decode(&results)) {
    return 0;
  }

  now = millis();

  if (now - lastReceiveMillis < RECEIVE_INTERVAL) {
    Serial.println("Ignored repeat input");
    irrecv.resume();
    return 0;
  }

  code = results.value;

  /* 長押し時のリピートコードは無視する */
  if (code == 0xFFFFFFFF) {
    Serial.println("Ignored repeat code");
    irrecv.resume();
    return 0;
  }

  Serial.print("IR Code: 0x");
  Serial.println(code, HEX);

  lastReceiveMillis = now;

  irrecv.resume();

  return code;
}

/* ===== 状態更新 ===== */
void updateState(unsigned long code) {
  if (code == 0) {
    Serial.println("State unchanged: code is 0");
    return;
  }

  if (code == CODE_LED) {
    currentState = STATE_LED;
    Serial.println("State: LED");
  }
  else if (code == CODE_BUZZER) {
    currentState = STATE_BUZZER;
    Serial.println("State: BUZZER");
  }
  else if (code == CODE_STOP) {
    currentState = STATE_STOP;
    Serial.println("State: STOP");
  }
  else if (code == CODE_WARNING) {
    currentState = STATE_WARNING;
    Serial.println("State: WARNING");
  }
  else {
    Serial.println("Unknown Code");
  }
}

/* ===== LED制御 ===== */
void controlLed(int state) {
  unsigned long now;

  if (state != STATE_LED) {
    Serial.println("LED Skip");
    return;
  }

  now = millis();

  if (now - lastBlinkMillis >= blinkInterval) {
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    digitalWrite(LED_PIN, ledState);
    lastBlinkMillis = now;

    Serial.print("LED Mode, ledState = ");
    Serial.println(ledState);
  }
}

/* ===== ブザー制御 ===== */
/* アクティブブザーなので digitalWrite() で制御する */
void controlBuzzer(int state) {
  if (state != STATE_BUZZER) {
    Serial.println("Buzzer Skip");
    return;
  }

  digitalWrite(BUZZER_PIN, HIGH);

  if (buzzerState == LOW) {
    buzzerState = HIGH;
    Serial.println("Buzzer Mode");
  }
}

/* ===== 警告モード ===== */
/* LED点滅とアクティブブザーを同時に動作させる */
void warningMode(int state) {
  unsigned long now;

  if (state != STATE_WARNING) {
    Serial.println("Warning Skip");
    return;
  }

  now = millis();

  /* LED点滅制御 */
  if (now - lastBlinkMillis >= blinkInterval) {
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    digitalWrite(LED_PIN, ledState);
    lastBlinkMillis = now;

    Serial.print("Warning Mode LED, ledState = ");
    Serial.println(ledState);
  }

  /* ブザーON/OFF制御 */
  if (now - lastBuzzerMillis >= WARNING_BUZZER_INTERVAL) {
    if (buzzerState == LOW) {
      buzzerState = HIGH;
    } else {
      buzzerState = LOW;
    }

    digitalWrite(BUZZER_PIN, buzzerState);
    lastBuzzerMillis = now;

    Serial.print("Warning Mode Buzzer, buzzerState = ");
    Serial.println(buzzerState);
  }
}

/* ===== 停止処理 ===== */
void stopAll(void) {
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  ledState = LOW;
  buzzerState = LOW;
  receivedCode = 0;

  currentState = STATE_WAIT;

  Serial.println("Stop All");
}  