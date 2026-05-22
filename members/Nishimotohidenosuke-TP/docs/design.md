# 詳細設計書 — 組込み開発実習

<!-- 作成者: 西本秀之介 / 日付: 2026-05-22 / グループ: C-2 -->

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
| 作品タイトル | 高齢者用見守りSOS緊急通知システム |
| 状態の種類（1-2 状態遷移から） | 2状態（待機 / 警報） |
| 実装する関数の数（2-2 関数一覧から） | 7個（`setup()`, `loop()`, `readIRCode()`, `updateOutput()`, `handleAlert()`, `resetOutputs()`, `validateIRCode()`） |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 暫定 26B（`currentState` 2B + `lastMillisAlertBlink` 4B + `lastMillisBuzzer` 4B + `lastMillisStandby` 4B + `receivedCode` 4B + `hasNewCode` 1B + `lastAcceptedIrMs` 4B + `buzzerOn` 1B + `ignoredCodeCount` 2B） |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_IR_RECV      : const int = 2    // 赤外線受信モジュール（IR receiver）
  PIN_LED_RED      : const int = 9    // 赤LED（緊急時に高速点滅）
  PIN_LED_STANDBY  : const int = 10   // 待機表示LED（任意機能）
  PIN_BUZZER       : const int = 11   // アクティブブザー

【状態管理】（basic_design.md 1-2 の状態名から転記）
  STATE_STANDBY : const int = 0
  STATE_ALERT   : const int = 1
  currentState  : int = STATE_STANDBY

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastMillisAlertBlink : unsigned long = 0   // 警報中の赤LED点滅用
  lastMillisBuzzer     : unsigned long = 0   // 警報中のブザー制御用
  lastMillisStandby    : unsigned long = 0   // 待機表示LED点滅用

【センサー・入力値】（basic_design.md 2-1 から転記）
  receivedCode : unsigned long = 0          // 受信したIRコード
  hasNewCode   : bool = false               // 新規コード受信フラグ

【その他のフラグ・カウンター】
  SOS_CODE          : const unsigned long = 0x00000000  // 事前計測したSOSボタンコードに置換
  CANCEL_CODE       : const unsigned long = 0x00000000  // 事前計測した解除ボタンコードに置換
  ALERT_BLINK_MS    : const unsigned long = 100         // 赤LED高速点滅周期
  STANDBY_BLINK_MS  : const unsigned long = 1000        // 待機表示点滅周期
  BUZZER_TOGGLE_MS  : const unsigned long = 200         // ブザーON/OFF切替周期
  IR_GUARD_MS       : const unsigned long = 200         // 連続同一受信の誤作動防止
  lastAcceptedIrMs  : unsigned long = 0                 // 直近で有効受信した時刻
  buzzerOn          : bool = false                      // ブザー出力状態
  ignoredCodeCount  : unsigned int = 0                 // 未登録コード無視回数（デバッグ用）
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

**basic_design.md 2-2 との対応：** 初期化処理

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_IR_RECV → INPUT
   - PIN_LED_RED, PIN_LED_STANDBY → OUTPUT
   - PIN_BUZZER → OUTPUT

2. 必要なライブラリを初期化する
   - 赤外線受信ライブラリの初期化

3. デバッグ用に Serial 通信を開始する
   - Serial.begin(9600)

4. 起動確認として、緑LEDを1秒間点灯させる
   - PIN_LED_STANDBY を HIGH → delay(1000) → LOW
```

---

### `loop()` — メインループ

**basic_design.md 2-2 との対応：** メイン処理

**引数：** なし

**戻り値：** void

```
【処理の流れ】

＜毎ループ実行すること＞
1. 赤外線コードを読み取る（readIRCode() を呼び出す）
2. 現在時刻を取得する（millis()）
3. hasNewCode が true の場合のみ、receivedCode を判定対象として処理する
4. 判定処理が終わったら hasNewCode を false に戻し、同一コードの再処理を防ぐ

＜currentState が STATE_STANDBY のとき＞
1. 新しいコードが受信され、SOS_CODE と一致した場合 → currentState = STATE_ALERT
2. 待機表示LEDを点滅させる（updateOutput() を呼び出す）

＜currentState が STATE_ALERT のとき＞
1. 赤LEDを点滅させる（updateOutput() を呼び出す）
2. ブザーを鳴らす（updateOutput() を呼び出す）
3. CANCEL_CODE を受信した場合 → currentState = STATE_STANDBY
```

---

### `readIRCode()` — 赤外線コードの読み取り

**basic_design.md 2-2 との対応：** センサー入力処理

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. 赤外線受信モジュールからコードを取得する
2. 取得したコードが有効で、IR_GUARD_MS を超えている場合:
   - receivedCode に保存
   - hasNewCode を true に設定
   - lastAcceptedIrMs を更新
3. 無効なコードの場合:
   - ignoredCodeCount をインクリメント
```

---

### `updateOutput()` — 出力の更新

**basic_design.md 2-2 との対応：** 出力制御

**引数：** なし

**戻り値：** void

```
【処理の流れ】
＜currentState に応じた処理＞

＜STATE_STANDBY＞
1. millis() を使い、STANDBY_BLINK_MS ごとに PIN_LED_STANDBY を点滅させる

＜STATE_ALERT＞
1. millis() を使い、ALERT_BLINK_MS ごとに PIN_LED_RED を点滅させる
2. millis() を使い、BUZZER_TOGGLE_MS ごとに PIN_BUZZER を ON/OFF 切り替え
```

---

### `handleAlert()` — 警報処理

**basic_design.md 2-2 との対応：** 警報状態の処理

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. 赤LEDを点滅させる（updateOutput() を呼び出す）
2. ブザーを鳴らす（updateOutput() を呼び出す）
3. CANCEL_CODE を受信した場合:
   - currentState を STATE_STANDBY に変更
   - 出力をリセット
```

---

### `resetOutputs()` — 出力のリセット

**basic_design.md 2-2 との対応：** 出力初期化

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. PIN_LED_RED を LOW に設定する（赤LEDを消灯）
2. PIN_LED_STANDBY を LOW に設定する（待機LEDを消灯）
3. PIN_BUZZER を LOW に設定する（ブザーを停止）
4. buzzerOn フラグを false に設定する
```

---

### `validateIRCode()` — 赤外線コードの検証

**basic_design.md 2-2 との対応：** 入力検証

**引数：** `code`（unsigned long）: 検証対象の赤外線コード

**戻り値：** bool

```
【処理の流れ】
1. code が SOS_CODE と一致する場合:
   - true を返す
2. code が CANCEL_CODE と一致する場合:
   - true を返す
3. 上記以外の場合:
   - false を返す
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  本システムは物理ボタンではなく赤外線リモコン入力を使うため、
  同一コードの短時間連続受信を「同じ1回の操作」とみなし無視する。
  IR_GUARD_MS（200ms）以内の再受信は採用しない。

【処理の流れ】
  1. readIRCode() で受信コード（candidateCode）を取得する
  2. now = millis() を取得する
  3. candidateCode が 0 または未登録形式の場合は無効として破棄する
  4. now - lastAcceptedIrMs < IR_GUARD_MS の場合は同一操作とみなし無視する
  5. 上記を満たさない場合のみ receivedCode = candidateCode, hasNewCode = true とする
  6. lastAcceptedIrMs = now を記録する

【必要な変数（Section 1 に追加済みか確認）】
  IR_GUARD_MS      : const unsigned long = 200
  lastAcceptedIrMs : unsigned long
  receivedCode     : unsigned long
  hasNewCode       : bool
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  delay() を使わず、状態ごとに独立したタイマーで出力を制御する。
  「今の時刻 - 前回時刻 >= 周期」を満たした処理だけを実行する。

【処理の流れ（本システム）】
  1. now = millis()
  2. currentState == STATE_STANDBY のとき
     - now - lastMillisStandby >= STANDBY_BLINK_MS なら待機LEDをトグルし、lastMillisStandby = now
  3. currentState == STATE_ALERT のとき
     - now - lastMillisAlertBlink >= ALERT_BLINK_MS なら赤LEDをトグルし、lastMillisAlertBlink = now
     - now - lastMillisBuzzer >= BUZZER_TOGGLE_MS ならブザー出力をトグルし、lastMillisBuzzer = now
  4. 条件を満たさない処理は実行せず、次ループで再判定する

【自分のシステムで millis() を使う処理】
  - 待機状態の生存確認LED点滅（1秒周期）
  - 警報状態の赤LED高速点滅（100ms周期）
  - 警報状態のブザー断続音制御（200ms周期）
  - IR連続受信のガード時間判定（200ms）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. validateIRCode(receivedCode) で登録済みコードか判定する
2. SOS_CODE 受信時は currentState を STATE_ALERT に遷移する
3. CANCEL_CODE 受信時は currentState を STATE_STANDBY に戻し resetOutputs() を実行する
4. 未登録コードは ignoredCodeCount を増加させ、状態を変更しない

【入力値と出力値の関係】
  - 入力: SOS_CODE → 出力: 赤LED点滅 + ブザー断続音（警報開始）
  - 入力: CANCEL_CODE → 出力: 赤LED消灯 + ブザー停止 + 待機LED点滅へ復帰
  - 入力: 未登録コード/無効値 → 出力: 変化なし（誤作動防止）
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | IRコードが正しく受信できているか | `readIRCode()` | `Serial.println(receivedCode, HEX);` |
| 2 | 未登録コードを無視できているか | `readIRCode()` | `Serial.println("ignored code");` |
| 3 | SOS/CANCELで状態遷移できているか | `loop()` | `Serial.println(currentState);` |
| 4 | 警報中の赤LED周期が設計通りか | `updateOutput()` | `Serial.println("red blink toggle");` |
| 5 | 警報中のブザー周期が設計通りか | `updateOutput()` | `Serial.println("buzzer toggle");` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readIRCode() | SOSボタンを1回押す | hasNewCode=true になり、receivedCode に SOS_CODE が入る | | [ ] |
| 2 | readIRCode() | 同じSOSボタンを 200ms 未満で連打する | 2回目以降は無視され、誤作動しない | | [ ] |
| 3 | validateIRCode() | SOS_CODE を渡す | true が返る | | [ ] |
| 4 | validateIRCode() | CANCEL_CODE を渡す | true が返る | | [ ] |
| 5 | validateIRCode() | 未登録コード（例: 0x12345678）を渡す | false が返る | | [ ] |
| 6 | readIRCode() | 無効コード（0や受信失敗）を入力する | hasNewCode=false のまま、ignoredCodeCount が増える | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutput() | currentState=STATE_STANDBY で動作させる | 待機LED（PIN_LED_STANDBY）が 1秒周期で点滅する | | [ ] |
| 2 | updateOutput() | currentState=STATE_ALERT で動作させる | 赤LED（PIN_LED_RED）が 100ms 周期で点滅する | | [ ] |
| 3 | updateOutput() | currentState=STATE_ALERT で動作させる | ブザー（PIN_BUZZER）が 200ms 周期で ON/OFF する | | [ ] |
| 4 | resetOutputs() | 警報中に resetOutputs() を呼ぶ | 赤LED消灯・待機LED消灯・ブザー停止・buzzerOn=false になる | | [ ] |
| 5 | loop() | SOS_CODE受信後に ALERT へ遷移させる | 警報出力（赤LED点滅＋ブザー断続音）が開始する | | [ ] |
| 6 | loop() | ALERT中に CANCEL_CODE を受信させる | 警報出力が停止し、待機点滅に戻る | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay() による処理停止がないか | 警報点滅中にリモコンCANCELを押す | 1秒以内に警報停止し、入力が無視されない | | [ ] |
| 2 | 応答時間要件（1秒以内開始）の確認 | 待機中にSOSボタンを押し、押下から警報開始までを計測する | 1秒以内に赤LED点滅またはブザーが開始する | | [ ] |
| 3 | 待機LED周期精度（STANDBY_BLINK_MS） | 待機中LEDをストップウォッチで計測 | 約1000ms周期で点滅する | | [ ] |
| 4 | 警報LED周期精度（ALERT_BLINK_MS） | 警報中赤LEDを計測 | 約100ms周期で点滅する | | [ ] |
| 5 | ブザー周期精度（BUZZER_TOGGLE_MS） | 警報中ブザーON/OFF間隔を計測 | 約200ms間隔で切り替わる | | [ ] |
| 6 | IRガード時間境界テスト | 同一コードを 199ms / 200ms / 201ms 間隔で送信 | 199msは無視、200msと201msは受付される | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- バグになりやすい点は、(1) hasNewCode のクリア忘れで同じコードを再処理してしまうこと、(2) SOS_CODE/CANCEL_CODE が初期値 0x00000000 のまま運用され誤判定すること、(3) `unsigned long` 以外の型を使って IRコード比較が不一致になること、(4) 状態遷移時に出力リセットしないため LED/ブザーが残留すること、(5) 受信失敗時や未登録コードの扱いが曖昧で誤作動すること。
- また、`millis()` の比較は差分比較（now - last >= interval）で実装し、delay() を多用しないことが重要。Serial出力を入れすぎると周期が乱れるため、デバッグ後は削減が必要。

**対応した内容：**
- `readIRCode()` で有効コード受信時のみ hasNewCode=true にし、`loop()` 側で処理後に hasNewCode=false へ戻す運用を明記した。
- SOS_CODE/CANCEL_CODE は事前計測したHEX値へ必ず置換し、0x00000000 のまま書き込まないチェック項目を追加した。
- IRコード関連（receivedCode, SOS_CODE, CANCEL_CODE）はすべて `unsigned long` で統一する方針にした。
- ALERT→STANDBY 遷移時は `resetOutputs()` を必ず呼び出し、LEDとブザーの残留出力を防止する方針にした。
- 無効値・未登録コードは状態遷移させず無視し、`ignoredCodeCount` で監視する設計を維持した。
- タイマー処理はすべて `millis()` 差分比較で統一し、delay() は起動確認の1秒以外で使わない方針にした。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- Section 5 のテストは主要関数（readIRCode / validateIRCode / updateOutput / resetOutputs / loop）の正常系を概ねカバーできており、基本動作の検証としては妥当。
- 追加すると良い点は、(1) 状態遷移の境界条件（STANDBY中のCANCEL受信、ALERT中のSOS再受信）、(2) `millis()` オーバーフロー近傍での差分比較、(3) 受信直後に別コードが連続到来するケース、(4) hasNewCode の消費後クリア確認、(5) 起動直後（時刻0付近）の初回トグル挙動。
- 境界値としては IR_GUARD_MS の前後（199/200/201ms など）を明確化すると、誤作動対策の妥当性を説明しやすい。
- 非機能要件の「1秒以内応答」は既存テストで一部確認できるが、SOS受信から警報開始までの実測項目を独立して持つと評価しやすい。

**対応した内容：**
- 既存の IRガード境界テスト（190ms/210ms）を、必要に応じて 199/200/201ms でも確認する方針を追記した。
- STANDBY中にCANCEL_CODEを受信した場合は状態維持、ALERT中にSOS_CODE再受信でも状態維持となる確認テストを追加候補として整理した。
- `loop()` で hasNewCode を処理後に false へ戻せているかを Serial 出力で確認する項目をデバッグ計画に追加候補とした。
- SOS受信から赤LED点滅/ブザー開始までが1秒以内かをストップウォッチで測るテストを実施項目に含める方針にした。
- `millis()` オーバーフロー試験は授業時間内での実機再現が難しいため、差分比較実装レビュー（now - last >= interval の形）で代替確認する方針にした。

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
