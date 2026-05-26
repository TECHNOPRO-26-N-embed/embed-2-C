# 詳細設計書 — 組込み開発実習

<!-- 作成者: 小松原一真 / 日付: 2026-05-25 / グループ: 2-C -->

> [!NOTE]
> 本システムは「異常時は安全側（施錠・出力停止）」に倒す設計とし、復帰条件も明確に定義する。ピン・電源要件（サーボは外部5V推奨、GND共通化、D0/D1未使用）も厳守する。タイミング判定・状態遷移は境界値テスト（例: 149/150/151ms, 29999/30000/30001ms, 179999/180000/180001ms）で検証し、誤判定やオーバーフローが起きないことを確認する。

> **このドキュメントの目的**
> 基本設計書（design_unit1.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | design_unit1.md から転記 |
|:--|:--|
| 作品タイトル | リモコン式デジタルロック |
| 状態の種類（1-2 状態遷移から） | 待機中、入力中、認証中、解錠中、エラー中、警告中（6種類） |
| 実装する関数の数（2-2 関数一覧から） | 10個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約42B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（design_unit1.md 3-1 から転記）
  PIN_IR_RECEIVER = 2   // IR受信モジュール OUT（D2、D0/D1未使用）
  PIN_TM1637_CLK  = 3   // 4桁7セグ CLK
  PIN_TM1637_DIO  = 4   // 4桁7セグ DIO
  PIN_BUZZER      = 8   // パッシブブザー
  PIN_SERVO       = 9   // サーボ SG90 信号（外部5V推奨、GNDはArduinoと共通化）
  PIN_LED_R       = 10  // RGB LED 赤（SPI未使用前提）
  PIN_LED_G       = 11  // RGB LED 緑
  PIN_LED_B       = 12  // RGB LED 青

【状態管理】（design_unit1.md 1-2 の状態名から転記）
  STATE_IDLE      : uint8_t = 0   // 待機中
  STATE_INPUT     : uint8_t = 1   // 入力中
  STATE_VERIFY    : uint8_t = 2   // 認証中
  STATE_UNLOCKED  : uint8_t = 3   // 解錠中
  STATE_ERROR     : uint8_t = 4   // エラー中
  STATE_ALERT     : uint8_t = 5   // 警告中
  currentState    : uint8_t = STATE_IDLE

【タイマー（millis()用）】（design_unit1.md 2-3 から転記）
  lastIrReceivedMs      : unsigned long = 0
  unlockStartMs         : unsigned long = 0
  lockUntilMs           : unsigned long = 0
  lastErrorBlinkMs      : unsigned long = 0
  last7SegRefreshMs     : unsigned long = 0
  lastLockCheckMs       : unsigned long = 0

【センサー・入力値】（design_unit1.md 2-1 から転記）
  inputBuffer         : char[5] = ""     // 4桁+終端
  inputLength         : uint8_t = 0       // 0〜4
  passcode            : char[5] = "1234" // 初期パスコード
  lastIrCode          : unsigned long = 0
  failedCount         : uint8_t = 0
  isPassChangeMode    : bool = false

【その他のフラグ・カウンター】
  LOCK_DURATION_MS       : const unsigned long = 30000   // 30秒
  AUTO_LOCK_DURATION_MS  : const unsigned long = 180000  // 3分
  IR_REPEAT_WINDOW_MS    : const unsigned long = 150     // 重複入力抑制
  LED_BLINK_INTERVAL_MS  : const unsigned long = 300     // エラー/警告点滅
  SEG_REFRESH_MS         : const unsigned long = 5       // 7セグ更新
  LOCK_CHECK_MS          : const unsigned long = 50      // 警告解除判定
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
   - PIN_IR_RECEIVER → INPUT
   - PIN_LED_R / PIN_LED_G / PIN_LED_B → OUTPUT
   - PIN_BUZZER → OUTPUT

2. ライブラリを初期化する
   - IR受信を有効化する
   - TM1637表示を初期化し、輝度を設定する
   - Servoを PIN_SERVO にアタッチする

3. 変数を初期化する
   - currentState = STATE_IDLE
   - inputBuffer を空文字にし、inputLength = 0
   - passcode = "1234"
   - failedCount = 0、isPassChangeMode = false
   - タイマー変数を0で初期化する

4. 初期出力を設定する
   - setLockState(false) で施錠位置へ移動
   - LED消灯、ブザー停止
   - 7セグに待機表示を出す（例: ----）

5. 起動確認（任意）
   - 緑LED短時間点灯と短音を出し、待機状態へ入る
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - now = millis() を取得する
  - key = readIrInput() を呼ぶ（入力なしは -1）
  - updateOutputByState(currentState) を呼ぶ
  - now - lastLockCheckMs >= LOCK_CHECK_MS のときだけ警告解除判定を行い、判定後は lastLockCheckMs = now を更新する

＜currentState が STATE_IDLE（待機中）のとき＞
  - 数字キー入力なら inputBuffer に追加して STATE_INPUT へ遷移
  - 変更モードキーは無視する（解錠中のみ受け付ける）

＜currentState が STATE_INPUT（入力中）のとき＞
  - 取消キーなら inputBuffer をクリアして STATE_IDLE へ戻る
  - 4桁入力完了で STATE_VERIFY へ遷移

＜currentState が STATE_VERIFY（認証中）のとき＞
  - verifyPasscode(inputBuffer, passcode) を実行
  - 一致時: failedCount = 0、setLockState(true)、STATE_UNLOCKED へ
  - 不一致時: failedCount++、STATE_ERROR へ
  - failedCount >= 3 なら lockUntilMs = now + LOCK_DURATION_MS として STATE_ALERT へ

＜currentState が STATE_UNLOCKED（解錠中）のとき＞
  - 施錠キー入力で setLockState(false) を実行し STATE_IDLE へ
  - now - unlockStartMs >= AUTO_LOCK_DURATION_MS なら自動施錠して STATE_IDLE へ
  - 変更モードキー入力で isPassChangeMode = true
  - isPassChangeMode が true の間は handlePasscodeChangeMode(key, passcode) を実行

＜currentState が STATE_ERROR（エラー中）のとき＞
  - エラー表示と短音通知を行う
  - inputBuffer をクリアして STATE_IDLE へ戻る

＜currentState が STATE_ALERT（警告中）のとき＞
  - 入力を受け付けない
  - 警告表示を継続する
  - ロック解除判定タイミングで now >= lockUntilMs なら failedCount を0にして STATE_IDLE へ戻る
```

---

### `readIrInput()` — IR受信値を取得しキー入力へ変換する

**design_unit1.md 2-2 との対応：** C01/F01（共通IR入力読取・必須機能①）

**引数：** なし

**戻り値：** int（数字キー0-9、制御キーコード、入力なしは-1）

```
【処理の流れ】
1. IR受信データの有無を確認し、なければ -1 を返す。
2. 受信コードをキー値へ変換する（IRコード表を参照）。
3. 同一コードが IR_REPEAT_WINDOW_MS 以内なら重複入力として無視する。
4. lastIrCode と lastIrReceivedMs を更新してキー値を返す。

【エラー・異常ケース】
- 未定義コード受信時: -1 を返して無視する。
```

---

### `updateOutputByState(state)` — 状態に応じた表示・通知を更新する

**design_unit1.md 2-2 との対応：** C02（共通 状態別出力更新）

**引数：** `state`（uint8_t）: 現在状態

**戻り値：** なし（void）

```
【処理の流れ】
1. updateLedIndicator(state) を呼ぶ。
2. playBuzzerPattern(state, failedCount) を呼ぶ。
3. 状態に応じた表示文字列を update7SegDisplay() に渡す。
   - 入力中: inputBuffer
   - 解錠中: OPEN
   - エラー中: ERR
   - 警告中: LOCK

【エラー・異常ケース】
- state が定義外: LED消灯、ブザー停止、待機表示に戻す。
```

---

### `verifyPasscode(input, passcode)` — 入力4桁と登録パスコードを照合する

**design_unit1.md 2-2 との対応：** F02（必須機能② パスコード照合）

**引数：**
- `input`（const char*）: 入力文字列
- `passcode`（const char*）: 登録済みパスコード

**戻り値：** bool（一致=true、不一致=false）

```
【処理の流れ】
1. input が4桁そろっているか確認する。
2. input と passcode を先頭から4文字比較する。
3. 全桁一致なら true、1文字でも不一致なら false を返す。

【エラー・異常ケース】
- NULLポインタや桁不足時: false を返す。
```

---

### `setLockState(unlock)` — サーボを施錠/解錠位置に移動する

**design_unit1.md 2-2 との対応：** F03（必須機能③ 施錠・解錠制御）

**引数：** `unlock`（bool）: true=解錠、false=施錠

**戻り値：** なし（void）

```
【処理の流れ】
1. unlock が true ならサーボを解錠角へ移動する。
2. unlock が false ならサーボを施錠角へ移動する。
3. 解錠時は unlockStartMs = millis() を記録する。
4. 状態遷移に備えて inputBuffer と inputLength を初期化する。

【エラー・異常ケース】
- サーボ制御失敗時: 安全側（施錠位置）を優先し、警告表示を出す。
```

---

### `updateLedIndicator(state)` — 状態に応じたRGB LED表示を行う

**design_unit1.md 2-2 との対応：** A01（追加機能① 状態表示）

**引数：** `state`（uint8_t）: 現在状態

**戻り値：** なし（void）

```
【処理の流れ】
1. state ごとにLED表示を切り替える。
   - 待機中: 消灯
   - 入力中: 待機色（例: 青）
   - 認証中: 一時点灯
   - 解錠中: 緑点灯
2. エラー中/警告中は LED_BLINK_INTERVAL_MS ごとに赤LEDを点滅させる。
3. 点滅更新時は lastErrorBlinkMs を更新する。

【エラー・異常ケース】
- 定義外state: LED消灯。
```

---

### `playBuzzerPattern(state, failedCount)` — 状態に応じた音パターンを出す

**design_unit1.md 2-2 との対応：** A02（追加機能② 通知）

**引数：**
- `state`（uint8_t）: 現在状態
- `failedCount`（uint8_t）: 認証失敗回数

**戻り値：** なし（void）

```
【処理の流れ】
1. 待機/入力/認証/解錠中はブザー停止。
2. エラー中は短音を1回鳴らす。
3. 警告中は断続音を繰り返す。
4. 音のON/OFF切替は millis() で行い、delay()は使わない。

【エラー・異常ケース】
- failedCount が異常値でも state 優先で安全側動作（停止または短音）にする。
```
```

---

### `update7SegDisplay(text)` — 4桁7セグ表示を更新する

**design_unit1.md 2-2 との対応：** A03（追加機能③ 4桁7セグ表示）

**引数：** `text`（const char*）: 表示文字列（最大4文字）

**戻り値：** なし（void）

```
【処理の流れ】
1. now - last7SegRefreshMs >= SEG_REFRESH_MS のときだけ更新する。
2. text を4文字に整形（不足分は空白埋め、超過分は切り捨て）。
3. TM1637へ表示データを書き込む。
4. last7SegRefreshMs = now を更新する。

【エラー・異常ケース】
- text が NULL のときは "----" を表示する。
```

---

### `handlePasscodeChangeMode(key, passcode)` — パスワード変更モードを処理する

**design_unit1.md 2-2 との対応：** A04（追加機能④ パスワード変更）

**引数：**
- `key`（int）: 最新キー入力
- `passcode`（char*）: 更新対象パスコード

**戻り値：** bool（更新あり=true、更新なし=false）

```
【処理の流れ】
1. 変更モード中は新しい4桁入力を受け付ける。
2. 取消キーなら変更を破棄して isPassChangeMode = false、falseを返す。
3. 確定キーかつ4桁入力済みなら passcode を新値に更新する。
4. 更新後は入力バッファをクリアして isPassChangeMode = false、trueを返す。

【エラー・異常ケース】
- 4桁未満で確定: 更新しない。
- passcode が NULL: 更新せず false を返す。
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  本システムは物理ボタンを使わないため、機械式チャタリング対策は不要。
  代わりにIRリモコンの長押し/反射による重複受信を抑制する。

【処理の流れ】
  1. readIrInput() でIRコードを受信する
  2. 受信コードをキー値へ変換する
  3. lastIrCode と比較し、同一コードか判定する
  4. 同一コードかつ (now - lastIrReceivedMs) < IR_REPEAT_WINDOW_MS（150ms）なら無視する
  5. 有効入力のみ採用し、lastIrCode と lastIrReceivedMs を更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastIrCode          : unsigned long        // 前回受信したIRコード
  lastIrReceivedMs    : unsigned long        // 前回受信時刻
  IR_REPEAT_WINDOW_MS : const unsigned long = 150  // 重複入力抑制時間（ms）
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。
  各タイミング判定・状態遷移は境界値テスト（例: 149/150/151ms, 29999/30000/30001ms, 179999/180000/180001ms）で検証し、誤判定やオーバーフローが起きないことを確認する。

【処理の流れ（本システム）】
  1. now = millis() を取得する
  2. IR重複判定: now - lastIrReceivedMs と IR_REPEAT_WINDOW_MS を比較する
  3. LED点滅: now - lastErrorBlinkMs >= LED_BLINK_INTERVAL_MS（300ms）で点滅更新
  4. 7セグ更新: now - last7SegRefreshMs >= SEG_REFRESH_MS（5ms）で表示更新
  5. ロック解除判定: now - lastLockCheckMs >= LOCK_CHECK_MS（50ms）で lockUntilMs 到達を確認
  6. 自動施錠判定: now - unlockStartMs >= AUTO_LOCK_DURATION_MS（180000ms）で施錠
  7. 各処理実行後に対応する lastXxxMs を now で更新する

【自分のシステムで millis() を使う処理】
  - IR信号受信監視（10ms程度）
  - IR重複入力の判定窓（150ms）
  - 4桁7セグメント表示更新（5ms）
  - エラー/警告時LED点滅（300ms）
  - ロック継続時間管理（30000ms）
  - ロック解除判定（50ms）
  - 解錠後の自動施錠（180000ms）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 変更モードキーを解錠中に受け付け、isPassChangeMode = true にする。
2. 変更モード中は新しい4桁入力を一時バッファへ格納する。
3. 取消キーなら変更を破棄して通常解錠状態へ戻す。
4. 確定キーかつ4桁入力済みなら passcode を更新する。
5. 更新後は inputBuffer をクリアし、isPassChangeMode = false に戻す。

【入力値と出力値の関係】
  - 入力: key（数字/確定/取消/変更モード）
  - 内部状態: isPassChangeMode, inputBuffer, inputLength
  - 出力: passcode 更新結果（bool）, 7セグ表示（入力値/完了表示）
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | IR受信コードとキー変換が正しいか | `readIrInput()` | `Serial.println(key);` |
| 2 | 状態遷移が設計どおりか | `loop()` | `Serial.println(currentState);` |
| 3 | 重複入力抑制が効いているか（150ms窓） | `readIrInput()` | `Serial.println("IR duplicate ignored");` |
| 4 | ロック解除/自動施錠タイマーが正しいか | `loop()` | `Serial.println(lockUntilMs - now);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readIrInput() | リモコンの数字キーを1回押す | 対応するキーコード（0-9）が返る | | [ ] |
| 2 | readIrInput() | 同じ数字キーを長押しする | IR_REPEAT_WINDOW_MS内の重複入力が無視される | | [ ] |
| 3 | verifyPasscode() | input="1234", passcode="1234" を与える | true が返る | | [ ] |
| 4 | verifyPasscode() | input="1235", passcode="1234" を与える | false が返る | | [ ] |
| 5 | handlePasscodeChangeMode() | 解錠中に変更モードで4桁入力後、確定キーを押す | passcode が新しい4桁へ更新される | | [ ] |
| 6 | readIrInput() | 同一コードを149ms/150ms/151ms間隔で入力する | 149msは無視、150ms以上は有効入力として扱う | | [ ] |
| 7 | readIrInput() | 未定義IRコードを受信する | `-1` を返し、入力バッファを更新しない | | [ ] |
| 8 | verifyPasscode() | input="123" を与える | false が返る | | [ ] |
| 9 | verifyPasscode() | input="12345" を与える | false が返る | | [ ] |
| 10 | verifyPasscode() | input="" または NULL を与える | false が返る | | [ ] |
| 11 | handlePasscodeChangeMode() | 変更モード中に取消キーを押す | passcode は更新されず、変更モードを終了する | | [ ] |
| 12 | handlePasscodeChangeMode() | 3桁入力で確定キーを押す | 更新せず、既存passcodeを維持する | | [ ] |
| 13 | handlePasscodeChangeMode() | 5桁目を入力する | 5桁目は無視され、4桁上限を維持する | | [ ] |
| 14 | handlePasscodeChangeMode() | 待機中/入力中で変更モードキーを押す | 変更モードへ遷移しない | | [ ] |
| 15 | loop() | failedCount=2の状態で誤入力を1回行う | failedCount=3で警告状態へ遷移する | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateLedIndicator() | state=STATE_UNLOCKED を渡す | 緑LEDが点灯する | | [ ] |
| 2 | playBuzzerPattern() | state=STATE_ERROR を渡す | 短音のエラー通知が1回鳴る | | [ ] |
| 3 | update7SegDisplay() | text="LOCK" を渡す | 4桁7セグにLOCK表示が出る | | [ ] |
| 4 | update7SegDisplay() | text=NULL を渡す | "----" を表示する | | [ ] |
| 5 | updateOutputByState() | 定義外stateを渡す | LED消灯・ブザー停止・待機表示へ戻る | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | ロック解除判定タイミング | 3回誤入力で警告状態にし、30秒経過を待つ | LOCK_CHECK_MS周期で解除判定され、待機中へ復帰する | | [ ] |
| 2 | 自動施錠タイマー精度 | 正しい4桁で解錠し、施錠キーを押さずに待つ | 約180000ms（3分）後に自動施錠へ戻る | | [ ] |
| 3 | 7セグ更新の並行動作 | 入力中に連続で数字キーを押す | SEG_REFRESH_MS周期で表示更新され、取りこぼしがない | | [ ] |
| 4 | ロック解除境界時刻 | 警告開始後に29999ms/30000ms/30001msで状態確認する | 30000ms到達で待機中へ復帰し、前後で判定が一貫する | | [ ] |
| 5 | 自動施錠境界時刻 | 解錠後に179999ms/180000ms/180001msで状態確認する | 180000ms到達で自動施錠し、前後で判定が一貫する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**

- `millis()` を使う周期判定は、`now >= last + interval` ではなく `now - last >= interval` で統一しないとオーバーフロー時に誤動作しやすい。
- `lockUntilMs = now + LOCK_DURATION_MS` は長時間運転時に境界で誤判定リスクがあるため、解除判定も差分比較で実装する方が安全。
- `inputBuffer` は 4 桁上限・終端文字の維持を毎回保証しないと、桁あふれや比較不一致の原因になる。
- `readIrInput()` は未定義コードとリピートコード（長押し時の連続受信）を明確に無視しないと誤入力が増える。
- `STATE_ERROR` を即時復帰させる設計は、表示/通知時間が短すぎると利用者に見えない可能性があるため、最小表示時間を設けると安定する。
- パスワード変更は「解錠中のみ許可」「4桁未満確定を拒否」「取消で元に戻す」を厳密にしないとロジック破綻しやすい。

**対応した内容：**

- すべてのタイミング処理（重複入力、LED点滅、7セグ更新、ロック解除、自動施錠）を差分比較で実装する方針を明記した。
- 入力処理は `inputLength` を 0〜4 に制限し、`inputBuffer[4]` の終端を常に維持する実装ルールを徹底する。
- IRの未定義コード/重複コードは `-1` 扱いで破棄し、入力バッファを更新しない方針で統一する。
- エラー表示が視認できるよう、短音と表示を最低 1 サイクル以上出してから `STATE_IDLE` に戻す実装方針を採用する。
- パスワード変更は `STATE_UNLOCKED` 時のみ有効とし、4桁確定時のみ更新、取消時は変更破棄を行う条件を再確認した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**

- 現在の Section 5 は、主要関数（入力・出力・タイミング）の正常系を中心に検証できており、基本的な妥当性はある。
- ただし、境界値・異常系の網羅はまだ不足している。特に以下の追加が有効。
  - `readIrInput()` の境界: 重複判定窓の 149ms / 150ms / 151ms、未定義IRコード受信時の無視。
  - `verifyPasscode()` の境界: 4桁未満、5桁以上、空文字、NULL入力。
  - `handlePasscodeChangeMode()` の境界: 取消時の不更新、4桁未満確定拒否、5桁目入力拒否、解錠中以外での無効化。
  - 状態遷移の境界: `failedCount` が 2→3 で警告遷移、成功時に 0 へリセット。
  - タイマー境界: 30000ms と 180000ms の直前/直後で期待どおり遷移するか。
  - 出力異常系: `update7SegDisplay(NULL)`、定義外 state 入力時の安全側動作。

**対応した内容：**

- Section 5 に境界値・異常系テストを追記し、No.1〜No.25 で確認できるように更新した。
- 追加した追試項目は以下。
  - IR重複判定窓の境界値テスト（149/150/151ms）。
  - パスコード照合の桁数異常テスト（3桁/5桁/空文字/NULL）。
  - 変更モードの取消・未確定・桁あふれ・解錠中以外無効化テスト。
  - failedCount の警告遷移境界（2→3）テスト。
  - 30秒ロック解除と3分自動施錠の境界時刻テスト（直前/到達/直後）。
  - 定義外入力（未定義IR、NULL文字列、定義外state）での安全側動作テスト。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 待機中→入力中→認証中→解錠中の遷移はどれくらいの時間間隔になりますか？ | 西本 | ユーザーの入力速度によりますが、通常は「数字キー4桁入力（約2〜5秒）」→「認証処理（即時）」→「解錠動作（サーボ動作含めて0.5〜1秒）」となります。 |
| 2 | 数字入力の際、入力だけでなく1つ削除する操作はできますか | バイケン | 可能です。入力中（STATE_INPUT）に「取消キー」を押すと、inputBufferの末尾1桁を削除する処理を実装しています。4桁すべて消すまで繰り返し削除でき、バッファが空になった時点で待機状態（STATE_IDLE）に戻ります。|
| 3 | 電源を入れ直した場合、入力途中のデータはどうなりますか？ | 西本 電源を入れ直すと、すべての変数（inputBufferやfailedCountなど）は初期化され、入力途中のデータは消去されます。安全側（施錠状態）で再起動します。|

### 7-2. レビューを受けて変更した点

- 指摘No.1「状態遷移の時間間隔」について、実際の動作時間を明記しました。
- 指摘No.2「1桁削除操作」について、取消キーで1桁ずつ削除できる旨を明記しました。
- 指摘No.3「電源再投入時のデータ」について、全変数が初期化される旨を明記しました。

---
---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
