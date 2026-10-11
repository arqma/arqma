@echo off
REM Companion stack for Windows (same repo, separate PIDs). Unix: utils/arqma-stack.sh
setlocal
set "BIN_DIR=%ARQMA_BIN_DIR%"
if "%BIN_DIR%"=="" set "BIN_DIR=%~dp0..\build\upgrade-test\bin"
set "STORAGE_LISTEN=%~1"
if "%STORAGE_LISTEN%"=="" set "STORAGE_LISTEN=127.0.0.1:22021"
set "ROUTER_LISTEN=%~2"
if "%ROUTER_LISTEN%"=="" set "ROUTER_LISTEN=127.0.0.1:1090"
set "STACK_DIR=%ARQMA_STACK_DIR%"
if "%STACK_DIR%"=="" set "STACK_DIR=%TEMP%\arqma-stack"

if not exist "%BIN_DIR%\arqma-storage.exe" (
  echo Build companions first, e.g.:
  echo   cmake --build build\upgrade-test --parallel --target arqma_storage arqma_router arqma_msg daemon
  exit /b 1
)

mkdir "%STACK_DIR%\storage" 2>nul
mkdir "%STACK_DIR%\router" 2>nul
if "%ARQMA_STACK_TOKEN%"=="" (
  for /f %%I in ('powershell -NoProfile -Command "[guid]::NewGuid().ToString('N').Substring(0,32)"') do set "ARQMA_STACK_TOKEN=%%I"
)
(
  echo ARQMA_STORAGE_URL=http://%STORAGE_LISTEN%
  echo ARQMA_ROUTER_URL=http://%ROUTER_LISTEN%
  echo ARQMA_STACK_TOKEN=%ARQMA_STACK_TOKEN%
) > "%STACK_DIR%\env"

start "arqma-storage" "%BIN_DIR%\arqma-storage.exe" --listen %STORAGE_LISTEN% --data-dir "%STACK_DIR%\storage" --token %ARQMA_STACK_TOKEN%
start "arqma-router" "%BIN_DIR%\arqma-router.exe" --listen %ROUTER_LISTEN% --data-dir "%STACK_DIR%\router" --storage-url http://%STORAGE_LISTEN% --token %ARQMA_STACK_TOKEN%

echo arqma-storage http://%STORAGE_LISTEN% data=%STACK_DIR%\storage
echo arqma-router  http://%ROUTER_LISTEN% data=%STACK_DIR%\router
echo env           %STACK_DIR%\env
echo.
echo Messenger:
echo   "%BIN_DIR%\arqma-msg.exe" gen
echo   "%BIN_DIR%\arqma-msg.exe" send ^<hex^> hello
echo   "%BIN_DIR%\arqma-msg.exe" inbox
echo   "%BIN_DIR%\arqma-msg.exe" open
echo.
echo Daemon (separate process):
echo   "%BIN_DIR%\arqmad.exe" --storage-client-url=http://%STORAGE_LISTEN% --arq-router
echo Close the companion console windows to stop the stack.
endlocal
