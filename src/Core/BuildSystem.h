#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <tlhelp32.h>

namespace VE {
class BuildSystem {
public:
    static BuildSystem& Get() { static BuildSystem b; return b; }
    void SetEngineRoot(const std::string& r) { m_root = r; }
    const std::vector<std::string>& GetLog() const { return m_log; }

    bool Build(const std::string& projectRoot, const std::string& scenePath, const std::string& gameName) {
        namespace fs = std::filesystem;
        m_log.clear();
        auto L = [&](const std::string& s){ m_log.push_back(s); };
        try {
            fs::path root = fs::path(projectRoot);
            fs::path outDir = root / "Build";
            fs::create_directories(outDir);
            L("Build dir: " + outDir.string());

            std::vector<fs::path> candidateSources;
            candidateSources.push_back(root / ".." / "x64" / "Release" / "VisualEngine.exe");
            candidateSources.push_back(root / ".." / "x64" / "Debug" / "VisualEngine.exe");
            candidateSources.push_back(root / ".." / "bin" / "Release" / "VisualEngine.exe");
            candidateSources.push_back(root / ".." / "bin" / "Debug" / "VisualEngine.exe");
            char exeBuf[MAX_PATH + 1] = { 0 };
            GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
            candidateSources.push_back(fs::path(exeBuf).parent_path() / "VisualEngine.exe");

            fs::path srcExe;
            for (const auto& p : candidateSources) {
                std::error_code ec;
                if (fs::exists(p, ec) && !ec) {
                    srcExe = p;
                    break;
                }
            }
            if (srcExe.empty()) {
                throw std::runtime_error("No VisualEngine.exe found in x64/Release or x64/Debug build output.");
            }

            std::string exeFilename = (gameName.empty() ? std::string("Game") : gameName) + ".exe";
            fs::path gameExe = outDir / exeFilename;

            if (fs::exists(gameExe)) {
                HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if (hSnap != INVALID_HANDLE_VALUE) {
                    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
                    std::wstring targetPath = gameExe.wstring();
                    while (Process32NextW(hSnap, &pe)) {
                        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (!hProc) continue;
                        wchar_t fullPath[MAX_PATH] = {0};
                        DWORD size = MAX_PATH;
                        if (QueryFullProcessImageNameW(hProc, 0, fullPath, &size)) {
                            std::wstring procPath = fullPath;
                            if (_wcsicmp(procPath.c_str(), targetPath.c_str()) == 0) {
                                TerminateProcess(hProc, 1);
                                L("terminated locked process: " + gameExe.filename().string());
                                Sleep(250);
                            }
                        }
                        CloseHandle(hProc);
                    }
                    CloseHandle(hSnap);
                }

                std::error_code ec;
                for (int i = 0; i < 20; ++i) {
                    if (!fs::exists(gameExe, ec)) break;
                    fs::remove(gameExe, ec);
                    if (!ec) break;
                    Sleep(100);
                }
            }

            fs::copy_file(srcExe, gameExe, fs::copy_options::overwrite_existing);
            L("built exe: " + gameExe.string());

            fs::path srcDir = srcExe.parent_path();
            for (const auto& e : fs::directory_iterator(srcDir)) {
                if (e.path().extension() == ".dll") {
                    fs::path dst = outDir / e.path().filename();
                    std::error_code ec;
                    if (fs::exists(dst, ec)) {
                        fs::remove(dst, ec);
                    }
                    fs::copy_file(e.path(), dst, fs::copy_options::overwrite_existing, ec);
                    if (ec) {
                        L("warn copy dll failed: " + std::string(ec.message()));
                    } else {
                        L("copied dll: " + e.path().filename().string());
                    }
                }
            }

            if (fs::exists(root / "Assets")) {
                fs::copy(root / "Assets", outDir / "Assets", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                L("copied: project Assets/");
            }

            if (fs::exists(root / "logo.png")) {
                fs::copy_file(root / "logo.png", outDir / "logo.png", fs::copy_options::overwrite_existing);
            }

            fs::path sceneSrc(scenePath);
            fs::path sceneDst = outDir / "Assets" / "Scenes" / sceneSrc.filename();
            fs::create_directories(sceneDst.parent_path());
            if (fs::exists(sceneSrc)) {
                fs::copy_file(sceneSrc, sceneDst, fs::copy_options::overwrite_existing);
                L("scene -> " + sceneDst.string());
            }

            std::string relScene = "Assets\\Scenes\\" + sceneSrc.filename().string();
            { std::ofstream cfg(outDir / "player.cfg"); cfg << relScene << "\n"; }
            L("player.cfg -> " + relScene);

            std::string exeName = (gameName.empty() ? "Game" : gameName) + ".exe";
            L("BUILD OK! Run: " + (outDir / exeName).string());
            return true;
        } catch (const std::exception& ex) {
            L(std::string("BUILD ERROR: ") + ex.what());
            return false;
        }
    }

private:
    std::string m_root;
    std::vector<std::string> m_log;
};
}
