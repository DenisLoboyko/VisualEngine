#pragma once
// =========================================================
//  BuildSystem.h  —  Build/Export для VisualEngine
//
//  Экспортирует игру в папку Build/:
//  - Копирует Assets/ (сцены, скрипты, текстуры, звуки)
//  - Копирует нужные .dll (glfw, openal и т.д.)
//  - Копирует уже собранный VisualEngine.exe -> <Имя игры>.exe
//  - Пишет player.cfg — движок сам подхватывает его при старте
//    и запускается в режиме готовой игры, без редактора
//    (запускается двойным кликом, никакой компиляции не нужно —
//     движок уже скомпилирован, просто переиспользуем тот же .exe
//     в специальном режиме)
//
//  Положи файл: src/Core/BuildSystem.h
//
//  C++ API:
//    VE::BuildSystem::Get().SetEngineRoot("C:\\...\\VisualEngine");
//    VE::BuildSystem::Get().Build(projectRoot, currentScenePath);
// =========================================================

#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace VE {

    class BuildSystem
    {
    public:
        static BuildSystem& Get()
        {
            static BuildSystem instance;
            return instance;
        }
        BuildSystem(const BuildSystem&)            = delete;
        BuildSystem& operator=(const BuildSystem&) = delete;

        void SetEngineRoot(const std::string& root) { m_EngineRoot = root; }

        // ── Главный метод: собрать игру ──────────────────────
        // projectRoot    — папка проекта (где лежит Assets/)
        // scenePath      — путь к главной сцене
        // outputName     — имя выходного exe (без .exe)
        bool Build(const std::string& projectRoot,
                   const std::string& scenePath,
                   const std::string& outputName = "Game")
        {
            m_Log.clear();
            log("=== VisualEngine Build ===");

            // Папка сборки
            std::string buildDir = projectRoot + "\\..\\Build";
            try { fs::create_directories(buildDir); }
            catch (const std::exception& e) { log("ERROR: " + std::string(e.what())); return false; }

            log("Build dir: " + buildDir);

            // 1. Копируем Assets
            std::string srcAssets = projectRoot + "\\Assets";
            std::string dstAssets = buildDir    + "\\Assets";
            if (fs::exists(srcAssets)) {
                CopyDir(srcAssets, dstAssets);
                log("Copied: Assets/");
            } else {
                log("WARN: No Assets folder found");
            }

            // 2. Копируем Saves если есть
            std::string srcSaves = projectRoot + "\\Saves";
            if (fs::exists(srcSaves)) {
                CopyDir(srcSaves, buildDir + "\\Saves");
                log("Copied: Saves/");
            }

            // 3. Копируем DLL из папки движка
            CopyDLLs(buildDir);

            // 4. Определяем главную сцену (относительный путь от Build/)
            std::string mainScene = scenePath.empty()
                ? "Assets\\scene.vescene"
                : RelativePath(scenePath, projectRoot);
            for (auto& c : mainScene) if (c=='\\') c='/';

            // 5. Копируем уже собранный движок и переименовываем в игру.
            //    Никакой отдельной компиляции не требуется — движок сам
            //    умеет запускаться в режиме готовой игры (см. player.cfg).
            std::string exeName = outputName + ".exe";
            if (!CopyEngineExe(buildDir, exeName)) {
                log("ERROR: Could not find a compiled VisualEngine.exe");
                log("Build the engine in Visual Studio first (Ctrl+Shift+B), then Build again");
                return false;
            }
            log("Copied engine as: " + exeName);

            // 6. Пишем player.cfg — движок при старте увидит этот файл
            //    рядом с собой и запустится сразу в игровом режиме.
            {
                std::ofstream pf(buildDir + "\\player.cfg");
                pf << mainScene << "\n";
            }
            log("Generated: player.cfg (scene = " + mainScene + ")");

            // 7. README
            GenerateReadme(buildDir, outputName, mainScene);

            log("");
            log("=== BUILD READY ===");
            log("Build folder: " + buildDir);
            log("Run " + exeName + " - it launches straight into the game, no editor.");

            return true;
        }

        const std::vector<std::string>& GetLog() const { return m_Log; }

    private:
        BuildSystem() = default;

        std::string m_EngineRoot;
        std::vector<std::string> m_Log;

        void log(const std::string& msg)
        {
            m_Log.push_back(msg);
            std::cout << "[Build] " << msg << "\n";
        }

        void CopyDir(const std::string& src, const std::string& dst)
        {
            try {
                fs::create_directories(dst);
                for (auto& e : fs::recursive_directory_iterator(src)) {
                    auto rel  = fs::relative(e.path(), src);
                    auto dest = fs::path(dst) / rel;
                    if (e.is_directory()) {
                        fs::create_directories(dest);
                    } else {
                        fs::create_directories(dest.parent_path());
                        fs::copy_file(e.path(), dest, fs::copy_options::overwrite_existing);
                    }
                }
            } catch (const std::exception& ex) {
                log("WARN copy: " + std::string(ex.what()));
            }
        }

        void CopyDLLs(const std::string& buildDir)
        {
            // Ищем DLL рядом с движком
            std::vector<std::string> dllNames = {
                "glfw3.dll", "assimp-vc143-mt.dll",
                "lua54.dll", "OpenAL32.dll"
            };

            for (auto& dll : dllNames) {
                for (auto& dir : EngineSearchDirs()) {
                    fs::path src = fs::path(dir) / dll;
                    if (fs::exists(src)) {
                        try {
                            fs::copy_file(src, fs::path(buildDir) / dll,
                                fs::copy_options::overwrite_existing);
                            log("Copied DLL: " + dll);
                        } catch (...) {}
                        break;
                    }
                }
            }
        }

        // Ищет уже скомпилированный VisualEngine.exe (Debug или Release)
        // в нескольких вероятных местах и копирует его как <exeName>.
        bool CopyEngineExe(const std::string& buildDir, const std::string& exeName)
        {
            std::vector<std::string> candidates = {
                fs::current_path().string() + "\\VisualEngine.exe",
                m_EngineRoot + "\\VisualEngine.exe",
                m_EngineRoot + "\\Debug\\VisualEngine.exe",
                m_EngineRoot + "\\Release\\VisualEngine.exe",
                m_EngineRoot + "\\x64\\Debug\\VisualEngine.exe",
                m_EngineRoot + "\\x64\\Release\\VisualEngine.exe",
                m_EngineRoot + "\\VisualEngine\\x64\\Debug\\VisualEngine.exe",
                m_EngineRoot + "\\VisualEngine\\x64\\Release\\VisualEngine.exe",
            };
            for (auto& c : candidates) {
                if (fs::exists(c)) {
                    try {
                        fs::copy_file(c, fs::path(buildDir) / exeName,
                            fs::copy_options::overwrite_existing);
                        return true;
                    } catch (const std::exception& ex) {
                        log("WARN copy exe: " + std::string(ex.what()));
                    }
                }
            }
            return false;
        }

        std::vector<std::string> EngineSearchDirs()
        {
            return {
                m_EngineRoot + "\\x64\\Debug",
                m_EngineRoot + "\\x64\\Release",
                m_EngineRoot,
                fs::current_path().string()
            };
        }

        std::string RelativePath(const std::string& full, const std::string& base)
        {
            try {
                return fs::relative(full, base).string();
            } catch (...) {
                return full;
            }
        }

        void GenerateReadme(const std::string& buildDir,
                            const std::string& outputName,
                            const std::string& mainScene)
        {
            std::ofstream f(buildDir + "\\README.txt");
            f << "=== " << outputName << " — VisualEngine Build ===\n\n";
            f << "Main scene: " << mainScene << "\n\n";
            f << "To run the game:\n";
            f << "  Just run " << outputName << ".exe — no compiling needed.\n";
            f << "  (player.cfg tells the engine which scene to launch straight into.)\n\n";
            f << "Files:\n";
            f << "  Assets/     — game assets (scenes, scripts, textures, sounds)\n";
            f << "  Saves/      — save files\n";
            f << "  *.dll       — required libraries\n";
            f << "  player.cfg  — tells the exe which scene to auto-launch\n";
        }
    };

} // namespace VE