# 1000base-t1-converter-soft

1000BASE-T1コンバーター基板のRP2040用ソフトウェアです。
基板を純粋な1000BASE-T／1000BASE-T1メディアコンバーターとして起動し、
USB CDCからPHYの検出、レジスタ操作、リンク監視も行える管理ソフトウェアです。
起動時に実機で確定したDP83867のMirror解除とRGMII遅延を適用します。

Ethernetデータは基板上の2個のPHY間をRGMIIで流れます。
RP2040はMDIO管理を担当し、Ethernetフレームのソフトウェア転送は行いません。

対応するKiCad設計と製造資料は
[`TLDSJPWORK/1000base-t1-converter`](https://github.com/TLDSJPWORK/1000base-t1-converter)
で公開します。

## ビルド

Pico SDK 2.2.0、CMake、Arm GNU Toolchain、Python 3を用意してください。
SDKのTinyUSBサブモジュールも必要です。SDKはこのリポジトリに含めません。

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build -DPICO_TOOLCHAIN_PATH=/path/to/arm-toolchain
cmake --build build -j
```

SDK付属のpioasm・picotoolを既にインストールしている場合は、CMakeに
`-Dpioasm_DIR=/path/to/pioasm` と `-Dpicotool_DIR=/path/to/picotool` を渡せます。
未指定の場合の補助ツールのビルド・取得はPico SDKの仕組みに従います。

出力は `build/converter_soft.uf2` と `build/converter_soft.elf` です。
ボード定義は `include/boards/converter_rp2040.h`。汎用PicoのGPIO機能は使用しません。

## 書き込みと操作

既定は内部プルアップ付き50kHzの診断モードです。電源・接続を確認したうえで、
基板をBOOTSELモードで接続し、UF2を書き込みます。
USB CDCシリアルを開き、改行付きでコマンドを送信してください。
端末のローカルエコーを有効にすると入力を確認できます。CDCのボーレート設定は任意です。
USB接続を待つ無限待機はありません。起動時はU203のT1 master/slaveストラップを
通常どおり尊重し、DP83867のMirror modeを解除してRGMII TX/RX遅延を2nsへ設定します。
設定は読み戻して、USB接続時の `Media config result` に結果と値を表示します。

```text
help
scan
status
watch on
watch off
read 8 2
mread 8 31 0x45d
```

アドレス8は例です。`scan` の結果で置き換えてください。
PHYアドレスを既存スキャナーの想定値0/8に固定していません。

| コマンド | 機能 |
| --- | --- |
| `scan` | PHYアドレス0〜31のIDレジスタを検出 |
| `status` | 検出PHYのBMCR/BMSRと現在のリンク状態 |
| `watch on/off` | 1秒ごとにリンク変化と通信異常を報告 |
| `read PHY REG` | Clause 22読み取り |
| `write PHY REG VALUE` | Clause 22書き込み |
| `mread PHY DEV REG` | Clause 22経由のMMD読み取り |
| `mwrite PHY DEV REG VALUE` | Clause 22経由のMMD書き込み |

数値は10進または `0x` 付き16進。範囲外や長すぎる入力は拒否します。
読み取り時は2ビットのターンアラウンドを送り、2ビット目のLOWを確認します。
BMSRはラッチLOWに対応して2回読み、2回目を現在のリンクとして表示します。

書き込みコマンドは明示的な操作であり、PHYの動作を変えます。
Clause 22書き込み自体にはACKがないため、送信完了と書き込み成功を区別します。
対象PHYの応答を事前確認しますが、設定の適用は該当レジスタの読み戻しで確認してください。
MMD読み取りもREGCR/ADDARのアドレス選択のため書き込みを行います。
手動コマンドの設定はフラッシュ保存しません。メディアコンバーター用の起動設定は、
RP2040が起動するたびに再適用します。

## 検証

```sh
cmake -S . -B build-host -DCONVERTER_HOST_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

[検証記録と実機確認手順](docs/validation.md) を参照してください。
UF2のビルド成功は、実機でのMDIO通信・Ethernet転送成功を意味しません。

2026-09-14に2台を `Mac ↔ converter ↔ converter ↔ Raspberry Pi` と接続し、
1000BASE-T full-duplex、両T1 PHYリンク、ARP、20回のping（損失0%）、SSHポート到達を確認しました。

## 構成

- `src/`: PIO MDIOドライバ、フレーム生成、MMDアクセス、USBコマンド
- `include/`: 公開APIと専用ボード定義
- `tests/`: ホスト上で実行するプロトコルテスト
- `docs/`: 回路図の根拠・確認手順
- `tools/extract_board.py`: KiCadネットリストと回路図ハッシュの抽出
- `tools/mdio-scan/`: 初期立ち上げ用の最小構成MDIOスキャナー

設計上の意思決定はハードウェアリポジトリの
[`docs/design-decisions/`](https://github.com/TLDSJPWORK/1000base-t1-converter/tree/main/docs/design-decisions)
に記録しています。
このフォルダ単独でビルドでき、スキャナーや回路図へのビルド依存はありません。

## 2台構成の役割別ビルド

同じ基板を対向接続するときは、一方をmaster、もう一方をslaveとしてビルドします。
起動時にT1 PHYのPMA制御（MMD 1、レジスタ0x834）へ役割を設定し、
値が変わった場合だけソフト再起動を行います。読み戻し結果はUSB接続時に表示します。

```sh
cmake -S . -B build-master -DCONVERTER_T1_ROLE=master
cmake --build build-master -j
cmake -S . -B build-slave -DCONVERTER_T1_ROLE=slave
cmake --build build-slave -j
```

成果物はそれぞれ `converter_soft_master.uf2` と `converter_soft_slave.uf2` です。
既定値 `diagnostic` は役割を変更しません。

## 低速診断モード（0.1.1）

外付けプルアップなしで低速なら読み取れるというユーザーの実機確認に基づき、
MDCを50kHz、MDIOの内部プルアップを有効にしました。フレーム間は20µs待機します。
以前のスキャナーと同様の低速条件ですが、PIO版でもPHY 0/8のID読み取りを実機確認済みです。
PHYの規定MDC周波数範囲への適合を示すモードではありません。

## メディアコンバーター起動設定（0.3.0）

DP83867のストラップで意図せず有効になったMirror modeを解除します。Mirror設定を
変更した場合だけDP83867をソフト再起動します。その後、RGMIICTLでTX/RX内部遅延を
有効化し、遅延量をRGMIIDCTL `0x0077`（各2.0ns）に設定します。
既存値の上位8bitは保持し、各設定を読み戻して一致しなければUSBへエラーを表示します。

実機ではMirror解除後に1000BASE-Tリンクが成立し、RGMIIDCTLを`0x0017`から
`0x0077`へ変更した後に双方向のIP疎通が成立しました。DP83TG720側の内部遅延は
TX/RX内部遅延を明示的に無効化し、両PHYで遅延を重複させません。

## ライセンス

このソフトウェアは[MIT License](LICENSE)で提供します。
