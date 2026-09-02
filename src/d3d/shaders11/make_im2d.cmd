@echo off
call "%~dp0findfxc.cmd" || exit /b 1
"%FXC%" /nologo /T vs_4_0 /Fh im2d_VS.h im2d_VS.hlsl
"%FXC%" /nologo /T ps_4_0 /Fh im2d_PS.h im2d_PS.hlsl
"%FXC%" /nologo /T ps_4_0 /DTEX /Fh im2d_tex_PS.h im2d_PS.hlsl
