# Установка зависимостей Lua

## Проблема
При запуске VisualEngine.exe выдаёт ошибку:
```
Не удается продолжить выполнение кода, поскольку система не обнаружила lua55.dll
```

## Решение

### Вариант 1: Скачать готовые DLL (Быстрый способ)

1. Скачайте Lua 5.5 для Windows x64:
   - https://github.com/thepinecone/lua-binaries/releases
   - Ищите `lua-5.5.0_Win64_dllw.zip`

2. Распакуйте архив и найдите:
   - `lua55.dll`
   - `lua55.lib` (по желанию)

3. Скопируйте файлы сюда:
   ```
   VisualEngine/external/Lua/lib/
   ```

4. Скопируйте `lua55.dll` сюда (для запуска):
   ```
   VisualEngine/x64/Debug/
   VisualEngine/x64/Release/
   ```

### Вариант 2: Скомпилировать самому (Сложный способ)

1. Клонируйте Lua:
   ```bash
   git clone https://github.com/lua/lua.git
   cd lua
   ```

2. Скомпилируйте для Windows x64 (нужен Visual Studio):
   ```bash
   cd src
   cl.exe /D_WINDOWS /MD /O2 *.c
   link.exe /DLL /OUT:lua55.dll *.obj
   ```

3. Скопируйте `lua55.dll` и `lua55.lib` в `external/Lua/lib/`

### Вариант 3: Использовать готовый пакет

Скачайте уже компилированные файлы:
- https://luabinaries.sourceforge.io/
- Версия Lua 5.5 для Windows (x64)

## Проверка

После копирования DLL, запустите:
```bash
cd x64/Debug
VisualEngine.exe
```

Если всё работает - чудесно! ✅

## Почему это происходит?

- `external/` папка содержит готовые скомпилированные библиотеки
- Она не была загружена в Git (слишком большая)
- При удалении `x64/` папки для оптимизации размера, DLL тоже удалились
- При запуске exe Windows ищет `lua55.dll` в папке приложения

## Как это избежать в будущем

1. **Для распространения:** Включайте только `src/` и `.sln` файлы
2. **Для разработчиков:** Добавьте инструкцию установки зависимостей в README
3. **В CI/CD:** Скачивайте DLL автоматически перед сборкой

## Ссылки

- Lua: https://www.lua.org/
- Pre-built binaries: https://github.com/thepinecone/lua-binaries
- Lua Binaries: https://luabinaries.sourceforge.io/

---

**Нужна помощь?** Проверьте что:
- ✓ DLL находится рядом с .exe файлом
- ✓ Архитектура DLL совпадает (x64 для x64 программы)
- ✓ Путь к DLL очищен в проекте (не должно быть абсолютных путей)
