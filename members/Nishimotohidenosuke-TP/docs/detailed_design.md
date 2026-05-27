# 詳細設計書 — 組込み開発実習

<!-- 作成者: 西本秀之介 / 最終更新: 2026-05-27 / グループ: C-2 -->

---

## 0. 基本設計との接続

| 項目 | 内容 |
|:--|:--|
| 状態 | STANDBY / ALARM |
| 主要I/O | D3(IR), D9(LED), D8(BUZZER) |
| 制御方式 | millis()周期制御 |
| 停止条件 | raw一致 または command=0x47 |

---

## 1. グローバル設計

### 1-1. 定数

| 定数名 | 値 | 説明 |
|:--|:--|:--|
| STATE_STANDBY | 0 | 待機状態 |
| STATE_ALARM | 1 | 警報状態 |
| STANDBY_BLINK_INTERVAL_MS | 1000 | 待機点滅周期 |
| ALARM_BLINK_INTERVAL_MS | 200 | 警報点滅周期 |
| CODE_FUNC_STOP | 0xFFE21DUL | 停止キーraw |
| CODE_FUNC_STOP_CMD | 0x47 | 停止キーcommand |

### 1-2. 変数

| 変数名 | 型 | 用途 |
|:--|:--|:--|
| currentState | int | 現在状態 |
| lastMillis | unsigned long | 点滅周期基準時刻 |
| irCode | unsigned long | 受信rawコード |
| irCommand | uint8_t | 受信command |
| sosCode | unsigned long | 表示用SOSコード |
| cancelCode | unsigned long | 停止rawコード |
| ledState | bool | LED現在状態 |
| buzzerState | bool | ブザー現在状態 |
| errorFlag | bool | 想定外状態検知 |

---

## 2. 処理詳細

### 2-1. setup()

1. D3/D9/D8 のピンモード設定
2. IrReceiver.begin() 実行
3. sosCode / cancelCode 設定
4. Serial 9600bps 開始
5. 起動確認としてLEDを1秒点灯
6. lastMillis 初期化

### 2-2. loop()

1. handleSerialCommand() で強制操作受付
2. readIR() で irCode / irCommand 更新
3. STANDBY時
- hasIrSignal() true なら startAlarm() 実行し ALARM へ
- showStandby() 実行
4. ALARM時
- isCancelSignal() true なら stopAlarm() 実行し STANDBY へ
- それ以外で信号ありなら Not CANCEL code を表示
- 信号なしなら updateOutput(STATE_ALARM)
5. 想定外状態時
- errorFlag=true
- stopAlarm() 実行
- STANDBYへ復帰

### 2-3. readIR()

1. 毎回 irCommand=0 へ初期化
2. decode成功時、decodedRawData と command を取得
3. rawが0xFFFFFFFFはリピートとして無視
4. [IR] raw と [IR] cmd をシリアル出力
5. resume() 後に raw を返却
6. decode失敗時は0を返却

### 2-4. hasIrSignal(code, command)

- code != 0 または command != 0 で true
- raw=0 環境でも command 受信を有効化

### 2-5. isCancelSignal(code, command)

- code == cancelCode で true
- command == CODE_FUNC_STOP_CMD(0x47) で true
- 上記以外は false

### 2-6. updateOutput(state)

- STANDBY: ブザーOFF、1000ms周期でLEDトグル
- ALARM: ブザーON、200ms周期でLEDトグル

### 2-7. startAlarm() / stopAlarm()

- startAlarm(): LED/ブザーON初期化、lastMillis更新
- stopAlarm(): LED/ブザーOFF初期化、lastMillis更新

### 2-8. showStandby()

- 1000ms周期でLEDトグル
- 待機中は常にブザーOFF維持

---

## 3. デバッグ出力仕様

| ログ | 出力タイミング |
|:--|:--|
| [INFO] System boot | 起動時 |
| [INFO] SOS code / CANCEL code | 起動時 |
| [IR] raw / [IR] cmd | IR受信時 |
| [STATE] -> ALARM/STANDBY | 状態遷移時 |
| [INFO] Not CANCEL code | 警報中に非停止キー受信 |
| [CMD] Force alarm ON/OFF | シリアル強制操作時 |

---

## 4. 単体テスト仕様書

### 4-1. 入力・判定系

| No | 対象 | 条件/入力 | 期待結果 |
|:--|:--|:--|:--|
| UT-01 | hasIrSignal | code=0, command=0 | false |
| UT-02 | hasIrSignal | code=0, command=0x47 | true |
| UT-03 | isCancelSignal | code=cancelCode | true |
| UT-04 | isCancelSignal | code=0, command=0x47 | true |
| UT-05 | isCancelSignal | code=0, command=0x11 | false |
| UT-06 | readIR | リピートコード(0xFFFFFFFF)受信 | 0を返す |

### 4-2. 状態遷移系

| No | 初期状態 | 入力 | 期待結果 |
|:--|:--|:--|:--|
| UT-07 | STANDBY | 任意IR受信(hasIrSignal=true) | ALARMへ遷移 |
| UT-08 | ALARM | 停止キー受信(isCancelSignal=true) | STANDBYへ遷移 |
| UT-09 | ALARM | 非停止キー受信 | ALARM維持 |
| UT-10 | 不正状態値 | loop()実行 | STANDBYへ復帰、errorFlag=true |

### 4-3. 出力制御系

| No | 対象 | 条件 | 期待結果 |
|:--|:--|:--|:--|
| UT-11 | updateOutput | state=STANDBY | ブザーOFF、1秒周期点滅 |
| UT-12 | updateOutput | state=ALARM | ブザーON、0.2秒周期点滅 |
| UT-13 | stopAlarm | 呼び出し | LED/ブザーOFF |

---

## 5. 実機確認項目

| No | 項目 | 判定基準 |
|:--|:--|:--|
| HW-01 | FUNC/STOP停止 | [IR] cmd=0x47 受信時に停止 |
| HW-02 | raw=0対応 | [IR] raw=0x0 でも command により停止可能 |
| HW-03 | 連続操作 | 開始/停止を連続10回で破綻しない |

---

*初版: 2026-05-25 / コード同期更新: 2026-05-27*