# 基板情報と根拠

参照元は
[`TLDSJPWORK/1000base-t1-converter`](https://github.com/TLDSJPWORK/1000base-t1-converter)
で公開するKiCad設計です。2026-09-11に`1000base-t1-converter.kicad_sch`を
KiCad CLIでネットリストへ変換しました。ハードウェアリポジトリはビルド時依存ではなく、
Gitサブモジュールにもしていません。

抽出結果と各回路図のSHA-256は [board-evidence.json](board-evidence.json)。
回路図の配線を根拠とし、実装部品・ジャンパ状態・改修配線の確認は実機で行う。

| 項目 | 回路図から確認できる内容 |
| --- | --- |
| MCU | U601 RP2040、U603 12MHz水晶 |
| Flash | U602 W25Q16JVUXIQ（16Mbit / 2MiB） |
| 1000BASE-T1 PHY | U206 DP83TG720RWRHATQ1。S品ではなくR品 |
| 1000BASE-T PHY | U301 DP83867ERGZR。IR品ではなくE品 |
| MDIO | U601 pin6 GPIO4 → R608 22Ω → U206 pin36 / U301 pin17 |
| MDC | U601 pin7 GPIO5 → R609 22Ω → U206 pin1 / U301 pin16 |
| 共通リセット | U206 pin3 / U301 pin43 / U601 pin26 RUN / U604 pin4 RESET / J601 pin30 |
| GPIO20 | U601 pin31 → J601 pin26のみ。PHYリセットではない |
| データ経路 | U206 RX信号 → U301 TX信号、U301 RX信号 → U206 TX信号。各線22Ω直列抵抗 |
| PHY基準クロック | U207、X301は各25MHz |

RJ501のCペアは、U301のTD_C_NがMDI2+、TD_C_PがMDI2-へ接続されており、
ペア内極性が反転している。DP83867のチャネル別極性自動補正で通信できることを
実機リンクで確認した。現行基板は使用可能だが、次回PCB改版ではデバッグと規格試験時の
曖昧さを減らすためP/Nを正規化する。

## 実機で解決する事項

MDIOとGPIO4両側のネットに外付けプルアップ抵抗はなく、ユーザーも未実装と確認した。
2026-09-13にユーザーから、低速では読み取り可能との実機報告があった。
これを踏まえ、現実装は内部プルアップを有効にした50kHzの診断モードとする。
外付けプルアップの追加を低速診断の前提条件にはしない。

MDIOは送信中のみ駆動し、読み取りTA/データと待機中は入力になる。
MDCはPIOで公称50kHz、各半周期4 PIOサイクル（10µs）。
標準125MHzのclk_sysでは分周312.5。フレーム間はLOWで停止し20µs待機する。
PHYの規定周波数範囲外であるため、仕様適合と実機での診断成功は区別する。
PIO版のTA処理は従来スキャナーと異なるため、IDの正しさを実機で確認する。

PHYアドレスとmaster/slaveはストラップやジャンパ状態に依存する。
回路図にはR219=10kΩなどのストラップ部品があるが、実際にラッチされた値を確認して採用する。
`scan` のIDと、対象データシートのストラップ状態レジスタを照合する。
既存READMEにあったGPIO20リセット・固定アドレスの前提は引き継がない。

## 参照資料

- [TI DP83TG720R-Q1 datasheet SNLS603D](https://www.ti.com/lit/ds/symlink/dp83tg720r-q1.pdf): SMIタイミング、ターンアラウンド、拡張レジスタアクセス。
- [TI DP83867E datasheet SNLS504G](https://www.ti.com/lit/ds/symlink/dp83867e.pdf): SMI、BMSR、RGMII設定。

回路図から部品と接続を確定し、レジスタの意味は上記の該当品種の資料で確認する。
資料URLは更新されるため、再検証時は版も確認する。

## 再抽出

```sh
python3 tools/extract_board.py /path/to/1000base-t1-converter.kicad_sch \
  --kicad-cli /path/to/kicad-cli --output docs/board-evidence.json
```
