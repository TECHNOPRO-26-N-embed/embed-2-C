# 基本設計書 — 組込み開発実習

<!-- 作成者: 西本秀之介 / 最終更新: 2026-05-27 / グループ: C-2 -->

---

## 0. 要件との対応

| 要件 | 設計方針 |
|:--|:--|
| 任意IR入力で警報開始 | 待機状態で hasIrSignal(code, command) を判定 |
| FUNC/STOPで停止 | isCancelSignal(code, command) で停止判定 |
| 待機/警報の点滅周期 | millis()ベースで 1000ms / 200ms を切替 |
| 常時監視 | loop()で毎回 readIR() を実行 |

---

## 1. システム全体像

### 1-1. ブロック構成

```
[IRリモコン] -> [IR受信モジュール D3] -> [Arduino UNO]
                                         |- [LED D9]
                                         |- [BUZZER D8]
                                         \- [Serial Monitor 9600bps]
```

### 1-2. 状態遷移

```
[起動]
  -> [待機(STANDBY)] --(任意IR信号)--> [警報(ALARM)]
         ^                                  |
         |------(FUNC/STOP cmd=0x47)--------|
```

---

## 2. ソフトウェア構成

### 2-1. 主要定数

| 名称 | 値 | 用途 |
|:--|:--|:--|
| PIN_IR_RECEIVER | 3 | IR受信入力 |
| PIN_LED_RED | 9 | LED出力 |
| PIN_BUZZER | 8 | ブザー出力 |
| STANDBY_BLINK_INTERVAL_MS | 1000 | 待機点滅周期 |
| ALARM_BLINK_INTERVAL_MS | 200 | 警報点滅周期 |
| CODE_FUNC_STOP | 0xFFE21D | 停止キーraw基準 |
| CODE_FUNC_STOP_CMD | 0x47 | 停止キーcommand基準 |

### 2-2. 関数責務

| 関数 | 責務 |
|:--|:--|
| setup() | ピン初期化、IR初期化、シリアル開始、起動確認 |
| loop() | 受信判定、状態遷移、出力更新 |
| readIR() | raw/commandを取り込み、デバッグ表示 |
| handleSerialCommand() | シリアル強制開始(s)/停止(c) |
| updateOutput(state) | 状態に応じたLED/ブザー制御 |
| startAlarm() | 警報開始初期化 |
| stopAlarm() | 警報停止初期化 |
| showStandby() | 待機中のLED低速点滅 |
| hasIrSignal(code, command) | 入力有無判定 |
| isCancelSignal(code, command) | 停止キー判定 |

### 2-3. 停止判定仕様

- raw一致: code == cancelCode
- command一致: command == CODE_FUNC_STOP_CMD（0x47）
- 上記いずれかで停止を成立させる

---

## 3. ハードウェア設計

| 部品 | 接続ピン | 備考 |
|:--|:--|:--|
| IR受信モジュール | D3 | IRremote使用 |
| 赤色LED | D9 | 220Ω直列 |
| アクティブブザー | D8 | Active High想定 |

- D0/D1（Serial）は未使用
- 使用ピン競合なし

---

## 4. 異常時方針

| 異常ケース | 対応 |
|:--|:--|
| 想定外状態値 | stopAlarm() 実行後 STANDBY に戻す |
| 停止キー以外入力（警報中） | 警報継続しログ出力 |
| 受信なし | 現在状態の出力継続 |

---

## 5. 結合テスト仕様書

| No | 観点 | 手順 | 期待結果 |
|:--|:--|:--|:--|
| IT-01 | 起動確認 | 電源投入 | LEDが1秒点灯、起動ログ表示 |
| IT-02 | 警報開始 | 待機中に任意キー押下 | ALARMへ遷移、LED高速点滅、ブザーON |
| IT-03 | 停止（command） | 警報中にFUNC/STOP押下（cmd=0x47） | STANDBYへ遷移、LED/ブザー停止 |
| IT-04 | 停止（raw） | 警報中にraw=cancelCode入力 | STANDBYへ遷移 |
| IT-05 | 非停止キー | 警報中に停止以外キー押下 | Not CANCEL code表示、警報継続 |
| IT-06 | 待機点滅周期 | 待機中に観察 | 約1秒周期で点滅 |
| IT-07 | 警報点滅周期 | 警報中に観察 | 約0.2秒周期で点滅 |

---

*初版: 2026-05-22 / コード同期更新: 2026-05-27*