# Windows コマンドラインを実行して、時間を計測・表示し、終了時にエラーの有無で音を鳴り分ける

<pre>
書式: proctime {オプション} [--] コマンドライン ...

コマンドラインを実行して、時間を計測・表示し、終了時に音を鳴らします

オプション:
  -H [ --help ]                         ヘルプ
  -V [ --version ]                      バージョン表示
  -v [ --verbose ]                      冗長表示
  -T [ --timeout ] arg (=1000)          wav タイムアウト[ミリ秒]
  -L [ --list ]                         wav ファイルリスト表示
  -W [ --wav-file ] arg                 wav ファイル
  -E [ --error-wav-file ] arg           エラー時 wav ファイル
  -B [ --background-mode ]              リソーススケジュールの優先度を下げる
  -G [ --speak-language ] arg (=jp)     発声言語(jp,en のみ対応)
  -R [ --speak-required-attribute ] arg 発声要求属性
  -O [ --speak-option-attribute ] arg   発声オプション属性
  -S [ --speak-text ] arg               発声テキスト
  -P [ --speak-error-text ] arg         エラー時発声テキスト

環境変数 PROCTIME にもオプションを指定できます

wav ファイルは絶対パス指定がなければ Windows Meida フォルダを使用します

wav ファイルは複数指定しても存在する最初のファイルのみを再生します

発声テキストは Windows 10 以降で有効です。wav ファイルの後に発声します。
</pre>

例:

    proctime -W alarm10 -E alarm08 -- build
    proctime -S 正常終了しました -P エラーがあります -- build
