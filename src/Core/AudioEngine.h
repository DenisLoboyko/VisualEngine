#pragma once
// AudioEngine.h — аудио VisualEngine (miniaudio, single-header).
// main.cpp: #define MINIAUDIO_IMPLEMENTATION, затем #include "Core/AudioEngine.h" ПЕРВЫМ.
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

struct SoundSlot {
    ma_sound sound;
    bool     inUse = false;
    int      id    = 0;
};

class AudioEngine
{
public:
    static AudioEngine& Get() { static AudioEngine instance; return instance; }
    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool Init()
    {
        if (m_Initialized) return true;
        ma_result result = ma_engine_init(nullptr, &m_Engine);
        if (result != MA_SUCCESS) {
            std::cerr << "[Audio] Init error: " << ma_result_description(result) << "\n";
            return false;
        }
        m_Initialized = true;
        std::cout << "[Audio] Initialized\n";
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

    int PlaySound(const std::string& path, float volume = 1.0f)
    {
        if (!m_Initialized) return 0;
        SoundSlot* slot = GetFreeSlot();
        if (!slot) {
            for (auto& s : m_Pool) {
                if (!ma_sound_is_playing(&s.sound)) {
 








                   ma_sound_uninit(&s.sound);
                    s.inUse = false;
                    slot = &s;
                    break;
                }
            }
 





           if (!slot) return 0;
        }
        ma_result result = ma_sound_init_from_file(&m_Engine, path.c_str(),
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, &slot->sound);
        if (result != MA_SUCCESS) {
 




           std::cerr << "[Audio] Failed to load: " << path << "\n";
            return 0;
        }
        ma_sound_set_volume(&slot->sound, std::clamp(volume, 0.0f, 1.0f));
        slot->inUse = true;
 




       slot->id    = ++m_NextSoundID;
        ma_sound_start(&slot->sound);
        return slot->id;
    }





    void StopSound(int id)
    {
        if (id <= 0) return;
 



       for (auto& s : m_Pool) {
            if (s.inUse && s.id == id) {
                ma_sound_stop(&s.sound);
                


ma_sound_uninit(&s.sound);
                s.inUse = false;
                s.id    = 0;
 


               return;
            }
        }
 


   }

    void StopAllSounds()
 


   {
        for (auto& s : m_Pool) {
            if (s.inUse) {
 


               ma_sound_stop(&s.sound);
                ma_sound_uninit(&s.sound);
 

               s.inUse = false;
                s.id    = 0;
            }
 


       }
    }

 


   void SetListenerPosition(float x, float y, float z)
    {
        if (!m_Initialized) return;
 


       ma_engine_listener_set_position(&m_Engine, 0, x, y, z);
    }



    void SetSoundPosition(int id, float x, float y, float z)
    {



        if (id <= 0) return;
        for (auto& s : m_Pool) {
 

           if (s.inUse && s.id == id) {
                ma_sound_set_position(&s.sound, x, y, z);
 

               ma_sound_set_spatialization_enabled(&s.sound, MA_TRUE);
                return;
 

           }
        }
 

   }
    void PlayMusic(const std::string& path, float volume = 1.0f)
    {
        if (!m_Initialized) return;
        if (m_MusicPlaying && m_CurrentMusic == path) { SetMusicVolume(volume); return; }
        StopMusic();
        m_Music = std::make_unique<ma_sound>();
        ma_result result = ma_sound_init_from_file(&m_Engine, path.c_str(),
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, m_Music.get());
        if (result != MA_SUCCESS) {
            std::cerr << "[Audio] Failed to load music: " << path << "\n";
            m_Music.reset();
            return;
        }
        m_MusicVol = std::clamp(volume, 0.0f, 1.0f);
        ma_sound_set_looping(m_Music.get(), MA_TRUE);
        ma_sound_set_volume(m_Music.get(), m_MusicVol);
        ma_sound_start(m_Music.get());
        m_CurrentMusic = path;
        m_MusicPlaying = true;
        std::cout << "[Audio] Music: " << path << "\n";
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

    void PauseMusic()  { if (m_Music) ma_sound_stop(m_Music.get()); }
    void ResumeMusic() { if (m_Music) ma_sound_start(m_Music.get()); }
    bool IsMusicPlaying() const { return m_Music && ma_sound_is_playing(m_Music.get()); }

    void SetMusicVolume(float v)
    {
        m_MusicVol = std::clamp(v, 0.0f, 1.0f);
        if (m_Music) ma_sound_set_volume(m_Music.get(), m_MusicVol);
    }
    float GetMusicVolume() const { return m_MusicVol; }

    void SetMasterVolume(float v)
    {
        m_MasterVol = std::clamp(v, 0.0f, 1.0f);
        if (m_Initialized) ma_engine_set_volume(&m_Engine, m_MasterVol);
    }
    float GetMasterVolume() const { return m_MasterVol; }

 
   void RegisterLua(lua_State* L);

private:
    AudioEngine()  = default;
    ~AudioEngine() { Shutdown(); }






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
    int                


 m_NextSoundID = 0;
    std::unique_ptr<ma_sound> m_Music;
    bool                m_MusicPlaying = false;
 


   std::string         m_CurrentMusic;
    float               m_MusicVol  = 1.0f;
    float               m_MasterVol = 1.0f;
}


;

inline void AudioEngine::RegisterLua(lua_State* L)
{
    lua_newtable(L);
    lua_pushstring(L, "PlaySound");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        const char* p = luaL_checkstring(LS, 1);
        float v = lua_isnumber(LS, 2) ? (float)lua_tonumber(LS, 2) : 1.0f;
        lua_pushinteger(LS, AudioEngine::Get().PlaySound(p, v));
        return 1;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "StopSound");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().StopSound((int)luaL_checkinteger(LS, 1));
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "StopAllSounds");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().StopAllSounds();
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "SetListenerPosition");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().SetListenerPosition((float)luaL_optnumber(LS, 1, 0.0),
            (float)luaL_optnumber(LS, 2, 0.0), (float)luaL_optnumber(LS, 3, 0.0));
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "SetSoundPosition");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().SetSoundPosition((int)luaL_checkinteger(LS, 1),
            (float)luaL_optnumber(LS, 2, 0.0), (float)luaL_optnumber(LS, 3, 0.0),
            (float)luaL_optnumber(LS, 4, 0.0));
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "PlayMusic");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        const char* p = luaL_checkstring(LS, 1);
        float v = lua_isnumber(LS, 2) ? (float)lua_tonumber(LS, 2) : 1.0f;
        AudioEngine::Get().PlayMusic(p, v);
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "StopMusic");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().StopMusic();
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "PauseMusic");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().PauseMusic();
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "ResumeMusic");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().ResumeMusic();
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "SetMusicVolume");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().SetMusicVolume((float)lua_tonumber(LS, 1));
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "SetMasterVolume");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        AudioEngine::Get().SetMasterVolume((float)lua_tonumber(LS, 1));
        return 0;
    }, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "IsMusicPlaying");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        lua_pushboolean(LS, AudioEngine::Get().IsMusicPlaying() ? 1 : 0);
        return 1;
    }, 0);
    lua_settable(L, -3);
    lua_setglobal(L, "Audio");
    std::cout << "[Audio] Lua API registered (Audio.*)\n";
}

} // namespace VE

