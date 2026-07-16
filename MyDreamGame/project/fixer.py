import sys
import re

files = [
    r'Engine\Core\Utility\UtilityFunctions.cpp',
    r'Engine\Effect\ParticleManager.cpp',
    r'Engine\GameObject\Object3D.cpp',
    r'Engine\GameObject\PrimitiveObject.cpp',
    r'Engine\Resource\Sprite\Sprite.cpp'
]

# A dictionary of common mojibake recoveries -> actual Japanese
fixes = {
    'メチEージ': 'メッセージ',
    '固有E': '固有の',
    '処琁E': '処理',
    '終亁E': '終了',
    '標準E': '標準の',
    'チEチE': 'デバッグ',
    '作Eして': '作成して',
    '作E': '作成',
    '使ぁE': '使います',
    'スレチE': 'スレッド',
    'チEレクトリ': 'ディレクトリ',
    'Eコード': '文字コード',
    'E追記': '（追記',
    'Eみ行う': 'のみ行う',
    '止まらなぁE': '止まらない',
    '冁E': '中身',
    'E力': '出力',
    '関連付けられてぁE': '関連付けられている',
    'なぁE': 'ない',
    '斁Eコード': '文字コード',
    'E持E': 'の指定',
    '基本皁E': '基本的に',
    '行優允E': '行優先',
    '致命皁E': '致命的な',
    '警告Eエラー': '警告やエラー',
    'EてぁE': '出ているか',
    '確誁E': '確認',
    '斁EE': '文字列',
    '安E': '安全',
    'アチEEロード': 'アップロード',
    'ヒEプE': 'ヒープの',
    'チEEタ': 'データ',
    'バチEァE': 'バッファを',
    '次允E': '次元',
    'E次允E': '（２次元）',
    '配E': '配列',
    'ミップEチEE': 'ミップマップ',
    'Eける': '避ける',
    '忁E': '必要',
    '幁E': '幅',
    '持E1': '備考1',
    '持E2': '備考2',
    'E琁E': '処理を',
    '生E': '生成',
    '設宁E': '設定',
    '初期匁E': '初期化',
    '取征E': '取得',
    '頁E': '順',
    '吁E': '例えば',
    'Eァ': 'ファ',
    '菴懈・': '作成',
    '蝗櫁ｻ｢陦悟・': '回転行列',
    '・': 'の',
    '縺': '',
    'ｮ': '',
    '菴': '',
    '懈': '',
}

def clean(text):
    text = text.replace('E', 'の') # generic E -> の
    # We can do exact line replacements for safety!
    return text

def fix():
    # just manual hardcode the lines
    pass

if __name__ == '__main__':
    fix()
