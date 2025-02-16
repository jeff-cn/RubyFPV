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
#include "r_initradio.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_radio_txpower.h"
#include "../base/hw_procs.h"
#include "../base/radio_utils.h"
#if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA_ZERO3)
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#endif
#include "../common/string_utils.h"

bool s_bIsStation = false;
bool configure_radios_succeeded = false;
int giDataRateMbAtheros = 0;

void _set_radio_region()
{
   //hw_execute_bash_command("iw reg set DE", NULL);
   //system("iw reg set BO");

   char szOutput[256];
   hw_execute_bash_command_raw("iw reg set 00", szOutput);
}

bool _configure_radio_interface_atheros(int iInterfaceIndex, radio_hw_info_t* pRadioHWInfo, u32 uDelayMS)
{
   if ( (NULL == pRadioHWInfo) || (iInterfaceIndex < 0) || (iInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return false;

   char szComm[128];
   char szOutput[2048];

   #ifdef HW_PLATFORM_OPENIPC_CAMERA

   //sprintf(szComm, "iwconfig %s mode monitor", pRadioHWInfo->szName);
   sprintf(szComm, "iw dev %s set type monitor", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   //sprintf(szComm, "ifconfig %s up", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   return true;
   #endif

   //sprintf(szComm, "ifconfig %s down", pRadioHWInfo->szName );
   //execute_bash_command(szComm, NULL);
   //hardware_sleep_ms(uDelayMS);
   //sprintf(szComm, "iw dev %s set type managed", pRadioHWInfo->szName );
   //execute_bash_command(szComm, NULL);
   //hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   //sprintf(szComm, "ifconfig %s up", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);
   int dataRateMb = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS/1000/1000;
   if ( ! s_bIsStation )
   if ( giDataRateMbAtheros > 0 )
      dataRateMb = giDataRateMbAtheros;
   
   if ( dataRateMb == 0 )
      dataRateMb = DEFAULT_RADIO_DATARATE_VIDEO/1000/1000;
   if ( dataRateMb > 0 )
      sprintf(szComm, "iw dev %s set bitrates legacy-2.4 %d lgi-2.4", pRadioHWInfo->szName, dataRateMb );
   else
      sprintf(szComm, "iw dev %s set bitrates ht-mcs-2.4 %d lgi-2.4", pRadioHWInfo->szName, -dataRateMb-1 );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);
   //sprintf(szComm, "ifconfig %s down 2>&1", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s down", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);
   
   sprintf(szComm, "iw dev %s set monitor none", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   //sprintf(szComm, "ifconfig %s up", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);
   //printf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   //execute_bash_command(szComm, NULL);
   //hardware_sleep_ms(uDelayMS);

   pRadioHWInfo->iCurrentDataRateBPS = dataRateMb*1000*1000;

   return true;

}

bool _configure_radio_interface_realtek(int iInterfaceIndex, radio_hw_info_t* pRadioHWInfo, u32 uDelayMS)
{
   if ( (NULL == pRadioHWInfo) || (iInterfaceIndex < 0) || (iInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return false;

   char szComm[128];
   char szOutput[2048];

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   
   //sprintf(szComm, "ifconfig %s up", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iwconfig %s mode monitor", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);
   
   return true;

   #endif

   //sprintf(szComm, "ifconfig %s down 2>&1", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s down", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor none 2>&1", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   //sprintf(szComm, "ifconfig %s up 2>&1", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   return true;
}

bool _configure_radio_interface(int iInterfaceIndex, u32 uDelayMS)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return false;

   char szComm[128];
   char szOutput[2048];
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_error_and_alarm("Failed to get radio info for interface %d.", iInterfaceIndex+1);
      return false;
   }

   pRadioHWInfo->iCurrentDataRateBPS = 0;
   //sprintf(szComm, "ifconfig %s mtu 2304 2>&1", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s mtu 1400", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   if ( pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS )
      _configure_radio_interface_atheros(iInterfaceIndex, pRadioHWInfo, uDelayMS);
   else
   {
      _configure_radio_interface_realtek(iInterfaceIndex, pRadioHWInfo, uDelayMS);
      // Set a default minimum tx power
      if ( hardware_radio_driver_is_rtl8812au_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812au(iInterfaceIndex, 10);
      if ( hardware_radio_driver_is_rtl8812eu_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812eu(iInterfaceIndex, 10);
   }

   if ( hardware_radioindex_supports_frequency(iInterfaceIndex, DEFAULT_FREQUENCY58) )
   {
      #ifdef HW_PLATFORM_RASPBERRY
      sprintf(szComm, "iw dev %s set freq %d 2>&1", pRadioHWInfo->szName, DEFAULT_FREQUENCY58/1000);
      #else
      sprintf(szComm, "iwconfig %s freq %u000 2>&1", pRadioHWInfo->szName, (u32)DEFAULT_FREQUENCY58);
      #endif
      pRadioHWInfo->uCurrentFrequencyKhz = DEFAULT_FREQUENCY58;
      hw_execute_bash_command(szComm, szOutput);
      if ( 0 != szOutput[0] )
         log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
      pRadioHWInfo->lastFrequencySetFailed = 0;
      pRadioHWInfo->uFailedFrequencyKhz = 0;
   }
   else if ( hardware_radioindex_supports_frequency(iInterfaceIndex, DEFAULT_FREQUENCY) )
   {
      #ifdef HW_PLATFORM_RASPBERRY
      sprintf(szComm, "iw dev %s set freq %d 2>&1", pRadioHWInfo->szName, DEFAULT_FREQUENCY/1000);
      #else
      sprintf(szComm, "iwconfig %s freq %u000 2>&1", pRadioHWInfo->szName, (u32)DEFAULT_FREQUENCY);
      #endif
      pRadioHWInfo->uCurrentFrequencyKhz = DEFAULT_FREQUENCY;
      hw_execute_bash_command(szComm, szOutput);
      if ( 0 != szOutput[0] )
         log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
      pRadioHWInfo->lastFrequencySetFailed = 0;
      pRadioHWInfo->uFailedFrequencyKhz = 0;
   }
   else
   {
      pRadioHWInfo->uCurrentFrequencyKhz = 0;
      pRadioHWInfo->lastFrequencySetFailed = 1;
      pRadioHWInfo->uFailedFrequencyKhz = DEFAULT_FREQUENCY;
   }

   sprintf(szComm, "iwconfig %s rts off 2>&1", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);

   return true;
}

int init_Radios()
{
   char szComm[128];
   char szOutput[2048];
   configure_radios_succeeded = false;

   log_line("=================================================================");
   log_line("Configuring radios: START.");

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_RADIOS_CONFIGURED);
   if( access( szFile, R_OK ) != -1 )
   {
      log_line("Radios already configured. Do nothing.");
      log_line("=================================================================");
      configure_radios_succeeded = true;
      return 1;
   }
   
   u32 uDelayMS = DEFAULT_DELAY_WIFI_CHANGE;
   s_bIsStation = hardware_is_station();

   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA_ZERO3)

   if ( s_bIsStation )
   {
      log_line("Configuring radios: we are station.");
      load_Preferences();
      load_ControllerInterfacesSettings();

      Preferences* pP = get_Preferences();
      if ( NULL != pP )
         uDelayMS = (u32) pP->iDebugWiFiChangeDelay;
      if ( (uDelayMS < 1) || (uDelayMS > 200) )
         uDelayMS = DEFAULT_DELAY_WIFI_CHANGE;
   }
   else
   {
      log_line("Configuring radios: we are vehicle.");
      reset_ControllerInterfacesSettings();
   }

   #endif

   _set_radio_region();

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("No network cards found. Nothing to configure!");
      log_line("=================================================================");
      return 0;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
      {
         log_error_and_alarm("Failed to get radio info for interface %d.", i+1);
         continue;
      }
      if ( ! pRadioHWInfo->isConfigurable )
      {
         log_line("Configuring radio interface %d (%s): radio interface is not configurable. Skipping it.", i+1, pRadioHWInfo->szName);
         continue;
      }
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         log_line("Configuring radio interface %d (%s): radio interface is SiK radio. Skipping it.", i+1, pRadioHWInfo->szName);
         continue;
      }
      if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
      {
         log_softerror_and_alarm("Radio interface %d is not a wifi radio type. Skipping it.", i+1);
         continue;
      }

      log_line( "Configuring wifi radio interface %d: %s ...", i+1, pRadioHWInfo->szName );
      if ( ! pRadioHWInfo->isSupported )
      {
         log_softerror_and_alarm("Found unsupported wifi radio: %s, skipping.",pRadioHWInfo->szName);
         continue;
      }

      _configure_radio_interface(i, uDelayMS);

      //if ( s_bIsStation && controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      //{
      //   sprintf(szComm, "ifconfig %s down", pRadioHWInfo->szName );
      //   execute_bash_command(szComm, NULL);
      //}
      hardware_sleep_ms(2*uDelayMS);

      for( int k=0; k<4; k++ )
      {
         //sprintf(szComm, "ifconfig | grep %s", pRadioHWInfo->szName);
         sprintf(szComm, "ip link | grep %s", pRadioHWInfo->szName);
         hw_execute_bash_command(szComm, szOutput);
         removeNewLines(szOutput);
         if ( 0 != szOutput[0] )
         {
            log_line("Radio interface %s state: [%s]", pRadioHWInfo->szName, szOutput);
            break;
         }
         hardware_sleep_ms(50);
      }
   }

   _set_radio_region();

   FILE* fd = fopen(szFile, "w");
   fprintf(fd, "done");
   fclose(fd);
   
   hardware_save_radio_info();

   log_line("Configuring radios COMPLETED.");
   log_line("=================================================================");
   log_line("Radio interfaces and frequencies assigned:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      log_line("   * Radio interface %d: %s on port %s, %s %s %s: %s", i+1, str_get_radio_card_model_string(pRadioHWInfo->iCardModel), pRadioHWInfo->szUSBPort, pRadioHWInfo->szName, pRadioHWInfo->szMAC, pRadioHWInfo->szDescription, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz));
   }
   log_line("=================================================================");
   configure_radios_succeeded = true;
   return 1;
}


int r_initradio(int argc, char *argv[])
{
   if ( argc >= 2 )
      giDataRateMbAtheros = atoi(argv[argc-1]);

   log_init("RubyRadioInit");
   log_arguments(argc, argv);

   char szOutput[1024];
   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   log_line("Network devices found: [%s]", szOutput);

   hardware_enumerate_radio_interfaces();

   int retry = 0;
   while ( 0 == hardware_get_radio_interfaces_count() && retry < 10 )
   {
      hardware_sleep_ms(200);
      char szComm[128];
      sprintf(szComm, "rm -rf %s%s", FOLDER_CONFIG, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
      hw_execute_bash_command(szComm, NULL);
      hardware_reset_radio_enumerated_flag();
      hardware_enumerate_radio_interfaces();
      retry++;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("There are no radio interfaces (2.4/5.8 wlans) on this device.");
      return -1;
   }    
   
   init_Radios();

   log_line("Ruby Init Radio process completed.");
   return (0);
}