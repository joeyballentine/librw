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
"%FXC%" /nologo /T ps_3_0 /DTOON /Fh default_toon_PS.h default_PS.hlsl
"%FXC%" /nologo /T ps_3_0 /DTOON /DTEX /Fh default_tex_toon_PS.h default_PS.hlsl

rem The inverted hull, and the flat ink that goes round it.
"%FXC%" /nologo /T vs_2_0 /DOUTLINE /Fh outline_VS.h default_VS.hlsl
"%FXC%" /nologo /T ps_2_0 /Fh outline_PS.h outline_PS.hlsl

"%FXC%" /nologo /T vs_2_0 /Fh im2d_VS.h im2d_VS.hlsl
"%FXC%" /nologo /T ps_2_0 /Fh im2d_PS.h im2d_PS.hlsl
"%FXC%" /nologo /T ps_2_0 /DTEX /Fh im2d_tex_PS.h im2d_PS.hlsl
