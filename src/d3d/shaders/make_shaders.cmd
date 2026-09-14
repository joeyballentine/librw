@echo off
rem Every Direct3D shader blob, for both backends, from the sources beside this
rem file.
rem
rem Each shader is compiled twice under one name: at shader model 2 or 3 into
rem this directory for the D3D9 backend, and at shader model 4 with SM4 defined
rem into ..\shaders11 for the D3D11 backend. rwshader.h is the whole of what
rem differs between the two. /Vn names the array after the shader, so a blob one
rem tree has and the other lacks is a compile error in a build carrying both.
rem
rem Run it when a .hlsl or a header here changes, and commit both trees' .h
rem with the change. The blobs are checked in so the build needs no compiler.
call "%~dp0findfxc.cmd" || exit /b 1
pushd "%~dp0" || exit /b 1

rem --- the default object pipeline, and the same with a texture coordinate
rem transform for the pipeline rw::GetUVTransformPipeline() returns. Separate
rem blobs rather than a branch so that a model with no animated UVs pays nothing.
call :vs default_amb_VS default_VS.hlsl || goto fail
call :vs default_amb_dir_VS default_VS.hlsl /DDIRECTIONALS || goto fail
call :vs default_all_VS default_VS.hlsl /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS || goto fail
call :vs uvxform_amb_VS default_VS.hlsl /DUVXFORM || goto fail
call :vs uvxform_amb_dir_VS default_VS.hlsl /DUVXFORM /DDIRECTIONALS || goto fail
call :vs uvxform_all_VS default_VS.hlsl /DUVXFORM /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS || goto fail

rem The per-pixel lighting path. One variant each and not three, because these
rem do no lighting at all -- they pass a normal across and the pixel shader does
rem the rest, so there is nothing for DIRECTIONALS to switch on.
call :vs default_pp_VS default_VS.hlsl /DPERPIXEL || goto fail
call :vs uvxform_pp_VS default_VS.hlsl /DPERPIXEL /DUVXFORM || goto fail

call :ps default_PS default_PS.hlsl || goto fail
call :ps default_tex_PS default_PS.hlsl /DTEX || goto fail
call :ps default_pp_PS default_PS.hlsl /DPERPIXEL || goto fail
call :ps default_tex_pp_PS default_PS.hlsl /DPERPIXEL /DTEX || goto fail

rem The cel look. TOON replaces the lighting rather than adding to it, so it
rem needs no light loop and is cheaper than the path it stands in for -- but
rem ps_2_0 has no branches, so it has to be its own program rather than an if.
rem
rem **ps_3_0 and not ps_2_0, alone among these.** Antialiasing a band edge
rem needs to know how fast the light term is changing across the screen, which
rem is ddx and ddy, and ps_2_0 has no derivative instructions at all. That makes
rem the toon path want Shader Model 3 hardware -- a 2004 card -- where the rest
rem of the renderer does not. It is a setting, and one that is off by default,
rem so the cost falls only on whoever turns it on.
call :ps3 default_toon_PS default_PS.hlsl /DTOON || goto fail
call :ps3 default_tex_toon_PS default_PS.hlsl /DTOON /DTEX || goto fail

rem The inverted hull, and the flat ink that goes round it.
call :vs outline_VS default_VS.hlsl /DOUTLINE || goto fail
call :ps outline_PS outline_PS.hlsl || goto fail

rem --- skinning
call :vs skin_amb_VS skin_VS.hlsl || goto fail
call :vs skin_amb_dir_VS skin_VS.hlsl /DDIRECTIONALS || goto fail
call :vs skin_all_VS skin_VS.hlsl /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS || goto fail
rem Per-pixel lighting. This one has room to spare where skin_all_VS does not:
rem it drops every light loop and only has to carry the skinned normal out.
call :vs skin_pp_VS skin_VS.hlsl /DPERPIXEL || goto fail
rem The skinned hull. Same pixel shader as the unskinned one -- a flat ink does
rem not care how the vertex got where it is.
call :vs skin_outline_VS skin_VS.hlsl /DOUTLINE || goto fail

rem --- the material effect, and it combined with skinning
call :vs matfx_env_amb_VS matfx_env_VS.hlsl || goto fail
call :vs matfx_env_amb_dir_VS matfx_env_VS.hlsl /DDIRECTIONALS || goto fail
call :vs matfx_env_all_VS matfx_env_VS.hlsl /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS || goto fail
call :ps matfx_env_PS matfx_env_PS.hlsl || goto fail
call :ps matfx_env_tex_PS matfx_env_PS.hlsl /DTEX || goto fail
call :vs skin_matfx_env_amb_VS skin_matfx_env_VS.hlsl || goto fail
call :vs skin_matfx_env_amb_dir_VS skin_matfx_env_VS.hlsl /DDIRECTIONALS || goto fail
call :vs skin_matfx_env_all_VS skin_matfx_env_VS.hlsl /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS || goto fail

rem --- immediate mode
call :vs im2d_VS im2d_VS.hlsl || goto fail
call :ps im2d_PS im2d_PS.hlsl || goto fail
call :ps im2d_tex_PS im2d_PS.hlsl /DTEX || goto fail

rem --- the virtual screen, on its way to the back buffer. D3D11 only.
call :sm4 vs blit_VS blit_VS.hlsl || goto fail
call :sm4 ps blit_PS blit_PS.hlsl || goto fail

popd
echo All shaders compiled.
exit /b 0

:fail
popd
exit /b 1

rem call :vs NAME SOURCE [DEFINES...] -- vs_2_0 here, vs_4_0 in ..\shaders11
:vs
"%FXC%" /nologo /T vs_2_0 /Vn %1 /Fh %1.h %3 %4 %5 %6 %2 || exit /b 1
"%FXC%" /nologo /T vs_4_0 /DSM4 /Vn %1 /Fh ..\shaders11\%1.h %3 %4 %5 %6 %2 || exit /b 1
exit /b 0

rem call :ps NAME SOURCE [DEFINES...] -- ps_2_0 here, ps_4_0 in ..\shaders11
:ps
"%FXC%" /nologo /T ps_2_0 /Vn %1 /Fh %1.h %3 %4 %5 %6 %2 || exit /b 1
"%FXC%" /nologo /T ps_4_0 /DSM4 /Vn %1 /Fh ..\shaders11\%1.h %3 %4 %5 %6 %2 || exit /b 1
exit /b 0

rem call :ps3 NAME SOURCE [DEFINES...] -- ps_3_0 here, ps_4_0 in ..\shaders11
:ps3
"%FXC%" /nologo /T ps_3_0 /Vn %1 /Fh %1.h %3 %4 %5 %6 %2 || exit /b 1
"%FXC%" /nologo /T ps_4_0 /DSM4 /Vn %1 /Fh ..\shaders11\%1.h %3 %4 %5 %6 %2 || exit /b 1
exit /b 0

rem call :sm4 vs^|ps NAME SOURCE -- ..\shaders11 only
:sm4
"%FXC%" /nologo /T %1_4_0 /DSM4 /Vn %2 /Fh ..\shaders11\%2.h %3 || exit /b 1
exit /b 0
