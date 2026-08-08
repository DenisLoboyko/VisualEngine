#pragma once
// =========================================================
//  TextureLoader.h  —  загрузка 2D текстур через stb_image
//
//  Требует: stb_image (vcpkg install stb  или  положи stb_image.h рядом)
//
//  Использование:
//    #define STB_IMAGE_IMPLEMENTATION   // ОДИН РАЗ, в одном .cpp файле
//    #include "TextureLoader.h"
//
//    GLuint id = VE::LoadTexture("assets/textures/wood.png");
//    VE::FreeTexture(id);
// =========================================================

#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// stb_image: подключаем ТОЛЬКО объявления функций (без STB_IMAGE_IMPLEMENTATION).
// Это безопасно делать в нескольких файлах — сам stb_image.h защищён от повторных
// объявлений своим include-guard'ом (STBI_INCLUDE_STB_IMAGE_H). Реализация (тела
// функций) генерируется только там, где ПЕРЕД инклудом стоит #define STB_IMAGE_IMPLEMENTATION
// — сейчас это делает Skybox.h, и здесь МЫ ЭТОТ МАКРОС НЕ ОПРЕДЕЛЯЕМ, так что
// дублирования реализации не будет, даже если TextureLoader.h подключится раньше Skybox.h.
#include "../../external/stb/stb_image.h"

namespace VE {

    // Кэш: путь -> GL текстура (не грузим одно и то же дважды)
    inline std::unordered_map<std::string, GLuint> g_TextureCache;

    // Белая 1x1 текстура — используется когда текстура не задана
    inline GLuint g_WhiteTexture = 0;

    inline void InitWhiteTexture()
    {
        if (g_WhiteTexture != 0) return;
        unsigned char white[4] = {255, 255, 255, 255};
        glGenTextures(1, &g_WhiteTexture);
        glBindTexture(GL_TEXTURE_2D, g_WhiteTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Вернуть белую текстуру (ID)
    inline GLuint GetWhiteTexture()
    {
        InitWhiteTexture();
        return g_WhiteTexture;
    }

    // Загрузить текстуру из файла. Возвращает GL ID, или 0 при ошибке.
    // При повторном вызове с тем же путём — вернёт кэш.
    inline GLuint LoadTextureRaw(const std::string& path)
    {
        // Проверить кэш
        auto it = g_TextureCache.find(path);
        if (it != g_TextureCache.end())
            return it->second;

        stbi_set_flip_vertically_on_load(true); // OpenGL UV снизу

        int w, h, channels;
        unsigned char* data = nullptr;
        // Retry с небольшой задержкой — Windows может ещё дописывать файл
        // после drag&drop (асинхронное копирование)
        for (int attempt = 0; attempt < 5 && !data; attempt++) {
            if (attempt > 0) {
                #ifdef _WIN32
                Sleep(60);
                #endif
            }
            data = stbi_load(path.c_str(), &w, &h, &channels, 0);
        }
        if (!data) {
            std::cerr << "[TextureLoader] Не удалось загрузить: " << path << "\n";
            return 0; // явный провал, НЕ белая текстура
        }

        GLenum fmt = GL_RGB;
        if (channels == 4) fmt = GL_RGBA;
        else if (channels == 1) fmt = GL_RED;

        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Повтор текстуры (тайлинг)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);

        g_TextureCache[path] = id;
        std::cout << "[TextureLoader] Загружено: " << path << " (" << w << "x" << h << ")\n";
        return id;
    }

    // Обёртка: при провале возвращает белую текстуру (старое поведение,
    // удобно для рендера — всегда что-то отрисуется).
    // Для применения текстуры к объекту (где важно знать успех/провал)
    // используй LoadTextureRaw() напрямую и проверяй на 0.
    inline GLuint LoadTexture(const std::string& path)
    {
        GLuint id = LoadTextureRaw(path);
        return id != 0 ? id : GetWhiteTexture();
    }

    // Освободить одну текстуру
    inline void FreeTexture(GLuint id)
    {
        for (auto it = g_TextureCache.begin(); it != g_TextureCache.end(); ++it) {
            if (it->second == id) {
                glDeleteTextures(1, &id);
                g_TextureCache.erase(it);
                return;
            }
        }
        glDeleteTextures(1, &id);
    }

    // Освободить весь кэш (вызвать при закрытии движка)
    inline void FreeAllTextures()
    {
        for (auto& [path, id] : g_TextureCache)
            glDeleteTextures(1, &id);
        g_TextureCache.clear();
        if (g_WhiteTexture) {
            glDeleteTextures(1, &g_WhiteTexture);
            g_WhiteTexture = 0;
        }
    }

} // namespace VE