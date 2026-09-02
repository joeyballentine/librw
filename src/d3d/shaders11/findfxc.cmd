@echo off
rem Sets FXC to a shader compiler that can emit vs_4_0 and ps_4_0.
rem
rem The Windows SDK's fxc is the one to use here: the June 2010 DirectX SDK's
rem predates nothing these shaders need, but the SDK has not shipped for over a
rem decade and every machine that can build the port has a Windows SDK.
rem
rem Called by the make_*.cmd beside this file. Sets FXC in the caller's
rem environment, so the caller must not have run setlocal first.

set "FXC="
for /f "delims=" %%D in ('dir /b /ad /o-n "%ProgramFiles(x86)%\Windows Kits\10\bin\10.*" 2^>nul') do (
	if not defined FXC if exist "%ProgramFiles(x86)%\Windows Kits\10\bin\%%D\x64\fxc.exe" (
		set "FXC=%ProgramFiles(x86)%\Windows Kits\10\bin\%%D\x64\fxc.exe"
	)
)
if defined FXC exit /b 0

set "FXC=%DXSDK_DIR%\utilities\bin\x86\fxc.exe"
if exist "%FXC%" exit /b 0

echo findfxc: no fxc.exe found. Install the Windows SDK, or set DXSDK_DIR.
exit /b 1
