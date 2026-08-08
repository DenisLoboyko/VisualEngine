#pragma once
// =========================================================
//  SaveSystem.h  —  Save/Load прогресса для VisualEngine
//
//  Сохраняет пары ключ=значение в простой текстовый файл.
//  Поддерживает: числа, строки, булевы значения.
//  Несколько слотов сохранения (slot 0, 1, 2...).
//
//  Положи файл: src/Core/SaveSystem.h
//
//  Lua API:
//    Save.SetInt("score", 100)
//    Save.SetFloat("posX", 3.14)
//    Save.SetString("playerName", "Hero")
//    Save.SetBool("levelUnlocked", true)
//
//    Save.GetInt("score")           -> number (0 если нет)
//    Save.GetFloat("posX")          -> number
//    Save.GetString("playerName")   -> string ("" если нет)
//    Save.GetBool("levelUnlocked")  -> bool
//
//    Save.Save()           -- сохранить слот 0
//    Save.Save(1)          -- сохранить слот 1
//    Save.Load()           -- загрузить слот 0
//    Save.Load(1)          -- загрузить слот 1
//    Save.Delete()         -- удалить слот 0
//    Save.Delete(1)        -- удалить слот 1
//    Save.Exists()         -> bool  -- существует ли слот 0
//    Save.Exists(1)        -> bool
//    Save.HasKey("score")  -> bool
// =========================================================

extern "C" {
    #include "../../external/Lua/include/lua.h"
    #include "../../external/Lua/include/lualib.h"
    #include "../../external/Lua/include/lauxlib.h"
}

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace VE {

    class SaveSystem
    {
    public:
        static SaveSystem& Get()
        {
            static SaveSystem instance;
            return instance;
        }
        SaveSystem(const SaveSystem&)            = delete;
        SaveSystem& operator=(const SaveSystem&) = delete;

        // ── Установить папку сохранений ──────────────────────
        // Вызови один раз из main после инициализации:
        //   VE::SaveSystem::Get().SetSaveDir(projectRoot + "\\Saves");
        void SetSaveDir(const std::string& dir)
        {
            m_SaveDir = dir;
            fs::create_directories(dir);
        }

        // ── Запись значений ──────────────────────────────────

        void SetInt   (const std::string& key, int val)
        {
            m_Data[key] = std::to_string(val);
        }
        void SetFloat (const std::string& key, float val)
        {
            m_Data[key] = std::to_string(val);
        }
        void SetString(const std::string& key, const std::string& val)
        {
            // Убираем переносы строк чтобы не сломать файл
            std::string safe = val;
            for (auto& c : safe) if (c=='\n'||c=='\r') c=' ';
            m_Data[key] = "\"" + safe + "\"";
        }
        void SetBool  (const std::string& key, bool val)
        {
            m_Data[key] = val ? "true" : "false";
        }

        // ── Чтение значений ──────────────────────────────────

        int GetInt(const std::string& key, int def=0) const
        {
            auto it = m_Data.find(key);
            if (it==m_Data.end()) return def;
            try { return std::stoi(it->second); } catch(...) { return def; }
        }
        float GetFloat(const std::string& key, float def=0.f) const
        {
            auto it = m_Data.find(key);
            if (it==m_Data.end()) return def;
            try { return std::stof(it->second); } catch(...) { return def; }
        }
        std::string GetString(const std::string& key, const std::string& def="") const
        {
            auto it = m_Data.find(key);
            if (it==m_Data.end()) return def;
            std::string v = it->second;
            // Убираем кавычки
            if (v.size()>=2 && v.front()=='"' && v.back()=='"')
                return v.substr(1, v.size()-2);
            return v;
        }
        bool GetBool(const std::string& key, bool def=false) const
        {
            auto it = m_Data.find(key);
            if (it==m_Data.end()) return def;
            return it->second == "true";
        }

        bool HasKey(const std::string& key) const
        {
            return m_Data.count(key) > 0;
        }

        void Clear() { m_Data.clear(); }

        // ── Сохранение в файл ────────────────────────────────

        bool Save(int slot=0)
        {
            std::string path = GetPath(slot);
            std::ofstream f(path);
            if (!f.is_open()) {
                std::cerr << "[Save] Cannot write: " << path << "\n";
                return false;
            }
            f << "# VisualEngine Save File (slot " << slot << ")\n";
            for (auto& [k, v] : m_Data)
                f << k << "=" << v << "\n";
            f.close();
            std::cout << "[Save] Saved slot " << slot << " -> " << path << "\n";
            return true;
        }

        // ── Загрузка из файла ────────────────────────────────

        bool Load(int slot=0)
        {
            std::string path = GetPath(slot);
            std::ifstream f(path);
            if (!f.is_open()) {
                std::cerr << "[Save] Slot " << slot << " not found: " << path << "\n";
                return false;
            }
            m_Data.clear();
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty() || line[0]=='#') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq+1);
                m_Data[key] = val;
            }
            std::cout << "[Save] Loaded slot " << slot << " (" << m_Data.size() << " keys)\n";
            return true;
        }

        // ── Удалить сохранение ───────────────────────────────

        bool Delete(int slot=0)
        {
            std::string path = GetPath(slot);
            if (!fs::exists(path)) return false;
            fs::remove(path);
            std::cout << "[Save] Deleted slot " << slot << "\n";
            return true;
        }

        bool Exists(int slot=0) const
        {
            return fs::exists(GetPath(slot));
        }

        // ── Lua биндинги ─────────────────────────────────────

        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            // Save.SetInt(key, val)
            lua_pushstring(L, "SetInt");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                int v = (int)luaL_checknumber(LS, 2);
                SaveSystem::Get().SetInt(k, v);
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Save.SetFloat(key, val)
            lua_pushstring(L, "SetFloat");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                float v = (float)luaL_checknumber(LS, 2);
                SaveSystem::Get().SetFloat(k, v);
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Save.SetString(key, val)
            lua_pushstring(L, "SetString");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                const char* v = luaL_checkstring(LS, 2);
                SaveSystem::Get().SetString(k, v);
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Save.SetBool(key, val)
            lua_pushstring(L, "SetBool");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                bool v = lua_toboolean(LS, 2) != 0;
                SaveSystem::Get().SetBool(k, v);
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Save.GetInt(key, default?)
            lua_pushstring(L, "GetInt");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                int def = lua_isnumber(LS,2)?(int)lua_tonumber(LS,2):0;
                lua_pushnumber(LS, SaveSystem::Get().GetInt(k, def));
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.GetFloat(key, default?)
            lua_pushstring(L, "GetFloat");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                float def = lua_isnumber(LS,2)?(float)lua_tonumber(LS,2):0.f;
                lua_pushnumber(LS, SaveSystem::Get().GetFloat(k, def));
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.GetString(key, default?)
            lua_pushstring(L, "GetString");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                const char* def = lua_isstring(LS,2)?lua_tostring(LS,2):"";
                lua_pushstring(LS, SaveSystem::Get().GetString(k, def).c_str());
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.GetBool(key, default?)
            lua_pushstring(L, "GetBool");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                bool def = lua_isboolean(LS,2)?lua_toboolean(LS,2)!=0:false;
                lua_pushboolean(LS, SaveSystem::Get().GetBool(k, def)?1:0);
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.HasKey(key)
            lua_pushstring(L, "HasKey");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* k = luaL_checkstring(LS, 1);
                lua_pushboolean(LS, SaveSystem::Get().HasKey(k)?1:0);
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.Save(slot?)
            lua_pushstring(L, "Save");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                int slot = lua_isnumber(LS,1)?(int)lua_tonumber(LS,1):0;
                lua_pushboolean(LS, SaveSystem::Get().Save(slot)?1:0);
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.Load(slot?)
            lua_pushstring(L, "Load");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                int slot = lua_isnumber(LS,1)?(int)lua_tonumber(LS,1):0;
                lua_pushboolean(LS, SaveSystem::Get().Load(slot)?1:0);
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.Delete(slot?)
            lua_pushstring(L, "Delete");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                int slot = lua_isnumber(LS,1)?(int)lua_tonumber(LS,1):0;
                lua_pushboolean(LS, SaveSystem::Get().Delete(slot)?1:0);
                return 1;
            }, 0);
            lua_settable(L, -3);

            // Save.Exists(slot?)
            lua_pushstring(L, "Exists");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                int slot = lua_isnumber(LS,1)?(int)lua_tonumber(LS,1):0;
                lua_pushboolean(LS, SaveSystem::Get().Exists(slot)?1:0);
                return 1;
            }, 0);
            lua_settable(L, -3);

            lua_setglobal(L, "Save");
            std::cout << "[SaveSystem] Lua API registered (Save.*)\n";

            // ── Удобные алиасы верхнего уровня — SaveGame()/LoadGame() ──
            // Просто вызывают Save.Save()/Save.Load() (слот 0 по умолчанию).
            // SaveGame(slot?) -> bool
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                int slot = lua_isnumber(LS,1)?(int)lua_tonumber(LS,1):0;
                lua_pushboolean(LS, SaveSystem::Get().Save(slot)?1:0);
                return 1;
            }, 0);
            lua_setglobal(L, "SaveGame");

            // LoadGame(slot?) -> bool
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                int slot = lua_isnumber(LS,1)?(int)lua_tonumber(LS,1):0;
                lua_pushboolean(LS, SaveSystem::Get().Load(slot)?1:0);
                return 1;
            }, 0);
            lua_setglobal(L, "LoadGame");
        }

    private:
        SaveSystem() = default;

        std::string GetPath(int slot) const
        {
            return m_SaveDir + "\\save_slot" + std::to_string(slot) + ".vesave";
        }

        std::string m_SaveDir = ".\\Saves";
        std::unordered_map<std::string, std::string> m_Data;
    };

} // namespace VE
