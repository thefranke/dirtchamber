@echo off

if "%PROCESSOR_ARCHITECTURE%"=="x86"   set FXC="%ProgramFiles%\Windows Kits\8.1\bin\x86\fxc.exe"
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" set FXC="%ProgramFiles(x86)%\Windows Kits\8.1\bin\x64\fxc.exe"

set OUTDIR="cso"

md cso

%FXC% /O3 /T ps_5_0 /E ps_lpv                     /Fo %OUTDIR%/ps_lpv.cso                     deferred_lpv.hlsl
%FXC% /O3 /T ps_5_0 /E ps_vct                     /Fo %OUTDIR%/ps_vct.cso                     deferred_vct.hlsl

%FXC% /O3 /T vs_5_0 /E vs_mesh                    /Fo %OUTDIR%/vs_mesh.cso                    d3d_mesh.hlsl
%FXC% /O3 /T ps_5_0 /E ps_mesh                    /Fo %OUTDIR%/ps_mesh.cso                    d3d_mesh.hlsl

%FXC% /O3 /T vs_5_0 /E vs_skydome                 /Fo %OUTDIR%/vs_skydome.cso                 d3d_mesh_skydome.hlsl
%FXC% /O3 /T ps_5_0 /E ps_skydome                 /Fo %OUTDIR%/ps_skydome.cso                 d3d_mesh_skydome.hlsl

%FXC% /O3 /T vs_5_0 /E vs_fs_triangle             /Fo %OUTDIR%/vs_fs_triangle.cso             fs_triangle.hlsl

%FXC% /O3 /T vs_5_0 /E vs_lpv_inject              /Fo %OUTDIR%/vs_lpv_inject.cso              lpv_inject.hlsl
%FXC% /O3 /T vs_5_0 /E vs_delta_lpv_direct_inject /Fo %OUTDIR%/vs_delta_lpv_direct_inject.cso lpv_inject.hlsl
%FXC% /O3 /T vs_5_0 /E vs_delta_lpv_inject        /Fo %OUTDIR%/vs_delta_lpv_inject.cso        lpv_inject.hlsl
%FXC% /O3 /T gs_5_0 /E gs_lpv_inject              /Fo %OUTDIR%/gs_lpv_inject.cso              lpv_inject.hlsl
%FXC% /O3 /T ps_5_0 /E ps_lpv_inject              /Fo %OUTDIR%/ps_lpv_inject.cso              lpv_inject.hlsl
%FXC% /O3 /T ps_5_0 /E ps_delta_lpv_inject        /Fo %OUTDIR%/ps_delta_lpv_inject.cso        lpv_inject.hlsl

%FXC% /O3 /T ps_5_0 /E ps_lpv_normalize           /Fo %OUTDIR%/ps_lpv_normalize.cso           lpv_normalize.hlsl

%FXC% /O3 /T vs_5_0 /E vs_lpv_propagate           /Fo %OUTDIR%/vs_lpv_propagate.cso           lpv_propagate.hlsl
%FXC% /O3 /T gs_5_0 /E gs_lpv_propagate           /Fo %OUTDIR%/gs_lpv_propagate.cso           lpv_propagate.hlsl
%FXC% /O3 /T ps_5_0 /E ps_lpv_propagate           /Fo %OUTDIR%/ps_lpv_propagate.cso           lpv_propagate.hlsl
%FXC% /O3 /T ps_5_0 /E ps_delta_lpv_propagate     /Fo %OUTDIR%/ps_delta_lpv_propagate.cso     lpv_propagate.hlsl

%FXC% /O3 /T vs_5_0 /E vs_svo_inject              /Fo %OUTDIR%/vs_svo_inject.cso              svo_inject.hlsl
%FXC% /O3 /T ps_5_0 /E ps_svo_inject              /Fo %OUTDIR%/ps_svo_inject.cso              svo_inject.hlsl

%FXC% /O3 /T vs_5_0 /E vs_svo_voxelize            /Fo %OUTDIR%/vs_svo_voxelize.cso            svo_voxelize.hlsl
%FXC% /O3 /T gs_5_0 /E gs_svo_voxelize            /Fo %OUTDIR%/gs_svo_voxelize.cso            svo_voxelize.hlsl
%FXC% /O3 /T ps_5_0 /E ps_svo_voxelize            /Fo %OUTDIR%/ps_svo_voxelize.cso            svo_voxelize.hlsl

%FXC% /O3 /T vs_5_0 /E vs_rendervol               /Fo %OUTDIR%/vs_rendervol.cso               lpv_rendervol.hlsl
%FXC% /O3 /T ps_5_0 /E ps_rendervol               /Fo %OUTDIR%/ps_rendervol.cso               lpv_rendervol.hlsl

%FXC% /O3 /T ps_5_0 /E ps_overlay                 /Fo %OUTDIR%/ps_overlay.cso                 overlay.hlsl
%FXC% /O3 /T ps_5_0 /E ps_overlay_depth           /Fo %OUTDIR%/ps_overlay_depth.cso           overlay.hlsl

%FXC% /O3 /T ps_5_0 /E ps_adapt_exposure          /Fo %OUTDIR%/ps_adapt_exposure.cso          pp_bloom.hlsl
%FXC% /O3 /T ps_5_0 /E ps_bloom                   /Fo %OUTDIR%/ps_bloom.cso                   pp_bloom.hlsl
%FXC% /O3 /T ps_5_0 /E ps_bloom_treshold          /Fo %OUTDIR%/ps_bloom_treshold.cso          pp_bloom.hlsl

%FXC% /O3 /T ps_5_0 /E ps_gauss_bloom_h           /Fo %OUTDIR%/ps_gauss_bloom_h.cso           pp_blur.hlsl
%FXC% /O3 /T ps_5_0 /E ps_gauss_bloom_v           /Fo %OUTDIR%/ps_gauss_bloom_v.cso           pp_blur.hlsl
%FXC% /O3 /T ps_5_0 /E ps_gauss_godrays_h         /Fo %OUTDIR%/ps_gauss_godrays_h.cso         pp_blur.hlsl
%FXC% /O3 /T ps_5_0 /E ps_gauss_godrays_v         /Fo %OUTDIR%/ps_gauss_godrays_v.cso         pp_blur.hlsl
%FXC% /O3 /T ps_5_0 /E ps_gauss_dof_h             /Fo %OUTDIR%/ps_gauss_dof_h.cso             pp_blur.hlsl
%FXC% /O3 /T ps_5_0 /E ps_gauss_dof_v             /Fo %OUTDIR%/ps_gauss_dof_v.cso             pp_blur.hlsl
%FXC% /O3 /T ps_5_0 /E ps_copy                    /Fo %OUTDIR%/ps_copy.cso                    pp_blur.hlsl

%FXC% /O3 /T ps_5_0 /E ps_crt                     /Fo %OUTDIR%/ps_crt.cso                     pp_crt.hlsl

%FXC% /O3 /T ps_5_0 /E ps_depth_of_field          /Fo %OUTDIR%/ps_depth_of_field.cso          pp_dof.hlsl

%FXC% /O3 /T ps_5_0 /E ps_film_grain              /Fo %OUTDIR%/ps_film_grain.cso              pp_film_grain.hlsl

%FXC% /O3 /T ps_5_0 /E ps_fxaa                    /Fo %OUTDIR%/ps_fxaa.cso                    pp_fxaa.hlsl

%FXC% /O3 /T ps_5_0 /E ps_godrays                 /Fo %OUTDIR%/ps_godrays.cso                 pp_godrays.hlsl
%FXC% /O3 /T ps_5_0 /E ps_godrays_halfres         /Fo %OUTDIR%/ps_godrays_halfres.cso         pp_godrays.hlsl
%FXC% /O3 /T ps_5_0 /E ps_godrays_merge           /Fo %OUTDIR%/ps_godrays_merge.cso           pp_godrays.hlsl

%FXC% /O3 /T ps_5_0 /E ps_ssao                    /Fo %OUTDIR%/ps_ssao.cso                    pp_ssao.hlsl