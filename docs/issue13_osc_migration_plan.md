## Issue 13：既存OSC機能のMin-DevKit移行計画の詳細分析

### 1. 現在のアーキテクチャ（JavaScript/OSCベース）

- **3層アーキテクチャ**：
  - **MCPサーバー層**：Node.jsで実装（node.script）- 別プロセスで実行
  - **通信層**：OSCプロトコルによる双方向通信
  - **Max実行層**：v8ui JavaScript環境 - Maxプロセス内で実行

- **JavaScript実装の役割分担**：
  - **node.script**：完全なNode.js環境、npmパッケージ利用可能、ファイルシステム操作可能
  - **v8ui**：Max内部で実行、UI操作が直接可能、リアルタイム性に優れる

### 2. 目標アーキテクチャ（Min-DevKit/C++ベース）

- **3層アーキテクチャ**：
  - **インテリジェンスレイヤー**：外部Node.jsプロセス、MCPサーバー機能
  - **統合レイヤー**：Min-DevKitネイティブOSCブリッジ、ステート同期
  - **実行レイヤー**：Max内部APIへの直接アクセス

- **将来拡張計画**（現在凍結中）：
  - **インタラクションレイヤー**：C++ベースUIフレームワーク（必要性を再評価中）

- **Min-DevKitの利点**：
  - ネイティブC++実装によるパフォーマンス向上（目標50ms以下）
  - Max内部APIへの直接アクセス
  - マルチスレッド処理による並列化
  - コンパイル時の型安全性
  - Max 7.x/8.xバージョンでの広範な互換性
  - Max for Live（Ableton Live内）環境での動作保証

### 3. OSC機能移行における考慮事項

1. **C++実装で保持すべき機能**：
   - OSCメッセージの送受信機能
   - アドレスパターンベースのメッセージルーティング
   - 型変換（C++/OSC型間）
   - エラーハンドリングと再接続機能
   - 動的ポート管理（M4L環境での複数インスタンス対応）
   - スレッドセーフな非同期処理（LiveのUIブロッキング防止）

2. **既存のJavaScriptとの違い**：
   - **型システム**：動的型付け（JS）→静的型付け（C++）
   - **メモリ管理**：ガベージコレクション（JS）→明示的メモリ管理（C++）
   - **非同期処理**：Promise/async（JS）→スレッド/コールバック（C++）
   - **コード変更**：インタープリタ実行（JS）→コンパイル必要（C++）

3. **Min-DevKit OSC実装の技術的要件**：
   - **oscpack**ライブラリの利用（BSD-3-Clauseライセンス）
   - UDPソケット通信の実装（ローカルホスト優先）
   - スレッドセーフな状態管理
   - オブジェクトのライフサイクル管理
   - Max for Live環境での軽量処理（CPU/メモリ制約）
   - Liveセット状態との同期メカニズム

### 4. 移行戦略と実装計画

1. **段階的アプローチ**：
   - 最初にoscpackを使用したC++ベースのOSCクライアント・サーバークラスを実装
   - Max for Live環境（Ableton Live 10 Suite, Max 8.0.2/8.1.0）をプライマリターゲットとして開発
   - 既存JavaScriptコードと並行稼働させて動作検証
   - 安定性確認後に完全移行

2. **実装すべきコンポーネント**：
   - `mcp.osc_bridge.cpp`：OSC通信のメインオブジェクト
   - `mcp.osc_client.cpp`：OSC送信処理
   - `mcp.osc_server.cpp`：OSC受信とメッセージハンドリング
   - `mcp.osc_types.hpp`：型変換ユーティリティ

3. **Min-DevKit実装パターン**：
   ```cpp
   class mcp_osc_bridge : public object<mcp_osc_bridge> {
   public:
       MIN_DESCRIPTION {"OSC Bridge for MCP-Max integration"};
       MIN_TAGS        {"mcp", "osc", "communication"};
       MIN_AUTHOR      {"Your Name"};
       
       inlet<>  input  { this, "(anything) Command to send via OSC" };
       outlet<> output { this, "(anything) Received OSC messages" };
       
       // ホスト・ポート設定
       attribute<symbol> host { this, "host", "localhost" };
       attribute<number> port_in { this, "port_in", 7500 };
       attribute<number> port_out { this, "port_out", 7400 };
       
       // 接続状態
       attribute<bool> connected { this, "connected", false, readonly_property };
       
       // メッセージハンドラ
       message<> connect { this, "connect", "Connect OSC client/server",
           MIN_FUNCTION {
               // 接続処理
               return {};
           }
       };
       
       // OSCメッセージ送信
       message<> anything { this, "anything", "Send OSC message",
           MIN_FUNCTION {
               // OSC送信処理
               return {};
           }
       };
       
       // 非同期OSCメッセージ受信ハンドラ
       void handle_osc_message(const string& address, const atoms& args) {
           // スレッドセーフな処理のためにdeferを使用
           defer([this, address, args]() {
               output.send(address, args);
           });
       }
   };
   
   MIN_EXTERNAL(mcp_osc_bridge);
   ```

### 5. JavaScriptからC++への移行で対処すべき課題

1. **動的な処理の実装**：
   - JavaScriptの柔軟な動的コード生成・実行をC++でどう実現するか
   - 解決策：テンプレートパターンやコマンドパターンによる静的な実装

2. **非同期処理の扱い**：
   - JavaScriptのPromise/async-awaitに相当する処理
   - 解決策：スレッドプール、ワーカースレッド、コールバックシステム

3. **外部ライブラリとの統合**：
   - npmエコシステムの代替
   - 解決策：C++ライブラリの選定とCMake統合

4. **デバッグ・開発効率**：
   - コンパイルサイクルによる開発速度の低下
   - 解決策：ユニットテストの充実、モジュール分割

### 6. 必要な追加情報と調査項目

1. **OSC実装詳細**：
   - サードパーティOSCライブラリの評価結果（oscpackが最適）
   - UDP/TCPソケット管理と動的ポート割り当ての実装観点

2. **M4L環境の詳細情報**：
   - M4Lリソース使用量のベンチマーク
   - Live環境での外部通信制限の詳細
   - Max 8.0.2/8.1.0の互換性テスト結果

3. **マルチインスタンス対応**：
   - 複数のM4Lデバイスが一つのLiveセット内で共存する場合のリソース管理
   - ポート競合を回避する動的割り当てメカニズムの設計

4. **パフォーマンス計測**：
   - C++/oscpack実装のパフォーマンスをM4L環境で評価する方法
   - CPU/メモリ使用量と応答時間の計測

### 7. 技術的リスクと対策

1. **C++実装の複雑性**：
   - リスク：メモリ管理や低レベルなエラーへの対応が複雑化
   - 対策：単体テストの充実、メモリリーク検出ツールの利用

2. **適切なスレッド管理**：
   - リスク：デッドロックやレースコンディションが発生する可能性
   - 対策：スレッドセーフなデザインパターンの采用、同期プリミティブの適切な使用

3. **Max for Live環境での制約**：
   - リスク：M4L環境のリソース制約（CPU/メモリ）やUIスレッドブロッキング
   - 対策：バックグラウンドスレッドでの処理、動的ポート管理、軽量設計

4. **Ableton Liveセットとの状態管理**：
   - リスク：Liveセットの保存・読込みサイクルとの状態不整合
   - 対策：Liveのライフサイクルイベントへの適切な対応、プリセット互換性の確保
