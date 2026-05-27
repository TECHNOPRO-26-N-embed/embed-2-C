#include <IRremote.hpp>

#if __has_include(<IRremote.hpp>)
#include <IRremote.hpp>
#elif __has_include(<IRremote.h>)
#include <IRremote.h>

#endif

// Pin definitions
const uint8_t PIN_IR_RECEIVER = 3;
const uint8_t PIN_LED_RED = 9;
const uint8_t PIN_BUZZER = 8;

// State definitions
const int STATE_STANDBY = 0;
const int STATE_ALARM = 1;

// Timing (ms)
const unsigned long STANDBY_BLINK_INTERVAL_MS = 1000;
const unsigned long ALARM_BLINK_INTERVAL_MS = 200;

// Set to false if your buzzer module is active-low.
const bool BUZZER_ACTIVE_HIGH = true;

// Replace these with measured IR codes
// FUNC/STOP key as cancel button.
#define CODE_FUNC_STOP 0xFFE21DUL
#define CODE_FUNC_STOP_CMD 0x47
const unsigned long DEFAULT_SOS_CODE = 0xBA45FF00;
const unsigned long DEFAULT_CANCEL_CODE = CODE_FUNC_STOP;

// State management
int currentState = STATE_STANDBY;
unsigned long lastMillis = 0;

// IR code management
unsigned long irCode = 0;
uint8_t irCommand = 0;
unsigned long sosCode = 0;
unsigned long cancelCode = 0;

// Output states
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
bool isCancelSignal(unsigned long code, uint8_t command);
bool hasIrSignal(unsigned long code, uint8_t command);

void setup() {
  pinMode(PIN_IR_RECEIVER, INPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  IrReceiver.begin(PIN_IR_RECEIVER, DISABLE_LED_FEEDBACK);

  sosCode = DEFAULT_SOS_CODE;
  cancelCode = DEFAULT_CANCEL_CODE;

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
  handleSerialCommand();
  irCode = readIR();

  if (currentState == STATE_STANDBY) {
    // Start alarm on any valid IR key press while in standby.
    if (hasIrSignal(irCode, irCommand)) {
      startAlarm();
      currentState = STATE_ALARM;
      Serial.println("[STATE] -> ALARM");
      Serial.print("[INFO] Trigger code: 0x");
      Serial.println(irCode, HEX);
    }
    showStandby();
  } else if (currentState == STATE_ALARM) {
    if (isCancelSignal(irCode, irCommand)) {
      stopAlarm();
      currentState = STATE_STANDBY;
      Serial.println("[STATE] -> STANDBY");
    } else if (hasIrSignal(irCode, irCommand)) {
      Serial.println("[INFO] Not CANCEL code");
    } else {
      updateOutput(STATE_ALARM);
    }
  } else {
    // Unexpected state fallback.
    errorFlag = true;
    stopAlarm();
    currentState = STATE_STANDBY;
  }
}

unsigned long readIR() {
  irCommand = 0;

  if (IrReceiver.decode()) {
    unsigned long code = IrReceiver.decodedIRData.decodedRawData;
    irCommand = IrReceiver.decodedIRData.command;

    if (code == 0xFFFFFFFFUL) {
      IrReceiver.resume();
      return 0;
    }

    Serial.print("[IR] raw=0x");
    Serial.println(code, HEX);
    Serial.print("[IR] cmd=0x");
    Serial.println(irCommand, HEX);

    IrReceiver.resume();

    return code;
  }

  return 0;
}

void handleSerialCommand() {
  if (!Serial.available()) {
    return;
  }

  char ch = static_cast<char>(Serial.read());
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
  bool outputHigh = BUZZER_ACTIVE_HIGH ? on : !on;
  digitalWrite(PIN_BUZZER, outputHigh ? HIGH : LOW);
}

void updateOutput(int state) {
  unsigned long now = millis();

  if (state == STATE_STANDBY) {
    buzzerState = false;
    setBuzzer(false);

    if (now - lastMillis >= STANDBY_BLINK_INTERVAL_MS) {
      lastMillis = now;
      ledState = !ledState;
      digitalWrite(PIN_LED_RED, ledState ? HIGH : LOW);
    }
  } else if (state == STATE_ALARM) {
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
  ledState = true;
  buzzerState = true;
  lastMillis = millis();

  digitalWrite(PIN_LED_RED, HIGH);
  setBuzzer(true);

  updateOutput(STATE_ALARM);
}

void stopAlarm() {
  ledState = false;
  buzzerState = false;
  lastMillis = millis();

  digitalWrite(PIN_LED_RED, LOW);
  setBuzzer(false);

  updateOutput(STATE_STANDBY);
}

void showStandby() {
  unsigned long now = millis();

  if (now - lastMillis >= STANDBY_BLINK_INTERVAL_MS) {
    lastMillis = now;
    ledState = !ledState;
    digitalWrite(PIN_LED_RED, ledState ? HIGH : LOW);
    Serial.println(ledState);
  }

  buzzerState = false;
  setBuzzer(false);
}

bool hasIrSignal(unsigned long code, uint8_t command) {
  return (code != 0) || (command != 0);
}

bool isCancelSignal(unsigned long code, uint8_t command) {
  if (code == cancelCode) {
    return true;
  }

  // Some IRremote configurations expose FUNC/STOP by command byte only.
  if (command == CODE_FUNC_STOP_CMD) {
    return true;
  }

  return false;
}
