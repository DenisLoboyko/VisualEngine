#pragma once
// =========================================================
//  SceneManager.h  —  Scene Manager для VisualEngine
//
//  Позволяет переключаться между сценами во время игры.
//
//  Lua API:
//    SceneManager.LoadScene("Assets/Scenes/menu.vescene")
//    SceneManager.ReloadScene()
//    SceneManager.GetCurrentScene()  -> string
//
//  C++ API:
//    VE::SceneManager::Get().RequestLoad("Assets/Scenes/menu.vescene");
//    VE::SceneManager::Get().RequestReload();
//    VE::SceneManager::Get().Tick();   // вызывать в начале каждого кадра
//    VE::SceneManager::Get().HasPendingLoad()
//    VE::SceneManager::Get().GetPendingPath()
// =========================================================

extern "C" {
    #include "../../external/Lua/include/lua.h"
    #include "../../external/Lua/include/lualib.h"
    #include "../../external/Lua/include/lauxlib.h"
}

#include <string>
#include <functional>
#include <iostream>

namespace VE {

    class SceneManager
    {
    public:
        static SceneManager& Get()
        {
            static SceneManager instance;
            return instance;
        }
        SceneManager(const SceneManager&)            = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        // ── Установить коллбэк загрузки (вызывается из main.cpp) ──
        // main.cpp передаёт свою функцию LoadScene сюда
        void SetLoadCallback(std::function<void(const std::string&)> cb)
        {
            m_LoadCallback = cb;
        }

        // ── Запросить загрузку сцены (можно вызывать из Lua/C++) ──
        void RequestLoad(const std::string& path)
        {
            m_PendingPath   = path;
            m_HasPending    = true;
            m_ReloadCurrent = false;
        }

        // ── Перезагрузить текущую сцену ──
        void RequestReload()
        {
            if (m_CurrentPath.empty()) return;
            m_PendingPath   = m_CurrentPath;
            m_HasPending    = true;
            m_ReloadCurrent = true;
        }

        // ── Обновить текущую сцену (вызывать из main после загрузки) ──
        void SetCurrent(const std::string& path)
        {
            m_CurrentPath = path;
        }

        std::string GetCurrent() const { return m_CurrentPath; }

        // ── Проверка и выполнение отложенной загрузки ──
        // Вызывай в начале кадра, ДО рендера.
        // Возвращает true если была загрузка (кадр надо пропустить).
        bool Tick()
        {
            if (!m_HasPending) return false;
            m_HasPending = false;

            if (m_LoadCallback) {
                std::cout << "[SceneManager] Loading: " << m_PendingPath << "\n";
                m_LoadCallback(m_PendingPath);
                m_CurrentPath = m_PendingPath;
            } else {
                std::cerr << "[SceneManager] No load callback set!\n";
            }

            m_PendingPath.clear();
            return true;
        }

        bool HasPendingLoad()    const { return m_HasPending; }
        std::string GetPendingPath() const { return m_PendingPath; }

        // ── Lua биндинги ──
        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            // SceneManager.LoadScene(path)
            lua_pushstring(L, "LoadScene");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                const char* path = luaL_checkstring(LS, 1);
                SceneManager::Get().RequestLoad(path);
                return 0;
            });
            lua_settable(L, -3);

            // SceneManager.ReloadScene()
            lua_pushstring(L, "ReloadScene");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                SceneManager::Get().RequestReload();
                return 0;
            });
            lua_settable(L, -3);

            // SceneManager.GetCurrentScene() -> string
            lua_pushstring(L, "GetCurrentScene");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                lua_pushstring(LS, SceneManager::Get().GetCurrent().c_str());
                return 1;
            });
            lua_settable(L, -3);

            lua_setglobal(L, "SceneManager");
            std::cout << "[SceneManager] Lua API registered (SceneManager.*)\n";
        }

    private:
        SceneManager() = default;

        std::function<void(const std::string&)> m_LoadCallback;

        std::string m_CurrentPath;
        std::string m_PendingPath;
        bool        m_HasPending    = false;
        bool        m_ReloadCurrent = false;
    };

} // namespace VE
