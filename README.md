# Frame Finger Correction

Steam Frame controller finger deadzone correction. See [Releases](https://github.com/wassan0330/frame-finger-correction/releases) for the preview and installation instructions.

## Build the v0.1.0 preview on Steam Frame

This source builds the ARM64 UI, correction driver and dashboard registration helper. On the Frame, use its existing C++17 compiler, Cairo development files and Valve OpenVR headers/runtime:

```sh
FRAME_FINGER_RELEASE=1 FRAME_FINGER_RUNTIME=november-beta sh scripts/build-frame-finger.sh
```

The driver targets the exact verified November SteamVR beta binary pair. See Releases for installation and removal. The app is unofficial. The bundled Valve header has its own license in third_party/openvr-driver.

## 注意事項・免責

**Frame Finger Correctionは、Steam Frame向けに個人が開発した非公式のツールです。Valve Corporationの公式製品・公式機能ではなく、Valveによる承認やサポートを受けていません。**

本ツールの開発には、コードの作成、調査、テストの補助に生成AI（OpenAI Codex）を使用しました。動作確認の範囲は以下のとおりです。

本ツールは、指を伸ばしても曲がったままに見える症状を緩和するため、SteamVRの非公開インターフェースを使用し、実行中の指入力処理を一時的に変更します。SteamVR本体のファイルや、コントローラーのファームウェアは書き換えません。

動作確認は、特定のSteamVRベータビルドと1台のSteam Frameで行っています。すべての端末、コントローラー、アプリでの動作や補正効果を保証するものではありません。SteamVRの更新などで互換性を確認できない場合、補正は有効になりません。

本ツールは現状のまま提供されます。保証および責任に関する条件は、同梱の `LICENSE` をご確認ください。本ツールが公開されていることは、Valveや利用先のサービスがこの補正方式を許可していることを意味しません。

適用法令で認められる範囲において、利用によって発生した損害について、作者は責任を負いません。
