# Max 9 - Claude Desktop MCP連携 エラーコード定義

本ドキュメントでは、Max 9とClaude Desktop MCP連携における標準エラーコードを定義します。

## エラーコードの分類

| エラーコード範囲 | カテゴリ | 説明 |
|------------------|----------|------|
| 100-199 | 通信エラー | OSC通信に関するエラー |
| 200-299 | パッチエラー | パッチ操作に関するエラー |
| 300-399 | オブジェクトエラー | オブジェクト操作に関するエラー |
| 400-499 | パラメータエラー | パラメータ操作に関するエラー |
| 500-599 | システムエラー | システム全般に関するエラー |

## 通信エラー (100-199)

| コード | 名称 | 説明 |
|--------|------|------|
| 100 | COMMUNICATION_ERROR | 一般的な通信エラー |
| 101 | CONNECTION_REFUSED | 接続が拒否された |
| 102 | HOST_UNREACHABLE | ホストに到達できない |
| 103 | TIMEOUT | 通信タイムアウト |
| 104 | INVALID_ADDRESS | 無効なアドレスパターン |
| 105 | INVALID_ARGUMENTS | 無効な引数 |
| 106 | MESSAGE_TOO_LARGE | メッセージサイズが大きすぎる |
| 107 | PROTOCOL_ERROR | プロトコルエラー |
| 108 | UNSUPPORTED_PROTOCOL | サポートされていないプロトコルバージョン |
| 109 | CONNECTION_LOST | 接続が切断された |

## パッチエラー (200-299)

| コード | 名称 | 説明 |
|--------|------|------|
| 200 | PATCH_ERROR | 一般的なパッチエラー |
| 201 | PATCH_NOT_FOUND | パッチが見つからない |
| 202 | PATCH_ALREADY_EXISTS | パッチが既に存在する |
| 203 | PATCH_CREATION_FAILED | パッチの作成に失敗した |
| 204 | PATCH_SAVE_FAILED | パッチの保存に失敗した |
| 205 | PATCH_LOAD_FAILED | パッチの読み込みに失敗した |
| 206 | PATCH_CLOSE_FAILED | パッチを閉じるのに失敗した |
| 207 | INVALID_PATCH_FORMAT | 無効なパッチ形式 |
| 208 | PATCH_LOCKED | パッチがロックされている |
| 209 | PATCH_PERMISSION_DENIED | パッチへのアクセス権限がない |

## オブジェクトエラー (300-399)

| コード | 名称 | 説明 |
|--------|------|------|
| 300 | OBJECT_ERROR | 一般的なオブジェクトエラー |
| 301 | OBJECT_NOT_FOUND | オブジェクトが見つからない |
| 302 | OBJECT_CREATION_FAILED | オブジェクトの作成に失敗した |
| 303 | OBJECT_DELETION_FAILED | オブジェクトの削除に失敗した |
| 304 | CONNECTION_FAILED | オブジェクト間の接続に失敗した |
| 305 | DISCONNECTION_FAILED | オブジェクト間の接続解除に失敗した |
| 306 | INVALID_OBJECT_TYPE | 無効なオブジェクトタイプ |
| 307 | INVALID_INLET | 無効なインレット |
| 308 | INVALID_OUTLET | 無効なアウトレット |
| 309 | OBJECT_MOVE_FAILED | オブジェクトの移動に失敗した |
| 310 | OBJECT_RESIZE_FAILED | オブジェクトのリサイズに失敗した |
| 311 | INCOMPATIBLE_CONNECTION | 互換性のない接続 |
| 312 | CIRCULAR_CONNECTION | 循環接続の検出 |

## パラメータエラー (400-499)

| コード | 名称 | 説明 |
|--------|------|------|
| 400 | PARAMETER_ERROR | 一般的なパラメータエラー |
| 401 | PARAMETER_NOT_FOUND | パラメータが見つからない |
| 402 | INVALID_PARAMETER_VALUE | 無効なパラメータ値 |
| 403 | PARAMETER_SET_FAILED | パラメータの設定に失敗した |
| 404 | PARAMETER_GET_FAILED | パラメータの取得に失敗した |
| 405 | PARAMETER_WATCH_FAILED | パラメータの監視に失敗した |
| 406 | PARAMETER_UNWATCH_FAILED | パラメータの監視解除に失敗した |
| 407 | PARAMETER_OUT_OF_RANGE | パラメータが範囲外 |
| 408 | READ_ONLY_PARAMETER | 読み取り専用パラメータ |
| 409 | WRITE_ONLY_PARAMETER | 書き込み専用パラメータ |

## システムエラー (500-599)

| コード | 名称 | 説明 |
|--------|------|------|
| 500 | SYSTEM_ERROR | 一般的なシステムエラー |
| 501 | SYSTEM_INIT_FAILED | システム初期化に失敗した |
| 502 | SYSTEM_SHUTDOWN_FAILED | システム終了に失敗した |
| 503 | INSUFFICIENT_RESOURCES | リソース不足 |
| 504 | OUT_OF_MEMORY | メモリ不足 |
| 505 | FILE_SYSTEM_ERROR | ファイルシステムエラー |
| 506 | ACCESS_DENIED | アクセス拒否 |
| 507 | INTERNAL_ERROR | 内部エラー |
| 508 | NOT_IMPLEMENTED | 実装されていない機能 |
| 509 | INCOMPATIBLE_VERSION | 互換性のないバージョン |
| 510 | CONFIG_ERROR | 設定エラー |
| 511 | SESSION_ERROR | セッション管理エラー |
| 512 | STATE_SYNC_ERROR | 状態同期エラー |
| 513 | AUTHENTICATION_ERROR | 認証エラー |
| 514 | PERMISSION_DENIED | 許可されていない操作 |
| 515 | RATE_LIMIT_EXCEEDED | レート制限を超過 |

## エラー処理ガイドライン

1. **エラーレスポンスの送信**:
   エラーが発生した場合、以下の形式でエラーレスポンスを送信すること：
   ```
   /max/error/[カテゴリ]/[アクション] [request_id] [エラーコード] [エラーメッセージ]
   ```

2. **エラーの記録**:
   すべてのエラーは適切にログに記録し、必要に応じてデバッグ情報を含めること。

3. **リカバリー手順**:
   重大なエラーからの回復手順を実装すること。例えば、接続が切断された場合（コード109）は自動的に再接続を試みるべき。

4. **エラーの詳細な情報**:
   エラーメッセージにはできるだけ具体的な情報を含め、問題の診断と解決を容易にすること。

5. **エラーの優先度**:
   エラーに優先度を設定し、クリティカルなエラーは即座に対応する必要があることを示すこと。

## カスタムエラーコードの追加

新しいエラーコードを追加する場合は、以下のガイドラインに従ってください：

1. 適切なカテゴリに追加すること
2. 既存のコードと衝突しないこと
3. 意味が明確であること
4. ドキュメントに追加して他の開発者に共有すること