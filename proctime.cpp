// <main.cpp>
//
// Project proctime
// Copyright (C) 2023 neige68
//
/// \file
/// \brief main
//
// Compiler: VC14.2
//

#include "pch.h"
#pragma hdrstop

#include <process.h>
#include <shlobj_core.h>

#include <boost/program_options.hpp> // boost::program_options

#include <filesystem>           // std::filesystem

using namespace std;

//------------------------------------------------------------

/// �o�[�W����
const char* str_version = "0.00";

//------------------------------------------------------------

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
string ErrorMessage(DWORD id, DWORD dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT))
{
    char* buf = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                  | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, 
                  0, id, dwLanguageId, (LPTSTR)&buf, 1, 0);
    string result(buf ? buf : "");
    LocalFree(buf);
    return result;
}

/// �o�b�N�O���E���h�������[�h���J�n
void BeginBackgroundProcessMode()
{
    if (!SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN)) { // error
        cerr << "ERROR: SetPriorityClass(PROCESS_MODE_BACKGROUND_BEGIN): " << ErrorMessage(GetLastError()) << endl;
        cerr << "INFO: ���s���܂�" << endl;
    }
}

/// ���ϐ��̒l�̎擾
string getenv_string(const string& name)
{
    vector<char> buf(128);
    for (;;) {
        size_t returnValue;
        errno_t r = getenv_s(&returnValue, &buf.at(0), buf.size(), name.c_str());
        cerr << "getenv_string|r=" << r << "|returnValue=" << returnValue << "\n";
        if (r == 0) // OK
            break;
        else if (r == ERANGE)
            buf.resize(returnValue);
        else
            throw runtime_error("getenv_s failure: code = " + to_string(r));
    }
    return string(&buf.at(0));
}

//------------------------------------------------------------

/// �o�[�W�����o��
void version()
{
    cout << "proctime";
#if defined(_WIN64)
    cout << " x64";
#else        
    cout << " x86";
#endif
#if !defined(NDEBUG)
    cout << " Debug";
#endif
    cout << " Version " << str_version << endl;
}

/// �w���v���b�Z�[�W�o��
void help(const boost::program_options::options_description& opt)
{
    version();
    cout << endl;
    cout << "����: proctime {�I�v�V����} [--] �R�}���h���C�� ..." << endl << endl;
    cout << "�R�}���h���C�������s���āA���Ԃ��v���E�\�����A�I�����ɉ���炵�܂�" << endl << endl;
    ostringstream oss;
    oss << opt;
    cout << oss.str() << endl;
    cout << "���ϐ� PROCTIME �ɂ��I�v�V�������w��ł��܂�" << endl << endl;
    cout << "wav �t�@�C���͕����w�肵�Ă����݂���ŏ��̃t�@�C���݂̂��Đ����܂�" << endl << endl;
}

/// Windows Media �t�H���_�p�X
filesystem::path GetWindowsMediaPath()
{
    return GetKnownFolderPath(FOLDERID_Windows) / "Media";
}

/// ���������؂蕶�� c �ŕ���
vector<string> split(const string& str, char c)
{
    vector<string> result;
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
        if (getenv_string("TERM") == "emacs") { // Windows �� Emacs �� shell �ł͊��ϐ� TERM=emacs �ɂȂ��Ă���
            auto cap = getenv_string("TERMCAP"); // "emacs:co#115:tc=unknown:" �̂悤�ɋN�����̕����L������Ă���
            auto scap = split(cap, ':');
            auto iscap = find_if(scap.begin(), scap.end(),
                                 [] (const string& s) { return s.substr(0, 3) == "co#"; });
            if (iscap != scap.end())
                return atoi(iscap->substr(3).c_str());
        }
    }
    return 80;
}

/// ���X�g�\��
void show_list()
{
    vector<string> v;
    size_t max_width = 0;
    for (const auto& ent : filesystem::directory_iterator(GetWindowsMediaPath())) {
        if (ent.is_regular_file()) {
            auto ext = ent.path().extension();
            if (ext == ".wav" || ext == ".mid") {
                string fname = ent.path().filename().string();
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
                cout << v[s];
                if (ic + 1 < c)
                    cout << string(max_width + 2 - v[s].size(), ' ');
            }
        }
        cout << endl;
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
        if (last)
            cerr << "ERROR: File " << wavPath << " not found." << endl;
        else if (vm.count("verbose"))
            cerr << "WARN: File " << wavPath << " not found." << endl;
        return false;
    }
    if (vm.count("verbose"))
        cout << "INFO: Play File: " << wavPath << endl;
    int timeOut = vm["timeout"].as<int>();
    ostringstream cmdLine;
    cmdLine << "mshta \"about:playing... "
            << "<OBJECT CLASSID='CLSID:22D6F312-B0F6-11D0-94AB-0080C74C7E95' WIDTH=1 HEIGHT=1>"
            << "  <PARAM NAME='src' VALUE='" << wavPath.string() << "'>"
            << "  <PARAM NAME='PlayCount' VALUE='1'>"
            << "  <PARAM NAME='AutoStart' VALUE='true'>"
            << "</OBJECT>"
            << "<SCRIPT>"
            << "  window.resizeTo(10,10);"
            << "  window.moveTo(-32000,-32000);"
            << "  setTimeout(function(){window.close()}," << timeOut << ");"
            << "</SCRIPT>\"";
    system(cmdLine.str().c_str());
    return true;
}

//------------------------------------------------------------

int main(int argc, char** argv)
{
    int result = -1;
    try {
        locale::global(locale{locale{}, "", locale::ctype});
        namespace po = boost::program_options;
        po::positional_options_description p;
        p.add("command-args", -1);
        po::options_description hidden("hidden options");
        hidden.add_options()
            ("command-args", po::value<vector<string>>(), "command args")
            ;
        po::options_description visible("�I�v�V����");
        visible.add_options()
            ("help,H", "�w���v")
            ("version,V", "�o�[�W�����\��")
            ("verbose,v", "�璷�\��")
            ("timeout,T", po::value<int>()->default_value(1000), "wav �^�C���A�E�g[�~���b]")
            ("list,L", "wav �t�@�C�����X�g�\��")
            ("wav-file,W", po::value<vector<string>>(), "wav �t�@�C��")
            ("background-mode,B", "���\�[�X�X�P�W���[���̗D��x��������")
            ;
        po::options_description opt("�I�v�V����");
        opt.add(visible).add(hidden);
        //
        po::variables_map vm;
        auto ev = getenv_string("PROCTIME");
        if (!ev.empty()) {
            auto args = po::split_winmain(ev);
            store(po::basic_command_line_parser<char>(args).options(opt).run(), vm);
        }
        store(po::basic_command_line_parser<char>(argc, argv).options(opt).positional(p).run(), vm);
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
        vector<const char*> args;
        for (const auto& str : vm["command-args"].as<vector<string>>())
            args.push_back(str.c_str());
        args.push_back(nullptr);
        //
        if (vm.count("verbose")) {
            cout << "INFO: args: ";
            bool first = true;
            for (const auto& str : args) {
                if (first)
                    first = false;
                else
                    cout << ' ';
                if (str)
                    cout << str;
            }
            cout << endl;
        }
        // ���Ԍv�����ăR�}���h���s
        DWORD t0 = GetTickCount();
        result = _spawnvp(P_WAIT, args[0], &args[0]);
        // ���s���ԕ\��
        DWORD t1 = GetTickCount();
        DWORD t = (t1 - t0);
        printf("proctime: %d.%03d seconds", t/1000, t%1000);
        if (t >= 1000 * 60 * 60 * 24)
            printf(" (%dd %dh %02dm %02ds %03d)", t/1000/60/60/24, t/1000/60/60%24, t/1000/60%60, t/1000%60, t%1000);
        else if (t >= 1000 * 60 * 60)
            printf(" (%dh %02dm %02ds %03d)", t/1000/60/60, t/1000/60%60, t/1000%60, t%1000);
        else if (t >= 1000 * 60)
            printf(" (%dm %02ds %03d)", t/1000/60, t/1000%60, t%1000);
        printf(".\n");
        fflush(stdout);
        // ����炷
        if (vm.count("wav-file")) {
            size_t count = vm["wav-file"].as<vector<string>>().size();
            size_t i = 0;
            for (const auto& str : vm["wav-file"].as<vector<string>>()) {
                if (play(str, vm, ++i == count))
                    break;
            }
        }
        else
            play("Ding.wav", vm, true);
    }
    catch (const exception& x) {
        cerr << "ERROR: " << x.what() << endl;
    }
    return result;
}

//------------------------------------------------------------

// end of <main.cpp>
