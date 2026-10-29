# 対応基板と通信設定

[1000base-t1-converter](https://github.com/TLDSJPWORK/1000base-t1-converter)のRev1.0に対応しています。

## 部品と接続

| 項目 | 内容 |
| --- | --- |
| MCU | U601 RP2040、U603 12MHz水晶 |
| Flash | U602 W25Q16JVUXIQ（16Mbit / 2MiB） |
| 1000BASE-T1 PHY | U206。回路図表記はDP83TG720RWRHATQ1 |
| 1000BASE-T PHY | U301 DP83867ERGZR |
| MDIO | U601 GPIO4 → R608 22Ω → U206 pin36 / U301 pin17 |
| MDC | U601 GPIO5 → R609 22Ω → U206 pin1 / U301 pin16 |
| 共通リセット | U601 RUN、両PHYのRESET_N、U604 RESETを接続 |
| データ経路 | 一方のPHYのRX信号を他方のTX信号へ接続するRGMII構成 |
| PHY基準クロック | U207・X301、各25MHz |

U206のBOM表記とRJ45のCペア極性については、[Rev1.0の既知事項](https://github.com/TLDSJPWORK/1000base-t1-converter/blob/main/docs/rev1.0-known-issues.md)を参照してください。

## MDIOの動作条件

本基板ではMDIOの外付けプルアップ抵抗は未実装です。
ファームウェアはRP2040の内部プルアップを有効にし、MDCを50kHzで動作させます。
この条件でPHYのID読み取り・設定とEthernet通信を確認していますが、PHYの規定MDC周波数範囲への適合を示すものではありません。

MDIOは送信中のみ駆動し、読み取りTA・データと待機中は入力になります。
MDCはPIOで生成し、各半周期は10µsです。フレーム間はMDCをLowで停止し、20µs待機します。
読取りではTAの2ビット目がLowであることを確認します。
リンク状態はBMSRを2回読み、2回目の値を現在の状態として表示します。

## PHYアドレスと起動設定

起動設定はDP83867のアドレス0とDP83TG720のアドレス8を対象とします。
異なるアドレスの基板に使用する場合は、`src/main.c`の対象アドレスも変更してください。
`scan`コマンドは全32アドレスを走査します。

DP83867のMirror modeを解除し、RGMIIのTX・RX内部遅延を有効にして、
RGMIIDCTLの下位8ビットを`0x77`（各2.0ns）に設定します。
DP83TG720側のTX・RX内部遅延は無効にします。
通常版はU203の役割設定を維持し、役割固定版はPMA MMD1:0x834へ指定した役割を設定します。

## 回路図の接続情報

[board-evidence.json](board-evidence.json)には、回路図から抽出した部品・ネット情報と、対応する回路図のSHA-256を収録しています。
更新時はKiCad CLIを使って再生成できます。回路図データはファームウェアのビルドには不要です。

```sh
python3 tools/extract_board.py /path/to/1000base-t1-converter.kicad_sch \
  --kicad-cli /path/to/kicad-cli --output docs/board-evidence.json
```

## 参照資料

- [TI DP83TG720R-Q1データシート](https://www.ti.com/lit/ds/symlink/dp83tg720r-q1.pdf)：SMIタイミング、ターンアラウンド、拡張レジスタアクセス
- [TI DP83867Eデータシート](https://www.ti.com/lit/ds/symlink/dp83867e.pdf)：SMI、BMSR、RGMII設定
