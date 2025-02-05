// <main.cpp>
//
// Project proctime
// Copyright (C) 2023-2024 neige68
//
/// \file
/// \brief main
//
// Compiler: VC14.2
//

#include "pch.h"
#pragma hdrstop

#include <Mmsystem.h>           // PlaySound
#include <process.h>            // _wspawnvp
#include <sapi.h>               // ISpVoice
#pragma warning(disable: 4996)
#include <shlobj_core.h>        // SHGetKnownFolderPath
#include <sphelper.h>           // CSpDynamicString
#pragma warning(default: 4996)

#include <boost/noncopyable.hpp>     // boost::noncopyable
#include <boost/program_options.hpp> // boost::program_options

#include <filesystem>           // std::filesystem
#include <sstream>              // std::ostringstream

using namespace std;

//============================================================
//
// global
//

/// バージョン
const wchar_t* str_version = L"0.00";

//============================================================
//
// Win32 API の拡張
//

/// 既知のフォルダーの完全パスを取得
filesystem::path GetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags = 0, HANDLE hToken = 0)
{
    PWSTR pszPath = 0;
    if (SHGetKnownFolderPath(rfid, dwFlags, hToken, &pszPath) != S_OK) {
        ::CoTaskMemFree(pszPath);
        throw runtime_error("SHGetKnownFolderPath failure.");
    }
    wstring result{pszPath};
    ::CoTaskMemFree(pszPath);
    return result;
}

/// GetLastError の値を対応するメッセージに変換する
wstring ErrorMessage(DWORD id, DWORD dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT))
{
    wchar_t* buf = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                  | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, 
                  0, id, dwLanguageId, (LPTSTR)&buf, 1, 0);
    wstring result(buf ? buf : L"");
    LocalFree(buf);
    return result;
}

/// バックグラウンド処理モードを開始
void BeginBackgroundProcessMode()
{
    if (!SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN)) { // error
        wcerr << L"ERROR: SetPriorityClass(PROCESS_MODE_BACKGROUND_BEGIN): " << ErrorMessage(GetLastError()) << endl;
        wcerr << L"INFO: 続行します" << endl;
    }
}

/// Windows Media フォルダパス
filesystem::path GetWindowsMediaPath()
{
    return GetKnownFolderPath(FOLDERID_Windows) / "Media";
}

//============================================================
//
// C/C++ Library の拡張
//

/// 環境変数の値の取得
wstring getenv_wstring(const wstring& name)
{
    vector<wchar_t> buf(128);
    for (;;) {
        size_t returnValue;
        errno_t r = _wgetenv_s(&returnValue, &buf.at(0), buf.size(), name.c_str());
        if (r == 0) // OK
            break;
        else if (r == ERANGE)
            buf.resize(returnValue);
        else
            throw runtime_error("getenv_s failure: code = " + to_string(r));
    }
    return wstring(&buf.at(0));
}

/// wstring に変換
wstring to_wstring(const string& str)
{
    return filesystem::path(str.c_str()).wstring();
}

/// string に変換
string to_string(const wstring& str)
{
    return filesystem::path(str.c_str()).string();
}

//============================================================
//
// COM Initializer
//

class TComInitializer : boost::noncopyable {
private:
    TComInitializer() {
        if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
            throw runtime_error{"CoInitializeEx failed."};
    }
public:
    static TComInitializer& Instance();
    ~TComInitializer() {
        CoUninitialize();
    }
};

//static
TComInitializer& TComInitializer::Instance()
{
    static TComInitializer theInstance;
    return theInstance;
}

//============================================================
//
// class TSpVoice
//

class TSpVoice {
public:
    TSpVoice();
    ~TSpVoice();
    void Speak(LPCWSTR pwcs, DWORD dwFlags = SPF_DEFAULT, ULONG* pulStreamNumber = nullptr);
    void GetVoice(ISpObjectToken** ppToken) {
        if (FAILED(ptr_->GetVoice(ppToken)))
            throw runtime_error{"ISpVoice::GetVoice failed."};
    }
    void SetVoice(ISpObjectToken* pToken) {
        if (FAILED(ptr_->SetVoice(pToken)))
            throw runtime_error{"ISpVoice::SetVoice failed."};
    }
    void SetVolume(USHORT volume) {
        if (FAILED(ptr_->SetVolume(volume)))
            throw runtime_error{"ISpVoice::SetVolume failed."};
    }
private:
    ISpVoice* ptr_;
};

//static
TSpVoice::TSpVoice() : ptr_{nullptr}
{
    TComInitializer::Instance();
    auto hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER, IID_ISpVoice,
                          reinterpret_cast<void**>(&ptr_));
    if (FAILED(hr))
        throw runtime_error{"CoCreateInstance CLSID_SpVoice failed."};
}

TSpVoice::~TSpVoice()
{
    if (ptr_)
        ptr_->Release();
}

void TSpVoice::Speak(LPCWSTR pwcs, DWORD dwFlags, ULONG* pulStreamNumber)
{
    auto hr = ptr_->Speak(pwcs, dwFlags, pulStreamNumber);
    if (FAILED(hr))
        throw runtime_error("ISpVoice::Speak failed.");
}

//============================================================
//
// ユーティリティ
//

/// 文字列を区切り文字 c で分解
vector<wstring> split(const wstring& str, wchar_t c)
{
    vector<wstring> result;
    size_t start = 0;
    for (;;) {
        size_t pos = str.find(c, start);
        if (pos == string::npos) break;
        result.push_back(str.substr(start, pos - 1));
        start = pos + 1;
    }
    return result;
}

/// ターミナルの桁数
int GetTerminalCols()
{
    CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleScreenBufferInfo))
        return ConsoleScreenBufferInfo.dwSize.X;
    else {
        // Emacs の shell ではエラー: ハンドルが無効です。
        if (getenv_wstring(L"TERM") == L"emacs") { // Windows の Emacs の shell では環境変数 TERM=emacs になっている
            auto cap = getenv_wstring(L"TERMCAP"); // "emacs:co#115:tc=unknown:" のように起動時の幅が記憶されている
            auto scap = split(cap, L':');
            auto iscap = find_if(scap.begin(), scap.end(),
                                 [] (const wstring& s) { return s.substr(0, 3) == L"co#"; });
            if (iscap != scap.end())
                return _wtoi(iscap->substr(3).c_str());
        }
    }
    return 80;
}

//============================================================
//
// アプリ特有
//

/// バージョン出力
void version()
{
    wcout << L"proctime";
#if defined(_WIN64)
    wcout << L" x64";
#else        
    wcout << L" x86";
#endif
#if !defined(NDEBUG)
    wcout << L" Debug";
#endif
    wcout << L" Version " << str_version << endl;
}

/// ヘルプメッセージ出力
void help(const boost::program_options::options_description& opt)
{
    version();
    wcout << endl;
    wcout << L"書式: proctime {オプション} [--] コマンドライン ..." << endl << endl;
    wcout << L"コマンドラインを実行して、時間を計測・表示し、終了時に音を鳴らします" << endl << endl;
    ostringstream oss;
    oss << opt;
    wcout << to_wstring(oss.str()) << endl;
    wcout << L"環境変数 PROCTIME にもオプションを指定できます" << endl << endl;
    wcout << L"wav ファイルは絶対パス指定がなければ Windows Meida フォルダを使用します" << endl << endl;
    wcout << L"wav ファイルは複数指定しても存在する最初のファイルのみを再生します" << endl << endl;
    wcout << L"発声テキストは Windows 10 以降で有効です。wav ファイルの後に発声します。" << endl << endl;
}

/// リスト表示
void show_list()
{
    vector<wstring> v;
    size_t max_width = 0;
    for (const auto& ent : filesystem::directory_iterator(GetWindowsMediaPath())) {
        if (ent.is_regular_file()) {
            auto ext = ent.path().extension();
            if (ext == L".wav") {
                wstring fname = ent.path().filename().wstring();
                v.push_back(fname);
                if (max_width < fname.size())
                    max_width = fname.size();
            }
        }
    }
    int c = 1 + (GetTerminalCols() - max_width) / (max_width + 2);
    int r = (v.size() + c - 1) / c;
    for (int ir = 0; ir < r; ++ir) {
        for (int ic = 0; ic < c; ++ic) {
            size_t s = ir + ic * r;
            if (s < v.size()) {
                wcout << v[s];
                if (ic + 1 < c)
                    wcout << wstring(max_width + 2 - v[s].size(), L' ');
            }
        }
        wcout << endl;
    }
}

/// wav ファイルを再生
/// \result true = ファイルが存在し再生した
///         false = ファイルが存在しないなどのエラー
bool play(filesystem::path wavPath, const boost::program_options::variables_map& vm, bool last)
{
    if (wavPath.is_relative())
        wavPath = GetWindowsMediaPath() / wavPath;
    if (!filesystem::exists(wavPath)) {
        // .wav を追加して試す
        filesystem::path wavPathExtAdded = wavPath;
        wavPathExtAdded += L".wav";
        if (filesystem::exists(wavPathExtAdded))
            wavPath = wavPathExtAdded;
    }
    if (!filesystem::exists(wavPath)) {
        if (last)
            wcerr << L"ERROR: File " << wavPath << L" not found." << endl;
        else if (vm.count("verbose"))
            wcerr << L"WARN: File " << wavPath << L" not found." << endl;
        return false;
    }
    if (vm.count("verbose"))
        wcout << L"INFO: Play File: " << wavPath << endl;
    //
    int timeOut = vm["timeout"].as<int>();
    bool result = PlaySound(wavPath.wstring().c_str(), NULL, SND_FILENAME | SND_ASYNC);
    Sleep(timeOut);
    PlaySound(NULL, NULL, SND_FILENAME | SND_ASYNC);
    return result;
}

/// ISpObjectToken の ID 文字列を返す
wstring TokenId(ISpObjectToken* pToken)
{
    CSpDynamicString id;
    pToken->GetId(&id);
    return wstring{PWCHAR(id)};
}

// 発声
void speak(wstring language, wstring reqAttribs, wstring optAttribs, wstring text,
           const boost::program_options::variables_map& vm)
{
    try {
        if (language == L"jp")
            reqAttribs += L";Language=411";
        else if (language == L"en")
            reqAttribs += L";Language=409";
        TComInitializer::Instance();
        ISpObjectToken* pObjectToken = nullptr;
        HRESULT hr = SpFindBestToken(SPCAT_VOICES, reqAttribs.c_str(), optAttribs.c_str(), &pObjectToken);
        if (FAILED(hr)) {
            auto em = ErrorMessage(hr, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT));
            wostringstream oss;
            oss << L"SpFindBestToken failed. "
                << L"(CategoryId=SPCAT_VOICES,ReqAttribs=\"" << reqAttribs
                << L"\",OptAttribs=\"" << optAttribs
                << L"\",Result=" << hex << showbase << hr << L",Msg=\"" << em << L"\")";
            throw runtime_error{to_string(oss.str())};
        }
        if (vm.count("verbose"))
            wcout << L"INFO: Found Token: " << TokenId(pObjectToken) << endl;
        TSpVoice spVoice;
        spVoice.SetVoice(pObjectToken);
        USHORT volume = 101;
        if (vm.count("speak-volume"))
            volume = static_cast<USHORT>(vm["speak-volume"].as<int>());;
        if (volume < 101)
            spVoice.SetVolume(volume);
        spVoice.Speak(text.c_str());
    }
    catch (const exception& x) {
        wcout << L"ERROR: " << to_wstring(x.what()) << endl;
    }
}

//============================================================
//
// メイン
//

int wmain(int argc, wchar_t** argv)
{
    int result = -1;
    try {
        locale::global(locale{locale{}, "", locale::ctype});
        namespace po = boost::program_options;
        po::positional_options_description p;
        p.add("command-args", -1);
        po::options_description hidden{"hidden options"};
        hidden.add_options()
            ("command-args", po::wvalue<vector<wstring>>(), "command args")
            ;
        po::options_description visible("オプション");
        visible.add_options() // 一文字オプション残り: ACDFIJKNQUXYZ
            ("help,H", "ヘルプ")
            ("version,V", "バージョン表示")
            ("verbose,v", "冗長表示")
            ("timeout,T", po::value<int>()->default_value(1000), "wav タイムアウト[ミリ秒]")
            ("list,L", "wav ファイルリスト表示")
            ("wav-file,W", po::wvalue<vector<wstring>>(), "wav ファイル")
            ("error-wav-file,E", po::wvalue<vector<wstring>>(), "エラー時 wav ファイル")
            ("background-mode,B", "リソーススケジュールの優先度を下げる")
            ("speak-language,G", po::wvalue<wstring>()->default_value(L"jp", "jp"), "発声言語(jp,en のみ対応)")
            ("speak-required-attribute,R", po::wvalue<wstring>()->default_value(L"", ""), "発声要求属性")
            ("speak-option-attribute,O", po::wvalue<wstring>()->default_value(L"", ""), "発声オプション属性")
            ("speak-text,S", po::wvalue<wstring>()->default_value(L"", ""), "発声テキスト")
            ("speak-error-text,P", po::wvalue<wstring>()->default_value(L"", ""), "エラー時発声テキスト")
            ("speak-volume,M", po::value<int>()->default_value(100), "発声ボリューム(0..100)")
            ;
        po::options_description opt("オプション");
        opt.add(visible).add(hidden);
        //
        po::variables_map vm;
        auto ev = getenv_wstring(L"PROCTIME");
        if (!ev.empty()) {
            auto args = po::split_winmain(ev);
            store(po::basic_command_line_parser<wchar_t>(args).options(opt).run(), vm);
        }
        store(po::basic_command_line_parser<wchar_t>(argc, argv).options(opt).positional(p).run(), vm);
        po::notify(vm);
        if (vm.count("help")) {
            help(visible);
            return 0;
        }
        if (vm.count("version")) {
            version();
            return 0;
        }
        if (vm.count("list")) {
            show_list();
            return 0;
        }
        if (vm.count("command-args") == 0) {
            help(visible);
            return 0;
        }
        if (vm.count("background-mode"))
            BeginBackgroundProcessMode();
        // コマンドと引数を組み立てる
        vector<const wchar_t*> args;
        for (const auto& str : vm["command-args"].as<vector<wstring>>())
            args.push_back(str.c_str());
        args.push_back(nullptr);
        //
        if (vm.count("verbose")) {
            wcout << L"INFO: args: ";
            bool first = true;
            for (const auto& str : args) {
                if (first)
                    first = false;
                else
                    wcout << L' ';
                if (str)
                    wcout << str;
            }
            wcout << endl;
        }
        // 時間計測してコマンド実行
        DWORD t0 = GetTickCount();
        result = _wspawnvp(P_WAIT, args[0], &args[0]);
        // 実行時間表示
        DWORD t1 = GetTickCount();
        DWORD t = (t1 - t0);
        wprintf(L"proctime: %d.%03d seconds", t/1000, t%1000);
        if (t >= 1000 * 60 * 60 * 24)
            wprintf(L" (%dd %dh %02dm %02ds %03d)", t/1000/60/60/24, t/1000/60/60%24, t/1000/60%60, t/1000%60, t%1000);
        else if (t >= 1000 * 60 * 60)
            wprintf(L" (%dh %02dm %02ds %03d)", t/1000/60/60, t/1000/60%60, t/1000%60, t%1000);
        else if (t >= 1000 * 60)
            wprintf(L" (%dm %02ds %03d)", t/1000/60, t/1000%60, t%1000);
        wprintf(L".\n");
        fflush(stdout);
        // 音を鳴らす
        if (result && vm.count("error-wav-file")) {
            size_t count = vm["error-wav-file"].as<vector<wstring>>().size();
            size_t i = 0;
            for (const auto& str : vm["error-wav-file"].as<vector<wstring>>()) {
                if (play(str, vm, ++i == count))
                    break;
            }
        }
        else if (vm.count("wav-file")) {
            size_t count = vm["wav-file"].as<vector<wstring>>().size();
            size_t i = 0;
            for (const auto& str : vm["wav-file"].as<vector<wstring>>()) {
                if (play(str, vm, ++i == count))
                    break;
            }
        }
        else
            play(L"Ding.wav", vm, true);
        // 発声
        wstring text = vm["speak-text"].as<wstring>();
        if (result) text = vm["speak-error-text"].as<wstring>();
        if (!text.empty())
            speak(vm["speak-language"].as<wstring>(),
                  vm["speak-required-attribute"].as<wstring>(),
                  vm["speak-option-attribute"].as<wstring>(),
                  text,
                  vm);
    }
    catch (const exception& x) {
        wcerr << L"ERROR: " << to_wstring(x.what()) << endl;
    }
    return result;
}

//============================================================

// end of <main.cpp>
