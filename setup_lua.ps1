#!/usr/bin/env powershell
# Скрипт для загрузки и установки Lua 5.5
# Запуск: powershell -ExecutionPolicy Bypass -File setup_lua.ps1

Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   Установка Lua 5.5 для VisualEngine   ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Пути
$ProjectDir = Split-Path -Parent $PSCommandPath
$LuaLibDir = Join-Path $ProjectDir "external\Lua\lib"
$TempZip = Join-Path $LuaLibDir "lua_temp.zip"
$DebugDir = Join-Path $ProjectDir "x64\Debug"
$ReleaseDir = Join-Path $ProjectDir "x64\Release"

Write-Host "[*] Путь проекта: $ProjectDir`n"

# Проверяем/создаём папку
if (-not (Test-Path $LuaLibDir)) {
	New-Item -ItemType Directory -Path $LuaLibDir -Force | Out-Null
	Write-Host "[OK] Создана папка external\Lua\lib`n"
}

# Скачиваем
Write-Host "[*] Загрузка Lua 5.5.0 для Windows x64..."
Write-Host "    Это может занять 1-2 минуты...`n"

$ProgressPreference = 'SilentlyContinue'
$Url = "https://github.com/thepinecone/lua-binaries/releases/download/lua5.5.0/lua-5.5.0_Win64_dllw.zip"

try {
	Invoke-WebRequest -Uri $Url -OutFile $TempZip -TimeoutSec 60
	Write-Host "[OK] Загружено`n" -ForegroundColor Green
} catch {
	Write-Host "[ERROR] Не удалось загрузить: $($_.Exception.Message)`n" -ForegroundColor Red
	Write-Host "Возможные причины:" -ForegroundColor Yellow
	Write-Host "  - Нет интернета"
	Write-Host "  - Ссылка на GitHub недоступна"
	Write-Host "  - Истекло время ожидания`n"
	Write-Host "Решение: Скачайте вручную с" -ForegroundColor Yellow
	Write-Host "https://github.com/thepinecone/lua-binaries/releases`n"
	exit 1
}

# Распаковываем
Write-Host "[*] Распаковка архива..."
try {
	Expand-Archive -Path $TempZip -DestinationPath $LuaLibDir -Force
	Write-Host "[OK] Распаковано`n" -ForegroundColor Green
} catch {
	Write-Host "[ERROR] Не удалось распаковать: $($_.Exception.Message)`n" -ForegroundColor Red
	exit 1
}

# Ищем lua55.dll
$LuaDll = Get-ChildItem -Path $LuaLibDir -Recurse -Filter "lua55.dll" -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $LuaDll) {
	Write-Host "[ERROR] lua55.dll не найден в архиве!`n" -ForegroundColor Red
	exit 1
}

Write-Host "[*] Копирование DLL в build папки..."
Write-Host "    Найден: $($LuaDll.FullName)`n"

# Копируем в Debug
if (Test-Path $DebugDir) {
	Copy-Item -Path $LuaDll.FullName -Destination "$DebugDir\lua55.dll" -Force
	Write-Host "[OK] Скопирован в x64\Debug" -ForegroundColor Green
} else {
	Write-Host "[*] x64\Debug не существует (создается при сборке)" -ForegroundColor Yellow
}

# Копируем в Release
if (Test-Path $ReleaseDir) {
	Copy-Item -Path $LuaDll.FullName -Destination "$ReleaseDir\lua55.dll" -Force
	Write-Host "[OK] Скопирован в x64\Release" -ForegroundColor Green
} else {
	Write-Host "[*] x64\Release не существует (создается при сборке)" -ForegroundColor Yellow
}

# Удаляем временный ZIP
Remove-Item $TempZip -Force -ErrorAction SilentlyContinue

Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║     Установка завершена успешно! ✓    ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "Теперь вы можете:" -ForegroundColor Cyan
Write-Host "  1. Пересобрать проект (Build -> Rebuild)" -ForegroundColor White
Write-Host "  2. Запустить VisualEngine.exe`n" -ForegroundColor White

Write-Host "Если вы переустановили x64 папку, запустите" -ForegroundColor Yellow
Write-Host "этот скрипт ещё раз чтобы скопировать DLL!`n" -ForegroundColor Yellow
