@echo off
rem Every shader model 4 blob the D3D11 backend loads, in one script.
rem
rem The permutations are the same ones the SM2 tree has, and they exist for the
rem same reason the D3D9 backend keeps them: a model lit by ambient alone should
rem not pay for a point light loop. vs_4_0 could branch on the counts instead,
rem but the driver picks a shader per atomic either way, so nothing is gained by
rem making it decide twice.
call "%~dp0findfxc.cmd" || exit /b 1

rem --- the virtual screen, on its way to the back buffer
"%FXC%" /nologo /T vs_4_0 /Fh blit_VS.h blit_VS.hlsl || exit /b 1
"%FXC%" /nologo /T ps_4_0 /Fh blit_PS.h blit_PS.hlsl || exit /b 1

rem --- immediate mode
"%FXC%" /nologo /T vs_4_0 /Fh im2d_VS.h im2d_VS.hlsl || exit /b 1
"%FXC%" /nologo /T ps_4_0 /Fh im2d_PS.h im2d_PS.hlsl || exit /b 1
"%FXC%" /nologo /T ps_4_0 /DTEX /Fh im2d_tex_PS.h im2d_PS.hlsl || exit /b 1

rem --- the default object pipeline
"%FXC%" /nologo /T vs_4_0 /Fh default_amb_VS.h default_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /Fh default_amb_dir_VS.h default_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh default_all_VS.h default_VS.hlsl || exit /b 1

rem The same shader with a texture coordinate transform, for the pipeline
rem rw::GetUVTransformPipeline() returns.
"%FXC%" /nologo /T vs_4_0 /DUVXFORM /Fh uvxform_amb_VS.h default_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DUVXFORM /DDIRECTIONALS /Fh uvxform_amb_dir_VS.h default_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DUVXFORM /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh uvxform_all_VS.h default_VS.hlsl || exit /b 1

rem The per-pixel lighting path. One variant each and not three, because these
rem do no lighting at all -- they pass a normal across and the pixel shader does
rem the rest, so there is nothing for DIRECTIONALS to switch on.
"%FXC%" /nologo /T vs_4_0 /DPERPIXEL /Fh default_pp_VS.h default_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DPERPIXEL /DUVXFORM /Fh uvxform_pp_VS.h default_VS.hlsl || exit /b 1

"%FXC%" /nologo /T ps_4_0 /Fh default_PS.h default_PS.hlsl || exit /b 1
"%FXC%" /nologo /T ps_4_0 /DTEX /Fh default_tex_PS.h default_PS.hlsl || exit /b 1
"%FXC%" /nologo /T ps_4_0 /DPERPIXEL /Fh default_pp_PS.h default_PS.hlsl || exit /b 1
"%FXC%" /nologo /T ps_4_0 /DPERPIXEL /DTEX /Fh default_tex_pp_PS.h default_PS.hlsl || exit /b 1

rem --- skinning
"%FXC%" /nologo /T vs_4_0 /Fh skin_amb_VS.h skin_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /Fh skin_amb_dir_VS.h skin_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh skin_all_VS.h skin_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DPERPIXEL /Fh skin_pp_VS.h skin_VS.hlsl || exit /b 1

rem --- the material effect, and it combined with skinning
"%FXC%" /nologo /T vs_4_0 /Fh matfx_env_amb_VS.h matfx_env_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /Fh matfx_env_amb_dir_VS.h matfx_env_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh matfx_env_all_VS.h matfx_env_VS.hlsl || exit /b 1

"%FXC%" /nologo /T ps_4_0 /Fh matfx_env_PS.h matfx_env_PS.hlsl || exit /b 1
"%FXC%" /nologo /T ps_4_0 /DTEX /Fh matfx_env_tex_PS.h matfx_env_PS.hlsl || exit /b 1

"%FXC%" /nologo /T vs_4_0 /Fh skin_matfx_env_amb_VS.h skin_matfx_env_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /Fh skin_matfx_env_amb_dir_VS.h skin_matfx_env_VS.hlsl || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh skin_matfx_env_all_VS.h skin_matfx_env_VS.hlsl || exit /b 1

echo All shaders compiled.
