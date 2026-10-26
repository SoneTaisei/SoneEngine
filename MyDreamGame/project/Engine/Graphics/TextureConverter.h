#pragma once
#include <string>

/// <summary>
/// テクスチャ事前変換・自動キャッシュ管理クラス
/// PNG/JPG等の画像を、GPUネイティブな圧縮フォーマット（BC7）およびミップマップ付きDDSに変換します。
/// </summary>
class TextureConverter {
public:
    /// <summary>
    /// 画像ファイル（PNG/JPG等）をDDS（BC7 / ミップマップ付き）に変換して保存する
    /// </summary>
    /// <param name="inputFilePath">元画像ファイルのパス</param>
    /// <param name="outputDdsPath">出力先DDSファイルのパス</param>
    /// <returns>変換および保存に成功したか</returns>
    static bool ConvertToDDS(const std::string &inputFilePath, const std::string &outputDdsPath);

    /// <summary>
    /// 元画像ファイルのパスから、最新のDDSファイルパスを取得する。
    /// DDSが存在しないか、元画像より古い場合は自動的にDDSを生成・更新する。
    /// 失敗した場合や非対応形式の場合は元のパスをそのまま返す（安全なフォールバック）。
    /// </summary>
    /// <param name="originalPath">元画像ファイルのパス（例: resources/texture.png）</param>
    /// <returns>読み込むべきテクスチャファイルのパス（.dds または originalPath）</returns>
    static std::string GetOrCreateDds(const std::string &originalPath);
};
