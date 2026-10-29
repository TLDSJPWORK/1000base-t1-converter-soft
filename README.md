# 1000base-t1-converter-soft

学習用1000BASE-T1メディアコンバーターのRP2040用ファームウェアです。
PHYの起動設定と、USB接続による状態確認・レジスタ操作に対応しています。

EthernetフレームはDP83TG720とDP83867の間をRGMIIで転送します。
RP2040はMDIO経由でPHYを管理します。

基板の設計データと接続方法は、[1000base-t1-converter](https://github.com/TLDSJPWORK/1000base-t1-converter)を参照してください。

## 主な機能

- 電源投入時にPHYの通信設定を適用
- 基板上のスイッチによる1000BASE-T1のmaster／slave選択
- USB CDCシリアルからのPHY検出、リンク状態の確認・監視
- Clause 22・MMDレジスタの読み書き

USBをPCに接続していなくても、電源投入時にPHYの起動設定を行います。

## ビルド

次の環境を用意してください。

- Raspberry Pi Pico SDK 2.2.0（TinyUSBサブモジュールを含む）
- CMake
- Arm GNU Toolchain（動作確認版：14.2.1）
- Python 3

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build -DPICO_TOOLCHAIN_PATH=/path/to/arm-toolchain
cmake --build build -j
```

書込み用ファイルは`build/converter_soft.uf2`です。デバッグ用の`build/converter_soft.elf`も生成されます。
ボード定義は`include/boards/converter_rp2040.h`にあります。

pioasm・picotoolの取得とビルドはPico SDKが行います。既に用意している場合は、
`-Dpioasm_DIR=/path/to/pioasm`と`-Dpicotool_DIR=/path/to/picotool`で指定できます。

## 書込みと接続

1. USBを含む全ての電源を外します。
2. 基板上のU203でmaster／slaveを選択し、U205を通常動作側に設定します。
3. Bootボタンを押したまま、データ通信対応USBケーブルでPCへ接続します。
4. `RPI-RP2`ドライブが表示されたらボタンを離し、`converter_soft.uf2`をコピーします。
5. ファームウェアの起動後、USB CDCシリアルを開きます。
6. 起動ログの`Media config result: OK`を確認します。

配線とスイッチの接点設定は[本体の取扱説明書](https://github.com/TLDSJPWORK/1000base-t1-converter/blob/main/docs/user-manual.md)を参照してください。

## USB診断

シリアル端末から、改行付きでコマンドを送信します。ボーレート設定は任意です。
端末のローカルエコーを有効にすると、入力したコマンドを確認できます。

最初に`scan`でPHYを検出し、`status`でリンク状態を確認します。
継続して監視する場合は`watch on`、停止する場合は`watch off`を実行します。

```text
scan
status
watch on
```

| コマンド | 機能 |
| --- | --- |
| `help` | コマンド一覧を表示 |
| `scan` | PHYアドレス0〜31を走査 |
| `status` | 検出したPHYのリンク状態とBMCR・BMSRを表示 |
| `watch on` / `watch off` | 1秒周期でリンク変化・MDIO通信異常の監視を開始／停止 |
| `read PHY REG` | Clause 22レジスタを読み取り |
| `write PHY REG VALUE` | Clause 22レジスタへ書込み |
| `mread PHY DEV REG` | MMDレジスタを読み取り |
| `mwrite PHY DEV REG VALUE` | MMDレジスタへ書込み |

`status`と`watch`は、`scan`で検出したPHYが対象です。
数値は10進数、または`0x`付きの16進数で指定します。

例えば、アドレス8のPHY IDレジスタとMMDレジスタを読む場合は、次のように入力します。

```text
read 8 2
mread 8 31 0x45d
```

書込み後は、対象レジスタを読み戻して設定を確認してください。
MMDの読み取りでも、内部ではアドレス選択用レジスタへの書込みを行います。
USBコマンドで変更した設定はフラッシュに保存されず、再起動時には起動設定が適用されます。

## PHYの起動設定

通常版は、U203で選択したmaster／slaveを維持し、次の設定を適用します。

| PHY | 設定 |
| --- | --- |
| DP83867（アドレス0） | Mirror modeを解除し、RGMIIのTX・RX内部遅延を各2.0nsに設定 |
| DP83TG720（アドレス8） | RGMIIのTX・RX内部遅延を無効化 |

設定値は読み戻し、USB接続時に`Media config result`とレジスタ値を表示します。
MDIOは内部プルアップを使用し、MDCは50kHzで動作します。
接続と通信条件の詳細は[基板情報](docs/board.md)を参照してください。

### ソフトウェアで役割を固定する場合

通常は両台とも`converter_soft.uf2`を使用し、基板上のU203で役割を選択します。
試験などでソフトウェアから役割を固定する場合は、次のオプションを指定します。
**役割固定版はU203のmaster／slave設定を上書きします。**

```sh
cmake -S . -B build-master -DCONVERTER_T1_ROLE=master
cmake --build build-master -j
cmake -S . -B build-slave -DCONVERTER_T1_ROLE=slave
cmake --build build-slave -j
```

| `CONVERTER_T1_ROLE` | 動作 | UF2ファイル名 |
| --- | --- | --- |
| `diagnostic`（既定値） | U203の役割設定を維持 | `converter_soft.uf2` |
| `master` | masterに固定 | `converter_soft_master.uf2` |
| `slave` | slaveに固定 | `converter_soft_slave.uf2` |

いずれもメディアコンバーターの起動設定を行います。

## 制限事項

MDIO管理通信のMDCは50kHzです。DP83TG720Rのデータシートに示される2.5〜20MHzの範囲を下回っており、規定範囲内での動作ではありません。
Rev1.0基板では外付けプルアップ抵抗を未実装とし、RP2040の内部プルアップを使用したこの設定で、PHYの設定・状態読取りとメディアコンバーターとしての通信を確認しています。
実機での動作確認は、規定範囲外のMDIO動作を保証するものではありません。

この周波数はPHYの管理通信に用いるもので、Ethernetの通信速度（1000Mbps）とは別です。
詳細は[基板情報](docs/board.md)と[TI DP83TG720R-Q1データシート](https://www.ti.com/lit/ds/symlink/dp83tg720r-q1.pdf)を参照してください。

## 動作確認とテスト

Rev1.0基板2台を`Mac ↔ コンバーター ↔ コンバーター ↔ Raspberry Pi`と接続し、
1000BASE-T full-duplex、両T1 PHYのリンク、ARP、20回のping（損失0%）、TCP/22への接続を確認しています。
確認条件と範囲は[動作確認・テスト](docs/validation.md)を参照してください。

MDIOフレーム生成などのホストテストは、次のコマンドで実行できます。

```sh
cmake -S . -B build-host -DCONVERTER_HOST_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

## ソース構成

| フォルダ・ファイル | 内容 |
| --- | --- |
| `src/` | PIO MDIOドライバ、PHY起動設定、USBコマンド |
| `include/` | ヘッダーファイル・ボード定義 |
| `tests/` | ホスト上で実行するテスト |
| `docs/` | 基板情報・動作確認結果 |
| `tools/extract_board.py` | KiCad回路図からの部品・接続情報の抽出 |
| `tools/mdio-scan/` | 補助診断用MDIOスキャナー |

## ライセンス

このソフトウェアは[MIT License](LICENSE)で提供します。
