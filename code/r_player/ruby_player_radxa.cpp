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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/random.h>
#include <inttypes.h>


#include "../base/base.h"
#include "../base/config.h"
#include "../base/config_obj_names.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/hdmi.h"
#include "../renderer/drm_core.h"
#include <ctype.h>
#include <pthread.h>
#include <sys/ioctl.h>

#ifndef HW_PLATFORM_RADXA_ZERO3
#error "ONLY FOR RADXA PLATFORM!"
#endif

#include "../renderer/drm_core.h"
#include "../renderer/render_engine_cairo.h"
#include "mpp_core.h"


bool g_bQuit = false;
bool g_bDebug = false;
bool g_bTestMode = false;
bool g_bPlayFile = false;
bool g_bPlayStreamPipe = false;
bool g_bPlayStreamUDP = false;
bool g_bInitUILayerToo = false;
bool g_bUseH265Decoder = false;

char g_szPlayFileName[MAX_FILE_PATH_SIZE];
int g_iCustomWidth = 0;
int g_iCustomHeight = 0;
int g_iCustomRefresh = 0;

#define PIPE_BUFFER_SIZE 400000
u8 g_uPipeBuffer[PIPE_BUFFER_SIZE];
int g_iPipeBufferWritePos = 0;
int g_iPipeBufferReadPos = 0;

void _do_test_mode()
{
   ruby_drm_core_init(0, DRM_FORMAT_ARGB8888, g_iCustomWidth, g_iCustomHeight, g_iCustomRefresh);
   RenderEngine* g_pRenderEngine = render_init_engine();

   //double pColorBg[4] = {255,0,0,1.0};
   double pColorFg[4] = {255,255,0,1.0};

   //cairo_surface_t *image = cairo_image_surface_create_from_png ("res/ruby_bg4.png");
   u32 uImg = g_pRenderEngine->loadImage("res/ruby_bg4.png");
   u32 uIcon = g_pRenderEngine->loadIcon("res/icon_v_plane.png");
   int iFont = g_pRenderEngine->loadRawFont("res/font_ariobold_32.dsc");

   u32 uLastTimeFPS = 0;
   u32 uFPS = 0;

   float fTmpX = 0.4;
   float fTmp1 = 0.1;
   while ( ! g_bQuit )
   {
      g_pRenderEngine->startFrame();

      
         g_pRenderEngine->drawImage(0,0,1.0,1.0, uImg);

         
         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawRoundRect(0.1, 0.1, 0.5, 0.5, 0.1);
         g_pRenderEngine->setStroke(255.0, 0, 0, 1.0);
         g_pRenderEngine->setFill(0,0,0,0);
         g_pRenderEngine->drawRoundRect(0.2, 0.2, 0.5, 0.5, 0.1);

         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawLine(0.4,0.1, 0.5, 0.8);
         g_pRenderEngine->drawTriangle(0.2,0.2, 0.22, 0.3, 0.3, 0.35);     
         g_pRenderEngine->fillTriangle(0.82,0.82, 0.92, 0.53, 0.83, 0.95);     
  
         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawText(0.05, 0.8, (u32)iFont, "Text1");

         g_pRenderEngine->drawIcon(0.5, 0.2, 0.2, 0.2, uIcon);
         
         g_pRenderEngine->bltIcon(fTmpX, 0.5, 20, 20, 60, 60, uIcon);
     
      g_pRenderEngine->endFrame();
      
      //hardware_sleep_ms(900);
      //hardware_sleep_ms(900);
      uFPS++;
      if ( get_current_timestamp_ms() > uLastTimeFPS+1000 )
      {
         uLastTimeFPS = get_current_timestamp_ms();
         uFPS = 0;
      }

      fTmpX += 0.01;
      if ( fTmpX > 0.7 )
         fTmpX = 0.4;

      fTmp1 += 0.002;
      if ( fTmp1 > 0.75 )
         fTmp1 = 0.1;
   }

   render_free_engine();
   ruby_drm_core_uninit();
}


void _do_player_mode()
{
   if ( mpp_init(g_bUseH265Decoder) != 0 )
      return;

   hdmi_enum_modes();
   int iHDMIIndex = hdmi_load_current_mode();
   if ( iHDMIIndex < 0 )
      iHDMIIndex = hdmi_get_best_resolution_index_for(DEFAULT_RADXA_DISPLAY_WIDTH, DEFAULT_RADXA_DISPLAY_HEIGHT, DEFAULT_RADXA_DISPLAY_REFRESH);
   log_line("HDMI mode to use: %d (%d x %d @ %d)", iHDMIIndex, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh() );

   if ( g_bInitUILayerToo )
   {
      ruby_drm_core_init(0, DRM_FORMAT_ARGB8888,  hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh());
      ruby_drm_swap_mainback_buffers();
   }
   ruby_drm_core_init(1, DRM_FORMAT_NV12, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh());

   FILE* fp = fopen(g_szPlayFileName,"rb");
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to open input file [%s]. Exit.", g_szPlayFileName);
      ruby_drm_core_uninit();
      mpp_uninit();
      return;
   }

   log_line("Opened input video file (%s)", g_szPlayFileName);
   mpp_start_decoding_thread();


   u32 uTimeLastCheck = get_current_timestamp_ms();
   unsigned char uBuffer[4096];
   int nRead = 1;
   int iCount =0;
   int iTotalRead = 0;
   while ( (nRead > 0) && (!g_bQuit) )
   {
      iCount++;
      int iToRead = 1024;
      if ( g_bDebug )
      {
         //iToRead = 1000 + (rand()%3000);
         //iToRead = 500;
         //log_line("Reading %d bytes", iToRead);
      }
      nRead = fread(uBuffer, 1, iToRead, fp);
      if ( nRead <= 0 )
         break;
      
      mpp_feed_data_to_decoder(uBuffer, nRead);
      iTotalRead += nRead;
      if ( g_bDebug )
      {
         //u32 uSleep = 10 + (rand()%10);
         //hardware_sleep_ms(uSleep);
         //log_line("Sleep %u ms", uSleep);
         //hardware_sleep_ms(1);
         usleep(2000);
      }
      if ( (iCount % 10) == 0 )
      {
         u32 uTime = get_current_timestamp_ms();
         if ( uTime > uTimeLastCheck + 4000 )
         {
            uTimeLastCheck = uTime;
            log_line("Video player alive, reading %d bits/sec", iTotalRead*8/4);
            iTotalRead = 0;
         }
      }
   }
   fclose(fp);
   log_line("Playback of file finished. End of file (%s).", g_szPlayFileName);
   mpp_mark_end_of_stream();

   ruby_drm_core_uninit();
   mpp_uninit();
}

void* _thread_consume_pipe_buffer(void *param)
{
   log_line("[Thread] Created pipe consume thread.");

   FILE* fpTmp = NULL;
   //fpTmp = fopen("rec2.h264", "wb");

   while ( ! g_bQuit )
   {
      if ( g_iPipeBufferReadPos == g_iPipeBufferWritePos )
      {
         //log_line("[Thread] Wait data (pos %d)...", g_iPipeBufferReadPos);
         hardware_sleep_ms(5);
         continue;
      }
      int iSize = g_iPipeBufferWritePos - g_iPipeBufferReadPos;
      if ( g_iPipeBufferWritePos < g_iPipeBufferReadPos )
         iSize = PIPE_BUFFER_SIZE - g_iPipeBufferReadPos;

      if ( iSize > 1024 )
         iSize = 1024;
      //log_line("[Thread] Consume %d", iSize);

      if ( NULL != fpTmp )
         fwrite( &(g_uPipeBuffer[g_iPipeBufferReadPos]), 1, iSize, fpTmp);

      int iRes = mpp_feed_data_to_decoder(&(g_uPipeBuffer[g_iPipeBufferReadPos]), iSize);
      if ( iRes > 10 )
         log_line("[Thread] Stalled consuming %d bytes at pos %d (write pos: %d), stall for %d ms", iSize, g_iPipeBufferReadPos, g_iPipeBufferWritePos, iRes);
      
      if ( mpp_get_clear_stream_changed_flag() )
         g_iPipeBufferReadPos = g_iPipeBufferWritePos;
      else
      {
         g_iPipeBufferReadPos += iSize;
         if ( g_iPipeBufferReadPos >= PIPE_BUFFER_SIZE )
            g_iPipeBufferReadPos = 0;
      }
   }
   if ( NULL != fpTmp )
      fclose(fpTmp);
   log_line("[Thread] Ended pipe consume thread.");
   return NULL;
}

void _do_stream_mode_pipe()
{
   if ( mpp_init(g_bUseH265Decoder) != 0 )
      return;

   hdmi_enum_modes();
   int iHDMIIndex = hdmi_load_current_mode();
   if ( iHDMIIndex < 0 )
      iHDMIIndex = hdmi_get_best_resolution_index_for(DEFAULT_RADXA_DISPLAY_WIDTH, DEFAULT_RADXA_DISPLAY_HEIGHT, DEFAULT_RADXA_DISPLAY_REFRESH);
   log_line("HDMI mode to use: %d (%d x %d @ %d)", iHDMIIndex, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh() );
   ruby_drm_core_init(1, DRM_FORMAT_NV12, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh());

   hw_increase_current_thread_priority("RubyPlayer", 50);

   pthread_t pDecodeThread;
   pthread_create(&pDecodeThread, NULL, _thread_consume_pipe_buffer, NULL);

   log_line("Created thread to consume pipe input buffer");

   int readfd = open(FIFO_RUBY_STATION_VIDEO_STREAM, O_RDONLY);// | O_NONBLOCK);
   if( -1 == readfd )
   {
      log_error_and_alarm("Failed to open video stream fifo.");
      ruby_drm_core_uninit();
      mpp_uninit();
      return;
   }

   log_line("Opened input video stream fifo (%s)", FIFO_RUBY_STATION_VIDEO_STREAM);
   mpp_start_decoding_thread();

   FILE* fpTmp = NULL;
   //fpTmp = fopen("rec.h264", "wb");

   u32 uTimeLastCheck = get_current_timestamp_ms();
   int nRead = 1;
   int nReadFailedCounter = 0;
   u32 uTimeLastReadFailedLog = 0;
   int iCount =0;
   int iTotalRead = 0;
   bool bAnyInputEver = false;
   fd_set readset;

   while ( !g_bQuit )
   {
      FD_ZERO(&readset);
      FD_SET(readfd, &readset);

      struct timeval timeInput;
      timeInput.tv_sec = 0;
      timeInput.tv_usec = 10*1000; // 10 miliseconds timeout

      int iSelectResult = select(readfd+1, &readset, NULL, NULL, &timeInput);
      if ( iSelectResult <= 0 )
      {
         if ( iSelectResult < 0 )
         {
            log_softerror_and_alarm("Failed to read input pipe.");
            break;
         }
         continue;
      }
      iCount++;

      int iSizeToRead = 4096;
      if ( g_iPipeBufferWritePos + iSizeToRead > PIPE_BUFFER_SIZE )
      {
         iSizeToRead = PIPE_BUFFER_SIZE - g_iPipeBufferWritePos;
      }
      nRead = read(readfd, &(g_uPipeBuffer[g_iPipeBufferWritePos]), iSizeToRead); 
      if ( nRead <= 0 )
      {
         if ( ! bAnyInputEver )
         {
            usleep(2*1000);
            u32 uTime = get_current_timestamp_ms();
            if ( uTime > uTimeLastCheck + 3000 )
            {
               log_softerror_and_alarm("No video input data ever read and timedout waiting for video data on pipe. Exit.");
               break;
            }
            continue;
         }
         if ( nRead < 0 )
         {
            log_line("Reached end of input stream data. Ending video streaming. errono: %d, (%s)", errno, strerror(errno));
            break;
         }
         nReadFailedCounter++;
         u32 uTimeNow = get_current_timestamp_ms();
         if ( (uTimeNow > uTimeLastReadFailedLog + 200) || (nReadFailedCounter > 50) )
         {
            log_line("No read data. Failed counter: %d. Continue, write pos: %d, write size: %d",
               nReadFailedCounter, g_iPipeBufferWritePos, iSizeToRead);
            nReadFailedCounter = 0;
            uTimeLastReadFailedLog = uTimeNow;
         }
         hardware_sleep_ms(2);
         continue;
      }

      if ( ! bAnyInputEver )
      {
         log_line("Start receiving video stream data through pipe (%d bytes)", nRead);
         bAnyInputEver = true;
      }
      
      if ( NULL != fpTmp )
         fwrite( &(g_uPipeBuffer[g_iPipeBufferWritePos]), 1, nRead, fpTmp);

      int iNewWritePos = g_iPipeBufferWritePos + nRead;
      if ( iNewWritePos >= PIPE_BUFFER_SIZE )
         iNewWritePos = 0;
      g_iPipeBufferWritePos = iNewWritePos;
      
      iTotalRead += nRead;
      if ( (iCount % 10) == 0 )
      {
         u32 uTime = get_current_timestamp_ms();
         if ( uTime > uTimeLastCheck + 4000 )
         {
            uTimeLastCheck = uTime;
            log_line("Video player alive, reading %d bits/sec", iTotalRead*8/4);
            iTotalRead = 0;
         }
      }
   }

   if ( NULL != fpTmp )
      fclose(fpTmp);

   if ( g_bQuit )
      log_line("Ending video stream play due to quit signal.");

   close(readfd);

   log_line("Ending thread to consume pipe input buffer...");
   pthread_join(pDecodeThread, NULL );
   log_line("Ended thread to consume pipe input buffer");

   mpp_mark_end_of_stream();

   ruby_drm_core_uninit();
   mpp_uninit();
}


void _do_stream_mode_udp()
{
   if ( mpp_init(g_bUseH265Decoder) != 0 )
      return;

   hdmi_enum_modes();
   int iHDMIIndex = hdmi_load_current_mode();
   if ( iHDMIIndex < 0 )
      iHDMIIndex = hdmi_get_best_resolution_index_for(DEFAULT_RADXA_DISPLAY_WIDTH, DEFAULT_RADXA_DISPLAY_HEIGHT, DEFAULT_RADXA_DISPLAY_REFRESH);
   log_line("HDMI mode to use: %d (%d x %d @ %d)", iHDMIIndex, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh() );
   ruby_drm_core_init(1, DRM_FORMAT_NV12, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh());

   int iSock = -1;
   struct sockaddr_in udpAddr;

   iSock = socket(AF_INET, SOCK_DGRAM, 0);
   if ( iSock < 0 )
   {
      log_error_and_alarm("Failed to create socket");
      ruby_drm_core_uninit();
      mpp_uninit();
      return;    
   }

   /*
   const int optval = 1;
   if(setsockopt(iSock, SOL_PACKET, PACKET_QDISC_BYPASS, (const void *)&optval , sizeof(optval)) !=0)
   {
      close(iSock);
      log_error_and_alarm("Failed to set socket options");
      ruby_drm_core_uninit();
      mpp_uninit();
      return;    
   }
   */

   memset(&udpAddr, 0, sizeof(udpAddr));
   udpAddr.sin_family = AF_INET;
   udpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
   udpAddr.sin_port = htons(DEFAULT_LOCAL_VIDEO_PLAYER_UDP_PORT);

   int iRes = bind(iSock, (struct sockaddr *)&udpAddr, sizeof(udpAddr));
   if ( iRes < 0 )
   {
      log_error_and_alarm("Failed to bind socket on port %d", DEFAULT_LOCAL_VIDEO_PLAYER_UDP_PORT);
      ruby_drm_core_uninit();
      mpp_uninit();
      return;    
   }

   log_line("Opened input video stream udp socket on port %d", DEFAULT_LOCAL_VIDEO_PLAYER_UDP_PORT);
   mpp_start_decoding_thread();

   u32 uTimeLastCheck = get_current_timestamp_ms();
   unsigned char uBuffer[2049];
   int iCount =0;
   int iTotalRead = 0;
   bool bAnyInputEver = false;
   while ( !g_bQuit )
   {
      iCount++;

      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(iSock, &readset);

      struct timeval timeUDPInput;
      timeUDPInput.tv_sec = 0;
      timeUDPInput.tv_usec = 10*1000; // 10 miliseconds timeout

      int iSelectResult = select(iSock+1, &readset, NULL, NULL, &timeUDPInput);
      if ( iSelectResult < 0 )
      {
         log_error_and_alarm("Failed to select socket.");
         continue;
      }
      if ( iSelectResult == 0 )
         continue;

      if( 0 == FD_ISSET(iSock, &readset) )
         continue;

      
      socklen_t  recvLen = 0;
      struct sockaddr_in addrClient;
      int iRecv = recvfrom(iSock, uBuffer, 2048, MSG_WAITALL, (sockaddr*)&addrClient, &recvLen);
      if ( iRecv < 0 )
      {
         if ( ! bAnyInputEver )
         {
            usleep(2*1000);
            u32 uTime = get_current_timestamp_ms();
            if ( uTime > uTimeLastCheck + 3000 )
            {
               log_softerror_and_alarm("No video input data ever read and timedout waiting for video data on UDP port. Exit.");
               break;
            }
            continue;
         }
         if ( iRecv < 0 )
         {
            log_line("Reached end of input stream data, failed to read UDP port. Ending video streaming. errono: %d, (%s)", errno, strerror(errno));
            break;
         }
         log_line("No read data. Continue");
         continue;
      }
      

      if ( ! bAnyInputEver )
      {
         log_line("Start receiving video stream data through udp port (%d bytes)", (int)iRecv);
         bAnyInputEver = true;
      }
      mpp_feed_data_to_decoder(uBuffer, iRecv);
      iTotalRead += iRecv;
      usleep(2*1000);
      if ( (iCount % 10) == 0 )
      {
         u32 uTime = get_current_timestamp_ms();
         if ( uTime > uTimeLastCheck + 4000 )
         {
            uTimeLastCheck = uTime;
            log_line("Video player alive, reading %d bits/sec", iTotalRead*8/4);
            iTotalRead = 0;
         }
      }
   }

   if ( g_bQuit )
      log_line("Ending video stream play due to quit signal.");

   close(iSock);
   mpp_mark_end_of_stream();

   ruby_drm_core_uninit();
   mpp_uninit();
}

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   g_bQuit = true;
}

int main(int argc, char *argv[])
{
   
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

  
   log_init("RubyPlayer");


   if ( argc < 2 )
   {
      printf("\nUsage: ruby_player_raxa [params]\nParams:\n\n");
      printf("-t Test mode\n");
      printf("-p Play the live video stream from pipe\n");
      printf("-u Play the live video stream from UDP socket\n");
      printf("-h265 use H265 decoder\n");
      printf("-f [filename] Play H264 file\n");
      printf("-m [wxh@r] Sets a custom video mode\n");
      printf("-i init UI layer too when playing stream or files\n");
      printf("-d debug output to stdout\n\n");
      return 0;
   }

   g_szPlayFileName[0] = 0;
   int iParam = 0;

   do
   {
      if ( 0 == strcmp(argv[iParam], "-t") )
         g_bTestMode = true;
      if ( 0 == strcmp(argv[iParam], "-p") )
         g_bPlayStreamPipe = true;
      if ( 0 == strcmp(argv[iParam], "-u") )
         g_bPlayStreamUDP = true;
      if ( 0 == strcmp(argv[iParam], "-i") )
         g_bInitUILayerToo = true;
      if ( 0 == strcmp(argv[iParam], "-h265") )
         g_bUseH265Decoder = true;
      if ( 0 == strcmp(argv[iParam], "-d") )
      {
         g_bDebug = true;
         log_enable_stdout();
      }
      if ( 0 == strcmp(argv[iParam], "-f") )
      {
         g_bPlayFile = true;
         iParam++;
         strncpy(g_szPlayFileName, argv[iParam], MAX_FILE_PATH_SIZE);
         if ( NULL != strstr(g_szPlayFileName, ".h265") )
            g_bUseH265Decoder = true;
      }
      if ( 0 == strcmp(argv[iParam], "-m") )
      {
         iParam++;
         char szTmp[256];
         strncpy(szTmp, argv[iParam], 255);
         szTmp[255] = 0;
         for( int i=0; i<(int)strlen(szTmp); i++ )
         {
            if ( ! isdigit(szTmp[i]) )
               szTmp[i] = ' ';
         }

         if ( 3 != sscanf(szTmp, "%d %d %d", &g_iCustomWidth, &g_iCustomHeight, &g_iCustomRefresh) )
         {
            g_iCustomWidth = 0;
            g_iCustomHeight = 0;
            g_iCustomRefresh = 0;            
         }
      }
      iParam++;
   }
   while (iParam < argc);

   if ( g_bPlayFile )
      log_line("Running mode: play file: [%s]", g_szPlayFileName);
   if ( g_bPlayStreamPipe )
      log_line("Running mode: stream from pipe");
   if ( g_bPlayStreamUDP )
      log_line("Running mode: stream from UDP");
   if ( g_bTestMode )
      log_line("Running mode: test mode");
   if ( 0 != g_iCustomWidth )
      log_line("Set custom video mode: %dx%d@%d", g_iCustomWidth, g_iCustomHeight, g_iCustomRefresh);

   if ( (!g_bTestMode) && (!g_bPlayFile) && (!g_bPlayStreamPipe) && (!g_bPlayStreamUDP) )
   {
      log_softerror_and_alarm("Invalid params, no mode specified. Exit.");
      return 0;
   }

   if ( g_bTestMode )
      _do_test_mode();
   else if ( g_bPlayFile )
      _do_player_mode();
   else if ( g_bPlayStreamPipe )
      _do_stream_mode_pipe();
   else if ( g_bPlayStreamUDP )
      _do_stream_mode_udp();

   return 0;
}

