# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

複数ボード対応の地震計（震度計）ファームウェア。100Hzサンプリングで加速度を取得し、NIEDの特許(#5946067)に基づくフィルタ処理・計測震度算出を行い、NMEA形式でシリアル出力する。Arduino + FreeRTOS ベース。

## Build System

PlatformIOを使用。VSCode拡張「PlatformIO IDE」が推奨。

```bash
# 全環境ビルド
pio run

# 特定環境のみビルド
pio run -e pidas
pio run -e pidas-pico-w
pio run -e eqis-1-rp2040
pio run -e eqis-1-esp32c3
pio run -e eqis-1-esp32s3

# 書き込み
pio run -e <env> -t upload

# シリアルモニタ (115200 baud)
pio device monitor
```

テストディレクトリ (`test/`) は現在空。

## Build Environments

| 環境名 | ボード | MCU | センサー | ADC |
|---|---|---|---|---|
| `pidas` | Raspberry Pi Pico | RP2040 | KXR94-2050 | MCP3204 (SPI) |
| `pidas-pico-w` | Raspberry Pi Pico W | RP2040 | KXR94-2050 | MCP3204 (SPI) |
| `eqis-1-rp2040` | Seeed XIAO RP2040 | RP2040 | LSM6DSO | 内蔵ADC |
| `eqis-1-esp32c3` | Seeed XIAO ESP32C3 | ESP32-C3 | LSM6DSO | 内蔵ADC |
| `eqis-1-esp32s3` | Seeed XIAO ESP32S3 | ESP32-S3 | LSM6DSO | 内蔵ADC |

各環境は `platformio.ini` で `build_flags` によりデバイス名・センサー名・ADCステップ等のマクロを定義。ボード固有コードは `board-lib/` 配下の `Board.h` が `build_flags` の `-I` で選択される。

## Architecture

### データフロー

```
Raw Sensor (100Hz)
  → gal変換 (ADC_STEP係数)
  → IntensityFilter: HPF + 6段IIRカスケード (重力除去・JMA周波数特性)
  → IntensityProcessor: 3軸合成加速度 → 60秒ローリングバッファ → 上位30値抽出 → 計測震度算出
  → 安定性判定 (60秒以上 & 標準偏差 < 0.05)
  → UI表示 + NMEAシリアル出力
```

### FreeRTOSタスク構成 (全ボード共通)

- **measureTask** (優先度10): センサー読み取り → `processor.process()` 呼び出し (100Hz)
- **displayTask** (優先度4-5): キュー経由で震度受信 → LED/OLED更新
- **serialCommandTask** (優先度5): USBシリアルでHWINFO・DSPCFGコマンド処理

### ソースコード構成

- `src/main.cpp` — エントリポイント、NMEAプロトコル処理、シリアルコマンドタスク
- `include/` — ボード非依存のコア処理
  - `IntensityFilter.h` — 特許#5946067に基づく6段IIRフィルタカスケード
  - `IntensityProcessor.h` — 100サンプル×60グループのローリングバッファ、震度算出、安定性判定
  - `IIRFilter.h` — Direct Form II IIRフィルタ基底クラス
  - `JmaIntensity.h` — JMA震度階級enum・閾値変換
  - `DisplayConfig.h` — 表示設定（閾値・リセット時間）の構造体・EEPROM永続化
- `board-lib/PiDAS/` — Raspberry Pi Pico向け (MCP3204 SPI ADC, 10連LED表示)
- `board-lib/EQIS/` — Seeed XIAO向け (LSM6DSO I2C加速度センサー, SSD1306 OLED表示)

### ボード固有コードの切り替え

`platformio.ini` の `build_flags` に `-I board-lib/PiDAS` または `-I board-lib/EQIS` を指定。各ボードの `Board.h` が `#include "Board.h"` で統一的に読み込まれ、`setup()` と各タスク関数を提供する。マクロ `SEISMOMETER_DEVICE_NAME`, `SEISMOMETER_SENSOR_NAME`, `SEISMOMETER_ADC_NAME`, `SEISMOMETER_ADC_STEP` がボードごとに定義される。

### NMEA出力プロトコル

- `XSACC` — フィルタ済み加速度 (100回/秒)
- `XSRAW` — 生センサーデータ (100回/秒)
- `XSINT` — 計測震度 (5回/秒)
- `XSHWI` — ハードウェア情報 (HWINFOコマンド応答)
- `XSCFG` — 設定値 (DSPCFGコマンド応答)。第1フィールドでカテゴリを識別 (`DSP`=表示設定)
- `XSEER` — エラー (100Hzサンプリング落ちなど)

### シリアルコマンド

- `HWINFO` — ハードウェア情報を返す (`XSHWI`)
- `DSPCFG` — 表示設定を照会 (`$XSCFG,DSP,<currentThreshold>,<maxThreshold>,<resetMinutes>*XX`)
- `DSPCFG C <0-9>` — 現在震度の表示閾値を設定 (JmaIntensity enum値)
- `DSPCFG M <0-9>` — 最大震度の表示閾値を設定
- `DSPCFG R <1-1440>` — 最大震度のリセット時間（分）を設定

設定はEEPROMに永続化され、リブート後も保持される。閾値は表示（LED/OLED）にのみ適用され、NMEA出力（XSINT）には影響しない。

## Notes

- 震度算出は特許技術(#5946067)を使用。非営利個人利用は問題ないが、商用利用にはライセンス契約が必要。
- ESP32C3/S3は実験的サポート。USBシリアル切断で数秒後にリセットされる既知問題あり。
- 安定性判定完了前（起動後60秒未満）は震度として `nan` を出力する。
