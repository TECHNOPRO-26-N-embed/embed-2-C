#include <Arduino.h>
#include <DHT.h>
#include <IRremote.hpp>

// ========= ピン設定 =========
#define PIN_IR 2
#define PIN_DHT 3
#define PIN_FAN 5

#define DHTTYPE DHT11

DHT dht(PIN_DHT, DHTTYPE);

// ========= IRコード定義（提供された対応表準拠） =========
#define CODE_POWER 0xFFA25D
#define CODE_1 0xFF30CF
#define CODE_2 0xFF18E7
#define CODE_3 0xFF7A85
#define CODE_4 0xFF10EF

// ========= タイミング定義 =========
const unsigned long SENSOR_INTERVAL_MS = 1000;
const unsigned long IR_DEBOUNCE_MS = 50;

// ========= 設定値 =========
const uint8_t TEMP_THRESHOLD = 25;
const uint8_t HUM_THRESHOLD = 60;

// ========= コマンド定義 =========
enum Command {
  CMD_NONE = -1,
  CMD_POWER_TOGGLE = 0,
  CMD_FAN_LOW,
  CMD_FAN_MED,
  CMD_FAN_HIGH,
  CMD_FAN_STOP
};

// ========= 状態定義 =========
enum Mode {
  AUTO_MONITOR = 0,
  AUTO_FAN,
  MANUAL,
  POWER_OFF
};

// ========= 状態変数 =========
Mode currentMode = AUTO_MONITOR;
bool powerOn = true;

float temperatureC = 0.0;
float humidityPct = 0.0;
uint8_t tempThreshold = TEMP_THRESHOLD;
uint8_t humThreshold = HUM_THRESHOLD;

// 0:停止 1:弱 2:中 3:強
uint8_t fanLevel = 0;

uint8_t sensorErrorCount = 0;
bool sensorError = false;

unsigned long lastSensorMillis = 0;
unsigned long lastIrMillis = 0;

// ========= 補助関数 =========
uint8_t fanLevelToPwm(uint8_t level) {
  if (level == 1) return 85;
  if (level == 2) return 170;
  if (level == 3) return 255;
  return 0;
}

int mapRawCodeToCommand(uint32_t code) {
  // POWER: 電源ON/OFF
  if (code == CODE_POWER) return CMD_POWER_TOGGLE;
  // 数字キー: 1=弱, 2=中, 3=強, 4=停止
  if (code == CODE_1) return CMD_FAN_LOW;
  if (code == CODE_2) return CMD_FAN_MED;
  if (code == CODE_3) return CMD_FAN_HIGH;
  if (code == CODE_4) return CMD_FAN_STOP;
  return CMD_NONE;
}

int readRemoteCommand(unsigned long now) {
  if (!IrReceiver.decode()) return CMD_NONE;

  uint32_t code = IrReceiver.decodedIRData.decodedRawData;
  IrReceiver.resume();

  if (now - lastIrMillis < IR_DEBOUNCE_MS) {
    return CMD_NONE;
  }

  lastIrMillis = now;
  return mapRawCodeToCommand(code);
}

void readEnvironment(unsigned long now) {
  if (now - lastSensorMillis < SENSOR_INTERVAL_MS) return;
  lastSensorMillis = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  bool invalid = isnan(t) || isnan(h) || t < 0 || t > 50 || h < 20 || h > 90;
  if (invalid) {
    sensorErrorCount++;
    if (sensorErrorCount >= 3) {
      sensorError = true;
      fanLevel = 0;
    }
    return;
  }

  temperatureC = t;
  humidityPct = h;
  sensorErrorCount = 0;
  sensorError = false;
}

bool evaluateAutoFanCondition() {
  if (sensorError) return false;
  bool tempOk = (temperatureC >= tempThreshold);
  bool humOk = (humidityPct >= humThreshold);
  return tempOk || humOk;
}

void handlePower(int command) {
  if (command != CMD_POWER_TOGGLE) return;

  if (powerOn) {
    powerOn = false;
    currentMode = POWER_OFF;
    fanLevel = 0;
    Serial.println("[POWER] OFF");
  } else {
    powerOn = true;
    currentMode = AUTO_MONITOR;
    fanLevel = 0;
    Serial.println("[POWER] ON -> AUTO_MONITOR");
  }
}

void handleManualFanCommand(int command) {
  if (command == CMD_FAN_LOW) {
    fanLevel = 1;
  } else if (command == CMD_FAN_MED) {
    fanLevel = 2;
  } else if (command == CMD_FAN_HIGH) {
    fanLevel = 3;
  } else if (command == CMD_FAN_STOP) {
    fanLevel = 0;
  }
}

void updateState(int command) {
  if (!powerOn || currentMode == POWER_OFF) return;

  if (currentMode == MANUAL) {
    handleManualFanCommand(command);
    return;
  }

  if (currentMode == AUTO_MONITOR) {
    if (evaluateAutoFanCondition()) {
      currentMode = AUTO_FAN;
      fanLevel = 2;
      Serial.println("[STATE] AUTO_FAN");
    } else {
      fanLevel = 0;
    }

    if (command == CMD_FAN_LOW || command == CMD_FAN_MED || command == CMD_FAN_HIGH || command == CMD_FAN_STOP) {
      handleManualFanCommand(command);
      currentMode = MANUAL;
      Serial.println("[STATE] MANUAL");
    }
    return;
  }

  if (currentMode == AUTO_FAN) {
    if (!evaluateAutoFanCondition()) {
      currentMode = AUTO_MONITOR;
      fanLevel = 0;
      Serial.println("[STATE] AUTO_MONITOR");
      return;
    }

    fanLevel = 2;

    if (command == CMD_FAN_LOW || command == CMD_FAN_MED || command == CMD_FAN_HIGH || command == CMD_FAN_STOP) {
      handleManualFanCommand(command);
      currentMode = MANUAL;
      Serial.println("[STATE] MANUAL");
    }
  }
}

void applyFanOutput(uint8_t level) {
  analogWrite(PIN_FAN, fanLevelToPwm(level));
}

// ========= SETUP =========
void setup() {
  pinMode(PIN_IR, INPUT);
  pinMode(PIN_FAN, OUTPUT);

  dht.begin();
  IrReceiver.begin(PIN_IR, ENABLE_LED_FEEDBACK);

  Serial.begin(9600);
  Serial.println("[BOOT] climate fan controller start");
}

// ========= LOOP =========
void loop() {
  unsigned long now = millis();

  int command = readRemoteCommand(now);
  handlePower(command);

  if (powerOn) {
    readEnvironment(now);
    updateState(command);
  } else {
    fanLevel = 0;
  }

  applyFanOutput(fanLevel);
}