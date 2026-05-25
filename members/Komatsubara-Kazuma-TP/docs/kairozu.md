# 回路図 — リモコン式デジタルロック

対象: Arduino UNO R3
作成日: 2026-05-25
参照: design_unit1.md

---

## 1. ピン接続表

| Arduino UNO R3 | 接続先 | 用途 |
|:--|:--|:--|
| DCジャック/Vin | 9V1A ACアダプタ | Arduino本体電源 |
| 5V | ブレッドボード+レール | ロジック系電源供給 |
| GND | ブレッドボード-レール | ロジック系GND供給 |
| D2 | IR受信モジュール OUT | IR信号受信 |
| D3 | TM1637 CLK | 7セグクロック |
| D4 | TM1637 DIO | 7セグデータ |
| D8 | パッシブブザー + | 警告音出力 |
| D9 | サーボ SG90 信号線(橙) | 施錠/解錠制御 |
| D10 | 330Ω抵抗(R) -> RGB LED R | 赤LED制御 |
| D11 | 330Ω抵抗(G) -> RGB LED G | 緑LED制御 |
| D12 | 330Ω抵抗(B) -> RGB LED B | 青LED制御 |

> 注意:
> - サーボ電源(赤/茶)は外部5Vを推奨。
> - 外部5V電源GNDとArduino GNDは必ず共通化すること。
> - D0/D1は未使用(Serial予約)。
> - D10-D12はLED専用とし、SPIは未使用前提。

---

## 2. 回路図（Mermaid: ブレッドボード配線込み）

```mermaid
flowchart LR
    RC[IR Remote Control]\n(送信器)

    subgraph UNO[Arduino UNO R3]
        D2[D2]
        D3[D3]
        D4[D4]
        D8[D8]
        D9[D9]
        D10[D10]
        D11[D11]
        D12[D12]
        V5[5V]
        GND[GND]
    end

        subgraph BB[ブレッドボード]
                P[+電源レール]
                N[-電源レール]
                Rr[330Ω 抵抗 R]
                Rg[330Ω 抵抗 G]
                Rb[330Ω 抵抗 B]
        end

    IR[IR Receiver Module\nVS1838B]
    SEG[4-Digit 7-Segment\nTM1637]
    BZ[Passive Buzzer]
    SV[Servo SG90]
        LED[RGB LED\n(共通カソード)]
    AC[9V1A AC Adapter]
    EXT[External 5V Power]

    AC --> UNO

        V5 -- 赤コード --> P
        GND -- 黒コード --> N

    RC -. IR信号 .-> IR
    IR -- OUT --> D2
        P --> IR
        N --> IR

    D3 --> SEG
    D4 --> SEG
        P --> SEG
        N --> SEG

    D8 --> BZ
        N --> BZ

    D9 --> SV
    EXT -- +5V --> SV
    EXT -- GND --> SV
    EXT -- GND共通 --> GND

        D10 -- 黄コード --> Rr
        D11 -- 緑コード --> Rg
        D12 -- 青コード --> Rb
        Rr --> LED
        Rg --> LED
        Rb --> LED
        LED --> N
```

---

## 3. ブレッドボード配線図（テキスト）

```text
[Arduino UNO R3]                          [Breadboard]
    DCジャック/Vin <---------------------- 9V1A ACアダプタ
    5V  ------------------------------->     +レール
    GND ------------------------------->     -レール

    D2  ------------------------------->     IR受信 OUT
    D3  ------------------------------->     TM1637 CLK
    D4  ------------------------------->     TM1637 DIO
    D8  ------------------------------->     Buzzer +
    D9  ------------------------------->     Servo 信号(橙)

    D10 --(ジャンパコード)--> [330Ω] --> RGB R端子
    D11 --(ジャンパコード)--> [330Ω] --> RGB G端子
    D12 --(ジャンパコード)--> [330Ω] --> RGB B端子

    RGB共通カソード --------------------->     -レール
    Buzzer - --------------------------->     -レール
    IR VCC/TM1637 VCC ------------------>     +レール
    IR GND/TM1637 GND ------------------>     -レール

[外部5V電源]
    +5V -------------------------------->     Servo 赤
    GND -------------------------------->     Servo 茶
    GND -------------------------------->     Arduino GNDと共通化
```

---

## 4. 実装チェックリスト

- D0/D1に何も接続していない
- D2, D3, D4, D8, D9, D10, D11, D12 の重複割り当てがない
- RGB LEDの各色に330Ω抵抗を1本ずつ入れている
- ジャンパコードの色分けを行い、信号線と電源線を識別できる
- サーボをArduino 5V直結で駆動していない
- Arduino本体は9V1A ACアダプタ(DCジャック/Vin)で給電している
- 外部5VのGNDとArduino GNDを共通化している
- 通電前に極性と短絡を最終確認した
