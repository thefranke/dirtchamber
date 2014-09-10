/* 
 * LPV render sample by Tobias Alexander Franke (tob@cyberhead.de) 2013 
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#ifndef COMMON_SHADER_H
#define COMMON_SHADER_H

#define SLOT_SVO_PARAMETERS_VS_GS_PS    7
#define SVO_SIZE 256
#define SVO_MIPS 8

#define SLOT_TEX_SVO_RSM_MU_START       12
#define SLOT_TEX_SVO_RSM_RHO_START      6

#define SLOT_TEX_SVO_V_START            7
#define SLOT_TEX_SVO_V_DELTA            8
#define SLOT_TEX_SVO_V_MU               9
#define SLOT_TEX_SVO_V_NORMAL           7

#define SLOT_TEX_LPV_INJECT_RSM_START   6
#define SLOT_TEX_LPV_PROPAGATE_START    7
#define SLOT_TEX_LPV_DEFERRED_START     7

#define SLOT_TEX_DEFERRED_START         2
#define SLOT_TEX_DEFERRED_KINECT_START  0

#define SLOT_TEX_DEF_RSM_LINEARDEPTH    6
#define SLOT_TEX_POSTPROCESSING_START   10
#define SLOT_TEX_DIFFUSE                0
#define SLOT_TEX_NORMAL                 1
#define SLOT_TEX_SPECULAR               2
#define SLOT_TEX_ALPHA                  3
#define SLOT_TEX_OVERLAY                10
#define SLOT_TEX_NOISE                  14

#define SLOT_ONETIME_VS                 2
#define SLOT_CAMERA_VS                  0

#define SLOT_ONETIME_PS                 6
#define SLOT_CAMERA_PS                  1

#define SLOT_ONETIME_GS                 2
#define SLOT_CAMERA_GS                  0

#define SLOT_GI_PARAMETERS_PS           3
#define SLOT_POSTPROCESSING_PS          4
#define SLOT_PER_FRAME_PS               5
#define SLOT_LIGHT_PS                   2
#define SLOT_LPV_PARAMETERS_VS_PS       7

#define USE_LPV
#define GAMMA                           2.2
#define SHADOW_BIAS                     0.015
#define LPV_SIZE                        32
#define M_PI                            3.14159265358
#define PCF_SAMPLES                     64
#define SSAO_SAMPLES                    8
#define MAX_VPLS                        512

#endif