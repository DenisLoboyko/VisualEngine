# VisualEngine — Поиск и решение проблем

## 📋 Быстрая индексация ошибок

| Ошибка | Код | Шаг решения |
|--------|-----|------------|
| lua55.dll not found | 0xc0000135 | [L1](#l1-lua55dll-not-found) |
| assimp-vc145-mt.dll not found | 0xc0000135 | [A1](#a1-assimp-vc145-mtdll-not-found) |
| MSVCP140.dll not found | 0xc0000135 | [R1](#r1-msvcp140dll-not-found) |
| Build failed: glfw3.lib | C1083 | [B1](#b1-build-failed-glfw3lib) |
| Build failed: lua55.a | LNK1104 | [B2](#b2-build-failed-lua55a) |
| No graphics device | Runtime | [G1](#g1-no-suitable-graphics-device) |
| Skybox texture failed | Runtime | [G2](#g2-skybox-texture-failed) |
| Permission denied | PowerShell | [P1](#p1-permission-denied) |

---

## 🔴 DLL Ошибки (Runtime)

### L1: lua55.dll not found

**Сообщение:**
```
Не удается продолжить выполнение кода, поскольку система не обнаружила lua55.dII
Код выхода: -1073741515 (0xc0000135)
```

**Причины:**
- ❌ Файл не скопирован в папку с .exe
- ❌ Файл удален или перемещен
- ❌ Неверный путь в проекте

**Решение быстро:**

```powershell
# Вариант 1: Скопировать вручную
Copy-Item "external\Lua\lib\lua55.dll" -Destination "x64\Debug\lua55.dll" -Force

# Вариант 2: Скопировать из резервной копии
Copy-Item "external\Lua\lib\lua55.dll" -Destination "x64\Debug\" -Force

# Проверить наличие
ls x64\Debug\lua55.dll
```

**Проверка проекта:**
```cpp
// В vcxproj должно быть:
// <AdditionalDependencies>...liblua55.a...</AdditionalDependencies>
// <AdditionalLibraryDirectories>...external\Lua...</AdditionalLibraryDirectories>
```

**Ссылки:**
- Где найти DLL: `external/Lua/lib/lua55.dll`
- Резервная копия: `external/Lua/backup/lua55.dll` (если есть)

---

### A1: assimp-vc145-mt.dll not found

**Сообщение:**
```
Не удается продолжить выполнение кода, поскольку система не обнаружила assimp-vc145-mt.dII
Код выхода: -1073741515 (0xc0000135)
```

**Причины:**
- ❌ Assimp не установлена
- ❌ DLL не скопирована в x64\Debug
- ❌ Неправильная версия MSVC (не vc145)

**Решение:**

```powershell
# 1. Проверить наличие Assimp
Test-Path "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll"

# Если True - копируем:
Copy-Item "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll" -Destination "x64\Debug\" -Force

# Если False - устанавливаем Assimp (см. ниже)
```

**Установка Assimp:**

1. **Скачайте инсталлятор:**
   - https://assimp-releases.github.io/
   - Ищите: "Windows (MSVC 2022, x64, DLL)"

2. **Установите:**
   - Запустите .exe
   - Выберите папку: `C:\Program Files\Assimp`
   - Нажмите Install

3. **Проверьте:**
   ```powershell
   ls "C:\Program Files\Assimp\bin\x64\"
   # Должны быть файлы assimp*.dll
   ```

4. **Скопируйте в проект:**
   ```powershell
   Copy-Item "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll" -Destination "x64\Debug\" -Force
   ```

**Если инсталлятор не подошёл (компиляция):**

```powershell
# Клонируем исходники
git clone https://github.com/assimp/assimp.git
cd assimp
mkdir build
cd build

# Компилируем Visual Studio 2022
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# DLL будет в: assimp\bin\Release\assimp-vc145-md.dll
# Переименуйте в assimp-vc145-mt.dll если нужно
```

---

### R1: MSVCP140.dll not found

**Сообщение:**
```
Не удается продолжить выполнение кода, поскольку система не обнаружила MSVCP140.dll
```

**Причины:**
- ❌ Visual C++ Redistributable не установлен
- ❌ Версия не совпадает (нужна x64)

**Решение:**

```powershell
# 1. Скачайте Visual C++ Redistributable
# https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist

# 2. Выберите:
#    Visual Studio 2022, x64 (64-bit)
#    vc_redist.x64.exe

# 3. Установите и перезагрузитесь
```

**Или скопируйте вручную из Visual Studio:**

```powershell
# Путь к MSVCP140.dll в Vi Studio
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33706\bin\Hostx64\x64\MSVCP140.dll"

# Скопировать в папку Debug
Copy-Item $vsPath -Destination "x64\Debug\MSVCP140.dll" -Force
```

---

### V1: VCOMP140.dll not found

**Сообщение:**
```
Время выполнения ошибка R6034
VCOMP140.dll not found
```

**Причины:**
- ❌ Параллелизация OpenMP включена, но библиотека отсутствует
- ❌ Visual Studio установлена без OpenMP поддержки

**Решение:**

```powershell
# 1. Скопировать из Visual Studio
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33706\bin\Hostx64\x64\vcomp140.dll"
Copy-Item $vsPath -Destination "x64\Debug\vcomp140.dll" -Force

# 2. Или отключить OpenMP в проекте
# В Visual Studio: Project → Properties → C/C++ → Language → 
# Open MP Support → False
```

---

## 🔴 Build Ошибки (Компиляция)

### B1: Build failed: glfw3.lib

**Сообщение:**
```
Error C1083: Cannot open include file: 'GLFW/glfw3.h'
Error LNK1104: cannot open file 'glfw3.lib'
```

**Причины:**
- ❌ Папка `external/GLFW/` отсутствует или повреждена
- ❌ Неверный путь в .vcxproj

**Решение:**

```powershell
# 1. Проверить наличие GLFW
Test-Path "external\GLFW\include\GLFW\glfw3.h"
Test-Path "external\GLFW\lib-vc2022\glfw3.lib"

# Если оба False - восстановить из git
git checkout HEAD -- external\GLFW

# 2. Пересобрать
# В Visual Studio: Build → Rebuild Solution
```

**Проверка .vcxproj:**
```xml
<!-- Должны быть такие пути: -->
<AdditionalIncludeDirectories>
  ...external\GLFW\include...
</AdditionalIncludeDirectories>
<AdditionalLibraryDirectories>
  ...external\GLFW\lib-vc2022...
</AdditionalLibraryDirectories>
```

---

### B2: Build failed: lua55.a

**Сообщение:**
```
Error LNK1104: cannot open file 'liblua55.a'
```

**Причины:**
- ❌ Папка `external/Lua/` пустая
- ❌ Файл liblua55.a удален
- ❌ Неверная архитектура (32-bit вместо 64-bit)

**Решение:**

```powershell
# 1. Восстановить Lua из git
git checkout HEAD -- external/Lua

# 2. Проверить наличие
ls external/Lua/lib/

# 3. Пересобрать
# В Visual Studio: Build → Rebuild Solution
```

**Если файл все равно не найден:**

```powershell
# Найти вручную
Get-ChildItem -Recurse -Filter "*lua55*" -Path "external"

# Скопировать из резервной копии (если есть)
Copy-Item "backup/liblua55.a" -Destination "external/Lua/lib/" -Force
```

---

### B3: Build failed: Bullet Physics

**Сообщение:**
```
Error LNK1104: cannot open file 'BulletDynamics.lib'
```

**Решение:**

```powershell
# 1. Проверить папку Bullet
ls "external\bullet\build_md\lib\Release\"

# Должны быть:
# - BulletDynamics.lib
# - BulletCollision.lib
# - LinearMath.lib

# 2. Если нет - пересобрать Bullet
cd external\bullet
mkdir build_md
cd build_md

# Используйте CMake и Visual Studio 2022
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## 🔴 Runtime Ошибки

### G1: No suitable graphics device

**Сообщение:**
```
Runtime Error: No suitable graphics device found
Cannot initialize OpenGL
```

**Причины:**
- ❌ Видеокарта не поддерживает OpenGL 4.3+
- ❌ Драйверы не обновлены
- ❌ Интегрированная графика отключена в BIOS

**Решение:**

1. **Обновить драйверы:**
   ```
   NVIDIA: https://www.nvidia.com/Download/driverDetails.aspx
   AMD: https://www.amd.com/en/support
   Intel: https://www.intel.com/content/www/us/en/support/detect.html
   ```

2. **Проверить OpenGL:**
   ```powershell
   # Скачайте OpenGL Extensions Viewer
   # https://www.khronos.org/opengl/wiki/OpenGL_Extension_Viewer

   # Или используйте GPU-Z
   # https://www.techpowerup.com/gpu-z/
   ```

3. **Включить видеокарту в BIOS:**
   - Перезагрузитесь и нажмите F2/F10/Del (зависит от ПК)
   - Найти: Integrated Graphics / iGPU
   - Переключить на: Auto или Enabled

4. **Понизить требования OpenGL:**
   ```cpp
   // В src/Graphics/GraphicsContext.h
   // Измените версию с 4.6 на 4.3 (минимум)
   ```

---

### G2: Skybox texture failed

**Сообщение:**
```
Skybox texture failed: C:\...\assets\skybox\right.jpg
```

**Причины:**
- ⚠️ Это НЕ критическая ошибка (обычно)
- ❌ Папка `assets/skybox/` отсутствует
- ❌ Текстуры не скопированы в Debug папку

**Решение:**

```powershell
# Проверить структуру
ls x64\Debug\assets\skybox\

# Должны быть:
# - right.jpg
# - left.jpg
# - top.jpg
# - bottom.jpg
# - front.jpg
# - back.jpg

# Если отсутствуют:
# 1. Скопировать вручную или
# 2. Отключить skybox в коде
```

**Отключить skybox (если текстур нет):**
```cpp
// В src/Graphics/Renderer.h
// Закомментировать:
// renderSkybox();
```

---

### L2: Lua script not found

**Сообщение:**
```
Lua Error: Cannot load script 'assets/scripts/main.lua'
File not found at C:\...\x64\Debug\assets\scripts\main.lua
```

**Решение:**

```powershell
# 1. Создать папку
New-Item -Path "x64\Debug\assets\scripts" -ItemType Directory -Force

# 2. Скопировать скрипты
Copy-Item "assets\scripts\*" -Destination "x64\Debug\assets\scripts\" -Recurse -Force

# 3. Проверить
ls x64\Debug\assets\scripts\
```

---

## 🟡 Предупреждения (Не ошибки)

### W1: Warning C4996: 'function' was declared deprecated

**Сообщение:**
```
Warning C4996: 'strcpy': This function or variable may be unsafe
```

**Как исправить:**
```cpp
// Вариант 1: Использовать безопасную версию
strcpy_s(dest, size, src);  // Вместо strcpy

// Вариант 2: Подавить предупреждение
#pragma warning(disable:4996)
strcpy(dest, src);
```

---

### W2: Multiple definition of inline function

**Сообщение:**
```
Warning: inline function 'Func' defined in header included multiple times
```

**Как исправить:**
```cpp
// Добавьте в .h файл
#pragma once

// ИЛИ (для совместимости)
#ifndef MY_HEADER_H
#define MY_HEADER_H
// ... код ...
#endif
```

---

## 🟢 Успешно решено

✅ **Если вы видите:**
```
Build succeeded.
0 errors, 0 warnings

Engine starting...
Window created: VisualEngine v0.1 (1920x1080)
[Audio] Initialized
```

**Поздравляем! VisualEngine работает корректно!** 🎉

---

## 📞 Ничего не помогло?

1. **Проверьте:**
   - Все ли DLL на месте?
   - Правильная ли архитектура (x64)?
   - Правильная ли конфигурация (Debug)?

2. **Сделайте Clean build:**
   ```powershell
   # В Visual Studio
   Build → Clean Solution
   Build → Rebuild Solution
   ```

3. **Удалите кэш:**
   ```powershell
   # PowerShell от админа
   rm -Recurse -Force x64\Debug\*
   # Пересоберите
   ```

4. **Откройте Issue на GitHub:**
   - https://github.com/DenisLoboyko/VisualEngineBETA/issues
   - Приложите скриншоты ошибок
   - Укажите: версию Windows, VS, видеокарту

---

## 📋 Чек-лист диагностики

Перед тем, как просить помощь, проверьте:

- [ ] Windows 10/11 x64?
- [ ] Visual Studio 2022 установлена?
- [ ] MSVC v145 выбран (не v140)?
- [ ] Debug | x64 выбран?
- [ ] lua55.dll в x64\Debug?
- [ ] assimp-vc145-mt.dll в x64\Debug?
- [ ] Проект пересобран (Rebuild)?
- [ ] Консоль показывает "Engine starting..."?

---

**Версия:** 1.0  
**Последнее обновление:** Январь 2025  
**Актуальность:** ✅ Проверено на VisualEngine v0.1
