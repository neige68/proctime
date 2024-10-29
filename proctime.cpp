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

/// �o�[�W����
const wchar_t* str_version = L"0.00";

//============================================================
//
// Win32 API �̊g��
//

/// ���m�̃t�H���_�[�̊��S�p�X���擾
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

/// GetLastError �̒l��Ή����郁�b�Z�[�W�ɕϊ�����
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

/// �o�b�N�O���E���h�������[�h���J�n
void BeginBackgroundProcessMode()
{
    if (!SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN)) { // error
        wcerr << L"ERROR: SetPriorityClass(PROCESS_MODE_BACKGROUND_BEGIN): " << ErrorMessage(GetLastError()) << endl;
        wcerr << L"INFO: ���s���܂�" << endl;
    }
}

/// Windows Media �t�H���_�p�X
filesystem::path GetWindowsMediaPath()
{
    return GetKnownFolderPath(FOLDERID_Windows) / "Media";
}

//============================================================
//
// C/C++ Library �̊g��
//

/// ���ϐ��̒l�̎擾
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

/// wstring �ɕϊ�
wstring to_wstring(const string& str)
{
    return filesystem::path(str.c_str()).wstring();
}

/// string �ɕϊ�
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
// ���[�e�B���e�B
//

/// ���������؂蕶�� c �ŕ���
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

/// �^�[�~�i���̌���
int GetTerminalCols()
{
    CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleScreenBufferInfo))
        return ConsoleScreenBufferInfo.dwSize.X;
    else {
        // Emacs �� shell �ł̓G���[: �n���h���������ł��B
        if (getenv_wstring(L"TERM") == L"emacs") { // Windows �� Emacs �� shell �ł͊��ϐ� TERM=emacs �ɂȂ��Ă���
            auto cap = getenv_wstring(L"TERMCAP"); // "emacs:co#115:tc=unknown:" �̂悤�ɋN�����̕����L������Ă���
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
// �A�v�����L
//

/// �o�[�W�����o��
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

/// �w���v���b�Z�[�W�o��
void help(const boost::program_options::options_description& opt)
{
    version();
    wcout << endl;
    wcout << L"����: proctime {�I�v�V����} [--] �R�}���h���C�� ..." << endl << endl;
    wcout << L"�R�}���h���C�������s���āA���Ԃ��v���E�\�����A�I�����ɉ���炵�܂�" << endl << endl;
    ostringstream oss;
    oss << opt;
    wcout << to_wstring(oss.str()) << endl;
    wcout << L"���ϐ� PROCTIME �ɂ��I�v�V�������w��ł��܂�" << endl << endl;
    wcout << L"wav �t�@�C���͐�΃p�X�w�肪�Ȃ���� Windows Meida �t�H���_���g�p���܂�" << endl << endl;
    wcout << L"wav �t�@�C���͕����w�肵�Ă����݂���ŏ��̃t�@�C���݂̂��Đ����܂�" << endl << endl;
    wcout << L"�����e�L�X�g�� Windows 10 �ȍ~�ŗL���ł��Bwav �t�@�C���̌�ɔ������܂��B" << endl << endl;
}

/// ���X�g�\��
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

/// wav �t�@�C�����Đ�
/// \result true = �t�@�C�������݂��Đ�����
///         false = �t�@�C�������݂��Ȃ��Ȃǂ̃G���[
bool play(filesystem::path wavPath, const boost::program_options::variables_map& vm, bool last)
{
    if (wavPath.is_relative())
        wavPath = GetWindowsMediaPath() / wavPath;
    if (!filesystem::exists(wavPath)) {
        // .wav ��ǉ����Ď���
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

/// ISpObjectToken �� ID �������Ԃ�
wstring TokenId(ISpObjectToken* pToken)
{
    CSpDynamicString id;
    pToken->GetId(&id);
    return wstring{PWCHAR(id)};
}

// ����
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
        spVoice.Speak(text.c_str());
    }
    catch (const exception& x) {
        wcout << L"ERROR: " << to_wstring(x.what()) << endl;
    }
}

//============================================================
//
// ���C��
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
        po::options_description visible("�I�v�V����");
        visible.add_options() // �ꕶ���I�v�V�����c��: ACDFIJKMNQUXYZ
            ("help,H", "�w���v")
            ("version,V", "�o�[�W�����\��")
            ("verbose,v", "�璷�\��")
            ("timeout,T", po::value<int>()->default_value(1000), "wav �^�C���A�E�g[�~���b]")
            ("list,L", "wav �t�@�C�����X�g�\��")
            ("wav-file,W", po::wvalue<vector<wstring>>(), "wav �t�@�C��")
            ("error-wav-file,E", po::wvalue<vector<wstring>>(), "�G���[�� wav �t�@�C��")
            ("background-mode,B", "���\�[�X�X�P�W���[���̗D��x��������")
            ("speak-language,G", po::wvalue<wstring>()->default_value(L"jp", "jp"), "��������(jp,en �̂ݑΉ�)")
            ("speak-required-attribute,R", po::wvalue<wstring>()->default_value(L"", ""), "�����v������")
            ("speak-option-attribute,O", po::wvalue<wstring>()->default_value(L"", ""), "�����I�v�V��������")
            ("speak-text,S", po::wvalue<wstring>()->default_value(L"", ""), "�����e�L�X�g")
            ("speak-error-text,P", po::wvalue<wstring>()->default_value(L"", ""), "�G���[�������e�L�X�g")
            ;
        po::options_description opt("�I�v�V����");
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
        // �R�}���h�ƈ�����g�ݗ��Ă�
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
        // ���Ԍv�����ăR�}���h���s
        DWORD t0 = GetTickCount();
        result = _wspawnvp(P_WAIT, args[0], &args[0]);
        // ���s���ԕ\��
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
        // ����炷
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
        // ����
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
