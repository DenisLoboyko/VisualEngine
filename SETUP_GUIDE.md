# VisualEngine — Пошаговое руководство установки

## 📋 Содержание
1. [Быстрый старт (5 минут)](#быстрый-старт)
2. [Требования](#требования)
3. [Полная установка](#полная-установка)
4. [Решение проблем](#решение-проблем)
5. [После установки](#после-установки)

---

## 🚀 Быстрый старт

### Если у вас уже есть VS2022 + Assimp:

```powershell
# 1. Перейти в папку проекта
cd VisualEngine

# 2. Открыть в Visual Studio
start VisualEngine.vcxproj

# 3. В Visual Studio: Debug | x64 → F5 или Ctrl+F5
```

✅ **Готово!** Если приложение запустится — пропустите установку.

---

## ✅ Требования

| Компонент | Версия | Обязателен? |
|-----------|--------|------------|
| Windows | 10/11 x64 | ✅ Да |
| Visual Studio | 2022+ (v145) | ✅ Да |
| C++ Desktop Tools | Любая | ✅ Да |
| Assimp | 5.2+ | ✅ Да |
| Lua 5.5 | 5.5+ | ✅ Да (включено) |
| Git | Любая | ⭕ Опционально |

---

## 🔧 Полная установка

### Шаг 1: Visual Studio 2022

**Уже установлен?** → Переходите к Шагу 2.

**Установка:**
1. Скачайте: https://visualstudio.microsoft.com/downloads/
2. Выберите: **Visual Studio 2022 Community**
3. При установке отметьте:
   - ✅ C++ desktop development tools
   - ✅ Windows 10 SDK (или 11)
   - ✅ MSVC v145

**Проверка:**
```powershell
# Откройте PowerShell и проверьте версию
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33706\bin\Hostx64\x64\cl.exe" /?
```

---

### Шаг 2: Assimp (3D Model Loader)

**Уже установлен?** Проверьте:
```powershell
Test-Path "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll"
```

**Установка Assimp:**

#### Вариант A: Готовый инсталлятор (легко)
1. Перейти: https://assimp-releases.github.io/
2. Скачать: **Windows (MSVC 2022, x64, DLL)**
3. Запустить инсталлятор
4. Выбрать: `C:\Program Files\Assimp`

#### Вариант B: Компиляция из исходников (если не работает A)
```powershell
# Скачиваем Assimp
git clone https://github.com/assimp/assimp.git
cd assimp
mkdir build
cd build

# Компилируем (MSVC 2022)
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# DLL будет в: assimp\bin\Release\assimp-vc145-md.dll
# Скопируйте в C:\Program Files\Assimp\bin\x64\
```

#### Проверка Assimp:
```powershell
# Проверить установку
Test-Path "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll"

# Вывод: True (если установлено)
```

---

### Шаг 3: Копирование DLL в проект

Запустите PowerShell **от администратора** в папке проекта:

```powershell
cd C:\Users\loboy\Downloads\VisualEngine\VisualEngine

# 1. Создаём папку Debug если не существует
New-Item -Path "x64\Debug" -ItemType Directory -Force | Out-Null

# 2. Копируем Assimp DLL
Copy-Item "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll" -Destination "x64\Debug\" -Force

# 3. Копируем Lua DLL (уже должна быть)
Copy-Item "external\Lua\lib\lua55.dll" -Destination "x64\Debug\" -Force

# 4. Проверяем
ls x64\Debug\*.dll

# Вывод должен содержать:
# - assimp-vc145-mt.dll
# - lua55.dll
```

---

### Шаг 4: Сборка проекта

1. **Откройте Visual Studio 2022**
2. **Откройте проект:** File → Open → Project → `VisualEngine.vcxproj`
3. **Выберите конфигурацию:**
   - Вверху: **Debug | x64**
4. **Пересоберите:** Build → Rebuild Solution (Ctrl+Alt+F9)
5. **Результат:**
   ```
   Build succeeded.
   0 errors, 0 warnings
   ```

---

### Шаг 5: Запуск

**Вариант A: Из Visual Studio**
- Нажмите **F5** или **Ctrl+F5**

**Вариант B: Прямой запуск**
```powershell
cd x64\Debug
.\VisualEngine.exe
```

**Успешно, если:**
- ✅ Откроется окно "VisualEngine v0.1"
- ✅ Разрешение 1920x1080 (по умолчанию)
- ✅ Консоль выведет "Engine starting..."

---

## 🐛 Решение проблем

### ❌ "lua55.dll not found"
```powershell
# Проверьте наличие файла
Test-Path "x64\Debug\lua55.dll"

# Если False, скопируйте
Copy-Item "external\Lua\lib\lua55.dll" -Destination "x64\Debug\lua55.dll" -Force
```

### ❌ "assimp-vc145-mt.dll not found"
```powershell
# Проверьте установку Assimp
Test-Path "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll"

# Если False:
# 1. Установите Assimp вручную (см. Шаг 2)
# 2. ИЛИ скопируйте DLL вручную:

Copy-Item "C:\Program Files\Assimp\bin\x64\assimp-vc145-mt.dll" -Destination "x64\Debug\" -Force
```

### ❌ "MSVCP140.dll not found"
```powershell
# Установите Visual C++ Redistributable
# Скачайте: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist
# 64-bit версия (vc_redist.x64.exe)
```

### ❌ "VCOMP140.dll not found"
```powershell
# Этот файл обычно есть в Visual Studio
# Если нет, скопируйте из:
# C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33706\bin\Hostx64\x64\vcomp140.dll

# В папку:
# x64\Debug\vcomp140.dll
```

### ❌ Build Error: "Cannot find glfw3.lib"
```
✓ Это нормально - GLFW встроена
✓ Проверьте path в .vcxproj:
   external\GLFW\lib-vc2022\glfw3.lib
```

### ❌ Runtime Error: "No suitable graphics device"
```
1. Обновите драйверы видеокарты
   - NVIDIA: https://www.nvidia.com/Download/driverDetails.aspx
   - AMD: https://www.amd.com/en/support
   - Intel: https://www.intel.com/content/www/us/en/support/detect.html
2. Убедитесь, что OpenGL 4.3+ поддерживается
```

---

## 📦 После установки

### Проверка работоспособности

Откройте PowerShell и выполните:

```powershell
cd C:\Users\loboy\Downloads\VisualEngine\VisualEngine\x64\Debug

# Запустите с логированием
$env:VCPKG_VERBOSE = $true
.\VisualEngine.exe 2>&1 | Tee-Object -FilePath "engine.log"

# Проверьте лог
cat engine.log
```

**Ожидаемый вывод:**
```
Engine starting...
Window created: VisualEngine v0.1 (1920x1080)
[Audio] Initialized
```

### Структура файлов после установки

```
VisualEngine/
├── x64/
│   └── Debug/
│       ├── VisualEngine.exe          ✅ Исполняемый файл
│       ├── lua55.dll                 ✅ Обязателен
│       ├── assimp-vc145-mt.dll       ✅ Обязателен
│       ├── vcomp140.dll              ✅ Может потребоваться
│       └── assets/                   📂 Ресурсы (текстуры, модели)
├── src/                              📂 Исходный код
├── external/                         📂 Библиотеки
├── VisualEngine.vcxproj              ⚙️ Конфигурация проекта
├── DEPENDENCIES.md                   📖 Список зависимостей
└── SETUP_GUIDE.md                    📖 Этот файл
```

### Дополнительная настройка

**Добавить больше памяти для кэша:**
```cpp
// В src/Core/Engine.h
static const int COMPONENT_POOL_SIZE = 10000; // По умолчанию 1024
```

**Изменить разрешение окна:**
```cpp
// В src/main.cpp или Engine.h
const int WINDOW_WIDTH = 2560;   // Вместо 1920
const int WINDOW_HEIGHT = 1440;  // Вместо 1080
```

---

## 📞 Помощь и поддержка

- **Документация:** 
  - DEPENDENCIES.md — все зависимости
  - OPTIMIZATION_GUIDE.md — оптимизации
  - README.md — общая информация

- **GitHub Issues:**
  - https://github.com/DenisLoboyko/VisualEngineBETA/issues

- **Проверки:**
  - Все ли DLL на месте?
  - Выбран ли Debug | x64?
  - Свежая ли сборка (Rebuild)?

---

## ✨ Готово!

Если следовали инструкциям и:
- ✅ VS2022 установлена
- ✅ Assimp установлен
- ✅ DLL скопированы
- ✅ Проект пересобран
- ✅ VisualEngine.exe запускается

**Поздравляем! VisualEngine готов к разработке!** 🎉

---

**Версия:** 1.0  
**Последнее обновление:** Январь 2025  
**Статус:** ✅ Протестировано и работает
