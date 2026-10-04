@echo off
rem Builds the VaelEditor target (Win64, Development). Extra arguments are passed on to UnrealBuildTool.
setlocal
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" VaelEditor Win64 Development -Project="%~dp0..\Vael.uproject" -WaitMutex -NoHotReloadFromIDE %*
exit /b %ERRORLEVEL%
