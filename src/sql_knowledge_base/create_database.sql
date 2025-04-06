-- Max/MSP と Min-DevKit 知識ベースのSQLiteデータベーススキーマ
-- 作成日: 2023年

-- テーブルが既に存在する場合は削除
DROP TABLE IF EXISTS max_objects;
DROP TABLE IF EXISTS min_devkit_api;
DROP TABLE IF EXISTS connection_patterns;
DROP TABLE IF EXISTS validation_rules;
DROP TABLE IF EXISTS api_mapping;

-- Max/MSPオブジェクト情報テーブル
CREATE TABLE max_objects (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,  -- オブジェクト名（例: metro, jit.gl.render）
    description TEXT,           -- オブジェクトの説明
    category TEXT,              -- カテゴリ（例: time, ui, audio, video）
    version_compatibility TEXT, -- バージョン互換性情報
    inlets TEXT,                -- 入力口情報（JSON形式）
    outlets TEXT,               -- 出力口情報（JSON形式）
    is_ui_object INTEGER,       -- UIオブジェクトかどうか（0=false, 1=true）
    is_deprecated INTEGER,      -- 非推奨かどうか（0=false, 1=true）
    alternative TEXT,           -- 非推奨の場合の代替オブジェクト
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Min-DevKit API関数情報テーブル
CREATE TABLE min_devkit_api (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    function_name TEXT NOT NULL UNIQUE, -- 関数名
    signature TEXT,                     -- 関数シグネチャ
    return_type TEXT,                   -- 戻り値の型
    description TEXT,                   -- 関数の説明
    parameters TEXT,                    -- パラメータ情報（JSON形式）
    example_usage TEXT,                 -- 使用例
    version_compatibility TEXT,         -- バージョン互換性情報
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 接続パターン情報テーブル
CREATE TABLE connection_patterns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_object TEXT NOT NULL,       -- 送信元オブジェクト
    source_outlet INTEGER,             -- 送信元アウトレット番号
    destination_object TEXT NOT NULL,  -- 送信先オブジェクト
    destination_inlet INTEGER,         -- 送信先インレット番号
    description TEXT,                  -- 接続の説明
    is_recommended INTEGER,            -- 推奨パターンかどうか（0=false, 1=true）
    audio_signal_flow INTEGER,         -- オーディオシグナルフローかどうか（0=false, 1=true）
    performance_impact TEXT,           -- パフォーマンスへの影響
    compatibility_issues TEXT,         -- 互換性の問題
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(source_object, source_outlet, destination_object, destination_inlet)
);

-- 検証ルール情報テーブル
CREATE TABLE validation_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_type TEXT NOT NULL,        -- ルールタイプ（例: syntax, performance, connection）
    pattern TEXT NOT NULL,          -- 正規表現パターン
    description TEXT,               -- ルールの説明
    severity TEXT,                  -- 重要度（例: warning, error, info）
    suggestion TEXT,                -- 改善提案
    example_fix TEXT,               -- 修正例
    context_requirements TEXT,      -- 適用コンテキスト（例: patcher, msp, jitter）
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 自然言語意図からAPI関数へのマッピングテーブル
CREATE TABLE api_mapping (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    natural_language_intent TEXT NOT NULL, -- 自然言語による意図（例: "メトロノームを作成する"）
    min_devkit_function_id INTEGER,        -- 対応するMin-DevKit API関数ID
    transformation_template TEXT,          -- 変換テンプレート
    context_requirements TEXT,             -- コンテキスト要件
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (min_devkit_function_id) REFERENCES min_devkit_api(id)
);

-- インデックス作成
CREATE INDEX idx_max_objects_name ON max_objects(name);
CREATE INDEX idx_max_objects_category ON max_objects(category);
CREATE INDEX idx_min_devkit_api_function_name ON min_devkit_api(function_name);
CREATE INDEX idx_connection_patterns_source ON connection_patterns(source_object);
CREATE INDEX idx_connection_patterns_destination ON connection_patterns(destination_object);
CREATE INDEX idx_validation_rules_rule_type ON validation_rules(rule_type);
CREATE INDEX idx_api_mapping_intent ON api_mapping(natural_language_intent);
