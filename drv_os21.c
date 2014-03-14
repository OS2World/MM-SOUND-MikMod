/*

Name: DRV_OS2.C, 12/06/1996

Author: Stefan Tibus

Description: Experimental Mikmod driver for output on OS/2 MMPM/2 using MCI.

Method: Double-Buffer, automatic refill using timer-semaphore in own thread.

Portability: OS/2 Warp v3.0 (Connect) PM - WATCOM v10.x

Copyright (c) Stefan Tibus, all rights reserved

*/

/*

Important:

This module is still in beta-state. There are no warranties at all.
The author is not responsible for any problems you have with using this module.

- This code and any part of it may not be published without the authors explicit
  permission.
- This code may only be distributed in complete and unmodified form - as it is
  now - and only for FREE !
- The unmodified code may be used in your own developments and distributed with
  them under the condition that there is a visible copyright message granting all
  rights on this module to the author.
  The copyright message has to contain the following:
  "copyright (c) Stefan Tibus, all rights reserved"  (e.g. "parts copyright..." or
  "OS/2-Sound-Driver copyright...")
- Any commercial use requires a special permission of the author !

- You are free to make changes to the source-code for your own, private use only.
- The modified code may not be published or distributed (separately or as part of
  your developments) without the authors explicit permission.
  Therefore it is necessary to report any changes to the author, with a description
  of what it is doing and how it works.

- Any feedback is welcome !

e-mail: Stefan Tibus, 2:246/8722.20@FidoNet
        Stefan.Tibus@ThePentagon.com

- The lines above may not be cut off this code.

*/

#define INCL_DOS
#define INCL_DOSPROCESS
#define INCL_32
#include <os2.h>
#include <mcios2.h>
#include <stdio.h>
#include <stdlib.h>
#include "mikmod.h"

static MCI_OPEN_PARMS mciOpenParms;
static MCI_GENERIC_PARMS mciGenericParms;
static MCI_PLAY_PARMS mciPlayParms;
static MCI_WAVE_SET_PARMS mciWaveSetParms;
static ULONG rc;
static PVOID pSound;
static TID tidSound;
static INT next;
static HEV hevSound;
static HTIMER htimerSound;

typedef struct
{
   ULONG ulCommand;
   ULONG ulOperand1;
   ULONG ulOperand2;
   ULONG ulOperand3;
} PLAYLISTSTRUCTURE;

static PLAYLISTSTRUCTURE playList[3];

static ULONG SOUNDBUFFERSIZE=65536; // 64k Sound-Puffer (2*32768 Bytes)

VOID APIENTRY SoundThread_SMALL(ULONG ul)
{
   ULONG ulPostCt;
   ulPostCt=ul;

   while(1) // Endlosschleife
   {
      // Auf Timer warten
      DosWaitEventSem(hevSound,SEM_INDEFINITE_WAIT);
      // Je nach aktuellem Status nÑchsten Puffer vorbereiten
      if (playList[0].ulOperand3>0)
      {
         if (next==1)
         {
            VC_WriteBytes((CHAR*)playList[1].ulOperand1,playList[1].ulOperand2);
            playList[1].ulOperand3=0;
            next=0;
         }
      }
      if (playList[1].ulOperand3>0)
      {
         if (next==0)
         {
            VC_WriteBytes((CHAR*)playList[0].ulOperand1,playList[0].ulOperand2);
            playList[0].ulOperand3=0;
            next=1;
         }
      }
      // Semaphore zurÅcksetzen
      DosResetEventSem(hevSound,&ulPostCt);
   }
}

BOOL OS2_MMPM2_SMALL_IsThere(VOID)
{
   // Parameter fÅr MCI-Befehl setzen
   mciOpenParms.hwndCallback=(HWND) NULL;                      
   mciOpenParms.usDeviceID=(USHORT) NULL;                      
   mciOpenParms.pszDeviceType=(PSZ) MCI_DEVTYPE_WAVEFORM_AUDIO;
   mciOpenParms.pszElementName=(PSZ) NULL;                     

   // WAVEAUDIO-Einheit îffnen
   rc=mciSendCommand(0, MCI_OPEN, MCI_WAIT|MCI_OPEN_SHAREABLE|MCI_OPEN_TYPE_ID,
      (PVOID) &mciOpenParms, 0);
   if (rc==0)
   {
      // Parameter fÅr MCI-Befehl setzen
      mciGenericParms.hwndCallback=(HWND) NULL;
      // Einheit schlie·en
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // Einheit konnte geîffnet werden, also Einheit vorhanden
      return TRUE;
   }
   else
      // Einheit konnte nicht geîffnet werden -> nicht vorhanden
      return FALSE;
}

BOOL OS2_MMPM2_SMALL_Init(VOID)
{
   ULONG ulInterval;
                                  
   // Speicher reservieren
   pSound=NULL;
   pSound=malloc(SOUNDBUFFERSIZE); // SOUNDBUFFERSIZE
   if (pSound==NULL)
   {
      // kein Speicher verfÅgbar
      myerr="Memory allocation for sound-buffer failed";
      return FALSE;
   }

   // PlayList erstellen:
   playList[0].ulCommand=DATA_OPERATION; // Daten spielen
   playList[0].ulOperand1=(ULONG)pSound; // Zeiger auf Daten
   playList[0].ulOperand2=(ULONG)(SOUNDBUFFERSIZE/2); // LÑnge der Daten
   playList[0].ulOperand3=0;
   playList[1].ulCommand=DATA_OPERATION; // Daten spielen
   playList[1].ulOperand1=(ULONG)pSound+(ULONG)(SOUNDBUFFERSIZE/2); // Zeiger auf Daten
   playList[1].ulOperand2=(ULONG)(SOUNDBUFFERSIZE/2); // LÑnge der Daten
   playList[1].ulOperand3=0;
   playList[2].ulCommand=BRANCH_OPERATION; // Sprung
   playList[2].ulOperand1=0;
   playList[2].ulOperand2=0; // Sprungziel (Block 0)
   playList[2].ulOperand3=0;

   // Parameter fÅr MCI-Befehl setzen
   mciOpenParms.hwndCallback=(HWND) NULL;
   mciOpenParms.usDeviceID=(USHORT) NULL;
   mciOpenParms.pszDeviceType=(PSZ) MCI_DEVTYPE_WAVEFORM_AUDIO;
   mciOpenParms.pszElementName=(PSZ) &playList;
   // WAVEAUDIO-Einheit îffnen
   rc=mciSendCommand(0,MCI_OPEN, MCI_WAIT|MCI_OPEN_TYPE_ID|MCI_OPEN_PLAYLIST,
      (PVOID) &mciOpenParms, 0);
   if (rc)
   {
      // ôffnen fehlgeschlagen
      myerr="Unable to open WAVEAUDIO-unit";
      free(pSound);
      return FALSE;
   }
   // MCI: set waveset-parameters
   mciWaveSetParms.hwndCallback=(HWND) NULL;
   mciWaveSetParms.ulSamplesPerSec=md_mixfreq;
   if (md_mode&DMODE_16BITS)
   {
      mciWaveSetParms.usBitsPerSample=16;
   }
   else
   {
      mciWaveSetParms.usBitsPerSample=8;
   }
   if (md_mode&DMODE_STEREO)
   {
      mciWaveSetParms.usChannels=2;
   }
   else
   {
      mciWaveSetParms.usChannels=1;
   }
   mciWaveSetParms.ulAudio=MCI_SET_AUDIO_ALL;
   // MCI: set WAVEAUDIO-parameters
   rc=mciSendCommand(mciOpenParms.usDeviceID,
      MCI_SET,MCI_WAIT|MCI_WAVE_SET_SAMPLESPERSEC|MCI_WAVE_SET_BITSPERSAMPLE|MCI_WAVE_SET_CHANNELS,
      (PVOID) &mciWaveSetParms, 0);
   // test returncode
   if (rc)
   { // bad
      // error during setting of waveaudio-parameters
      myerr="Unable to set output parameters";
      // MCI: set generic parameters
      mciGenericParms.hwndCallback=(HWND) NULL;
      // MCI: close device
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // free soundbuffer
      DosFreeMem(pSound);
      return FALSE;
   }
   if (!VC_Init())
   {
      // MikMod-Fehler
      // Parameter fÅr MCI-Befehl setzen
      mciGenericParms.hwndCallback=(HWND) NULL;
      // Einheit schlie·en
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      free(pSound);
      return FALSE;
   }

   // Semaphore fÅr Puffer-Aktualisierung
   rc=DosCreateEventSem("\\SEM32\\MikMod\\Sound",&hevSound,DC_SEM_SHARED,FALSE);
   if (rc)
   {
      // konnte Semaphore nicht erstellen
      myerr="Unable to create semaphore";
      // Parameter fÅr MCI-Befehl setzen
      mciGenericParms.hwndCallback=(HWND) NULL;
      // Einheit schlie·en
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      free(pSound);
      return FALSE;
   }

   // Timer fÅr Puffer-Aktualisierung
   ulInterval=SOUNDBUFFERSIZE*1000/4/(mciWaveSetParms.usChannels*
      mciWaveSetParms.usBitsPerSample*mciWaveSetParms.ulSamplesPerSec);
   rc=DosStartTimer(ulInterval,(HSEM)hevSound,&htimerSound);
   if (rc)
   {
      // Timer nicht starten
      myerr="Unable to start timer";
      // Semaphore schlie·en
      DosCloseEventSem(hevSound);
      // Parameter fÅr MCI-Befehl setzen
      mciGenericParms.hwndCallback=(HWND) NULL;
      // Einheit schlie·en
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      free(pSound);
      return FALSE;
   }

   // eigener Thread fÅr Sound
   rc=DosCreateThread(&tidSound,&SoundThread_SMALL,0,CREATE_READY|STACK_SPARSE,4096);
   if (rc)
   {
      // konnte Thread nicht erstellen
      myerr="Unable to create thread";
      // Timer stoppen
      DosStopTimer(htimerSound);
      // Semaphore schlie·en
      DosCloseEventSem(hevSound);
      // Parameter fÅr MCI-Befehl setzen
      mciGenericParms.hwndCallback=(HWND) NULL;
      // Einheit schlie·en
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      free(pSound);
      return FALSE;
   }
//   DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,0,tidSound);

   return TRUE;
}

VOID OS2_MMPM2_SMALL_Exit(VOID)
{
   // Timer fÅr Sound stoppen
   DosStopTimer(htimerSound);

   // Semaphore fÅr Sound schlie·en
   DosCloseEventSem(hevSound);

   // Parameter fÅr MCI-Befehl setzen
   mciGenericParms.hwndCallback=(HWND) NULL;
   // Einheit schlie·en
   rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
      (PVOID) &mciGenericParms, 0);

   // Thread fÅr Sound beenden
   DosKillThread(tidSound);

   // Speicher freigeben
   free(pSound);
   VC_Exit();
}

VOID OS2_MMPM2_SMALL_PlayStart(VOID)
{
   ULONG ulInterval;
                                  
   ulInterval=SOUNDBUFFERSIZE*1000/4/(mciWaveSetParms.usChannels*
      mciWaveSetParms.usBitsPerSample*mciWaveSetParms.ulSamplesPerSec);
 
   VC_PlayStart();
   next=0;
   playList[1].ulOperand3=1;
   DosPostEventSem(hevSound);

   mciPlayParms.hwndCallback=(HWND) NULL;
   mciPlayParms.ulFrom=0;
   mciPlayParms.ulTo=4;
   rc=mciSendCommand(mciOpenParms.usDeviceID,MCI_PLAY,0,&mciPlayParms,0);
}

VOID OS2_MMPM2_SMALL_PlayStop(VOID)
{
   mciGenericParms.hwndCallback=(HWND) NULL;
   rc=mciSendCommand(mciOpenParms.usDeviceID,MCI_STOP,MCI_WAIT,&mciGenericParms,0);
   VC_PlayStop();
}

VOID OS2_MMPM2_SMALL_Update(VOID)
{
   // Does nothing, buffers are updated in the background
}

DRIVER drv_os2_mmpm2_smallbuffers=
{
   NULL,
   "OS/2 MMPM/2 MCI (small buffers)",
   "Mikmod OS/2 MMPM/2 MCI driver 0.1beta (small buffers)",
   OS2_MMPM2_SMALL_IsThere,
   VC_SampleLoad,
   VC_SampleUnload,
   OS2_MMPM2_SMALL_Init,
   OS2_MMPM2_SMALL_Exit,
   OS2_MMPM2_SMALL_PlayStart,
   OS2_MMPM2_SMALL_PlayStop,
   OS2_MMPM2_SMALL_Update,
   VC_VoiceSetVolume,
   VC_VoiceSetFrequency,
   VC_VoiceSetPanning,
   VC_VoicePlay
};
