# ✨ Manxo - AI駆動型Maxパッチジェネレーター ✨

![Max Meets AI](https://img.shields.io/badge/Max%20Meets%20AI-%F0%9F%8E%B5%20%F0%9F%A4%96-blueviolet)
![Built with Love](https://img.shields.io/badge/Built%20with-%E2%9D%A4%EF%B8%8F-red)
![Kawaii Factor](https://img.shields.io/badge/Kawaii%20Factor-%E2%98%85%E2%98%85%E2%98%85%E2%98%85%E2%98%85-pink)

## 💫 プロジェクト概要

Manxoは、ClaudeのAI能力とMax/MSPの創造性を組み合わせた魔法のようなプロジェクト！🪄✨

自然言語からMaxパッチを自動生成するAI駆動型システムで、音楽制作やメディアアートの世界に革命を起こします。

### 🌟 主な機能

- 💬 **自然言語からパッチ生成**: 「ボイスエフェクト付きシンセサイザー作って」だけでパッチ完成！
- 🔍 **ベクトル検索**: 意味を理解するセマンティック検索でぴったりのオブジェクトを発見
- 🧩 **パターン認識**: よく使われる接続パターンを学習して最適な構成を提案
- 🛠️ **テンプレート**: オーディオ・ビデオ処理用の特製テンプレートを用意
- 🔄 **自己修正機能**: 生成したパッチを自動検証&修正

## 🚀 はじめ方

### 📋 必要なもの

- Node.js 14.x以上
- SQLite 3.x
- MCPサポート付きのClaude Desktop

### 💻 インストール方法

1. リポジトリをクローン:
   ```bash
   git clone https://github.com/yasunoritani/manxo.git
   cd manxo
   ```

2. 依存パッケージをインストール:
   ```bash
   npm install
   ```

### 🎮 使い方

#### 🪄 AIパッチジェネレーターを起動:

```bash
npm run start:generator
```

#### 🧠 SQLナレッジベースのみ起動:

```bash
npm run start:sql
```

#### 🔌 MCPサーバー全体を起動:

```bash
npm start
```

## 🛠️ MCPツール一覧

### 🔍 ナレッジベースツール

- `getMaxObject` → Maxオブジェクトの詳細情報を取得 📚
- `searchMaxObjects` → オブジェクトを名前や説明で検索 🔎
- `checkConnectionPattern` → オブジェクト間の接続を検証 ✅
- `searchConnectionPatterns` → よく使われる接続パターンを検索 🔄

### 🎨 パッチジェネレーターツール

- `generateMaxPatch` → テンプレートからパッチを生成 ✨
- `generateFromDescription` → 自然言語からパッチを生成 💬
- `listTemplates` → 利用可能なテンプレート一覧を取得 📋

## 💌 ライセンス

このプロジェクトはMITライセンスの下で公開されています。詳細はLICENSEファイルをご覧ください。

## 🙏 謝辞

このプロジェクトは、Max/MSPコミュニティの知恵と、AIの可能性を組み合わせることで実現しました。
すべての音楽家、アーティスト、開発者に感謝します！🎵✨
