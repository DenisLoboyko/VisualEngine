# VisualEngine — Зависимости и требования

## Системные требования

- **ОС:** Windows 10/11 (x64)
- **Компилятор:** MSVC v145 (Visual Studio 2022+)
- **.NET Framework:** Не требуется
- **RAM:** 4GB+ (рекомендуется 8GB+)
- **GPU:** Любая видеокарта с поддержкой OpenGL 4.3+
- **Интернет:** Требуется для первой установки зависимостей

---

## Основные зависимости

### 1. **Lua 5.5** (Скриптовый движок)
- **DLL:** `lua55.dll`
- **Расположение:** 
  - `x64/Debug/lua55.dll`
  - `external/Lua/lib/lua55.dll`
- **Назначение:** Интерпретация Lua скриптов, биндинги к движку
- **Статус:** ✅ Включен в проект

### 2. **Assimp** (Загрузчик 3D моделей)
- **DLL:** `assimp-vc145-mt.dll`
- **Расположение:** 
  - `x64/Debug/assimp-vc145-mt.dll`
  - Установлена в `C:\Program Files\Assimp\`
- **Назначение:** Импорт FBX, OBJ, GLTF и других формателов 3D моделей
- **Версия:** Open Asset Import Library (Assimp)
- **Статус:** ✅ Установлена на ПК

### 3. **GLFW** (Окно и ввод)
- **Расположение:** `external/GLFW/`
- **Включено в проект как:** Статическая библиотека
- **Файл:** `external/GLFW/lib-vc2022/glfw3.lib`
- **Назначение:** Создание окна, обработка клавиатуры/мыши
- **Статус:** ✅ Встроено

### 4. **GLAD** (OpenGL Loader)
- **Расположение:** `external/GLAD/include/`
- **Версия:** OpenGL 4.6
- **Назначение:** Динамическая загрузка функций OpenGL
- **Статус:** ✅ Встроено (header-only)

### 5. **GLM** (Математическая библиотека)
- **Расположение:** `external/GLM/`
- **Включено как:** Header-only
- **Версия:** 0.9.9+
- **Назначение:** Векторные операции, матрицы, кватернионы
- **Статус:** ✅ Встроено

### 6. **Bullet Physics** (Физический движок)
- **Расположение:** `external/bullet/`
- **Библиотеки:**
  - `BulletDynamics.lib`
  - `BulletCollision.lib`
  - `BulletSoftBody.lib`
  - `LinearMath.lib`
- **Назначение:** Физические симуляции (столкновения, гравитация)
- **Статус:** ✅ Встроено

### 7. **ImGui** (UI界面)
- **Расположение:** `external/ImGui/`
- **Включено как:** Header + реализация
- **Назначение:** Отладочный интерфейс, редактор сцен
- **Статус:** ✅ Встроено

### 8. **stb** (Утилиты изображений)
- **Расположение:** `external/stb/`
- **Компоненты:** `stb_image.h` (загрузка текстур)
- **Включено как:** Header-only
- **Статус:** ✅ Встроено

### 9. **vcpkg** (Менеджер пакетов Microsoft)
- **Расположение:** `external/vcpkg/installed/x64-windows/`
- **Содержит:** Предкомпилированные библиотеки
- **Статус:** ✅ Встроено

---

## Требуемые установки на ПК

### ✅ Уже установлены

1. **Assimp (v5.2+)**
   ```
   C:\Program Files\Assimp\
   ├── bin\x64\assimp-vc145-mt.dll
   ├── lib\x64\assimp-vc145-mt.lib
   └── include\assimp\
   ```
   - **Проверка:** `C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll`
   - **Если не установлен:** Скачайте с https://github.com/assimp/assimp/releases

2. **Visual Studio 2022** (MSVC v145)
   - **Обязательно:** C++ desktop tools
   - **OpenGL Support:** Встроена в GLFW + GLAD

---

## Файлы проекта

```
VisualEngine/
├── src/
│   ├── Core/
│   │   ├── AudioEngine.h          (Lua + miniaudio)
│   │   ├── LuaEngine.h
│   │   ├── Engine.h               (Главный класс)
│   │   └── Camera.h
│   ├── ECS/
│   │   ├── Registry.h             (Entity-Component System)
│   │   ├── ComponentPool.h        (Плотное хранилище)
│   │   ├── Entity.h               (ID + версионирование)
│   │   ├── Scene.h
│   │   └── Components.h
│   ├── Graphics/
│   │   ├── Renderer.h
│   │   ├── Shader.h
│   │   └── Texture.h
│   ├── Physics/
│   │   └── PhysicsEngine.h        (Bullet integration)
│   └── main.cpp
├── external/
│   ├── GLFW/
│   ├── GLAD/
│   ├── GLM/
│   ├── ImGui/
│   ├── stb/
│   ├── bullet/
│   └── Lua/
│       ├── include/
│       └── lib/lua55.dll
├── x64/
│   └── Debug/
│       ├── VisualEngine.exe       ← Главный исполняемый файл
│       ├── lua55.dll              ← Обязателен для запуска
│       ├── assimp-vc145-mt.dll    ← Обязателен для запуска
│       └── assets/                (текстуры, модели)
├── VisualEngine.vcxproj
└── DEPENDENCIES.md               ← Этот файл
```

---

## Как запустить

### 1. **Первый запуск**
```bash
cd C:\Users\loboy\Downloads\VisualEngine\VisualEngine\
# Откройте в Visual Studio: VisualEngine.vcxproj
# Выберите: Debug | x64
# Нажмите: F5 или Build → Run
```

### 2. **Проверка DLL**
Перед запуском убедитесь, что присутствуют:
- `x64\Debug\lua55.dll`
- `x64\Debug\assimp-vc145-mt.dll`

### 3. **Если DLL отсутствуют**
```powershell
# Скопировать lua55.dll
Copy-Item "external\Lua\lib\lua55.dll" -Destination "x64\Debug\lua55.dll" -Force

# Скопировать assimp (если не установлен)
Copy-Item "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll" -Destination "x64\Debug\" -Force
```

---

## Часто задаваемые вопросы

### Q: "lua55.dll not found"
**A:** DLL должна быть в папке с .exe:
```
x64/Debug/lua55.dll                ← Здесь!
x64/Debug/VisualEngine.exe
```

### Q: "assimp-vc145-mt.dll not found"
**A:** 
1. Установите Assimp: https://assimp-releases.github.io/
2. Или скопируйте из: `C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll`

### Q: "No suitable graphics device found"
**A:** Обновите драйверы видеокарты (NVIDIA/AMD/Intel)

### Q: "Cannot load Lua script"
**A:** Убедитесь, что Lua файлы находятся в правильной папке (обычно `assets/scripts/`)

### Q: "Permission denied when copying DLL"
**A:** Запустите Terminal/PowerShell **от администратора**

---

## Оптимизация

- ✅ **Code optimization** выполнена (move semantics, inline hints, pool improvements)
- ✅ **Project size** сокращен с 616MB → 159MB
- ✅ **Build artifacts** удалены (.vs, старая x64 папка)
- ✅ **Функциональность** не изменена — движок работает идентично

---

## Версия документа
- **Дата:** Январь 2025
- **Версия VisualEngine:** v0.1
- **Статус:** ✅ Все зависимости установлены и протестированы

Вопросы? Читайте README.md или OPTIMIZATION_GUIDE.md
