# 検証記録

2026-09-11: ホストテスト成功、RP2040向けReleaseビルド成功。
Pico SDK 2.2.0、Arm GNU 14.2.1、AppleClang 17で確認。
生成物: converter_soft.uf2 / converter_soft.elf。
コンパイルは `-Wall -Wextra -Werror` を有効にした。

## 自動検証の範囲

- 全32 PHY × 全32レジスタ × read/writeのフレームを独立にデコードし、ST/OP/PHY/REGを照合。
- 32ビットプリアンブル、2ビットTA、読み取り中のMDIO解放、書き込みデータを確認。
- 全65536通りの読み取りデータについて値の保持とTA不成立時の拒否を確認。
- MMDアドレス指定・データフェーズと通信異常時の途中停止を確認。
- 範囲外アドレス、NULL出力ポインタでは転送しない。

PIOの実時間動作、電気特性、USB通信、PHY初期化、Ethernet転送はホストテスト対象外。

## 実機確認手順

1. 回路図の版・実装部品・MDIOプルアップを照合する。
2. UF2を書き込み、USBで `help` を確認する。
3. `scan` 時のGPIO4/5を測定する。MDC 50kHz（低速診断）、TA Z0、読み取り16ビットを確認する。
4. 検出IDを確認し、アドレスとストラップレジスタ値を記録する。
5. RGMIIの各方向についてクロック遅延が重複しない設定を確定する。
6. T側を1000M/fullに整合させ、T1側の相手とmaster/slaveを組み合わせる。
7. 管理モードの起動手順と該当シリコン版の推奨初期化をデータシートに照らして適用する。
8. 双方向トラフィックを流し、リンクだけでなくフレーム損失・CRCエラーを確認する。

手順1〜7は2026-09-14までに実施した。手順8はARP、ping、SSHポートで基本的な
双方向転送を確認済み。長時間負荷、複数フレームサイズ、PHYエラーカウンタによる
損失・CRC評価は今後の耐久試験として残る。

## 2026-09-13: 低速版0.1.1の実機確認

ユーザー依頼によりMDC 50kHz・内部プルアップ有効に変更。ホストテストとビルド成功。
USBシリアルで旧スキャナーの稼働とIDを確認し、旧UF2を保存・照合してから新UF2を書き込んだ。
初回版の1ビットずれは、PIOのIN命令をLOW末尾へ移して修正した。
修正版をpicotoolで書き込み・Flash照合（OK）・再起動し、USB CDC経由で確認した。

- PHY 0: ID 2000a231、BMCR 1140、BMSR 7949、link down
- PHY 8: ID 2000a284、BMCR 0140、BMSR 0141、link down
- scanでは0と8のみ検出。個別readでも旧スキャナーと同じIDを取得。
- ログ: [serial-20260913.log](serial-20260913.log)
- 書き込み済みUF2: build-artifacts/converter_soft.uf2
- 旧プログラム: build-artifacts/before-slow-20260913.uf2

USBとClause 22の読み取りは実機確認済み。MMDアクセス・PHY設定書き込み・リンク確立・
双方向Ethernet転送は未検証。MDC波形の測定も未実施。

UF2 SHA-256: `4a750ffef7659784ae24b85bae0e603cdefa4e9f35c2cb7021af3b42a76c6b17`

## 2026-09-13: 2台目の準備

2台同時接続を確認し、BOOTSELにあった2台目だけへ同一UF2を書き込み・照合（OK）。
1台目を強制再起動しないため、loadには-fを付けなかった。
起動後USBシリアルで0.1.1、PHY 0=2000a231、PHY 8=2000a284を確認。
両PHYはlink down。master/slave設定はまだ変更していない。

| 基板 | アプリ動作時USBシリアル番号 | 今回のポート |
| --- | --- | --- |
| 1台目 | E6658C344B929F2A | /dev/cu.usbmodem314401 |
| 2台目 | E6658C344B28AB2B | /dev/cu.usbmodem314201 |

ポート名は接続先で変わるため、今後も固有番号で識別する。
2台目のBOOTSEL USB識別子E0C9125B0D9Bはアプリの固有番号とは異なる。
ログ: [serial-board2-20260913.log](serial-board2-20260913.log)。

## 2026-09-13: T1対向リンク

CN201同士を接続した状態で、両ボードのT1 PHY設定を読み取った。
両方ともSOR_VECTOR_1=0x1108、PMA MMD1:0x834=0x8001でslaveだった。
1台目だけを0xC001（master）へ変更し、Clause 22 register 0x1Fへ0x4000を
書いてソフト再起動した。読み戻しは0xC001。2台目は0x8001のまま。
両ボードのT1 PHY BMSRは0x0145となり、リンクアップを確認した。

最初に結合表記0x1834をMMD内アドレスとして渡した操作は値が変わらず、
リンク設定にも影響しなかった。正しいUSBコマンド表記はMMD 1、アドレス0x834。

電源再投入でも役割を復元するため、0.2.0から役割別ビルドを追加した。
master/slaveの両ビルドとホストテストは成功。実機書き込み後に再起動を含めて検証する。

役割別0.2.0をUSB固有番号で指定して両台へ書き込み、Flash照合はいずれもOK。
起動直後の自動適用結果は次の通り。

- 1台目: `T1 boot role: master`、PMA MMD1:0x834=0xC001、T1 link up
- 2台目: `T1 boot role: slave`、PMA MMD1:0x834=0x8001、T1 link up

手動の役割設定を行わない再起動後にも両側でリンクアップしたため、役割の起動時復元を確認済み。
1000BASE-T側PHYは両台ともlink downであり、RJ45対向機器との接続確認は次工程。

## 2026-09-13: U203ジャンパの電源投入ストラップ確認

ソフトウェアで役割を上書きしないdiagnostic版0.2.0を両台へ書き込み、
ユーザーが電源を切ってU203を1台目=1–4、2台目=2–3へ設定した後、再投入した。

- 1台目: SOR_VECTOR_1=0x1128（masterビット1）、PMA MMD1:0x834=0xC001
- 2台目: SOR_VECTOR_1=0x1108（masterビット0）、PMA MMD1:0x834=0x8001
- 両台: T1 PHY BMSR=0x0145、link up

U203の1–4=master、2–3=slaveという回路図記載と実機結果が一致した。
以後はdiagnostic版を通常構成とし、ジャンパの初期値を尊重する。
役割別UF2は試験用成果物として残すが、通常使用では書き込まない。

## 2026-09-14: 2台構成のEthernet疎通と起動設定の確定

MacのGigabit Ethernetを1台目のRJ45、2台目のRJ45をRaspberry Piへ接続した。
U203は一方をmaster、他方をslaveとし、U205は通常動作側とした。

DP83867のSTRAP_STS1は両台ともMirror mode有効を示した。CFG4（MMD31:0x0031）
bit0を解除してソフト再起動すると、両側の1000BASE-Tリンクが成立した。
初期RGMII設定は両台ともRGMIICTL下位2bit=11、RGMIIDCTL=0x0017、
DP83TG720 RGMII_DELAY_CTRL=0x0000だった。DP83867のTX遅延0.5nsに対しRX遅延は
2.0nsで非対称だったため、RGMIIDCTLを0x0077（TX/RX各2.0ns）へ変更した。

Mac `192.168.4.110/24`、Pi eth0 `192.168.4.21/24` の専用サブネットで確認した。

- 両基板: DP83867 link up、DP83TG720 link up
- PiのARP MAC: `2c:cf:67:d5:37:b2`
- ping: 10送信10受信、損失0%、平均0.612ms
- TCP/22: 接続成功

Mirror解除と遅延設定はいずれも手動変更直後に読み戻した。これを0.3.0の起動処理へ
組み込み、U203の役割ストラップは引き続き上書きしない。RGMII設定値はTI資料の
DP83867 RGMIICTL（0x0032）とRGMIIDCTL（0x0086）に照合した。

### 0.3.0起動処理の実機確認

通常用diagnostic版0.3.0をUSB固有番号で指定して両台へ書き込み、Flash照合はいずれもOK。
再起動後、両台で次の読み戻し結果を得た。

- `Media config result: OK`
- DP83867 CFG4=`0x10b0`（Mirror無効）
- DP83867 RGMIICTL=`0x00d3`（TX/RX遅延有効）
- DP83867 RGMIIDCTL=`0x0077`（TX/RX各2.0ns）
- DP83TG720 RGMII_DELAY_CTRL=`0x0000`（TX/RX遅延無効）
- 1台目PMA=`0xc001`、2台目PMA=`0x8001`（U203の役割を維持）
- 両台のDP83867、DP83TG720ともlink up

自動起動後の状態でPiへ20回pingし、20受信、損失0%、平均0.653msだった。
ARPは`2c:cf:67:d5:37:b2`を解決し、TCP/22への接続も成功した。

| 成果物 | SHA-256 |
| --- | --- |
| converter_soft.uf2 | `13fb566d299c6de7f3e91d2cfaccdd0c475adfbe41220194c25103aa3a7baa0c` |
| converter_soft_master.uf2 | `2b666bb4fe87c4b258e12f2c9f1fa1e35d09b89850413efd0f69694b675013ea` |
| converter_soft_slave.uf2 | `41e1a4238872f990a8274cea0c09fece0f91256df47f6d4a63c90192bd992981` |
