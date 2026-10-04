# 1000BASE-T1 converter MDIO scanner

RP2040のGPIO4（MDIO）とGPIO5（MDC）を使い、Clause 22 PHYアドレス0〜31を走査します。
GPIO20の共通PHYリセットは、TPS3838と競合しないよう入力（High-Z）のままです。

```sh
mkdir build
cd build
cmake ..
make -j
```

`build/mdio_scan.uf2`を書き込み、USB CDCシリアルを開いてください。異なるPHY
アドレスで2行の`RESPONDS`が出るのが期待結果です。
