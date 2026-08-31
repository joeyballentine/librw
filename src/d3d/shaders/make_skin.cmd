@echo off
call "%~dp0findfxc.cmd" || exit /b 1
"%FXC%" /nologo /T vs_2_0 /Fh skin_amb_VS.h skin_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DDIRECTIONALS /Fh skin_amb_dir_VS.h skin_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh skin_all_VS.h skin_VS.hlsl

rem Per-pixel lighting. This one has room to spare where skin_all_VS does not:
rem it drops every light loop and only has to carry the skinned normal out.
"%FXC%" /nologo /T vs_2_0 /DPERPIXEL /Fh skin_pp_VS.h skin_VS.hlsl
