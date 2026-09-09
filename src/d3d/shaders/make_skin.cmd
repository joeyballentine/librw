@echo off
call "%~dp0findfxc.cmd" || exit /b 1
"%FXC%" /nologo /T vs_2_0 /Fh skin_amb_VS.h skin_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DDIRECTIONALS /Fh skin_amb_dir_VS.h skin_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh skin_all_VS.h skin_VS.hlsl

rem Per-pixel lighting. This one has room to spare where skin_all_VS does not:
rem it drops every light loop and only has to carry the skinned normal out.
"%FXC%" /nologo /T vs_2_0 /DPERPIXEL /Fh skin_pp_VS.h skin_VS.hlsl

rem The skinned hull. Same pixel shader as the unskinned one -- a flat ink does
rem not care how the vertex got where it is.
"%FXC%" /nologo /T vs_2_0 /DOUTLINE /Fh skin_outline_VS.h skin_VS.hlsl
