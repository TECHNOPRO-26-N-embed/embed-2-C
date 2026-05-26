# 詳細設計書 — 組込み開発実習

<!-- 作成者: バイケン / 日付: 2026-05-25 / グループ: 2-C -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で決めた「赤外線リモコンによるLED・ブザー制御システム」について、各処理をどのように実装するかを具体化する。
> 本詳細設計書では、関数ごとの処理内容、重要ロジック、デバッグ出力、単体テスト仕様を整理する。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | 赤外線リモコンによるLED・ブザー制御システム |
| 状態の種類（1-2 状態遷移から） | 0:待機中 / 1:LED通知中 / 2:ブザー通知中 / 3:停止中 |
| 実装する関数の数（2-2 関数一覧から） | 9個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約30B程度 |

---

## 1. グローバル変数・定数の設計

> 基本設計書 2-1 のデータ設計をもとに、実装時に使用する変数・定数を整理する。

```
【ピン定義】（basic_design.md 3-1 から転記）
  IR_PIN      : const int = 2   // 赤外線受信モジュール（入力）
  LED_PIN     : const int = 9   // LED（出力）
  BUZZER_PIN  : const int = 8   // ブザー（出力）

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState : int = 0
  // 0:待機中
  // 1:LED通知中
  // 2:ブザー通知中
  // 3:停止中

【赤外線リモコン入力】
  receivedCode : unsigned long = 0
  // 赤外線リモコンから受信した信号コードを保存する

【連続入力対策】
  lastReceiveMillis : unsigned long = 0
  // 前回リモコン信号を受信・処理した時刻を保存する

【LED制御】
  ledState : bool = false
  // false:消灯、true:点灯
  lastBlinkMillis : unsigned long = 0
  // LED点滅の前回切り替え時刻を保存する

【ブザー制御】
  buzzerState : bool = false
  // false:停止、true:鳴動

【リモコンコード定義】
  CODE_LED          : const unsigned long = 実機確認後に記入
  CODE_BUZZER       : const unsigned long = 実機確認後に記入
  CODE_STOP         : const unsigned long = 実機確認後に記入
  CODE_BLINK_SPEED  : const unsigned long = 実機確認後に記入（追加機能）
  CODE_WARNING      : const unsigned long = 実機確認後に記入（追加機能）

【タイミング定数】
  BLINK_INTERVAL_DEFAULT : const unsigned long = 500
  // LED点滅の標準間隔 500ms

  BLINK_INTERVAL_FAST : const unsigned long = 200
  // 追加機能：速い点滅用

  BLINK_INTERVAL_SLOW : const unsigned long = 800
  // 追加機能：遅い点滅用

  RECEIVE_INTERVAL : const unsigned long = 200
  // 同じボタン入力が短時間に連続処理されることを防ぐための間隔

  WARNING_BUZZER_INTERVAL : const unsigned long = 300
  // 警告モード時のブザーON/OFF切替間隔

【追加機能用】
  blinkInterval : unsigned long = 500
  // 現在使用するLED点滅周期を保存する

  lastBuzzerMillis : unsigned long = 0
  // 警告モード時のブザー切替時刻を保存する
```

### 1-1. 変数使用方針

| 変数名 | 使用する主な関数 | 使用目的 |
|:--|:--|:--|
| currentState | loop(), updateState(), stopAll() | 現在の状態を管理する |
| receivedCode | loop(), readRemote(), updateState() | 受信した赤外線コードを保持する |
| lastReceiveMillis | readRemote() | 連続入力を防ぐ |
| ledState | controlLed(), stopAll(), warningMode() | LEDのON/OFF状態を保持する |
| buzzerState | controlBuzzer(), stopAll(), warningMode() | ブザーのON/OFF状態を保持する |
| lastBlinkMillis | controlLed(), warningMode() | LED点滅周期を管理する |
| blinkInterval | controlLed(), updateBlinkSpeed() | LED点滅速度を変更する |
| lastBuzzerMillis | warningMode() | 警告モード時のブザー周期を管理する |

---

## 2. 各関数の詳細設計

### 2-0. 関数一覧

| 関数名 | 役割 | 引数 | 戻り値 |
|:--|:--|:--|:--|
| setup() | ピン設定、Serial通信、赤外線受信機能の初期化を行う | なし | なし |
| loop() | リモコン入力を確認し、現在の状態に応じてLED・ブザーを制御する | なし | なし |
| readRemote() | 赤外線リモコンの信号を読み取り、受信コードを返す | なし | unsigned long |
| updateState() | 受信したリモコンコードに応じて currentState を更新する | unsigned long code | なし |
| controlLed() | LED通知中にLEDの点灯・点滅を制御する | int state | なし |
| controlBuzzer() | ブザー通知中にブザーを鳴らす | int state | なし |
| stopAll() | LEDとブザーを停止し、待機状態に戻す | なし | なし |
| updateBlinkSpeed() | LEDの点滅間隔を変更する | unsigned long code | なし |
| warningMode() | LED点滅とブザーを同時に動作させる | なし | なし |

> ※ `updateBlinkSpeed()` と `warningMode()` は追加機能として扱い、基本機能である `readRemote()`、`updateState()`、`controlLed()`、`controlBuzzer()`、`stopAll()` の実装を優先する。

---

### `setup()` — 初期化処理

**basic_design.md 2-2 との対応：**  
ピンモード設定、赤外線受信機能の初期化、LED・ブザーOFFを行う。

**引数：** なし  
**戻り値：** なし

```text
【処理の流れ】
1. LED_PIN を OUTPUT に設定する。
2. BUZZER_PIN を OUTPUT に設定する。
3. IR_PIN を赤外線受信用として使用できる状態にする。
4. 赤外線受信ライブラリを初期化する。
5. Serial.begin(9600) を実行し、デバッグ出力を確認できるようにする。
6. LEDを消灯する。
7. ブザーを停止する。
8. ledState を false にする。
9. buzzerState を false にする。
10. currentState を 0（待機中）に設定する。
11. 起動確認用に Serial に "System Start" を表示する。

【エラー・異常ケース】
- 赤外線受信ができない場合は、IR_PIN の配線とライブラリ初期化を確認する。
- LEDまたはブザーが起動時にONになる場合は、setup()内で必ずOFFを出力する。
```

---

### `loop()` — メイン制御

**basic_design.md 2-2 との対応：**  
リモコン入力を確認し、状態に応じて出力を更新する。

**引数：** なし  
**戻り値：** なし

```text
【処理の流れ】

＜毎ループ実行すること＞
1. 現在時刻 now = millis() を取得する。
2. readRemote() を呼び出し、赤外線リモコンの受信コードを取得する。
3. readRemote() の戻り値が 0 以外の場合、receivedCode に保存する。
4. receivedCode を updateState(receivedCode) に渡し、状態を更新する。
5. currentState に応じて、LED・ブザー・停止処理を切り替える。

＜currentState が 0（待機中）のとき＞
1. LEDを消灯する。
2. ブザーを停止する。
3. ledState を false にする。
4. buzzerState を false にする。
5. リモコン入力を待つ。

＜currentState が 1（LED通知中）のとき＞
1. controlLed(currentState) を呼び出す。
2. LEDを点灯または点滅させる。
3. ブザーは基本的に停止する。
4. 停止ボタンを受信した場合は、updateState() により currentState を 3 に変更する。
5. ブザー操作ボタンを受信した場合は、currentState を 2 に変更する。

＜currentState が 2（ブザー通知中）のとき＞
1. controlBuzzer(currentState) を呼び出す。
2. ブザーを鳴らす。
3. 必要に応じてLEDは直前状態を維持、または消灯する。
4. 停止ボタンを受信した場合は、updateState() により currentState を 3 に変更する。
5. LED操作ボタンを受信した場合は、currentState を 1 に変更する。

＜currentState が 3（停止中）のとき＞
1. stopAll() を呼び出す。
2. LEDとブザーを両方停止する。
3. currentState の 0（待機中）への復帰は stopAll() 内で行う。
4. 次のリモコン入力を待つ。

＜追加機能を使用する場合＞
1. 点滅速度変更ボタンを受信した場合、updateBlinkSpeed(receivedCode) を呼び出す。
2. 警告モード用ボタンを受信した場合、warningMode() を呼び出す状態に切り替える。
```

---

### `readRemote()` — 赤外線リモコン入力受信

**basic_design.md 2-2 との対応：**  
リモコン信号を受信する。

**引数：** なし  
**戻り値：** `unsigned long`  
受信したリモコンコードを返す。受信していない場合、または連続入力として無視する場合は `0` を返す。

```text
【処理の流れ】
1. 赤外線受信モジュールが信号を受信しているか確認する。
2. 受信していない場合は 0 を返す。
3. 受信している場合、現在時刻 now = millis() を取得する。
4. now - lastReceiveMillis が RECEIVE_INTERVAL 未満の場合は、連続入力と判断し 0 を返す。
5. RECEIVE_INTERVAL 以上経過している場合、受信コードを取得する。
6. 取得したコードを receivedCode に保存する。
7. lastReceiveMillis に now を保存する。
8. Serial.println(receivedCode) で受信コードを表示する。
9. 次の信号を受信できるように赤外線受信処理を再開する。
10. receivedCode を戻り値として返す。

【エラー・異常ケース】
- 未登録のリモコンコードを受信した場合でも、この関数ではコードの取得だけを行う。
- 未登録コードかどうかの判断は updateState() で行う。
- 短時間に同じボタンが連続して処理されないよう、RECEIVE_INTERVAL を確認する。
```

---

### `updateState()` — 状態更新

**basic_design.md 2-2 との対応：**  
受信したリモコン信号に応じて現在状態を変更する。

**引数：** `code`（unsigned long）: 受信した赤外線リモコンコード  
**戻り値：** なし

```text
【処理の流れ】
1. code が 0 の場合は、何もせず処理を終了する。
2. code が CODE_LED と一致するか確認する。
3. 一致した場合、currentState を 1（LED通知中）に変更する。
4. code が CODE_BUZZER と一致するか確認する。
5. 一致した場合、currentState を 2（ブザー通知中）に変更する。
6. code が CODE_STOP と一致するか確認する。
7. 一致した場合、currentState を 3（停止中）に変更する。
8. code が CODE_BLINK_SPEED と一致する場合、追加機能として updateBlinkSpeed(code) を呼び出す。
9. code が CODE_WARNING と一致する場合、追加機能として warningMode() の実装対象とする。ただし、基本機能の状態遷移には含めない。
10. どのコードにも一致しない場合、currentState は変更しない。
11. 状態を変更した場合は、Serial に変更後の currentState を表示する。

【エラー・異常ケース】
- 未登録のリモコン信号を受信した場合は、誤動作防止のため状態を変更しない。
- 停止ボタンはどの状態からでも優先して currentState = 3 に変更する。
- 追加機能のコードが未設定の場合は、その処理を実行しない。
```

---

### `controlLed()` — LED制御

**basic_design.md 2-2 との対応：**  
受信したボタンに応じてLED点灯・点滅・消灯を行う。

**引数：** `state`（int）: 現在の状態  
**戻り値：** なし

```text
【処理の流れ】
1. state が 1（LED通知中）か確認する。
2. state が 1 でない場合は、LED制御を行わず処理を終了する。
3. 現在時刻 now = millis() を取得する。
4. now - lastBlinkMillis が blinkInterval 以上か確認する。
5. 条件を満たさない場合は何もしない。
6. 条件を満たした場合、ledState を反転する。
7. ledState が true の場合、LED_PIN に HIGH を出力する。
8. ledState が false の場合、LED_PIN に LOW を出力する。
9. lastBlinkMillis に now を保存する。

【エラー・異常ケース】
- blinkInterval が 0 にならないようにする。
- delay() は使用しない。
- 停止状態に入った場合は、この関数ではなく stopAll() でLEDを確実にOFFにする。
```

---

### `controlBuzzer()` — ブザー通知

**basic_design.md 2-2 との対応：**  
特定ボタンを押したときにブザーを鳴らす。

**引数：** `state`（int）: 現在の状態  
**戻り値：** なし

```text
【処理の流れ】
1. state が 2（ブザー通知中）か確認する。
2. state が 2 でない場合は、ブザー制御を行わず処理を終了する。
3. ブザーを鳴らすため、BUZZER_PIN に HIGH を出力する。
   または tone() を使用する場合は指定周波数で鳴らす。
4. buzzerState を true にする。
5. Serial に "Buzzer ON" を表示する。
6. 停止ボタンが押された場合は updateState() により currentState が 3 になり、stopAll() で停止する。

【エラー・異常ケース】
- ブザーが鳴り続けることを防ぐため、停止処理では必ず LOW 出力または noTone() を実行する。
- ブザーの種類により、digitalWrite() を使うか tone() を使うかを実装時に確認する。
```

---

### `stopAll()` — 停止処理

**basic_design.md 2-2 との対応：**  
LEDとブザーを停止する。

**引数：** なし  
**戻り値：** なし

```text
【処理の流れ】
1. LED_PIN に LOW を出力し、LEDを消灯する。
2. BUZZER_PIN に LOW を出力し、ブザーを停止する。
   tone() を使用している場合は noTone(BUZZER_PIN) を実行する。
3. ledState を false にする。
4. buzzerState を false にする。
5. receivedCode を 0 に戻す。
6. currentState を 0（待機中）に戻す。
7. Serial に "Stop All" を表示する。

【エラー・異常ケース】
- どの状態から呼ばれても、LEDとブザーが必ず停止するようにする。
- 停止処理後にLEDやブザーが再度ONにならないよう、currentState を待機中へ戻す。
```

---

### `updateBlinkSpeed()` — 点滅速度変更（追加機能）

**basic_design.md 2-2 との対応：**  
ボタンごとにLED点滅速度を変更する。

**引数：** `code`（unsigned long）: 受信した赤外線リモコンコード  
**戻り値：** なし

```text
【処理の流れ】
1. code が CODE_BLINK_SPEED と一致するか確認する。
2. 一致しない場合は何もせず処理を終了する。
3. 現在の blinkInterval を確認する。
4. blinkInterval が BLINK_INTERVAL_DEFAULT の場合、BLINK_INTERVAL_FAST に変更する。
5. blinkInterval が BLINK_INTERVAL_FAST の場合、BLINK_INTERVAL_SLOW に変更する。
6. blinkInterval が BLINK_INTERVAL_SLOW の場合、BLINK_INTERVAL_DEFAULT に戻す。
7. 変更後の blinkInterval を Serial に表示する。

【エラー・異常ケース】
- blinkInterval が想定外の値になった場合は、BLINK_INTERVAL_DEFAULT に戻す。
- 追加機能のため、実装時間が足りない場合は必須機能を優先する。
```

---

### `warningMode()` — 警告モード（追加機能）

**basic_design.md 2-2 との対応：**  
LED点滅とブザー音を同時に動作させる。

**引数：** なし  
**戻り値：** なし

```text
【処理の流れ】
1. 現在時刻 now = millis() を取得する。
2. now - lastBlinkMillis が blinkInterval 以上か確認する。
3. 条件を満たす場合、ledState を反転する。
4. ledState に応じて LED_PIN に HIGH または LOW を出力する。
5. lastBlinkMillis に now を保存する。
6. now - lastBuzzerMillis が WARNING_BUZZER_INTERVAL 以上か確認する。
7. 条件を満たす場合、buzzerState を反転する。
8. buzzerState が true の場合、ブザーを鳴らす。
9. buzzerState が false の場合、ブザーを停止する。
10. lastBuzzerMillis に now を保存する。
11. 停止ボタンが押された場合は、stopAll() によりLEDとブザーを停止する。

【エラー・異常ケース】
- delay() を使わず、LED点滅中でも停止ボタンを受け付けられるようにする。
- ブザーが鳴り続けないよう、停止処理では必ずブザーをOFFにする。
- 追加機能のため、必須機能が完成してから実装する。
```

---

## 3. 重要ロジックの詳細設計

---

### 3-1. 赤外線リモコン信号の判定

```text
【考え方】
赤外線リモコンの各ボタンには固有のコードがある。
受信したコードを updateState() で判定し、LED通知、ブザー通知、停止中の状態へ切り替える。
未登録コードの場合は誤動作防止のため、現在状態を維持する。

【処理の流れ】
1. loop() から readRemote() を呼び出す。
2. readRemote() で赤外線コードを取得する。
3. 取得できなかった場合は 0 を返す。
4. 取得できた場合は receivedCode に保存する。
5. updateState(receivedCode) を呼び出す。
6. CODE_LED の場合、currentState = 1 にする。
7. CODE_BUZZER の場合、currentState = 2 にする。
8. CODE_STOP の場合、currentState = 3 にする。
9. 未登録コードの場合、currentState を変更しない。

【入力値と出力値の関係】
CODE_LED    → LED通知中へ移行
CODE_BUZZER → ブザー通知中へ移行
CODE_STOP   → 停止中へ移行
未登録コード → 状態を変更しない
```

---

### 3-2. 連続入力対策

```text
【考え方】
赤外線リモコンでは、ボタンを1回押しただけでも同じ信号が短時間に複数回受信される場合がある。
そのため、前回受信時刻から一定時間以内の入力は無視する。

【処理の流れ】
1. 赤外線信号を受信する。
2. now = millis() を取得する。
3. now - lastReceiveMillis を計算する。
4. RECEIVE_INTERVAL 未満の場合は、連続入力として無視し 0 を返す。
5. RECEIVE_INTERVAL 以上の場合は、正式な入力として処理する。
6. lastReceiveMillis = now に更新する。

【必要な変数】
lastReceiveMillis : unsigned long
RECEIVE_INTERVAL : const unsigned long = 200
```

---

### 3-3. millis() を使ったタイマー管理

```text
【考え方】
delay() を使用すると、LED点滅やブザー動作中にリモコン入力を受け付けにくくなる。
そのため、LED点滅・警告モード・連続入力対策は millis() で管理する。

【処理の流れ：LED点滅】
1. now = millis() を取得する。
2. now - lastBlinkMillis >= blinkInterval か確認する。
3. 条件を満たす場合、ledState を反転する。
4. LED_PIN に HIGH または LOW を出力する。
5. lastBlinkMillis = now に更新する。
6. 条件を満たさない場合は何もしない。

【処理の流れ：ブザー周期制御】
1. now = millis() を取得する。
2. now - lastBuzzerMillis >= WARNING_BUZZER_INTERVAL か確認する。
3. 条件を満たす場合、buzzerState を反転する。
4. BUZZER_PIN を HIGH または LOW にする。
5. lastBuzzerMillis = now に更新する。
6. 条件を満たさない場合は何もしない。
```

---

### 3-4. 停止処理の優先

```text
【考え方】
LEDやブザーが意図せず動作した場合でも、停止ボタンで必ず出力を止められるようにする。
停止ボタンは、LED通知中・ブザー通知中・警告モード中のどの状態でも優先する。

【処理の流れ】
1. readRemote() で停止ボタンのコードを受信する。
2. updateState() で CODE_STOP と一致するか確認する。
3. 一致した場合、currentState = 3 にする。
4. loop() 内で currentState == 3 を確認する。
5. stopAll() を呼び出す。
6. LEDをOFFにする。
7. ブザーをOFFにする。
8. currentState = 0 に戻す。

【入力値と出力値の関係】
停止ボタン受信 → LED消灯、ブザー停止、待機中へ戻る
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 赤外線リモコンのコードが受信できているか | `readRemote()` | `Serial.println(receivedCode);` |
| 2 | 連続入力が無視されているか | `readRemote()` | `Serial.println("Ignored repeat input");` |
| 3 | 状態遷移が正しく起きているか | `updateState()` | `Serial.println(currentState);` |
| 4 | LED通知中に入っているか | `controlLed()` | `Serial.println("LED Mode");` |
| 5 | ブザー通知中に入っているか | `controlBuzzer()` | `Serial.println("Buzzer Mode");` |
| 6 | 停止処理が実行されているか | `stopAll()` | `Serial.println("Stop All");` |
| 7 | 点滅速度が変更されているか | `updateBlinkSpeed()` | `Serial.println(blinkInterval);` |
| 8 | 警告モードが動作しているか | `warningMode()` | `Serial.println("Warning Mode");` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> 各関数・部品が単体で正しく動くかを確認する。  
> 「実際の結果」は実装後に記入する。

---

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `readRemote()` | LED用ボタンを押す | LED用コードが取得される | シリアルモニタに `IR Code: 0xFF30CF` と表示された | [OK] |
| 2 | `readRemote()` | ブザー用ボタンを押す | ブザー用コードが取得される | シリアルモニタに `IR Code: 0xFF18E7` と表示された | [OK] |
| 3 | `readRemote()` | 停止ボタンを押す | 停止用コードが取得される | シリアルモニタに `IR Code: 0xFF7A85` と表示された | [OK] |
| 4 | `readRemote()` | 何も押さない | 0 が返る | IR Code は表示されず、状態も変化しなかった | [OK] |
| 5 | `readRemote()` | 同じボタンを短時間に連続で押す | RECEIVE_INTERVAL以内の入力は無視される | `Ignored repeat input` と表示され、連続入力が無視された | [OK] |

---

### 5-2. 状態更新テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `updateState()` | CODE_LED を渡す | currentState が 1 になる | `State: LED` と表示され、LED通知中に遷移した | [OK] |
| 2 | `updateState()` | CODE_BUZZER を渡す | currentState が 2 になる | `State: BUZZER` と表示され、ブザー通知中に遷移した | [OK] |
| 3 | `updateState()` | CODE_STOP を渡す | currentState が 3 になる | `State: STOP` と表示され、停止処理に遷移した | [OK] |
| 4 | `updateState()` | 未登録コードを渡す | currentState が変化しない | 未登録ボタンで `Unknown Code` と表示され、状態は変化しなかった | [OK] |
| 5 | `updateState()` | 0 を渡す | currentState が変化しない | 何も押さない場合、状態は変化しなかった | [OK] |

---

### 5-3. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `controlLed()` | state = 1 にする | LEDが点灯または点滅する | `LED Mode, ledState = 1 / 0` と表示され、LEDが点滅した | [OK] |
| 2 | `controlLed()` | state = 0 にする | LED制御を行わない | 待機中はLEDが消灯したままで、LED制御は行われなかった | [OK] |
| 3 | `controlBuzzer()` | state = 2 にする | ブザーが鳴る | `Buzzer Mode` と表示され、ブザーが鳴った | [OK] |
| 4 | `controlBuzzer()` | state = 0 にする | ブザー制御を行わない | 待機中はブザーが鳴らなかった | [OK] |
| 5 | `stopAll()` | LED動作中に呼び出す | LEDが消灯する | LED点滅中に停止ボタンを押すと `Stop All` と表示され、LEDが消灯した | [OK] |
| 6 | `stopAll()` | ブザー動作中に呼び出す | ブザーが停止する | ブザー鳴動中に停止ボタンを押すと `Stop All` と表示され、ブザーが停止した | [OK] |
| 7 | `stopAll()` | LED・ブザー両方動作中に呼び出す | 両方が停止し、待機中に戻る | 停止処理後、LEDとブザーがOFFになり、待機中に戻った | [OK] |

---

### 5-4. タイミング制御テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `controlLed()` | blinkInterval = 500 に設定する | 約500ms間隔でLEDが切り替わる | LEDが約500ms間隔で点滅した | [OK] |
| 2 | `controlLed()` | blinkInterval = 200 に設定する | 約200ms間隔でLEDが切り替わる | 点滅速度変更機能は未実装のため対象外 | [-] |
| 3 | `updateBlinkSpeed()` | CODE_BLINK_SPEED を渡す | blinkInterval が切り替わる | 追加機能は未実装のため対象外 | [-] |
| 4 | `warningMode()` | 警告モードを実行する | LED点滅とブザーが同時に動作する | 追加機能は未実装のため対象外 | [-] |
| 5 | `warningMode()` | 警告モード中に停止ボタンを押す | LEDとブザーが停止する | 追加機能は未実装のため対象外 | [-] |

---

### 5-5. 異常系テスト

| No | テスト対象 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 未登録信号 | 設計していないボタンを押す | 状態が変化せず、誤動作しない | `Unknown Code` と表示され、LED・ブザーは誤動作しなかった | [OK] |
| 2 | 停止処理 | LED通知中に停止ボタンを押す | LEDが消灯し、待機中に戻る | `State: STOP`、`Stop All` と表示され、LEDが消灯した | [OK] |
| 3 | 停止処理 | ブザー通知中に停止ボタンを押す | ブザーが停止し、待機中に戻る | `State: STOP`、`Stop All` と表示され、ブザーが停止した | [OK] |
| 4 | delay不使用確認 | LED点滅中に別ボタンを押す | 入力を受け付け、状態が切り替わる | LED点滅中にブザーボタンを押すと `State: BUZZER` と表示され、状態が切り替わった | [OK] |
| 5 | 配線確認 | LEDまたはブザーが動作しない | ピン番号・GND・抵抗を確認できる | D9のLED、D8のブザー、D2の赤外線受信モジュールで動作確認できた | [OK] |

---

### 5-6. 単体テストまとめ

シリアルモニタにより、各ボタンの受信コード、状態遷移、LED制御、ブザー制御、停止処理、未登録コード処理、連続入力対策の各分岐が実行されることを確認した。

なお、点滅速度変更および警告モードは追加機能のため、今回は未実装・対象外とした。

---

## 6. AIレビュー記録

> グループレビュー前に、詳細設計書の内容についてAIで確認した。

### Q1: 詳細設計書の内容確認

> この詳細設計書について、関数の役割、引数、戻り値、処理の流れに大きな不足や矛盾がないか確認してください。

**AIの回答（要約）：**

- 関数一覧に、各関数の役割・引数・戻り値が整理されている。
- `readRemote()`、`updateState()`、`controlLed()`、`controlBuzzer()`、`stopAll()` の流れは、基本機能として実装しやすい構成になっている。
- 停止処理は、どの状態からでもLEDとブザーを停止できるようにしておくとよい。
- `updateBlinkSpeed()` と `warningMode()` は追加機能として扱い、基本機能完成後に実装する方がよい。

**対応した内容：**

- 停止ボタンを受信した場合は、どの状態からでも `stopAll()` によりLEDとブザーを停止する方針にした。
- 追加機能である `updateBlinkSpeed()` と `warningMode()` は、基本機能完成後に実装する方針にした。

---

### Q2: 単体テスト仕様書の確認

> Section 5 の単体テスト仕様書について、基本機能の確認項目として不足がないか確認してください。

**AIの回答（要約）：**

- 入力、状態更新、出力、停止処理のテスト項目が分かれており、基本機能の確認としては問題ない。
- 未登録のリモコン信号を受信した場合に、状態が変化しないことを確認するとよい。
- 停止ボタンでLEDとブザーが停止することを確認するとよい。

**対応した内容：**

- 未登録信号を受信した場合、状態が変化しないことを異常系テストに入れた。
- LED通知中、ブザー通知中に停止ボタンを押した場合のテストを入れた。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 警告モードの実装方針や注意事項について |小松原さん  |追加機能として設計に反映し、余裕があれば実装予定です  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

- 警告モード（LED点滅＋ブザー同時動作）の実装方針や注意事項を設計書の該当箇所に追記しました。
- 最終的には必須機能優先したため

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
