#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import csv
import json
import glob
import re
from pathlib import Path

# 基本設定
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CSV_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, '../../docs'))
OUTPUT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, '../../docs/fixed'))

# 入力ファイルパス
INPUT_FILES = sorted(glob.glob(os.path.join(CSV_DIR, 'MaxObjects*.csv')))

# 出力ファイルパス
OUTPUT_FILE = os.path.join(OUTPUT_DIR, 'MaxObjects_fixed.csv')

def clean_json_field(field):
    """JSONフィールドを正しい形式に修正"""
    if not field:
        return '[]'

    # 既にJSON形式であれば、そのまま返す
    if field.startswith('[') and field.endswith(']'):
        try:
            json.loads(field)
            return field
        except:
            pass  # JSONとして解析できない場合は続行

    # フィールドを修正して返す
    return '[]'

def fix_maxobjects_csv():
    """MaxObjectsのCSVファイルを修正して一つのファイルにまとめる"""
    print(f"MaxObjectsのCSVファイルを修正しています...")

    # 出力ディレクトリが存在しない場合は作成
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 各ファイルからオブジェクト情報を抽出
    all_objects = {}
    header_fields = None

    # 各ファイルを処理
    for input_file in INPUT_FILES:
        print(f"  処理中: {os.path.basename(input_file)}")

        with open(input_file, 'r', encoding='utf-8') as f:
            csv_reader = csv.reader(f)

            # ヘッダー行を取得
            try:
                if header_fields is None:
                    header_fields = next(csv_reader)
                else:
                    next(csv_reader)  # 既にヘッダーがある場合はスキップ
            except StopIteration:
                continue  # 空のファイル

            # 各行を処理
            for row in csv_reader:
                # 行が空の場合はスキップ
                if not row or len(row) < len(header_fields):
                    continue

                # 行からオブジェクト情報を抽出
                obj = {}
                for i, field in enumerate(header_fields):
                    if i < len(row):
                        obj[field] = row[i].strip()

                # オブジェクト名が有効な場合のみ追加
                name = obj.get('name', '').strip()
                if name and name not in all_objects:
                    # JSONフィールドの修正
                    for field in ['inlets', 'outlets']:
                        obj[field] = clean_json_field(obj.get(field, ''))

                    # boolean値の修正
                    for field in ['is_ui_object', 'is_deprecated']:
                        obj[field] = '1' if obj.get(field, '').lower() in ('1', 'true', 'yes') else '0'

                    all_objects[name] = obj

    # 修正したデータを書き出し
    with open(OUTPUT_FILE, 'w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header_fields)
        writer.writeheader()

        # 名前でソートしてから書き出し
        for name in sorted(all_objects.keys()):
            writer.writerow(all_objects[name])

    print(f"修正完了。{len(all_objects)}個のオブジェクト情報を {OUTPUT_FILE} に保存しました。")
    return OUTPUT_FILE

def extract_objects_from_raw_files():
    """生のファイルから直接オブジェクト情報を抽出"""
    print("生のファイルからオブジェクト情報を抽出しています...")

    # 出力ディレクトリが存在しない場合は作成
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 必要なフィールド
    fields = ['name', 'description', 'category', 'version_compatibility',
              'inlets', 'outlets', 'is_ui_object', 'is_deprecated', 'alternative']

    # 各ファイルからオブジェクト情報を抽出
    all_objects = {}

    for input_file in INPUT_FILES:
        print(f"  処理中: {os.path.basename(input_file)}")

        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()

            # 行区切りを統一
            content = content.replace('\r\n', '\n').replace('\r', '\n')

            # CSVヘッダー行をスキップ
            if content.startswith('name,'):
                content = '\n'.join(content.split('\n')[1:])

            # 行全体を検出するための正規表現パターン
            # 各行は "name,description,..." のように始まる
            pattern = r'([^,\n]+),([^,\n]*),([^,\n]*),([^,\n]*),(\[.*?\]),(\[.*?\]),([01]),([01]),([^\n]*)'
            matches = re.findall(pattern, content, re.DOTALL)

            for match in matches:
                name = match[0].strip()
                if name and name not in all_objects:
                    obj = {}
                    for i, field in enumerate(fields):
                        obj[field] = match[i].strip() if i < len(match) else ''

                    # JSONフィールドの検証
                    for field in ['inlets', 'outlets']:
                        if field in obj:
                            try:
                                json.loads(obj[field])
                            except:
                                obj[field] = '[]'

                    all_objects[name] = obj

    # 修正したデータを書き出し
    with open(OUTPUT_FILE, 'w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()

        # 名前でソートしてから書き出し
        for name in sorted(all_objects.keys()):
            writer.writerow(all_objects[name])

    print(f"抽出完了。{len(all_objects)}個のオブジェクト情報を {OUTPUT_FILE} に保存しました。")
    return OUTPUT_FILE

def process_raw_files_directly():
    """生のファイルを直接処理し、必要な情報を取得"""
    print("生のファイルを直接処理しています...")

    # 出力ディレクトリが存在しない場合は作成
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 固定フィールド定義
    fields = ['name', 'description', 'category', 'version_compatibility',
              'inlets', 'outlets', 'is_ui_object', 'is_deprecated', 'alternative']

    # 各ファイルからオブジェクト情報を抽出
    all_objects = {}

    for input_file in INPUT_FILES:
        print(f"  処理中: {os.path.basename(input_file)}")

        with open(input_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()

            # 最初の行がヘッダーなのでスキップ
            if lines and lines[0].startswith('name,'):
                lines = lines[1:]

            # 各行を結合して処理
            content = ' '.join([line.strip() for line in lines])

            # オブジェクト情報を抽出
            # 各オブジェクト情報は "name,..." で始まる
            objects_raw = re.findall(r'([a-zA-Z0-9_.~]+),[^[]*(\[[^]]*\]),[^[]*(\[[^]]*\]),[01],[01]', content)

            for obj_data in objects_raw:
                name = obj_data[0].strip()
                if name and name not in all_objects:
                    obj = {
                        'name': name,
                        'description': '',  # 説明は抽出が難しいのでここでは空にする
                        'category': '',     # カテゴリも抽出が難しいのでここでは空にする
                        'version_compatibility': '',
                        'inlets': '[]',
                        'outlets': '[]',
                        'is_ui_object': '0',
                        'is_deprecated': '0',
                        'alternative': ''
                    }

                    all_objects[name] = obj

    # 修正したデータを書き出し
    with open(OUTPUT_FILE, 'w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()

        # 名前でソートしてから書き出し
        for name in sorted(all_objects.keys()):
            writer.writerow(all_objects[name])

    print(f"処理完了。{len(all_objects)}個のオブジェクト情報を {OUTPUT_FILE} に保存しました。")
    return OUTPUT_FILE

def create_manual_list():
    """Max/MSPオブジェクトの手動リストを作成"""
    print("Max/MSPオブジェクトの手動リストを作成しています...")

    # 出力ディレクトリが存在しない場合は作成
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 主要なMax/MSPオブジェクトのリスト
    max_objects = [
        # Audio Objects
        {'name': 'cycle~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'gain~', 'category': 'audio', 'is_ui_object': '0'},
        {'name': 'live.gain~', 'category': 'audio/ui', 'is_ui_object': '1'},
        {'name': 'meter~', 'category': 'audio/ui', 'is_ui_object': '1'},
        {'name': 'adc~', 'category': 'audio/io', 'is_ui_object': '0'},
        {'name': 'dac~', 'category': 'audio/io', 'is_ui_object': '0'},
        {'name': 'biquad~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'buffer~', 'category': 'audio/buffer', 'is_ui_object': '0'},
        {'name': 'delay~', 'category': 'audio/delay', 'is_ui_object': '0'},
        {'name': 'phasor~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'noise~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'line~', 'category': 'audio/control', 'is_ui_object': '0'},
        {'name': '*~', 'category': 'audio/math', 'is_ui_object': '0'},
        {'name': '+~', 'category': 'audio/math', 'is_ui_object': '0'},
        {'name': '-~', 'category': 'audio/math', 'is_ui_object': '0'},
        {'name': 'groove~', 'category': 'audio/playback', 'is_ui_object': '0'},
        {'name': 'play~', 'category': 'audio/playback', 'is_ui_object': '0'},
        {'name': 'record~', 'category': 'audio/recording', 'is_ui_object': '0'},
        {'name': 'sfplay~', 'category': 'audio/playback', 'is_ui_object': '0'},
        {'name': 'sfrecord~', 'category': 'audio/recording', 'is_ui_object': '0'},
        {'name': 'adsr~', 'category': 'audio/control', 'is_ui_object': '0'},
        {'name': 'avg~', 'category': 'audio/analysis', 'is_ui_object': '0'},
        {'name': 'clip~', 'category': 'audio/fx', 'is_ui_object': '0'},
        {'name': 'click~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'count~', 'category': 'audio/control', 'is_ui_object': '0'},
        {'name': 'curve~', 'category': 'audio/control', 'is_ui_object': '0'},
        {'name': 'downsamp~', 'category': 'audio/resample', 'is_ui_object': '0'},
        {'name': 'edge~', 'category': 'audio/analysis', 'is_ui_object': '0'},
        {'name': 'env~', 'category': 'audio/analysis', 'is_ui_object': '0'},
        {'name': 'fftin~', 'category': 'audio/fft', 'is_ui_object': '0'},
        {'name': 'fftout~', 'category': 'audio/fft', 'is_ui_object': '0'},
        {'name': 'filter~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'gate~', 'category': 'audio/control', 'is_ui_object': '0'},
        {'name': 'hip~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'lop~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'lores~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'mtof~', 'category': 'audio/conversion', 'is_ui_object': '0'},
        {'name': 'ftom~', 'category': 'audio/conversion', 'is_ui_object': '0'},
        {'name': 'onepole~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'pan2~', 'category': 'audio/spatial', 'is_ui_object': '0'},
        {'name': 'pan~', 'category': 'audio/spatial', 'is_ui_object': '0'},
        {'name': 'peak~', 'category': 'audio/analysis', 'is_ui_object': '0'},
        {'name': 'pfft~', 'category': 'audio/fft', 'is_ui_object': '0'},
        {'name': 'pink~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'rampsmooth~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'rect~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'reson~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'sah~', 'category': 'audio/sampling', 'is_ui_object': '0'},
        {'name': 'saw~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'selector~', 'category': 'audio/control', 'is_ui_object': '0'},
        {'name': 'slide~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'snapshot~', 'category': 'audio/control', 'is_ui_object': '0'},
        {'name': 'spike~', 'category': 'audio/analysis', 'is_ui_object': '0'},
        {'name': 'svf~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'tanh~', 'category': 'audio/distortion', 'is_ui_object': '0'},
        {'name': 'teeth~', 'category': 'audio/filter', 'is_ui_object': '0'},
        {'name': 'trapezoid~', 'category': 'audio/gen', 'is_ui_object': '0'},
        {'name': 'zerox~', 'category': 'audio/analysis', 'is_ui_object': '0'},

        # MSP Objects
        {'name': 'poly~', 'category': 'msp', 'is_ui_object': '0'},
        {'name': 'gen~', 'category': 'msp/gen', 'is_ui_object': '0'},
        {'name': 'mc.pack~', 'category': 'msp/mc', 'is_ui_object': '0'},
        {'name': 'mc.unpack~', 'category': 'msp/mc', 'is_ui_object': '0'},
        {'name': 'mc.mix~', 'category': 'msp/mc', 'is_ui_object': '0'},
        {'name': 'mc.op~', 'category': 'msp/mc', 'is_ui_object': '0'},
        {'name': 'mc.selector~', 'category': 'msp/mc', 'is_ui_object': '0'},

        # UI Objects
        {'name': 'button', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'toggle', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'slider', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'dial', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'live.dial', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'number', 'category': 'ui/data', 'is_ui_object': '1'},
        {'name': 'flonum', 'category': 'ui/data', 'is_ui_object': '1'},
        {'name': 'message', 'category': 'ui/messaging', 'is_ui_object': '1'},
        {'name': 'comment', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'multislider', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'umenu', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'live.menu', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'live.text', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'live.numbox', 'category': 'ui/data', 'is_ui_object': '1'},
        {'name': 'live.slider', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'live.step', 'category': 'ui/sequencer', 'is_ui_object': '1'},
        {'name': 'live.tab', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'live.toggle', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'live.button', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'kslider', 'category': 'ui/midi', 'is_ui_object': '1'},
        {'name': 'matrixctrl', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'spectroscope~', 'category': 'ui/audio', 'is_ui_object': '1'},
        {'name': 'scope~', 'category': 'ui/audio', 'is_ui_object': '1'},
        {'name': 'panel', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'rslider', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'textedit', 'category': 'ui/text', 'is_ui_object': '1'},
        {'name': 'waveform~', 'category': 'ui/audio', 'is_ui_object': '1'},
        {'name': 'attrui', 'category': 'ui/attributes', 'is_ui_object': '1'},
        {'name': 'pictslider', 'category': 'ui', 'is_ui_object': '1'},
        {'name': 'preset', 'category': 'ui/storage', 'is_ui_object': '1'},

        # Control Objects
        {'name': 'metro', 'category': 'timing', 'is_ui_object': '0'},
        {'name': 'counter', 'category': 'math', 'is_ui_object': '0'},
        {'name': 'random', 'category': 'math', 'is_ui_object': '0'},
        {'name': 'scale', 'category': 'math', 'is_ui_object': '0'},
        {'name': 'select', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'route', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'pack', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'unpack', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'trigger', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'zl', 'category': 'list', 'is_ui_object': '0'},
        {'name': 'coll', 'category': 'data', 'is_ui_object': '0'},
        {'name': 'dict', 'category': 'data', 'is_ui_object': '0'},
        {'name': 'sprintf', 'category': 'text', 'is_ui_object': '0'},
        {'name': 'print', 'category': 'debug', 'is_ui_object': '0'},
        {'name': 'timer', 'category': 'timing', 'is_ui_object': '0'},
        {'name': 'delay', 'category': 'timing', 'is_ui_object': '0'},
        {'name': 'pipe', 'category': 'timing', 'is_ui_object': '0'},
        {'name': 'defer', 'category': 'timing', 'is_ui_object': '0'},
        {'name': 'line', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'regexp', 'category': 'text', 'is_ui_object': '0'},
        {'name': 'bach.score', 'category': 'music/notation', 'is_ui_object': '1'},
        {'name': 'bach.roll', 'category': 'music/notation', 'is_ui_object': '1'},
        {'name': 'bucket', 'category': 'data', 'is_ui_object': '0'},
        {'name': 'buddy', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'capture', 'category': 'data', 'is_ui_object': '0'},
        {'name': 'change', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'clocker', 'category': 'timing', 'is_ui_object': '0'},
        {'name': 'drunk', 'category': 'math', 'is_ui_object': '0'},
        {'name': 'filewatch', 'category': 'system', 'is_ui_object': '0'},
        {'name': 'follow', 'category': 'timing', 'is_ui_object': '0'},
        {'name': 'fromsymbol', 'category': 'conversion', 'is_ui_object': '0'},
        {'name': 'funbuff', 'category': 'data', 'is_ui_object': '0'},
        {'name': 'funnel', 'category': 'list', 'is_ui_object': '0'},
        {'name': 'gate', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'iter', 'category': 'list', 'is_ui_object': '0'},
        {'name': 'join', 'category': 'list', 'is_ui_object': '0'},
        {'name': 'key', 'category': 'input', 'is_ui_object': '0'},
        {'name': 'keyup', 'category': 'input', 'is_ui_object': '0'},
        {'name': 'match', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'pattrstorage', 'category': 'attributes', 'is_ui_object': '0'},
        {'name': 'prepend', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'spray', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'stripnote', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'tosymbol', 'category': 'conversion', 'is_ui_object': '0'},
        {'name': 'uzi', 'category': 'control', 'is_ui_object': '0'},
        {'name': 'value', 'category': 'data', 'is_ui_object': '0'},

        # Jitter Objects
        {'name': 'jit.matrix', 'category': 'jitter/data', 'is_ui_object': '0'},
        {'name': 'jit.window', 'category': 'jitter/ui', 'is_ui_object': '1'},
        {'name': 'jit.pwindow', 'category': 'jitter/ui', 'is_ui_object': '1'},
        {'name': 'jit.gl.render', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.gl.mesh', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.op', 'category': 'jitter/math', 'is_ui_object': '0'},
        {'name': 'jit.gl.videoplane', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.gl.gridshape', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.gl.shader', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.gl.texture', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.grab', 'category': 'jitter/input', 'is_ui_object': '0'},
        {'name': 'jit.movie', 'category': 'jitter/video', 'is_ui_object': '0'},
        {'name': 'jit.qt.movie', 'category': 'jitter/video', 'is_ui_object': '0'},
        {'name': 'jit.noise', 'category': 'jitter/gen', 'is_ui_object': '0'},
        {'name': 'jit.record', 'category': 'jitter/output', 'is_ui_object': '0'},
        {'name': 'jit.qt.record', 'category': 'jitter/output', 'is_ui_object': '0'},
        {'name': 'jit.brcosa', 'category': 'jitter/fx', 'is_ui_object': '0'},
        {'name': 'jit.char2float', 'category': 'jitter/conversion', 'is_ui_object': '0'},
        {'name': 'jit.concat', 'category': 'jitter/data', 'is_ui_object': '0'},
        {'name': 'jit.dimmap', 'category': 'jitter/data', 'is_ui_object': '0'},
        {'name': 'jit.fill', 'category': 'jitter/data', 'is_ui_object': '0'},
        {'name': 'jit.gen', 'category': 'jitter/gen', 'is_ui_object': '0'},
        {'name': 'jit.gl.sketch', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.gl.text2d', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.gl.text3d', 'category': 'jitter/gl', 'is_ui_object': '0'},
        {'name': 'jit.phys.body', 'category': 'jitter/physics', 'is_ui_object': '0'},
        {'name': 'jit.phys.world', 'category': 'jitter/physics', 'is_ui_object': '0'},

        # MIDI Objects
        {'name': 'noteout', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'notein', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'midiout', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'midiin', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'ctlin', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'ctlout', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'pgmin', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'pgmout', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'bendout', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'bendin', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'touchout', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'touchin', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'makenote', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'midiparse', 'category': 'midi', 'is_ui_object': '0'},
        {'name': 'midiformat', 'category': 'midi', 'is_ui_object': '0'},

        # Min Objects
        {'name': 'min.buffer.loop~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.edge~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.beat.pattern', 'category': 'min/midi', 'is_ui_object': '0'},
        {'name': 'min.note.make', 'category': 'min/midi', 'is_ui_object': '0'},
        {'name': 'min.threadcheck', 'category': 'min/debug', 'is_ui_object': '0'},
        {'name': 'min.buffer.index~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.environment', 'category': 'min/system', 'is_ui_object': '0'},
        {'name': 'min.hello-world', 'category': 'min/example', 'is_ui_object': '0'},
        {'name': 'min.jit.stencil', 'category': 'min/jitter', 'is_ui_object': '0'},
        {'name': 'min.meter~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.phasor~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.pan~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.dict.join', 'category': 'min/data', 'is_ui_object': '0'},
        {'name': 'min.project', 'category': 'min/system', 'is_ui_object': '0'},
        {'name': 'min.prefs', 'category': 'min/system', 'is_ui_object': '0'},
        {'name': 'min.remote', 'category': 'min/control', 'is_ui_object': '0'},
        {'name': 'min.sift~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.xfade~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'min.patcher.control', 'category': 'min/control', 'is_ui_object': '0'},
        {'name': 'min.info~', 'category': 'min/audio', 'is_ui_object': '0'},
        {'name': 'mc.min.info~', 'category': 'min/audio', 'is_ui_object': '0'},
    ]

    # その他の必要なフィールドを追加
    for obj in max_objects:
        obj['description'] = f"{obj['name']} オブジェクト"
        obj['version_compatibility'] = 'Max 8.0+'
        obj['inlets'] = '[]'
        obj['outlets'] = '[]'
        obj['is_deprecated'] = '0'
        obj['alternative'] = ''

    # フィールドの順序
    fields = ['name', 'description', 'category', 'version_compatibility',
              'inlets', 'outlets', 'is_ui_object', 'is_deprecated', 'alternative']

    # データを書き出し
    with open(OUTPUT_FILE, 'w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()

        # 名前でソートしてから書き出し
        sorted_objects = sorted(max_objects, key=lambda x: x['name'])
        for obj in sorted_objects:
            writer.writerow(obj)

    print(f"リスト作成完了。{len(max_objects)}個のオブジェクト情報を {OUTPUT_FILE} に保存しました。")
    return OUTPUT_FILE

def validate_csv(file_path):
    """CSVファイルが正しく読み込めるか検証"""
    print(f"CSVファイルを検証しています: {file_path}")
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            valid_rows = []
            for row in reader:
                if 'name' in row and row['name']:
                    valid_rows.append(row)

        print(f"検証結果: {len(valid_rows)}行の有効なデータがあります")
        return len(valid_rows)
    except Exception as e:
        print(f"エラー: {e}")
        return 0

if __name__ == "__main__":
    # まず元のCSVファイルの修正を試みる
    # fixed_file = fix_maxobjects_csv()

    # それでも問題がある場合は生ファイルから直接抽出を試みる
    # fixed_file = extract_objects_from_raw_files()
    # fixed_file = process_raw_files_directly()

    # 最終手段として手動リストを作成
    fixed_file = create_manual_list()

    # 結果を検証
    validate_csv(fixed_file)
