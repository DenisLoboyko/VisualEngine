#pragma once
// =========================================================
//  AudioEngine.h  —  аудио система для VisualEngine
//
//  Использует miniaudio (один header-файл, без зависимостей)
//  Скачай: https://github.com/mackron/miniaudio/blob/master/miniaudio.h
//  Положи файл сюда: src/Core/miniaudio.h
//
//  Подключение в main.cpp (ОДИН РАЗ, перед остальными #include):
//    #define MINIAUDIO_IMPLEMENTATION
//    #include "Core/AudioEngine.h"
//
//  Инициализация в main() после создания окна:
//    VE::AudioEngine::Get().Init();
//
//  Завершение перед закрытием окна:
//    VE::AudioEngine::Get().Shutdown();
//
//  Регистрация Lua API (в LuaEngine::registerFunctions() или после Init):
//    VE::AudioEngine::Get().RegisterLua(L);
//
//  ── C++ API ──────────────────────────────────────────────
//  VE::AudioEngine::Get().PlaySound("assets/sounds/jump.wav");
//  VE::AudioEngine::Get().PlaySound("assets/sounds/hit.wav", 0.5f);
//  VE::AudioEngine::Get().PlayMusic("assets/music/theme.mp3");
//  VE::AudioEngine::Get().StopMusic();
//  VE::AudioEngine::Get().SetMasterVolume(0.7f);
//
//  ── Lua API ──────────────────────────────────────────────
//  Audio.PlaySound("assets/sounds/jump.wav")
//  Audio.PlaySound("assets/sounds/shoot.wav", 0.8)
//  Audio.PlayMusic("assets/music/theme.mp3")
//  Audio.PlayMusic("assets/music/theme.mp3", 0.5)
//  Audio.StopMusic()
//  Audio.PauseMusic()
//  Audio.ResumeMusic()
//  Audio.StopAllSounds()
//  Audio.SetMusicVolume(0.5)
//  Audio.SetMasterVolume(0.7)
//  Audio.IsMusicPlaying()   -- returns bool
// =========================================================

#include "miniaudio.h"

extern "C" {
    #include "../../external/Lua/include/lua.h"
    #include "../../external/Lua/include/lualib.h"
    #include "../../external/Lua/include/lauxlib.h"
}

#include <string>
#include <memory>
#include <iostream>
#include <algorithm>

namespace VE {

    // =========================================================
    //  Слот для одного короткого звука (эффект)
    // =========================================================
    struct SoundSlot {
        ma_sound    sound;
        bool        inUse = false;
    };

    // =========================================================
    //  AudioEngine — синглтон, вся аудио система
    // =========================================================
    class AudioEngine
    {
    public:
        static AudioEngine& Get()
        {
            static AudioEngine instance;
            return instance;
        }
        AudioEngine(const AudioEngine&)            = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        // ── Инициализация ────────────────────────────────────

        bool Init()
        {
            if (m_Initialized) return true;

            ma_result result = ma_engine_init(nullptr, &m_Engine);
            if (result != MA_SUCCESS) {
                std::cerr << "[Audio] Ошибка инициализации: "
                          << ma_result_description(result) << "\n";
                return false;
            }

            m_Initialized = true;
            std::cout << "[Audio] Инициализирован\n";
            return true;
        }

        void Shutdown()
        {
            if (!m_Initialized) return;

            StopMusic();
            StopAllSounds();

            ma_engine_uninit(&m_Engine);
            m_Initialized = false;
        }

        // ── Звуковые эффекты (несколько одновременно) ────────

        // path   — путь к файлу (.wav / .mp3 / .ogg)
        // volume — громкость 0.0 .. 1.0 (по умолчанию 1.0)
        void PlaySound(const std::string& path, float volume = 1.0f)
        {
            if (!m_Initialized) return;

            SoundSlot* slot = GetFreeSlot();
            if (!slot) {
                // Если все слоты заняты — ищем уже закончившийся
                // Начинаем с m_LastSearchSlot для избежания линейного поиска каждый раз
                for (int i = 0; i < POOL_SIZE; ++i) {
                    int idx = (m_LastSearchSlot + i) % POOL_SIZE;
                    if (!m_Pool[idx].inUse || !ma_sound_is_playing(&m_Pool[idx].sound)) {
                        ma_sound_uninit(&m_Pool[idx].sound);
                        m_Pool[idx].inUse = false;
                        slot = &m_Pool[idx];
                        m_LastSearchSlot = (idx + 1) % POOL_SIZE;
                        break;
                    }
                }
                if (!slot) return; // реально переполнен
            }

            ma_result result = ma_sound_init_from_file(
                &m_Engine, path.c_str(),
                MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
                nullptr, nullptr, &slot->sound
            );

            if (result != MA_SUCCESS) {
                std::cerr << "[Audio] Не удалось загрузить: " << path << "\n";
                return;
            }

            float v = std::clamp(volume, 0.0f, 1.0f);
            ma_sound_set_volume(&slot->sound, v);
            slot->inUse = true;

            ma_sound_start(&slot->sound);
        }

        void StopAllSounds()
        {
            for (auto& s : m_Pool) {
                if (s.inUse) {
                    ma_sound_stop(&s.sound);
                    ma_sound_uninit(&s.sound);
                    s.inUse = false;
                }
            }
        }

        // ── Музыка (зациклённый фоновый трек) ────────────────

        void PlayMusic(const std::string& path, float volume = 1.0f)
        {
            if (!m_Initialized) return;

            // Тот же трек уже играет — просто обновим громкость
            if (m_MusicPlaying && m_CurrentMusic == path) {
                SetMusicVolume(volume);
                return;
            }

            StopMusic();

            m_Music = std::make_unique<ma_sound>();

            ma_result result = ma_sound_init_from_file(
                &m_Engine, path.c_str(),
                MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
                nullptr, nullptr, m_Music.get()
            );

            if (result != MA_SUCCESS) {
                std::cerr << "[Audio] Не удалось загрузить музыку: " << path << "\n";
                m_Music.reset();
                return;
            }

            m_MusicVol = std::clamp(volume, 0.0f, 1.0f);
            ma_sound_set_looping(m_Music.get(), MA_TRUE);
            ma_sound_set_volume(m_Music.get(), m_MusicVol);
            ma_sound_start(m_Music.get());

            m_CurrentMusic = path;
            m_MusicPlaying = true;

            std::cout << "[Audio] Музыка: " << path << "\n";
        }

        void StopMusic()
        {
            if (!m_MusicPlaying || !m_Music) return;
            ma_sound_stop(m_Music.get());
            ma_sound_uninit(m_Music.get());
            m_Music.reset();
            m_MusicPlaying = false;
            m_CurrentMusic = "";
        }

        void PauseMusic()
        {
            if (m_Music) ma_sound_stop(m_Music.get());
        }

        void ResumeMusic()
        {
            if (m_Music) ma_sound_start(m_Music.get());
        }

        bool IsMusicPlaying() const
        {
            return m_Music && ma_sound_is_playing(m_Music.get());
        }

        // Громкость музыки 0..1 (не перезапускает трек)
        void SetMusicVolume(float v)
        {
            m_MusicVol = std::clamp(v, 0.0f, 1.0f);
            if (m_Music) ma_sound_set_volume(m_Music.get(), m_MusicVol);
        }

        float GetMusicVolume() const { return m_MusicVol; }

        // ── Мастер-громкость ──────────────────────────────────

        void SetMasterVolume(float v)
        {
            m_MasterVol = std::clamp(v, 0.0f, 1.0f);
            if (m_Initialized) ma_engine_set_volume(&m_Engine, m_MasterVol);
        }

        float GetMasterVolume() const { return m_MasterVol; }

        // ── Lua биндинги ──────────────────────────────────────
        // Вызвать один раз после Init(), передав lua_State из LuaEngine:
        //   VE::AudioEngine::Get().RegisterLua(luaEngine.L);

        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            // Audio.PlaySound(path, volume?)
            lua_pushstring(L, "PlaySound");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                const char* path = luaL_checkstring(LS, 1);
                float vol = lua_isnumber(LS, 2) ? (float)lua_tonumber(LS, 2) : 1.0f;
                AudioEngine::Get().PlaySound(path, vol);
                return 0;
            });
            lua_settable(L, -3);

            // Audio.PlayMusic(path, volume?)
            lua_pushstring(L, "PlayMusic");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                const char* path = luaL_checkstring(LS, 1);
                float vol = lua_isnumber(LS, 2) ? (float)lua_tonumber(LS, 2) : 1.0f;
                AudioEngine::Get().PlayMusic(path, vol);
                return 0;
            });
            lua_settable(L, -3);

            // Audio.StopMusic()
            lua_pushstring(L, "StopMusic");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                AudioEngine::Get().StopMusic(); return 0;
            });
            lua_settable(L, -3);

            // Audio.PauseMusic()
            lua_pushstring(L, "PauseMusic");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                AudioEngine::Get().PauseMusic(); return 0;
            });
            lua_settable(L, -3);

            // Audio.ResumeMusic()
            lua_pushstring(L, "ResumeMusic");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                AudioEngine::Get().ResumeMusic(); return 0;
            });
            lua_settable(L, -3);

            // Audio.StopAllSounds()
            lua_pushstring(L, "StopAllSounds");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                AudioEngine::Get().StopAllSounds(); return 0;
            });
            lua_settable(L, -3);

            // Audio.SetMusicVolume(v)
            lua_pushstring(L, "SetMusicVolume");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                AudioEngine::Get().SetMusicVolume((float)lua_tonumber(LS, 1)); return 0;
            });
            lua_settable(L, -3);

            // Audio.SetMasterVolume(v)
            lua_pushstring(L, "SetMasterVolume");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                AudioEngine::Get().SetMasterVolume((float)lua_tonumber(LS, 1)); return 0;
            });
            lua_settable(L, -3);

            // Audio.IsMusicPlaying() -> bool
            lua_pushstring(L, "IsMusicPlaying");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                lua_pushboolean(LS, AudioEngine::Get().IsMusicPlaying() ? 1 : 0);
                return 1;
            });
            lua_settable(L, -3);

            lua_setglobal(L, "Audio");

            std::cout << "[Audio] Lua API зарегистрирован (Audio.*)\n";
        }

    private:
        AudioEngine()  = default;
        ~AudioEngine() { Shutdown(); }

        // Вспомогательная функция для регистрации Lua функций
        static void LuaRegisterFunc(lua_State* L, const char* name, lua_CFunction func)
        {
            lua_pushstring(L, name);
            lua_pushcfunction(L, func);
            lua_settable(L, -3);
        }

        SoundSlot* GetFreeSlot()
        {
            for (auto& s : m_Pool)
                if (!s.inUse) return &s;
            return nullptr;
        }

        static constexpr int POOL_SIZE = 32;

        ma_engine           m_Engine{};
        bool                m_Initialized = false;

        SoundSlot           m_Pool[POOL_SIZE];
        int                 m_LastSearchSlot = 0;

        std::unique_ptr<ma_sound> m_Music;
        bool                m_MusicPlaying = false;
        std::string         m_CurrentMusic;

        float               m_MusicVol  = 1.0f;
        float               m_MasterVol = 1.0f;
    };

} // namespace VE
