#include <Arduino.h>
#include <DHT.h>
#include <IRremote.hpp>
#include <TM1637Display.h>

// ピン定義
const uint8_t PIN_IR_RECV = 2;
const uint8_t PIN_DHT = 3;
const uint8_t PIN_PIR = 4;
const uint8_t PIN_TM1637_CLK = 5;
const uint8_t PIN_TM1637_DIO = 6;
const uint8_t PIN_FAN_PWM = 9;

// 状態定義（設計書準拠）
enum State : uint8_t {
  AUTO_MONITOR = 0,
  AUTO_FAN = 1,
  MANUAL = 2,
  SETTING = 3,
  POWER_OFF = 4,
};

// リモコン操作コマンド
// 要件:
// - 電源ボタン: ON/OFFトグル
// - 1: 温度表示
// - 2: 湿度表示
// - 3: 温湿度非表示
// - 4: ファン弱
// - 5: ファン停止
// - 6: ファン強
// - その他: 何もしない
enum Command : int {
  CMD_NONE = -1,
  CMD_POWER_TOGGLE = 0,
  CMD_SHOW_TEMP = 1,
  CMD_SHOW_HUM = 2,
  CMD_DISPLAY_OFF = 3,
  CMD_FAN_LOW = 4,
  CMD_FAN_STOP = 5,
  CMD_FAN_HIGH = 6,
};

// 表示モード
enum DisplayMode : uint8_t {
  DISPLAY_TEMP = 0,
  DISPLAY_HUM = 1,
  DISPLAY_OFF = 2,
};

// 典型的なNECリモコンのコード例
// 実機と違う場合は必ず書き換えてください。
const uint32_t IR_CODE_POWER = 0x00FFA25D;
const uint32_t IR_CODE_BTN_1 = 0x00FF30CF;
const uint32_t IR_CODE_BTN_2 = 0x00FF18E7;
const uint32_t IR_CODE_BTN_3 = 0x00FF7A85;
const uint32_t IR_CODE_BTN_4 = 0x00FF10EF;
const uint32_t IR_CODE_BTN_5 = 0x00FF38C7;
const uint32_t IR_CODE_BTN_6 = 0x00FF5AA5;

// 実行時変数
uint8_t currentState = AUTO_MONITOR;
bool powerOn = true;
uint8_t displayMode = DISPLAY_TEMP;
uint8_t fanLevel = 0;  // 0:停止 1:弱 2:中 3:強

// しきい値（設計書値）
uint8_t tempThreshold = 25;
uint8_t humThreshold = 60;

float temperatureC = 0.0F;
float humidityPct = 0.0F;
bool motionDetected = false;
uint8_t pirHighStreak = 0;
uint8_t sensorErrorCount = 0;

unsigned long lastSensorMillis = 0;
unsigned long lastPirMillis = 0;
unsigned long lastDisplayMillis = 0;
unsigned long lastIrMillis = 0;
unsigned long fanStrongStartMs = 0;

// タイミング定数
const unsigned long SENSOR_INTERVAL_MS = 1000;
const unsigned long PIR_INTERVAL_MS = 200;
const unsigned long DISPLAY_INTERVAL_MS = 250;
const unsigned long IR_DEBOUNCE_MS = 50;
const unsigned long FAN_STRONG_LIMIT_MS = 600000;

DHT dht(PIN_DHT, DHT11);
TM1637Display display(PIN_TM1637_CLK, PIN_TM1637_DIO);

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

int mapRawCodeToCommand(uint32_t rawCode) {
  switch (rawCode) {
    case IR_CODE_POWER:
      return CMD_POWER_TOGGLE;
    case IR_CODE_BTN_1:
      return CMD_SHOW_TEMP;
    case IR_CODE_BTN_2:
      return CMD_SHOW_HUM;
    case IR_CODE_BTN_3:
      return CMD_DISPLAY_OFF;
    case IR_CODE_BTN_4:
      return CMD_FAN_LOW;
    case IR_CODE_BTN_5:
      return CMD_FAN_STOP;
    case IR_CODE_BTN_6:
      return CMD_FAN_HIGH;
    default:
      return CMD_NONE;
  }
}

int readRemoteCommand() {
  if (!IrReceiver.decode()) {
    return CMD_NONE;
  }

  const unsigned long now = millis();
  const uint32_t rawCode = IrReceiver.decodedIRData.decodedRawData;
  IrReceiver.resume();

  if (now - lastIrMillis < IR_DEBOUNCE_MS) {
    return CMD_NONE;
  }

  const int command = mapRawCodeToCommand(rawCode);
  if (command != CMD_NONE) {
    lastIrMillis = now;
  }

  return command;
}

bool checkMotionGate() {
  return motionDetected;
}

bool evaluateAutoFanCondition() {
  if (sensorErrorCount >= 3) {
    return false;
  }

  const bool tempOk = (temperatureC >= tempThreshold);
  const bool humOk = (humidityPct >= humThreshold);
  const bool motionOk = checkMotionGate();
  return (tempOk || humOk) && motionOk;
}

void readEnvironment(unsigned long now) {
  if (now - lastSensorMillis >= SENSOR_INTERVAL_MS) {
    lastSensorMillis = now;

    const float newTemp = dht.readTemperature();
    const float newHum = dht.readHumidity();

    const bool tempValid = !isnan(newTemp) && newTemp >= -10.0F && newTemp <= 60.0F;
    const bool humValid = !isnan(newHum) && newHum >= 0.0F && newHum <= 100.0F;

    if (tempValid && humValid) {
      temperatureC = newTemp;
      humidityPct = newHum;
      sensorErrorCount = 0;
    } else {
      if (sensorErrorCount < 255) {
        sensorErrorCount++;
      }
    }
  }

  if (now - lastPirMillis >= PIR_INTERVAL_MS) {
    lastPirMillis = now;

    const int pirValue = digitalRead(PIN_PIR);
    if (pirValue == HIGH) {
      if (pirHighStreak < 255) {
        pirHighStreak++;
      }
    } else {
      pirHighStreak = 0;
    }
    motionDetected = (pirHighStreak >= 2);
  }
}

void applyRemoteCommand(int command) {
  // 未割り当て操作は何もしない
  if (command == CMD_NONE) {
    return;
  }

  // 電源トグルは常に有効
  if (command == CMD_POWER_TOGGLE) {
    if (currentState == POWER_OFF) {
      powerOn = true;
      currentState = AUTO_MONITOR;
    } else {
      powerOn = false;
      currentState = POWER_OFF;
      fanLevel = 0;
      display.clear();
    }
    return;
  }

  // 電源OFF中は電源ボタン以外を無視
  if (!powerOn) {
    return;
  }

  switch (command) {
    case CMD_SHOW_TEMP:
      displayMode = DISPLAY_TEMP;
      break;
    case CMD_SHOW_HUM:
      displayMode = DISPLAY_HUM;
      break;
    case CMD_DISPLAY_OFF:
      displayMode = DISPLAY_OFF;
      break;
    case CMD_FAN_LOW:
      fanLevel = 1;
      currentState = MANUAL;
      break;
    case CMD_FAN_STOP:
      fanLevel = 0;
      currentState = MANUAL;
      break;
    case CMD_FAN_HIGH:
      fanLevel = 3;
      currentState = MANUAL;
      break;
    default:
      // その他操作: 何も動作しない
      break;
  }
}

void updateStateByDesign() {
  if (currentState == POWER_OFF || currentState == SETTING) {
    return;
  }

  // センサー異常時は安全側で停止
  if (sensorErrorCount >= 3) {
    if (currentState != MANUAL) {
      currentState = AUTO_MONITOR;
    }
    fanLevel = 0;
    return;
  }

  switch (currentState) {
    case AUTO_MONITOR:
      fanLevel = 0;
      if (evaluateAutoFanCondition()) {
        currentState = AUTO_FAN;
      }
      break;
    case AUTO_FAN:
      fanLevel = 2;
      if (!evaluateAutoFanCondition()) {
        currentState = AUTO_MONITOR;
        fanLevel = 0;
      }
      break;
    case MANUAL:
      // 手動運転は電源OFFまで維持
      break;
    default:
      currentState = AUTO_MONITOR;
      fanLevel = 0;
      break;
  }
}

void updateDisplay(unsigned long now) {
  if (now - lastDisplayMillis < DISPLAY_INTERVAL_MS) {
    return;
  }
  lastDisplayMillis = now;

  if (!powerOn || displayMode == DISPLAY_OFF) {
    display.clear();
    return;
  }

  if (sensorErrorCount >= 3) {
    const uint8_t errSeg[] = {
      SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
      SEG_E | SEG_G,
      SEG_E | SEG_G,
      0x00,
    };
    display.setSegments(errSeg);
    return;
  }

  if (displayMode == DISPLAY_TEMP) {
    const int temp = static_cast<int>(temperatureC + 0.5F);
    display.showNumberDec(clampInt(temp, -99, 9999), false);
  } else {
    const int hum = static_cast<int>(humidityPct + 0.5F);
    display.showNumberDec(clampInt(hum, -99, 9999), false);
  }
}

void applyFanOutput(uint8_t level) {
    uint8_t effectiveLevel = level;

    // 強運転は10分超で中速へ自動降格
    if (effectiveLevel == 3) {
      if (fanStrongStartMs == 0) {
        fanStrongStartMs = millis();
      } else if (millis() - fanStrongStartMs > FAN_STRONG_LIMIT_MS) {
        effectiveLevel = 2;
        fanLevel = 2;
      }
    } else {
      fanStrongStartMs = 0;
    }

  uint8_t pwmValue = 0;

    switch (effectiveLevel) {
    case 0:
      pwmValue = 0;
      break;
    case 1:
      pwmValue = 85;
      break;
      case 2:
        pwmValue = 170;
        break;
    case 3:
      pwmValue = 255;
      break;
    default:
      pwmValue = 0;
      break;
  }

  analogWrite(PIN_FAN_PWM, pwmValue);
}

void setup() {
  pinMode(PIN_IR_RECV, INPUT);
  pinMode(PIN_DHT, INPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_FAN_PWM, OUTPUT);

  IrReceiver.begin(PIN_IR_RECV, ENABLE_LED_FEEDBACK);
  dht.begin();
  display.setBrightness(0x0A, true);
  Serial.begin(9600);

  currentState = AUTO_MONITOR;
  powerOn = true;
  displayMode = DISPLAY_TEMP;
  fanLevel = 0;
  tempThreshold = 25;
  humThreshold = 60;

  display.showNumberDec(8888, false);
  delay(300);
  display.clear();
}

void loop() {
  const unsigned long now = millis();

  const int command = readRemoteCommand();
  applyRemoteCommand(command);

  if (powerOn) {
    readEnvironment(now);
    updateStateByDesign();
  }

  updateDisplay(now);
  applyFanOutput(powerOn ? fanLevel : 0);
}
