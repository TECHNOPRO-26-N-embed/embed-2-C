# 詳細設計書 — 組込み開発実習

<!-- 作成者: 石川真大 / 日付: 2026-05-25 / グループ: 2-C -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | 一定以上の気温と湿度でファンを回す温度計 |
| 状態の種類（1-2 状態遷移から） | 自動監視 / 自動送風 / 手動運転 / しきい値設定 / 電源OFF |
| 実装する関数の数（2-2 関数一覧から） | 12個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約26B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_IR_RECV    : const uint8_t = 2   // IR受信モジュール入力
  PIN_DHT        : const uint8_t = 3   // DHT11データ入力
  PIN_PIR        : const uint8_t = 4   // PIR人感入力
  PIN_TM1637_CLK : const uint8_t = 5   // 7セグCLK
  PIN_TM1637_DIO : const uint8_t = 6   // 7セグDIO
  PIN_FAN_PWM    : const uint8_t = 9   // モーターPWM出力

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState       : uint8_t = 0      // 0:AUTO_MONITOR 1:AUTO_FAN 2:MANUAL 3:SETTING 4:POWER_OFF
  powerOn            : bool = true

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastSensorMillis   : unsigned long = 0   // DHT11更新周期1000ms
  lastPirMillis      : unsigned long = 0   // PIR更新周期200ms
  lastDisplayMillis  : unsigned long = 0   // 表示更新周期250ms
  lastIrMillis       : unsigned long = 0   // IRデバウンス50ms

【センサー・入力値】（basic_design.md 2-1 から転記）
  temperatureC       : float = 0.0
  humidityPct        : float = 0.0
  motionDetected     : bool = false
  pirHighStreak      : uint8_t = 0
  lastRemoteCommand  : int = -1

【その他のフラグ・カウンター】
  tempThreshold      : uint8_t = 25
  humThreshold       : uint8_t = 60
  fanLevel           : uint8_t = 0    // 0:停止 1:弱 2:中 3:強
  displayMode        : uint8_t = 0    // 0:温度 1:湿度
  settingTarget      : uint8_t = 0    // 0:温度しきい値 1:湿度しきい値
  sensorErrorCount   : uint8_t = 0
  fanStrongStartMs   : unsigned long = 0

【定数】
  SENSOR_INTERVAL_MS   : const unsigned long = 1000
  PIR_INTERVAL_MS      : const unsigned long = 200
  DISPLAY_INTERVAL_MS  : const unsigned long = 250
  IR_DEBOUNCE_MS       : const unsigned long = 50
  FAN_STRONG_LIMIT_MS  : const unsigned long = 600000   // 10分
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. 使用ピンを初期化する
  - pinMode(PIN_IR_RECV, INPUT)
  - pinMode(PIN_DHT, INPUT)
  - pinMode(PIN_PIR, INPUT)
  - pinMode(PIN_FAN_PWM, OUTPUT)

2. ライブラリと通信を初期化する
  - IR受信ライブラリ開始（受信有効化）
  - DHT11ライブラリ開始
  - TM1637表示ライブラリ開始（輝度初期化）
  - Serial.begin(9600)

3. 変数を初期状態へセットする
  - currentState = 0（AUTO_MONITOR）, powerOn = true
  - fanLevel = 0, displayMode = 0
  - tempThreshold = 25, humThreshold = 60
  - 7セグに起動表示を短時間出して監視画面へ移行
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), readSensor() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（待機中）のとき＞
  - センサー値を監視する
  - 検知条件を満たしたら → currentState = 1

＜currentState が 1（動作中）のとき＞
  - メイン処理を行う
  - 終了条件を満たしたら → currentState = 2

＜currentState が 2（完了）のとき＞
  - 完了表示をする
  - リセットボタンが押されたら → currentState = 0

＜currentState が 3（エラー）のとき＞
  - エラー表示をする / リセットを待つ
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】

＜毎ループ実行すること＞
  - now = millis() を取得
  - command = readRemoteCommand() を実行
  - readEnvironment(now) を周期条件付きで実行
  - handlePower(command) で電源状態を更新
  - powerOn == true なら updateState(command) を実行
  - updateDisplay(now) を周期条件付きで実行
  - applyFanOutput(fanLevel) を実行

＜currentState が 0（AUTO_MONITOR）のとき＞
  - evaluateAutoFanCondition() が true なら currentState = 1
  - fanLevel = 0（停止）

＜currentState が 1（AUTO_FAN）のとき＞
  - fanLevel = 2（中速）
  - 条件未達（温湿度未満 または 人未検知）で currentState = 0
  - リモコンの強弱/停止操作が来たら currentState = 2

＜currentState が 2（MANUAL）のとき＞
  - handleRemoteCommand(command) で fanLevel を 0/1/2/3 に変更
  - 電源OFFされるまで自動状態へ戻さない

＜currentState が 3（SETTING）のとき＞
  - updateThresholdSetting(command) を実行
  - 設定確定操作で currentState = 0

＜currentState が 4（POWER_OFF）のとき＞
  - fanLevel = 0、7セグ消灯
  - 電源ONコマンド受信まで待機

```

---

### `readRemoteCommand()` — IR受信コードを内部コマンドへ変換する

**basic_design.md 2-2 との対応：** （共通）IR読出

**引数：** なし

**戻り値：** int（未受信/未割当は -1）

```
【処理の流れ】
1. IR受信データがあるか確認する
2. 受信時刻 - lastIrMillis が 50ms未満ならノイズとして破棄する
3. 受信コードをコマンド表（電源ON/OFF、表示切替、停止/弱/中/強、設定）へ変換する
4. 変換結果を返し、lastIrMillis を更新する

【エラー・異常ケース】
- 未割当コードの場合: -1を返し、状態は変更しない
```

---

### `readEnvironment()` — DHT11/PIRを周期読取して状態変数を更新する

**basic_design.md 2-2 との対応：** （共通）センサー読出、F01

**引数：** `now`（unsigned long）: 現在時刻（millis）

**戻り値：** void

```
【処理の流れ】
1. now - lastSensorMillis >= 1000ms なら DHT11を読み取る
2. 温度/湿度が正常範囲なら temperatureC/humidityPct を更新し sensorErrorCount = 0
3. NaNや範囲外なら sensorErrorCount++ し、3回連続で fanLevel = 0・エラー表示フラグを立てる
4. now - lastPirMillis >= 200ms なら PIRを読み、2回連続HIGHで motionDetected = true にする

【エラー・異常ケース】
- DHT失敗が連続した場合: 安全側（ファン停止）へ倒す
```

---

### `checkMotionGate()` — 人感条件の有効/無効を判定する

**basic_design.md 2-2 との対応：** A01

**引数：** なし

**戻り値：** bool

```
【処理の流れ】
1. motionDetected の最新値を確認する
2. trueなら「人感条件OK」を返す
3. falseなら「人感条件NG」を返す

【エラー・異常ケース】
- PIR値が不安定な場合: 直近2回連続HIGHルールで誤判定を抑制する
```

---

### `evaluateAutoFanCondition()` — 自動送風開始/継続条件を判定する

**basic_design.md 2-2 との対応：** F02

**引数：** なし

**戻り値：** bool

```
【処理の流れ】
1. tempOk = (temperatureC >= tempThreshold) を判定する
2. humOk  = (humidityPct >= humThreshold) を判定する
3. motionOk = checkMotionGate() を取得する
4. (tempOk または humOk) かつ motionOk のとき true を返す

【エラー・異常ケース】
- センサー異常フラグ中は常に false を返す
```

---

### `handlePower(int command)` — 電源ON/OFF状態を切り替える

**basic_design.md 2-2 との対応：** F05

**引数：** `command`（int）: IRコマンド値

**戻り値：** void

```
【処理の流れ】
1. command が 電源OFF のとき powerOn = false、currentState = 4、fanLevel = 0、表示消灯を設定する
2. command が 電源ON かつ currentState = 4 のとき powerOn = true、currentState = 0 に戻す
3. 電源ON復帰時に手動設定値（しきい値、表示モード）を保持したまま監視を再開する

【エラー・異常ケース】
- 連続同一コマンドの場合: 状態を再変更せず無視する
```

---

### `handleRemoteCommand(int command)` — リモコン操作を状態/出力へ反映する

**basic_design.md 2-2 との対応：** F04

**引数：** `command`（int）: IRコマンド値

**戻り値：** void

```
【処理の流れ】
1. 表示切替コマンドなら displayMode を反転する
2. 停止/弱/中/強コマンドなら fanLevel を更新し currentState = 2（MANUAL）へ遷移する
3. 設定変更コマンドなら currentState = 3（SETTING）へ遷移する

【エラー・異常ケース】
- 未割当コマンド: 何もしない
```

---

### `updateThresholdSetting(int command)` — 温湿度しきい値を変更・確定する

**basic_design.md 2-2 との対応：** A02

**引数：** `command`（int）: 設定モード中の操作コマンド

**戻り値：** void

```
【処理の流れ】
1. 対象切替コマンドで settingTarget を温度/湿度で切り替える
2. 増減コマンドで tempThreshold または humThreshold を1ずつ変更する
3. 上限下限を適用する（温度: 15〜35、湿度: 30〜80）
4. 決定コマンドで currentState = 0（AUTO_MONITOR）へ戻る

【エラー・異常ケース】
- 範囲外に出る操作: 最小値/最大値に丸める
```

---

### `updateState(int command)` — 現在値と入力から次状態を決定する

**basic_design.md 2-2 との対応：** （共通）状態更新

**引数：** `command`（int）: IRコマンド値

**戻り値：** void

```
【処理の流れ】
1. currentState が POWER_OFF なら即時returnする
2. handleRemoteCommand(command) を先に適用する
3. AUTO_MONITORで evaluateAutoFanCondition() が true なら AUTO_FANへ遷移
4. AUTO_FANで条件がfalseなら AUTO_MONITORへ戻す
5. MANUAL中は電源OFF以外で状態遷移させない

【エラー・異常ケース】
- 不正なstate値: AUTO_MONITORへ初期化して安全側へ復帰
```

---

### `updateDisplay()` — 7セグ表示を現在状態に合わせて更新する

**basic_design.md 2-2 との対応：** （共通）表示更新、F03

**引数：** `now`（unsigned long）: 現在時刻（millis）

**戻り値：** void

```
【処理の流れ】
1. now - lastDisplayMillis >= 250ms のときだけ表示処理を行う
2. POWER_OFFなら消灯、SETTINGなら設定対象値を表示する
3. 通常時は displayMode に応じて温度または湿度を表示する
4. センサー異常時は Err 表示を優先する

【エラー・異常ケース】
- 表示値が範囲外: クリップして表示崩れを防ぐ
```

---

### `applyFanOutput(uint8_t level)` — fanLevelをPWM出力へ反映する

**basic_design.md 2-2 との対応：** （共通）ファン制御

**引数：** `level`（uint8_t）: 0:停止 1:弱 2:中 3:強

**戻り値：** void

```
【処理の流れ】
1. level を PWM値へ変換する（0, 85, 170, 255）
2. analogWrite(PIN_FAN_PWM, pwmValue) を実行する
3. 強運転(level=3)の継続時間を計測し、10分超で level=2へ降格する

【エラー・異常ケース】
- level が0〜3以外: 0（停止）として扱う
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  IRリモコン入力を受けたとき、50ms以内の連続受信は同一入力として無視する。

【処理の流れ】
  1. IR受信コードを読む
  2. 前回確定した時刻（lastIrMillis）からの経過時間を計算する
  3. 経過時間 < IR_DEBOUNCE_MS（例: 50ms）→ 無視する
  4. 経過時間 >= IR_DEBOUNCE_MS → コマンド入力として確定する
  5. lastIrMillis を更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastIrMillis    : unsigned long         // 前回受信を確定した時刻
  IR_DEBOUNCE_MS  : const unsigned long = 50  // デバウンス時間（ms）
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  - DHT11読取: 1000msごと（lastSensorMillis）
  - PIR読取: 200msごと（lastPirMillis）
  - 7セグ表示更新: 250msごと（lastDisplayMillis）
  - IR入力受付: 常時監視 + 50msデバウンス（lastIrMillis）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 自動送風中に手動コマンド（停止/弱/中/強）を受けたら currentState = MANUAL に固定する
2. MANUAL中は evaluateAutoFanCondition() の結果を参照しても自動遷移しない
3. 電源OFFコマンド受信時のみ POWER_OFFへ遷移し、次の電源ONでAUTO_MONITORへ戻す

【入力値と出力値の関係】
  - 入力: currentState=AUTO_FAN, command=弱/中/強/停止
  - 出力: currentState=MANUAL, fanLevel=指定値
  - 入力: currentState=MANUAL, 温湿度が閾値未満
  - 出力: MANUALを維持（状態変更なし）

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 温湿度の取得値が妥当か | `readEnvironment()` | `Serial.println(String(temperatureC) + "," + String(humidityPct));` |
| 2 | 人感ゲート判定が想定通りか | `checkMotionGate()` | `Serial.println(motionDetected);` |
| 3 | 状態遷移が正しく起きているか | `updateState()` | `Serial.println(currentState);` |
| 4 | IR未割当入力が無視されるか | `readRemoteCommand()` | `Serial.println("unknown IR");` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力・コマンド変換テスト（`readRemoteCommand()` / `mapRawCodeToCommand()`）

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readRemoteCommand() | 電源ボタンを1回押す | `CMD_POWER_TOGGLE` を返す | | [ ] |
| 2 | readRemoteCommand() | 1ボタンを1回押す | `CMD_SHOW_TEMP` を返す | | [ ] |
| 3 | readRemoteCommand() | 2ボタンを1回押す | `CMD_SHOW_HUM` を返す | | [ ] |
| 4 | readRemoteCommand() | 3ボタンを1回押す | `CMD_DISPLAY_OFF` を返す | | [ ] |
| 5 | readRemoteCommand() | 4/5/6ボタンを押す | それぞれ `CMD_FAN_LOW` / `CMD_FAN_STOP` / `CMD_FAN_HIGH` を返す | | [ ] |
| 6 | readRemoteCommand() | 未割当ボタンを押す | `CMD_NONE` を返す（状態変化なし） | | [ ] |
| 7 | readRemoteCommand() | 同一ボタンを50ms以内で連打する | 1回分のみ受理（デバウンス） | | [ ] |

### 5-2. センサー・条件判定テスト（`readEnvironment()` / `checkMotionGate()` / `evaluateAutoFanCondition()`）

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readEnvironment() | 室温環境で30秒動作 | 温湿度が約1秒周期で更新される | | [ ] |
| 2 | readEnvironment() | DHT11を一時切断する | `sensorErrorCount` が増加する | | [ ] |
| 3 | checkMotionGate() | PIRに単発HIGHを入力する | `motionDetected` は true にならない | | [ ] |
| 4 | checkMotionGate() | PIRを2回連続HIGHにする | `motionDetected` が true になる | | [ ] |
| 5 | evaluateAutoFanCondition() | `temperatureC=25.0, humidityPct=59.0, motionDetected=true` | true（温度しきい値境界） | | [ ] |
| 6 | evaluateAutoFanCondition() | `temperatureC=24.9, humidityPct=60.0, motionDetected=true` | true（湿度しきい値境界） | | [ ] |
| 7 | evaluateAutoFanCondition() | `temperatureC=26.0, humidityPct=65.0, motionDetected=false` | false（人感ゲートで抑止） | | [ ] |
| 8 | evaluateAutoFanCondition() | `sensorErrorCount>=3` | false（異常時は自動送風しない） | | [ ] |

### 5-3. 状態遷移・リモコン反映テスト（`applyRemoteCommand()` / `updateStateByDesign()`）

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | applyRemoteCommand() | 電源ON中に電源ボタンを押す | `powerOn=false`, `currentState=POWER_OFF`, `fanLevel=0` | | [ ] |
| 2 | applyRemoteCommand() | 電源OFF中に電源ボタンを押す | `powerOn=true`, `currentState=AUTO_MONITOR` へ復帰 | | [ ] |
| 3 | applyRemoteCommand() | 電源OFF中に1〜6ボタンを押す | 電源ボタン以外は無視（状態変化なし） | | [ ] |
| 4 | applyRemoteCommand() | 1/2/3ボタンを押す | 表示モードが温度/湿度/非表示へ切替 | | [ ] |
| 5 | applyRemoteCommand() | 4/5/6ボタンを押す | `fanLevel=1/0/3` になり `currentState=MANUAL` へ遷移 | | [ ] |
| 6 | updateStateByDesign() | `AUTO_MONITOR` で自動条件成立 | `AUTO_FAN` へ遷移 | | [ ] |
| 7 | updateStateByDesign() | `AUTO_FAN` で条件不成立 | `AUTO_MONITOR` へ戻り `fanLevel=0` | | [ ] |
| 8 | updateStateByDesign() | `MANUAL` 中に自動条件不成立 | `MANUAL` を維持（電源OFFまで固定） | | [ ] |
| 9 | updateStateByDesign() | `sensorErrorCount>=3` | 安全側で `fanLevel=0` | | [ ] |

### 5-4. 出力・タイミングテスト（`updateDisplay()` / `applyFanOutput()`）

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateDisplay() | `displayMode=DISPLAY_TEMP` | 温度値を7セグ表示 | | [ ] |
| 2 | updateDisplay() | `displayMode=DISPLAY_HUM` | 湿度値を7セグ表示 | | [ ] |
| 3 | updateDisplay() | `displayMode=DISPLAY_OFF` または `powerOn=false` | 7セグ消灯 | | [ ] |
| 4 | updateDisplay() | `sensorErrorCount>=3` | `Err` 表示を優先 | | [ ] |
| 5 | applyFanOutput(0) | level=0 | PWM=0（停止） | | [ ] |
| 6 | applyFanOutput(1) | level=1 | PWM=85（弱） | | [ ] |
| 7 | applyFanOutput(2) | level=2 | PWM=170（中） | | [ ] |
| 8 | applyFanOutput(3) | level=3を10分以上維持 | 自動でlevel=2へ降格 | | [ ] |
| 9 | applyFanOutput(255) | 範囲外level | PWM=0（安全側停止） | | [ ] |
| 10 | loop全体 | IR連続操作しながら表示更新を観察 | 非ブロッキングで操作遅延がない | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- 実装時にバグ化しやすいのは、`now`（millis値）の受け渡し漏れ、`powerOn`と`currentState`の不整合、IR入力デバウンス記述の曖昧さ。
- `readEnvironment()`と`updateDisplay()`は`now`を引数にすると依存が明確になり、グローバル依存バグを防ぎやすい。
- 電源ON/OFFは`currentState`だけでなく`powerOn`も同時更新しないと、loop側条件分岐と矛盾する可能性がある。
- 強運転降格やセンサー異常時の安全側制御は、境界条件の仕様を明記しないと実装差異が出やすい。

**対応した内容：**
- `loop()`の処理フローを修正し、`readEnvironment(now)`・`updateDisplay(now)`として時刻を引き回す設計に変更。
- `handlePower()`に`powerOn`更新を追加し、状態遷移と電源フラグの整合を明確化。
- 3-1のデバウンス説明をIR入力向けに修正し、対象入力と確定条件を一致させた。
- 安全側制御（異常時停止・強運転時間制限）をテストで担保する前提を追加。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 既存テストは主要機能を押さえているが、しきい値境界（温度25℃/湿度60%）、PIR連続判定、範囲外入力、電源復帰、手動固定維持の検証が不足。
- `evaluateAutoFanCondition()`は境界値テストを追加しないと、`>=`と`>`の実装違いを見逃す可能性がある。
- `applyFanOutput()`は範囲外level入力時の安全動作（停止扱い）をテストで固定すべき。
- `updateDisplay()`は通常表示だけでなくErr優先表示を検証対象に含める必要がある。

**対応した内容：**
- 5-1に境界値テスト（温度25.0、湿度60.0）とPIR連続HIGH判定テストを追加。
- 5-2にErr優先表示テストと範囲外level入力テストを追加。
- 5-3に手動固定動作テストと電源OFF/ON復帰テストを追加。
- 単体テストで比較演算子の違い、異常入力、安全側遷移を検出できる構成に拡張。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
