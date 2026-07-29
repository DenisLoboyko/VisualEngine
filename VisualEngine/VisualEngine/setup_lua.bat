@echo off
REM Скрипт для загрузки и установки Lua 5.5
REM Требует PowerShell и интернет соединение

echo.
echo ╔════════════════════════════════════════╗
echo ║   Установка Lua 5.5 для VisualEngine   ║
echo ╚════════════════════════════════════════╝
echo.

setlocal enabledelayedexpansion

REM Путь к папке проекта
set PROJECT_DIR=%~dp0\..
set LUA_LIB_DIR=%PROJECT_DIR%\external\Lua\lib
set TEMP_ZIP=%LUA_LIB_DIR%\lua_temp.zip
set DEBUG_DIR=%PROJECT_DIR%\x64\Debug
set RELEASE_DIR=%PROJECT_DIR%\x64\Release

echo [*] Путь проекта: %PROJECT_DIR%
echo.

REM Проверяем PowerShell
powershell -Command "Write-Host '[OK] PowerShell установлен'" 2>nul
if errorlevel 1 (
	echo [ERROR] PowerShell не найден!
	pause
	exit /b 1
)

echo.
echo [*] Загрузка Lua 5.5.0 для Windows x64...
echo     Это может занять 1-2 минуты...
echo.

REM Скачиваем ZIP
powershell -Command ^
  "$ProgressPreference = 'SilentlyContinue'; ^
   $url = 'https://github.com/thepinecone/lua-binaries/releases/download/lua5.5.0/lua-5.5.0_Win64_dllw.zip'; ^
   $output = '%TEMP_ZIP%'; ^
   try { ^
	 Invoke-WebRequest -Uri $url -OutFile $output -TimeoutSec 60; ^
	 Write-Host '[OK] Загружено'; ^
	 exit 0 ^
   } catch { ^
	 Write-Host '[ERROR] Не удалось загрузить: ' $_.Exception.Message; ^
	 exit 1 ^
   }"

if errorlevel 1 (
	echo.
	echo [ERROR] Загрузка не удалась!
	echo.
	echo Возможные причины:
	echo   - Нет интернета
	echo   - Ссылка на GitHub недоступна
	echo   - Истекло время ожидания
	echo.
	echo Решение: Скачайте вручную с
	echo https://github.com/thepinecone/lua-binaries/releases
	echo.
	pause
	exit /b 1
)

echo [*] Распаковка архива...

REM Распаковываем ZIP
powershell -Command ^
  "Expand-Archive -Path '%TEMP_ZIP%' -DestinationPath '%LUA_LIB_DIR%' -Force; ^
   Write-Host '[OK] Распаковано'"

if errorlevel 1 (
	echo [ERROR] Не удалось распаковать архив!
	pause
	exit /b 1
)

echo [*] Копирование DLL в build папки...

REM Ищем lua55.dll в распакованной папке
for /r "%LUA_LIB_DIR%" %%f in (lua55.dll) do (
	set LUA_DLL=%%f
)

if not defined LUA_DLL (
	echo [ERROR] lua55.dll не найден в архиве!
	pause
	exit /b 1
)

echo     Найден: !LUA_DLL!

REM Копируем в Debug
if exist "%DEBUG_DIR%" (
	copy "!LUA_DLL!" "%DEBUG_DIR%\lua55.dll" >nul 2>&1
	echo [OK] Скопирован в x64\Debug
) else (
	echo [*] x64\Debug не существует (создается при сборке)
)

REM Копируем в Release
if exist "%RELEASE_DIR%" (
	copy "!LUA_DLL!" "%RELEASE_DIR%\lua55.dll" >nul 2>&1
	echo [OK] Скопирован в x64\Release
) else (
	echo [*] x64\Release не существует (создается при сборке)
)

REM Удаляем временный ZIP
del "%TEMP_ZIP%" >nul 2>&1

echo.
echo ╔════════════════════════════════════════╗
echo ║     Установка завершена успешно! ✓    ║
echo ╚════════════════════════════════════════╝
echo.
echo Теперь вы можете:
echo   1. Пересобрать проект (Build -> Rebuild)
echo   2. Запустить VisualEngine.exe
echo.
echo Если вы переустановили x64 папку, запустите
echo этот скрипт ещё раз чтобы скопировать DLL!
echo.
pause
