/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "adaptive_video.h"
#include "shared_vars.h"
#include "timers.h"
#include "video_tx_buffers.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"
#include "packets_utils.h"

u8 s_uLastVideoProfileRequestedByController = 0xFF;
u32 s_uTimeLastVideoProfileRequestedByController = 0;
u32 s_uTimeLastTimeAdaptivePeriodicLoop = 0;

u16 s_uCurrentKFValue = 0;
u16 s_uPendingKFValue = 0;

int s_iPendingAdaptiveRadioDataRate = 0;
u32 s_uTimeSetPendingAdaptiveRadioDataRate = 0;

u32 s_uLastAdaptiveAppliedVideoBitrate = 0;
int s_iLastIPQuantizationSet = -1000;

void adaptive_video_init()
{
   log_line("[AdaptiveVideo] Init...");
   s_uCurrentKFValue = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.user_selected_video_link_profile);
   s_uTimeLastTimeAdaptivePeriodicLoop = get_current_timestamp_ms();
   s_uTimeSetPendingAdaptiveRadioDataRate = 0;
   s_iPendingAdaptiveRadioDataRate = 0;
   log_line("[AdaptiveVideo] Current KF ms: %d, pending KF ms: %d", s_uCurrentKFValue, s_uPendingKFValue);
}


void adaptive_video_set_kf_for_current_video_profile(u16 uKeyframe)
{
   s_uPendingKFValue = uKeyframe;
}

void adaptive_video_set_last_profile_requested_by_controller(int iVideoProfile)
{
   s_uLastVideoProfileRequestedByController = iVideoProfile;
   log_line("[AdaptiveVideo] Set new video profile requested by controller: %s", str_get_video_profile_name(iVideoProfile));
   if ( NULL != g_pVideoTxBuffers )
   {
      g_pVideoTxBuffers->updateVideoHeader(g_pCurrentModel);
      s_uPendingKFValue = g_pCurrentModel->getInitialKeyframeIntervalMs(iVideoProfile);
      log_line("[AdaptiveVideo] Set new KF ms value requested by controller: %d (current KF ms: %d)", s_uPendingKFValue, s_uCurrentKFValue);
   }

   // Update capture video bitrate
   u32 uBitrateBPS = g_pCurrentModel->video_link_profiles[iVideoProfile].bitrate_fixed_bps;

   if ( uBitrateBPS != s_uLastAdaptiveAppliedVideoBitrate )
   {
      s_uLastAdaptiveAppliedVideoBitrate = uBitrateBPS;
      if ( g_pCurrentModel->hasCamera() )
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
         video_source_csi_send_control_message(RASPIVID_COMMAND_ID_VIDEO_BITRATE, uBitrateBPS/100000, 0);
      
      if ( g_pCurrentModel->hasCamera() )
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
         video_source_majestic_set_videobitrate_value(uBitrateBPS); 
   }

   // Update IP quantization delta
   if ( g_pCurrentModel->hasCamera() )
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   if ( s_iLastIPQuantizationSet != g_pCurrentModel->video_link_profiles[iVideoProfile].iIPQuantizationDelta )
   {
      s_iLastIPQuantizationSet = g_pCurrentModel->video_link_profiles[iVideoProfile].iIPQuantizationDelta;    
      video_source_majestic_set_qpdelta_value(s_iLastIPQuantizationSet);
   }

   // Update adaptive video rate for tx radio:

   if ( s_uLastVideoProfileRequestedByController == g_pCurrentModel->video_params.user_selected_video_link_profile )
   {
      packet_utils_set_adaptive_video_datarate(0);
      s_iPendingAdaptiveRadioDataRate = 0;
      s_uTimeSetPendingAdaptiveRadioDataRate = 0;
   }
   else
   {
      int nRateTxVideo = DEFAULT_RADIO_DATARATE_VIDEO;
      if ( s_uLastVideoProfileRequestedByController == VIDEO_PROFILE_MQ )
         nRateTxVideo = utils_get_video_profile_mq_radio_datarate(g_pCurrentModel);

      if ( s_uLastVideoProfileRequestedByController == VIDEO_PROFILE_LQ )
         nRateTxVideo = utils_get_video_profile_lq_radio_datarate(g_pCurrentModel);

      // If datarate is increasing, set it right away
      if ( (0 != packet_utils_get_last_set_adaptive_video_datarate()) &&
           (getRealDataRateFromRadioDataRate(nRateTxVideo, 0) >= getRealDataRateFromRadioDataRate(packet_utils_get_last_set_adaptive_video_datarate(), 0)) )
      {
         packet_utils_set_adaptive_video_datarate(nRateTxVideo);
         s_iPendingAdaptiveRadioDataRate = 0;
         s_uTimeSetPendingAdaptiveRadioDataRate = 0;
      }
      // If datarate is decreasing, set it after a short period
      else
      {
         s_iPendingAdaptiveRadioDataRate = nRateTxVideo;
         s_uTimeSetPendingAdaptiveRadioDataRate = g_TimeNow;
      }
   } 

   s_uTimeLastVideoProfileRequestedByController = g_TimeNow;
}

int adaptive_video_get_current_active_video_profile()
{
   int iVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   if ( 0xFF != s_uLastVideoProfileRequestedByController )
      iVideoProfile = s_uLastVideoProfileRequestedByController;
   return iVideoProfile;
}

u16 adaptive_video_get_current_kf()
{
   return s_uCurrentKFValue;
}

bool _adaptive_video_send_kf_to_capture_program(u16 uNewKeyframeMs)
{
   // Send the actual keyframe change to video source/capture

   int iVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   if ( 0xFF != s_uLastVideoProfileRequestedByController )
      iVideoProfile = s_uLastVideoProfileRequestedByController;
   
   int iCurrentFPS = 30;
   if ( NULL != g_pCurrentModel )
     iCurrentFPS = g_pCurrentModel->video_link_profiles[iVideoProfile].fps;

   int iKeyFrameCountValue = (iCurrentFPS * (int)uNewKeyframeMs) / 1000; 

   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      video_source_csi_send_control_message(RASPIVID_COMMAND_ID_KEYFRAME, (u16)iKeyFrameCountValue, 0);

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      float fGOP = 1.0;
      fGOP = ((float)uNewKeyframeMs)/1000.0;
      video_source_majestic_set_keyframe_value(fGOP);                
   }
   return true;
}

void adaptive_video_on_capture_restarted()
{
   s_uLastAdaptiveAppliedVideoBitrate = 0;
   s_iLastIPQuantizationSet = -1000;
}

void adaptive_video_on_new_camera_read(bool bEndOfFrame, bool bIsInsideIFrame)
{
   if ( 0 != s_uPendingKFValue )
   {
      if ( s_uPendingKFValue == s_uCurrentKFValue )
         s_uPendingKFValue = 0;
   }
   if ( s_uPendingKFValue != 0 )
   if ( s_uPendingKFValue != s_uCurrentKFValue )
   if ( NULL != g_pVideoTxBuffers )
   if ( bEndOfFrame )
   if ( ! bIsInsideIFrame )
   {
      if ( _adaptive_video_send_kf_to_capture_program(s_uPendingKFValue) )
      {
         log_line("[AdaptiveVideo] Changed KF ms value from %d to %d", s_uCurrentKFValue, s_uPendingKFValue);
         s_uCurrentKFValue = s_uPendingKFValue;
         s_uPendingKFValue = 0;
         g_pVideoTxBuffers->updateCurrentKFValue();
      }
   }
}

void adaptive_video_periodic_loop()
{
   if ( g_TimeNow < s_uTimeLastTimeAdaptivePeriodicLoop + 10 )
      return;
   if ( g_bNegociatingRadioLinks )
      return;
   
   s_uTimeLastTimeAdaptivePeriodicLoop = g_TimeNow;

   if ( (0 != s_iPendingAdaptiveRadioDataRate) && (0 != s_uTimeSetPendingAdaptiveRadioDataRate) )
   if ( g_TimeNow >= s_uTimeSetPendingAdaptiveRadioDataRate + DEFAULT_LOWER_VIDEO_RADIO_DATARATE_AFTER_MS )
   {
      packet_utils_set_adaptive_video_datarate(s_iPendingAdaptiveRadioDataRate);
      s_iPendingAdaptiveRadioDataRate = 0;
      s_uTimeSetPendingAdaptiveRadioDataRate = 0;
   }
}