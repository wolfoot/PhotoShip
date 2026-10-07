[简体中文](README.zh_CN.md) · [English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Español](README.es.md)

# PhotoShip

<p align="center">
  <img src="packaging/icons/photoship-128.png" width="96" height="96" alt="PhotoShip — Layer Sail">
</p>

**Ubuntu と Windows 向けの軽量なレイヤー画像エディター。**

PhotoShip は Qt 6 と C++17 で開発され、ブラシ、選択範囲、レイヤーマスク、非破壊調整、編集可能なプロジェクトを提供します。基本的な画像編集、合成、グラフィック制作に利用できます。アプリのコードは [MIT ライセンス](LICENSE) です。

[ビルドワークフロー](https://github.com/wolfoot/PhotoShip/actions/workflows/build.yml) · [問題を報告](https://github.com/wolfoot/PhotoShip/issues) · [サードパーティのライセンス](THIRD_PARTY_NOTICES.md)

![PhotoShip — 英語のインターフェース](docs/preview.png)

## 機能

| 分類 | 対応機能 |
| --- | --- |
| ドキュメントとレイヤー | ドキュメントタブ、入れ子グループ、複数選択、ドラッグによる並べ替え、一括複製・削除、隣接レイヤーの結合 |
| 描画と修整 | ブラシ、消しゴム、スポイト、タブレット筆圧、スタンプ、基本的な修復 |
| 選択と変形 | 矩形・楕円選択、なげなわ、連続領域の自動選択、選択ピクセルの移動・コピー、移動、拡縮、回転、反転、切り抜き |
| レイヤー合成 | 12 種類の描画モード、不透明度、ラスターマスク、クリッピングマスク、編集可能な文字と長方形・楕円 |
| 調整 | レベル補正、RGB カーブ、色相・彩度・明度、階調反転。調整レイヤーまたは直接ピクセル編集 |
| ファイル | PNG/JPEG の読み込み・書き出し、基本的な PSD と Compositor `.comp` の読み込み、編集可能な `.psproj` |
| 作業フロー | 元に戻す・やり直す、バックグラウンド保存・書き出し、自動復元、クリップボード、ドラッグ読み込み、表示領域のタイルキャッシュ |
| 言語 | 簡体字中国語、英語、日本語、韓国語、フランス語、ドイツ語、スペイン語。即時切り替えと設定の保存 |

現在のバージョン：**0.2.1**。Ubuntu ではローカルビルド、自動チェック、X11 起動確認が完了しています。Windows 用のビルド・パッケージ作成スクリプトはありますが、Windows 環境での検証が必要です。筆圧チェックは模擬イベントを使用し、実機の対応はデバイスとドライバーに依存します。

## 入手とインストール

### Ubuntu

ビルドの基準：Ubuntu 22.04/24.04 x86_64、Qt 6.2 以上、CMake 3.21 以上、C++17 コンパイラー。

```bash
git clone https://github.com/wolfoot/PhotoShip.git
cd PhotoShip
sudo apt update
sudo apt install qt6-base-dev qt6-qpa-plugins zlib1g-dev cmake ninja-build g++
./scripts/build-linux.sh
```

スクリプトはビルドとチェックを実行し、`dist/photoship-0.2.1-Linux.deb` を生成します：

```bash
sudo apt install ./dist/photoship-0.2.1-Linux.deb
photoship
```

DEB はシステムの Qt に動的リンクします。apt が必要な依存関係をインストールします。中日韓のフォントが不足する場合は `fonts-noto-cjk` をインストールしてください。オフラインでは実行ライブラリーとフォントを事前に用意します。

開発時はビルドディレクトリーから直接起動できます：

```bash
./scripts/run.sh
./scripts/run.sh --demo
./scripts/run.sh /absolute/path/to/project.psproj
```

### Windows

対象：Windows 10/11 x64。以下を用意してください：

- Visual Studio 2022 の **Desktop development with C++** ワークロード。
- CMake と Qt 6.8.3 の **MSVC 2022 64-bit** コンポーネント。
- PSD ZIP デコード用の vcpkg と `zlib:x64-windows-static-md`。
- Inno Setup 6。インストーラーの作成時のみ必要です。

プロジェクトディレクトリーで PowerShell を実行します：

```powershell
vcpkg install zlib:x64-windows-static-md
./scripts/build-windows.ps1 -QtPrefix 'C:\Qt\6.8.3\msvc2022_64' -ZlibToolchain 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
```

Qt 実行ライブラリーを含むポータブル版 `dist/PhotoShip-0.2.1-windows-x64.zip` が生成されます。展開して `photoship.exe` を実行してください。`-Installer` を追加すると `dist/PhotoShip-0.2.1-windows-x64-setup.exe` を生成し、既定では現在のユーザーのディレクトリーにインストールします。

リポジトリーの [GitHub Actions ワークフロー](.github/workflows/build.yml) は両プラットフォームのビルド、チェック、成果物アップロードを設定しています。成功した実行から成果物を取得できます。設定があるだけでは、プラットフォームの検証完了を意味しません。

## 使い方

1. ドキュメントを新規作成するか画像を開き、レイヤーパネルで編集するレイヤーを選びます。
2. 左のツールバーで描画・選択・変形します。右のプロパティパネルで正確な変形値を入力できます。
3. レイヤーメニューでマスク、クリッピング、結合を操作し、画像メニューで調整レイヤーを作成します。
4. **プロジェクトを保存**で編集可能な内容を保持し、**PNG / JPEG に書き出す**で結合画像を出力します。

スタンプ・修復ツールは、同じラスターレイヤー上で **Alt を押してクリック**し、元の位置を指定してから描画します。修復は局所的な RGB 色調合わせによる基本的な修整です。マスクでは黒が非表示、白が表示を表します。

結合には、同じ階層で隣接する「通常」描画モードのレイヤーが必要です。クリッピング・調整の依存関係や半透明の親グループがある場合、画像の変化を避けるため結合を拒否することがあります。グループは pass-through 合成で、調整レイヤーはグループ外の下の内容にも影響します。

### 言語

**Language / 语言** メニューで再起動せずに言語を切り替えます。初回はシステム言語に従い、未対応の場合は英語になります。切り替えても内容、レイヤー名、取り消し履歴は保持されます。

保存した設定を変更せず、今回の起動だけ言語を指定できます：

```bash
./scripts/run.sh --language ja
# en / zh_CN / ja / ko / fr / de / es
```

メニュー、パネル、編集ダイアログ、一般的なメッセージは翻訳されています。一部のパーサーや OS の診断は英語のままの場合があります。

### 主なショートカット

| 操作 | キー |
| --- | --- |
| 移動 / ブラシ / 消しゴム | V / B / E |
| スタンプ / 修復 | S / J, Alt クリックで元を指定 |
| 矩形 / 楕円 / なげなわ / 自動選択 | M / Shift+M / L / W |
| 切り抜き / 長方形 / 楕円形 | C / U / Shift+U |
| 文字 / スポイト / 手のひら | T / I / H |
| 表示移動 / ズーム | Space ドラッグまたは中ボタン / ホイール |
| ブラシサイズ | [ / ] |
| 元に戻す / やり直す | Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z |
| 新規 / 開く / 保存 / 別名保存 | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| 読み込み / 書き出し | Ctrl+Shift+O / Ctrl+Shift+E |
| レイヤー複製 / 選択レイヤー結合 | Ctrl+J / Ctrl+E |
| クリッピングマスク切り替え | Ctrl+Alt+G |
| 全選択 / 選択解除 | Ctrl+A / Ctrl+D |
| 全体表示 / 実際のピクセル | Ctrl+0 / Ctrl+1 |
| 筆画またはドラッグをキャンセル | Esc |

## ファイルとデータ保存

### 編集可能なプロジェクト

`.psproj` は `manifest.json` と `images/` を含むフォルダーです。移動・バックアップ時はフォルダー全体を保持してください。レイヤー、グループ、マスク、変形、文字、調整値を保存します。選択範囲、取り消し履歴、表示位置は保存しません。文字はローカルフォントに依存し、別のマシンでは代替フォントになる場合があります。

保存は新しいアセットを書き込み、マニフェストをアトミックに確定します。ファイルロックで同時書き込みを防ぎます。バックグラウンド保存は開始時点のスナップショットを記録し、その後の編集には再保存が必要です。書き出しではプロジェクトは保存済みになりません。

自動復元は編集停止の約 1.5 秒後にスナップショットを保存し、30 秒ごとにも確認します。起動時に復元、破棄、後で処理を選べます。元のプロジェクトを保持し、別の未保存ドキュメントとして復元します。書き込みが完了していない編集は復元できない場合があります。

PhotoShip は旧 Pixel Studio の形式識別子 `org.pixelstudio.project` と保存場所を継続し、既存プロジェクト、言語設定、復元データを保持します。形式バージョンは 2 で、バージョン 1 も読み込めます。復元先は `PHOTOSHIP_RECOVERY_DIR` で指定でき、旧 `PIXELSTUDIO_RECOVERY_DIR` も対応します。

### PSD と Compositor の読み込み

PSD v1、8-bit RGB、ラスターレイヤー、グループ、ラスターマスク、一部のクリッピング関係に対応します。チャンネルは raw、RLE、ZIP、ZIP 予測圧縮に対応します。読み込み前に互換性レポートを表示します。文字、ベクトル、スマートオブジェクト、効果、調整値はキャッシュ画像への変換や省略となる場合があります。PSD 書き出しは未対応です。

基本的な `.comp` 読み込みは、ラスターレイヤー、グループ、変形、対応する描画モード、リンクされたラスターマスクを扱います。未対応機能はエラーになります。元ファイルを保持するため `.psproj` として保存してください。

## 現在の制限

- CPU/QPainter で合成し、表示領域に 256×256 タイルを使います。一部のフィルター、PSD 解析、構造操作はメインスレッドで実行します。
- 一辺 8192 ピクセル、キャンバス全体で 1600 万ピクセルまで。元レイヤーは合計 3200 万、マスクは別枠で 3200 万ピクセルまで。256 レイヤー、PSD は 256 MiB まで。
- 取り消しは最大 100 ステップ、履歴ピクセルバッファーの予算は約 256 MiB です。画像、合成キャッシュ、バックグラウンドスナップショットは追加のメモリーを使用し、プロセス全体の上限ではありません。
- 作業空間は 8-bit sRGB です。PSB、RAW、16-bit/CMYK、ペンパス、リッチテキスト、高度なレイヤースタイル、グループマスク、ゆがみ、AI 切り抜きは未対応です。
- 基本の入出力は PNG/JPEG です。その他の形式は Qt プラグインに依存します。PSD 読み込みや描画モードは他のエディターとのピクセル単位の一致を保証しません。

## 開発と貢献

C++17、Qt 6 Widgets/Concurrent、zlib を使用します。アプリにネットワークサービスやアカウントは不要です。主なディレクトリー：

```text
src/                 界面、ドキュメント、合成、プロジェクト・PSD 入出力
assets/i18n/         内蔵言語カタログ
packaging/           デスクトップ項目、アイコン、インストーラー、ライセンス
scripts/             ビルド、起動、アイコン生成、X11 起動チェック
tests/               コア、v2 機能、言語チェック
third_party/         元のライセンスを保持するサードパーティコード
```

一般的なビルドとチェック：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

自動チェックは合成、選択、取り消し、プロジェクト入出力、PSD デコード、バックグラウンド保存、復元、言語切り替えを扱います。Xvfb をインストール後、X11 ウィンドウの起動チェックを実行できます：

```bash
python3 scripts/check-x11.py --language ja
```

オリジナルの **Layer Sail** アイコンは `packaging/photoship.svg` を元にし、7 サイズの PNG と Windows ICO を含みます。SVG 編集後は `python3 scripts/render-icons.py` で再生成します。Linux の librsvg、Cairo、Python Pillow が必要です。翻訳は `assets/i18n/*.json` にあり、変更後に再ビルドします。

[Issues](https://github.com/wolfoot/PhotoShip/issues) に再現手順、OS、Qt バージョン、サンプルを添えて報告してください。修正、翻訳、機能の Pull Request を歓迎します。既存形式の互換性を保持し、関連チェックを実行してください。提出するプロジェクト、画像、ログから個人情報を除いてください。

## ライセンスと謝辞

PhotoShip のコードは [MIT ライセンス](LICENSE) です。レイヤーの作業フローは Compositor を参考にし、その MIT ライセンスの自動選択・境界追跡コードを再利用しています。元の著作権とライセンスは [third_party/compositor/LICENSE](third_party/compositor/LICENSE) に保持しています。

Qt、Qt プラグイン、zlib には各自のライセンスがあり、アプリの MIT ライセンスはそれらを置き換えません。配布時はライセンス表示と要件を守ってください。[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。

PhotoShip は Adobe と関係のない独立プロジェクトで、Photoshop のコードやアセットを含みません。
