# AGENTS.md - instructions for AI assistants working on VisualEngine

## What is this
C++17 game engine + editor: ImGui + OpenGL 3.3 + GLFW/GLAD + Lua 5.5 + miniaudio.
Editor and game runtime share one codebase; game builds are packaged by BuildSystem.

## Build and run
- Solution: VisualEngine.sln, config Release|x64 (VS2022). CLI: msbuild VisualEngine.sln /p:Configuration=Release /p:Platform=x64
- Editor exe: x64/Release/VisualEngine.exe (run from repo root, paths are relative)
- Game build: in editor press ~ and type: build_game MyGame  ->  project/Build/MyGame.exe
- Player logs: project/Build/player_log.txt and player_log_err.txt (stdout/stderr mirror)
- Console key: ~ ; useful cmds: help, scene_list, sel N, obj_pos, map, build_game

## Architecture map
- src/main.cpp (~4100 lines): entry point, editor UI, main loop, renderScene orchestration, player mode branch at loop end: if (!g_PlayerMode) { editor UI } else { player view }
- src/Core/Console.h: dev console (commands, cvars, aliases)
- src/Core/BuildSystem.h: standalone build packaging (copies exe + dlls + assets, writes player.cfg)
- src/Core/SceneManager.h: deferred scene load (RequestLoad/Tick)
- src/Core/AudioEngine.h: miniaudio singleton
- src/SceneObject.h: SceneObject struct, logInfo() helper (mirrors to console log)
- project/Assets/Scenes/*.scene: scene files; project/Build/: game output (gitignored)

## Player mode flow
args --player/--scene -> g_PlayerMode=true -> freopen log hooks -> init -> load scene + StartPlay() before main loop -> in loop editor branch skipped, player branch shows fullscreen game view.

#



# HARD RULES (learned the hard way, do not repeat)
1. NEVER apply multi-line PowerShell .Replace() to source files - it corrupts them. Edit line-based: Get-Content into array, Insert/RemoveRange at verified indexes, verify with Select-String after.
2. while(!window->ShouldClose()) has its opening brace on the NEXT line. Never insert statements between the while line and the brace.
3. git commit BEFORE and AFTER any risky automated edit.
4. Before trusting a test: compare timestamps of x64/Release/VisualEngine.exe and project/Build/MyGame.exe - stale binaries caused many false conclusions. build_game removes dest exe before copy now.




5. main.cpp is huge: make small anchored edits; temporary log markers ([boot]/[frame]) must be removed before commits.
6. PowerShell: wrap C++ snippets in single-quoted strings; prefer line arrays over regex for structural edits.




## Conventions
- VE:: namespace for core modules; globals prefixed g_; console registration via VE::Console::Get().AddCmd/AddInt/AddBool/AddFloat.
-


 Russian comments are normal in this codebase.

## Status / TODO
-


 Editor: stable, console with 40+ commands works.
- Player mode: rendering path under improvement (see git history: direct backbuffer prototype). Verify scene visibility in built exe next.
-

 Build & Export: build_game command works from console; editor menu integration planned.
