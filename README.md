# Windows コマンドラインを実行して、時間を計測・表示し、終了時に音を鳴らす

<pre>
書式: proctime {オプション} コマンドライン ...

コマンドラインを実行して、時間を計測・表示し、終了時に音を鳴らします

オプション:
  -H [ --help ]                ヘルプ
  -V [ --version ]             バージョン表示
  -v [ --verbose ]             冗長表示
  -T [ --timeout ] arg (=1000) wav タイムアウト[ミリ秒]
  -L [ --list ]                wav ファイルリスト表示
  -W [ --wav-file ] arg        wav ファイル

wav ファイルは複数指定しても存在する最初のファイルのみを再生します
</pre>

例:

    proctime -W "Windows Ding.wav" -W ding.wav build