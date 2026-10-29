# MDIOスキャナー

GPIO4（MDIO）・GPIO5（MDC）を使う、ビットバンギング方式の補助診断ツールです。
通常のメディアコンバーター動作には、リポジトリ直下のPIO版ファームウェアを使用してください。
このツールはPHYのID・レジスタ読取り用です。メディアコンバーター用の起動設定は行いません。

## ビルド

Pico SDKとArm GNU Toolchainを用意して実行します。

```sh
cmake -S tools/mdio-scan -B build-scan -DPICO_SDK_PATH=/path/to/pico-sdk \
  -DPICO_TOOLCHAIN_PATH=/path/to/arm-toolchain
cmake --build build-scan -j
```

`build-scan/mdio_scan.uf2`を書き込み、USB CDCシリアルを開いてください。
低速・高速の診断を交互に行い、アドレス0と8のレジスタ値と、全32アドレスのTA応答を表示します。
高速側の周波数は推定値です。通信条件の確認には波形測定が必要です。
