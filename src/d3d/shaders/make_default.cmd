@echo off
call "%~dp0findfxc.cmd" || exit /b 1
"%FXC%" /nologo /T vs_2_0 /Fh default_amb_VS.h default_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DDIRECTIONALS /Fh default_amb_dir_VS.h default_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh default_all_VS.h default_VS.hlsl

rem The same shader with a texture coordinate transform, for the pipeline
rem rw::GetUVTransformPipeline() returns. Separate blobs rather than a branch in
rem the default one so that a model with no animated UVs pays nothing.
"%FXC%" /nologo /T vs_2_0 /DUVXFORM /Fh uvxform_amb_VS.h default_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DUVXFORM /DDIRECTIONALS /Fh uvxform_amb_dir_VS.h default_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DUVXFORM /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh uvxform_all_VS.h default_VS.hlsl

rem The per-pixel lighting path. One variant each and not three, because these
rem do no lighting at all -- they pass a normal across and the pixel shader does
rem the rest, so there is nothing for DIRECTIONALS to switch on.
"%FXC%" /nologo /T vs_2_0 /DPERPIXEL /Fh default_pp_VS.h default_VS.hlsl
"%FXC%" /nologo /T vs_2_0 /DPERPIXEL /DUVXFORM /Fh uvxform_pp_VS.h default_VS.hlsl

"%FXC%" /nologo /T ps_2_0 /Fh default_PS.h default_PS.hlsl
"%FXC%" /nologo /T ps_2_0 /DTEX /Fh default_tex_PS.h default_PS.hlsl
"%FXC%" /nologo /T ps_2_0 /DPERPIXEL /Fh default_pp_PS.h default_PS.hlsl
"%FXC%" /nologo /T ps_2_0 /DPERPIXEL /DTEX /Fh default_tex_pp_PS.h default_PS.hlsl

"%FXC%" /nologo /T vs_2_0 /Fh im2d_VS.h im2d_VS.hlsl
"%FXC%" /nologo /T ps_2_0 /Fh im2d_PS.h im2d_PS.hlsl
"%FXC%" /nologo /T ps_2_0 /DTEX /Fh im2d_tex_PS.h im2d_PS.hlsl
