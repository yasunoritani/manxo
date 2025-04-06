#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import csv
import sqlite3
import sys
import json
import glob
from pathlib import Path

# 基本設定
DB_NAME = 'max_claude_kb.db'  # DB名を修正
CSV_DIR = '../../docs'
FIXED_CSV_DIR = '../../docs/fixed'
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(SCRIPT_DIR, DB_NAME)
CSV_PATH = os.path.normpath(os.path.join(SCRIPT_DIR, CSV_DIR))
FIXED_CSV_PATH = os.path.normpath(os.path.join(SCRIPT_DIR, FIXED_CSV_DIR))

# CSVファイルパス定義（実際のファイル名に合わせる）
CSV_FILES = {
    'max_objects': os.path.join(FIXED_CSV_PATH, 'MaxObjects_fixed.csv'),  # 修正したファイルを使用
    'min_devkit_api': os.path.join(CSV_PATH, 'Min-DevKit_API.csv'),
    'connection_patterns': os.path.join(CSV_PATH, 'ConnectionPatterns.csv'),
    'validation_rules': os.path.join(CSV_PATH, 'ValidationRules.csv'),
    'api_mapping': os.path.join(CSV_PATH, 'API_Intent_Mapping.csv')
}

def create_database():
    """データベースを初期化し、スキーマを作成する"""
    print(f"データベースを初期化: {DB_PATH}")

    # 既存のDBファイルを削除（存在する場合）
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)

    # 新しいDBに接続
    conn = sqlite3.connect(DB_PATH)

    # SQLスクリプトを読み込んで実行
    schema_path = os.path.join(SCRIPT_DIR, 'create_database.sql')
    with open(schema_path, 'r', encoding='utf-8') as f:
        sql_script = f.read()
        conn.executescript(sql_script)

    conn.commit()
    print("データベーススキーマを作成しました")
    return conn

def import_max_objects(conn, csv_path):
    """Maxオブジェクト情報をインポート"""
    print(f"Maxオブジェクト情報をインポート: {csv_path}")

    cursor = conn.cursor()
    # 処理済みの名前を記録するセット
    processed_names = set()
    imported_count = 0

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for i, row in enumerate(reader, 1):
            # 重複チェック
            name = row.get('name', '')
            if not name or name in processed_names:
                print(f"  重複または無効な名前をスキップ: {name}")
                continue

            processed_names.add(name)

            try:
                # BOOLEAN値を適切に変換
                is_ui_object = 1 if row.get('is_ui_object', '0') in ('1', 'true', 'True', 'TRUE') else 0
                is_deprecated = 1 if row.get('is_deprecated', '0') in ('1', 'true', 'True', 'TRUE') else 0

                # 互換性情報の処理
                version_compatibility = row.get('version_compatibility', '')
                # 元のファイルにmin_max_versionとmax_max_versionが含まれている場合の処理
                if 'min_max_version' in row and 'max_max_version' in row:
                    version_compatibility = f"{row.get('min_max_version', '')} - {row.get('max_max_version', '')}"

                # 入出力情報の処理
                inlets = row.get('inlets', '[]')
                outlets = row.get('outlets', '[]')

                # NULLやデフォルト値の処理
                alternative = row.get('alternative', row.get('recommended_alternative', None))
                if alternative in ('NULL', '', None):
                    alternative = None

                # データ挿入（スキーマに合わせる）
                cursor.execute('''
                    INSERT INTO max_objects (
                        name, description, category, version_compatibility,
                        inlets, outlets, is_ui_object, is_deprecated, alternative
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    name,
                    row.get('description', ''),
                    row.get('category', ''),
                    version_compatibility,
                    inlets,
                    outlets,
                    is_ui_object,
                    is_deprecated,
                    alternative
                ))
                imported_count += 1
            except sqlite3.IntegrityError as e:
                print(f"  インポートエラー ({name}): {e}")
                continue
            except Exception as e:
                print(f"  処理エラー ({name}): {e}")
                continue

    conn.commit()
    print(f"{imported_count}件のMaxオブジェクト情報をインポートしました")

def import_min_devkit_api(conn, csv_path):
    """MinDevKit API情報をインポート（元の大規模CSVファイル形式に合わせる）"""
    print(f"MinDevKit API情報をインポート: {csv_path}")

    cursor = conn.cursor()
    # 処理済みの関数名を記録するセット
    processed_functions = set()
    imported_count = 0

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for i, row in enumerate(reader, 1):
            # 重複チェック
            function_name = row.get('function_name', '')
            if not function_name or function_name in processed_functions:
                print(f"  重複または無効な関数名をスキップ: {function_name}")
                continue

            processed_functions.add(function_name)

            try:
                # 互換性情報の処理
                version_compatibility = row.get('version_compatibility', '')
                # 元のファイルにmin_versionとmax_versionが含まれている場合の処理
                if 'min_version' in row and 'max_version' in row:
                    version_compatibility = f"{row.get('min_version', '')} - {row.get('max_version', '')}"

                # データ挿入（スキーマに合わせる）
                cursor.execute('''
                    INSERT INTO min_devkit_api (
                        function_name, signature, return_type, description,
                        parameters, example_usage, version_compatibility
                    ) VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', (
                    function_name,
                    row.get('signature', ''),
                    row.get('return_type', ''),
                    row.get('description', ''),
                    row.get('parameters', '[]'),
                    row.get('example_usage', ''),
                    version_compatibility
                ))
                imported_count += 1
            except sqlite3.IntegrityError as e:
                print(f"  インポートエラー ({function_name}): {e}")
                continue

    conn.commit()
    print(f"{imported_count}件のMinDevKit API情報をインポートしました")

def import_connection_patterns(conn, csv_path):
    """接続パターン情報をインポート（ファイルが存在する場合のみ）"""
    if not os.path.exists(csv_path):
        print(f"警告: 接続パターンファイルが見つかりません: {csv_path}")
        return

    print(f"接続パターン情報をインポート: {csv_path}")

    cursor = conn.cursor()
    # 処理済みの接続パターンを記録するセット
    processed_patterns = set()
    imported_count = 0

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for i, row in enumerate(reader, 1):
            try:
                # BOOLEAN値を適切に変換
                is_recommended = 1 if row.get('is_recommended', '0') in ('1', 'true', 'True', 'TRUE') else 0
                audio_signal_flow = 1 if row.get('audio_signal_flow', '0') in ('1', 'true', 'True', 'TRUE') else 0

                # 数値型への変換
                try:
                    source_outlet = int(row.get('source_outlet', 0))
                except (ValueError, TypeError):
                    source_outlet = 0

                try:
                    destination_inlet = int(row.get('destination_inlet', 0))
                except (ValueError, TypeError):
                    destination_inlet = 0

                # 重複チェック用キー
                pattern_key = (row.get('source_object', ''), source_outlet, row.get('destination_object', ''), destination_inlet)
                if not pattern_key[0] or not pattern_key[2] or pattern_key in processed_patterns:
                    print(f"  重複または無効な接続パターンをスキップ: {pattern_key}")
                    continue

                processed_patterns.add(pattern_key)

                # データ挿入
                cursor.execute('''
                    INSERT INTO connection_patterns (
                        source_object, source_outlet, destination_object, destination_inlet,
                        description, is_recommended, audio_signal_flow, performance_impact, compatibility_issues
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    row['source_object'],
                    source_outlet,
                    row['destination_object'],
                    destination_inlet,
                    row.get('description', ''),
                    is_recommended,
                    audio_signal_flow,
                    row.get('performance_impact', ''),
                    row.get('compatibility_issues', '')
                ))
                imported_count += 1
            except sqlite3.IntegrityError as e:
                print(f"  インポートエラー ({row.get('source_object', '')}->{row.get('destination_object', '')}): {e}")
                continue

    conn.commit()
    print(f"{imported_count}件の接続パターン情報をインポートしました")

def import_validation_rules(conn, csv_path):
    """検証ルール情報をインポート（ファイルが存在する場合のみ）"""
    if not os.path.exists(csv_path):
        print(f"警告: 検証ルールファイルが見つかりません: {csv_path}")
        return

    print(f"検証ルール情報をインポート: {csv_path}")

    cursor = conn.cursor()
    # 処理済みのルールパターンを記録するセット
    processed_patterns = set()
    imported_count = 0

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for i, row in enumerate(reader, 1):
            try:
                # 重複チェック用キー
                rule_key = (row.get('rule_type', ''), row.get('pattern', ''))
                if not rule_key[0] or not rule_key[1] or rule_key in processed_patterns:
                    print(f"  重複または無効な検証ルールをスキップ: {rule_key}")
                    continue

                processed_patterns.add(rule_key)

                # データ挿入
                cursor.execute('''
                    INSERT INTO validation_rules (
                        rule_type, pattern, description, severity, suggestion, example_fix, context_requirements
                    ) VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', (
                    row['rule_type'],
                    row['pattern'],
                    row.get('description', ''),
                    row.get('severity', 'warning'),
                    row.get('suggestion', ''),
                    row.get('example_fix', ''),
                    row.get('context_requirements', '')
                ))
                imported_count += 1
            except sqlite3.IntegrityError as e:
                print(f"  インポートエラー ({row.get('rule_type', '')}-{row.get('pattern', '')}): {e}")
                continue

    conn.commit()
    print(f"{imported_count}件の検証ルール情報をインポートしました")

def import_api_mapping(conn, csv_path):
    """API意図マッピング情報をインポート（ファイルが存在する場合のみ）"""
    if not os.path.exists(csv_path):
        print(f"警告: API意図マッピングファイルが見つかりません: {csv_path}")
        return

    print(f"API意図マッピング情報をインポート: {csv_path}")

    cursor = conn.cursor()
    cursor_lookup = conn.cursor()
    # 処理済みの意図を記録するセット
    processed_intents = set()
    imported_count = 0

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for i, row in enumerate(reader, 1):
            try:
                # 重複チェック
                intent = row.get('natural_language_intent', '')
                if not intent or intent in processed_intents:
                    print(f"  重複または無効な意図をスキップ: {intent}")
                    continue

                processed_intents.add(intent)

                # 関数IDを取得（関数名からの検索を試みる）
                function_name = row.get('min_devkit_function', row.get('min_devkit_function_id', ''))
                function_id = None

                if function_name and not function_name.isdigit():
                    # 関数名として解釈し、IDを検索
                    cursor_lookup.execute(
                        "SELECT id FROM min_devkit_api WHERE function_name = ?",
                        (function_name,)
                    )
                    result = cursor_lookup.fetchone()
                    if result:
                        function_id = result[0]
                elif function_name and function_name.isdigit():
                    function_id = int(function_name)

                # データ挿入
                cursor.execute('''
                    INSERT INTO api_mapping (
                        natural_language_intent, min_devkit_function_id,
                        transformation_template, context_requirements
                    ) VALUES (?, ?, ?, ?)
                ''', (
                    intent,
                    function_id,
                    row.get('transformation_template', ''),
                    row.get('context_requirements', '')
                ))
                imported_count += 1
            except sqlite3.IntegrityError as e:
                print(f"  インポートエラー ({intent}): {e}")
                continue

    conn.commit()
    print(f"{imported_count}件のAPI意図マッピング情報をインポートしました")

def check_database(conn):
    """データベースの内容をチェック"""
    print("\nデータベース内容の確認:")

    cursor = conn.cursor()

    # 各テーブルの行数を取得
    tables = [
        'max_objects', 'min_devkit_api', 'connection_patterns',
        'validation_rules', 'api_mapping'
    ]

    for table in tables:
        cursor.execute(f"SELECT COUNT(*) FROM {table}")
        count = cursor.fetchone()[0]
        print(f"テーブル {table}: {count}行")

    print("\nデータベースインポート完了！")

def main():
    """メイン処理"""
    try:
        # コマンドライン引数から接続パターンファイルを取得
        custom_connection_patterns_path = None
        if len(sys.argv) > 1:
            custom_path = sys.argv[1]
            if os.path.exists(custom_path):
                custom_connection_patterns_path = custom_path
                print(f"カスタム接続パターンファイルを使用: {custom_path}")
            else:
                print(f"警告: 指定されたファイルが存在しません: {custom_path}")

        # データベース作成
        conn = create_database()

        # 各CSVをインポート（存在するファイルのみ）
        if os.path.exists(CSV_FILES['max_objects']):
            import_max_objects(conn, CSV_FILES['max_objects'])
        else:
            print(f"警告: ファイルが見つかりません: {CSV_FILES['max_objects']}")

        if os.path.exists(CSV_FILES['min_devkit_api']):
            import_min_devkit_api(conn, CSV_FILES['min_devkit_api'])
        else:
            print(f"警告: ファイルが見つかりません: {CSV_FILES['min_devkit_api']}")

        # カスタム接続パターンファイルパスがある場合はそれを使用、なければデフォルトを使用
        connection_patterns_path = custom_connection_patterns_path or CSV_FILES['connection_patterns']
        if os.path.exists(connection_patterns_path):
            import_connection_patterns(conn, connection_patterns_path)
        else:
            print(f"警告: ファイルが見つかりません: {connection_patterns_path}")

        if os.path.exists(CSV_FILES['validation_rules']):
            import_validation_rules(conn, CSV_FILES['validation_rules'])
        else:
            print(f"警告: ファイルが見つかりません: {CSV_FILES['validation_rules']}")

        if os.path.exists(CSV_FILES['api_mapping']):
            import_api_mapping(conn, CSV_FILES['api_mapping'])
        else:
            print(f"警告: ファイルが見つかりません: {CSV_FILES['api_mapping']}")

        # データベース内容を確認
        check_database(conn)

        # 接続を閉じる
        conn.close()

    except Exception as e:
        print(f"エラーが発生しました: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
