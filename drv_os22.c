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

#define INCL_DOS // include OS/2-DOS-functions
#define INCL_DOSPROCESS // include process-management (threads,...)
#define INCL_DOSMEMMGR // include memory-management
#define INCL_32 // include 32Bit-functions
#include <os2.h> // include OS/2-functions
#include <mcios2.h> // include MCI-functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset
#include "mikmod.h"

static MCI_OPEN_PARMS mciOpenParms; // parameters for opening of an MCI-device
static MCI_GENERIC_PARMS mciGenericParms; // generic parameters for an MCI-command
static MCI_PLAY_PARMS mciPlayParms; // parameters for playing of an MCI-device
static MCI_WAVE_SET_PARMS mciWaveSetParms; // parameters for setting of an WAVEAUDIO-MCI-device
static ULONG rc; // general-use returncode
static PVOID pSound; // pointer to soundbuffer
static TID tidSound; // thread-ID of sound-thread
static UINT uiNext; // signals next buffer to update
static HEV hevSound; // handle for sound-eventsemaphore
static HEV hevPlay; // handle for play-semaphore
static HTIMER htimerSound; // handle for sound-timer

// typedef for playlist-structure
typedef struct
{
   ULONG ulCommand; // playlist-command
   ULONG ulOperand1; // 1st operand
   ULONG ulOperand2; // 2nd operand
   ULONG ulOperand3; // 3rd operand
} PLAYLISTSTRUCTURE;

#define SOUND_BUFFER_UPDATE_SIZE 32768
#define SOUND_BUFFER_UPDATES_PER_BLOCK 8
#define SOUND_BUFFER_BLOCK_COUNT 2 // number of soundbuffer-blocks

static UINT SOUNDBUFFERUPDATESIZE=SOUND_BUFFER_UPDATE_SIZE;
static UINT SOUNDBUFFERUPDATESPERBLOCK=SOUND_BUFFER_UPDATES_PER_BLOCK;
static UINT SOUNDBUFFERBLOCKCOUNT=SOUND_BUFFER_BLOCK_COUNT; // number of soundbuffer-blocks
static UINT SOUNDBUFFERBLOCKSIZE=SOUND_BUFFER_UPDATE_SIZE*SOUND_BUFFER_UPDATES_PER_BLOCK; // size of 1 soundbuffer-block
static ULONG SOUNDBUFFERSIZE=SOUND_BUFFER_BLOCK_COUNT*SOUND_BUFFER_UPDATES_PER_BLOCK*SOUND_BUFFER_UPDATE_SIZE; // size of soundbuffer
static PLAYLISTSTRUCTURE playList[SOUND_BUFFER_BLOCK_COUNT+1]; // array with playlist (count+1 commands)

// mixer-support, for use with SetAmpKnob:
typedef enum KNOB_ {VOLUME, BALANCE, BASS, TREBLE} KNOB;
BOOL OS2_MMPM2_LARGE_SetAmpKnob(KNOB eKnob, ULONG ulLevel);

// ***** SoundThread_LARGE *****
// Does update of the soundbuffer
VOID APIENTRY SoundThread_LARGE(ULONG ul)
{
   ULONG ulPostCt; // number of events happened
   UINT uiCount; // counter

   ulPostCt=ul; // really does nothing, just to avoid warning

   while(1) // endless loop
   {
      // wait for play
      DosWaitEventSem(hevPlay,SEM_INDEFINITE_WAIT);
// s.t. das soll verhindern, das schon angefangen wird die Puffer zu aktualisieren, obwohl noch
// kein Play erfolgt ist, das File evtl noch gar nicht geladen -> NULL-Zeiger-Fehler
      // wait for timer
      DosWaitEventSem(hevSound,SEM_INDEFINITE_WAIT);
      // update next buffer-block corresponding to uiNext
      if(uiNext==0)
      { // 1st block
         if (playList[SOUNDBUFFERBLOCKCOUNT-1].ulOperand3>0)
         { // only if last block is currently playing
            playList[uiNext].ulCommand=NOP_OPERATION;
            // update 1st half-buffer
            for (uiCount=0;uiCount<SOUNDBUFFERUPDATESPERBLOCK;uiCount++)
            { // update entire block in several parts of < 64k
               VC_WriteBytes((CHAR*)playList[uiNext].ulOperand1+(SOUNDBUFFERUPDATESIZE*uiCount),
                  SOUNDBUFFERUPDATESIZE); // update block
            }
            playList[uiNext].ulOperand3=0; // reset play-position (necessary?) -> avoids updating before being played
            playList[uiNext].ulCommand=DATA_OPERATION;
            uiNext++; // increment counter -> signal update of the next block
         }
      }
      else if(uiNext==SOUNDBUFFERBLOCKCOUNT-1)
      { // last block
         if (playList[uiNext-1].ulOperand3>0)
         { // only if block before is currently playing
            playList[uiNext].ulCommand=NOP_OPERATION;
            // update 2nd=last half-buffer
            for (uiCount=0;uiCount<SOUNDBUFFERUPDATESPERBLOCK;uiCount++)
            { // update entire block in several parts of < 64k
               VC_WriteBytes((CHAR*)playList[uiNext].ulOperand1+(SOUNDBUFFERUPDATESIZE*uiCount),
                  SOUNDBUFFERUPDATESIZE); // update block
            }
            playList[uiNext].ulOperand3=0; // reset play-position (necessary?) -> avoids updating before being played
            playList[uiNext].ulCommand=DATA_OPERATION;
            uiNext=0; // reset counter -> signal update of first block (looping)
         }
      }
      else
      { // any block else
         if (playList[uiNext-1].ulOperand3>0)
         { // only if block before is currently playing
            playList[uiNext].ulCommand=NOP_OPERATION;
            // update half-buffer
            for (uiCount=0;uiCount<SOUNDBUFFERUPDATESPERBLOCK;uiCount++)
            { // update entire block in several parts of < 64k
               VC_WriteBytes((CHAR*)playList[uiNext].ulOperand1+(SOUNDBUFFERUPDATESIZE*uiCount),
                  SOUNDBUFFERUPDATESIZE); // update block
            }
            playList[uiNext].ulOperand3=0; // reset play-position (necessary?) -> avoids updating before being played
            playList[uiNext].ulCommand=DATA_OPERATION;
            uiNext++; // increment counter -> signal update of the next block
         }
      }
      // reset timer-semaphore
      DosResetEventSem(hevSound,&ulPostCt);
/////      DosResetEventSem(hevSound,NULL);
// s.t. wird " weggelassen, bleibt das semaphore gesetzt und man hat eine echte Endlosschleife,
// die nicht auf den Timer wartet
   }
}

// ***** OS2_MMPM2_LARGE_IsThere *****
// checks for availability of an OS/2 MCI-WAVEAUDIO-device
BOOL OS2_MMPM2_LARGE_IsThere(VOID)
{
   // MCI: set open-parameters
   mciOpenParms.hwndCallback=(HWND) NULL;                      
   mciOpenParms.usDeviceID=(USHORT) NULL;                      
   mciOpenParms.pszDeviceType=(PSZ) MCI_DEVTYPE_WAVEFORM_AUDIO;
   mciOpenParms.pszElementName=(PSZ) NULL;                     

   // MCI: open WAVEAUDIO-device
   rc=mciSendCommand(0, MCI_OPEN, MCI_WAIT|MCI_OPEN_SHAREABLE|MCI_OPEN_TYPE_ID,
      (PVOID) &mciOpenParms, 0);
   // test returncode
   if (rc==0)
   { // good
      // MCI: set generic parameters
      mciGenericParms.hwndCallback=(HWND) NULL;
      // MCI: close device
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // device is there -> return TRUE
      return TRUE;
   }
   else
      // unable to open device -> return FALSE
      return FALSE;
}

// ***** OS2_MMPM2_LARGE_Init *****
// initializes the OS/2 MCI-WAVEAUDIO-device, allocates soundbuffer etc.
BOOL OS2_MMPM2_LARGE_Init(VOID)
{
   ULONG ulInterval; // timer-interval
   ULONG ulPostCt; // counter for semaphore-events
   UINT uiCount; // counter

   // reset pointer to soundbuffer
   pSound=NULL;
   // allocate (commited) memory for soundbuffer
   rc=DosAllocMem(&pSound,SOUNDBUFFERSIZE,PAG_READ|PAG_WRITE|PAG_COMMIT);
   // test returncode
   if (rc)
   { // bad
      // error during memory allocation
      myerr="Memory allocation failed";
      return FALSE;
   }

   // initialize playlist
   for (uiCount=0;uiCount<SOUNDBUFFERBLOCKCOUNT;uiCount++)
   { // for each entry:
      playList[uiCount].ulCommand=DATA_OPERATION; // play data
      playList[uiCount].ulOperand1=(ULONG)pSound+(SOUNDBUFFERBLOCKSIZE*uiCount); // pointer to data
      playList[uiCount].ulOperand2=(ULONG)SOUNDBUFFERBLOCKSIZE; // length of data
      playList[uiCount].ulOperand3=0; // current-play-position (used to determine currently playing block)
   }
   // last entry:
   playList[SOUNDBUFFERBLOCKCOUNT].ulCommand=BRANCH_OPERATION; // jump
   playList[SOUNDBUFFERBLOCKCOUNT].ulOperand1=0; // nothing
   playList[SOUNDBUFFERBLOCKCOUNT].ulOperand2=0; // target (block 0 i.e. 1st block)
   playList[SOUNDBUFFERBLOCKCOUNT].ulOperand3=0; // nothing
   // signal first block to be next
   uiNext=0;
   playList[SOUNDBUFFERBLOCKCOUNT-1].ulOperand3=1;

   // MCI: set open-parameters
   mciOpenParms.hwndCallback=(HWND) NULL;
   mciOpenParms.usDeviceID=(USHORT) NULL;
   mciOpenParms.pszDeviceType=(PSZ) MCI_DEVTYPE_WAVEFORM_AUDIO;
   mciOpenParms.pszElementName=(PSZ) &playList;
   // MCI: open WAVEAUDIO-device
   rc=mciSendCommand(0,MCI_OPEN, MCI_WAIT|MCI_OPEN_TYPE_ID|MCI_OPEN_PLAYLIST,
      (PVOID) &mciOpenParms, 0);
   // test returncode
   if (rc)
   { // bad
      // error during opening of device
      myerr="Cannot open WAVEAUDIO-device";
      // free soundbuffer
      DosFreeMem(pSound);
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
   // mikmod: initialize
   if (!VC_Init())
   { // bad
      // mikmod-error
      // MCI: set generic parameters
      mciGenericParms.hwndCallback=(HWND) NULL;
      // MCI: close device
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // free soundbuffer
      DosFreeMem(pSound);
      return FALSE;
   }

   // create play-semaphore
   rc=DosCreateEventSem("\\SEM32\\MikMod\\Play",&hevPlay,DC_SEM_SHARED,FALSE);
   // test returncode
   if (rc)
   { // bad
      // error during creation of semaphore
      myerr="Cannot create play-semaphore";
      // MCI: set generic parameters
      mciGenericParms.hwndCallback=(HWND) NULL;
      // MCI: close device
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // free soundbuffer
      DosFreeMem(pSound);
      return FALSE;
   }
   // reset play-semaphore
   DosResetEventSem(hevPlay,&ulPostCt);

   // create sound-semaphore
   rc=DosCreateEventSem("\\SEM32\\MikMod\\Sound",&hevSound,DC_SEM_SHARED,FALSE);
   // test returncode
   if (rc)
   { // bad
      // error during creation of semaphore
      myerr="Cannot create sound-semaphore";
      // close semaphore
      DosCloseEventSem(hevPlay);
      // MCI: set generic parameters
      mciGenericParms.hwndCallback=(HWND) NULL;
      // MCI: close device
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // free soundbuffer
      DosFreeMem(pSound);
      return FALSE;
   }
   // reset sound-semaphore
   DosResetEventSem(hevSound,&ulPostCt);

   // calculate interval for sound-timer (1 times per block)
   ulInterval=SOUNDBUFFERBLOCKSIZE*1000/1/(mciWaveSetParms.usChannels*
      mciWaveSetParms.usBitsPerSample*mciWaveSetParms.ulSamplesPerSec);
// s.t. 1 mal pro Block sollte wohl mindestens drin sein (normalerweise denke ich 2 mal)
// also 2 bzw. 4 mal beim ganzen Puffer (in 2 Blîcken)
   // start sound-timer
   rc=DosStartTimer(ulInterval,(HSEM)hevSound,&htimerSound);
   // test returncode
   if (rc)
   { // bad
      // error during starting of timer
      myerr="Cannot start timer";
      // close semaphore
      DosCloseEventSem(hevSound);
      // close semaphore
      DosCloseEventSem(hevPlay);
      // MCI: set generic parameters
      mciGenericParms.hwndCallback=(HWND) NULL;
      // MCI: close device
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // free soundbuffer
      DosFreeMem(pSound);
      return FALSE;
   }

   // create sound-thread
   rc=DosCreateThread(&tidSound,&SoundThread_LARGE,0,CREATE_READY|STACK_SPARSE,4096);
   // test returncode
   if (rc)
   { // bad
      // error during creation of thread
      myerr="Cannot create thread";
      // stop timer
      DosStopTimer(htimerSound);
      // close semaphore
      DosCloseEventSem(hevSound);
      // close semaphore
      DosCloseEventSem(hevPlay);
      // MCI: set generic parameters
      mciGenericParms.hwndCallback=(HWND) NULL;
      // MCI: close device
      rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
         (PVOID) &mciGenericParms, 0);
      // free soundbuffer
      DosFreeMem(pSound);
      return FALSE;
   }
   // set priority of sound-thread
//////   rc=DosSetPriority(PRTYS_THREAD,PRTYC_FOREGROUNDSERVER,0,tidSound);
// s.t. PRTYC_FOREGROUNDSERVER gibt dem Thread eine DEUTLICH hîhere PrioritÑt,
// solange er arbeitet (nicht wenn er auf ein semaphore wartet)
// ich wei· nicht ob's tatsÑchlich was bringt ;-)

   // set volume to 80% and test returncode
   if(!OS2_MMPM2_LARGE_SetAmpKnob(VOLUME,80))
   { // bad
      // error setting volume
      myerr="Error setting volume";
      return FALSE;
   }
   // if the program arrives here, everything is ok.
   return TRUE;
}

// ***** OS2_MMPM2_LARGE_Exit *****
// closes the OS/2 MCI-WAVEAUDIO-device, frees soundbuffer etc.
VOID OS2_MMPM2_LARGE_Exit(VOID)
{
   // stop sound-thread
   DosKillThread(tidSound);

   // stop sound-timer
   DosStopTimer(htimerSound);

   // close sound-semaphore
   DosCloseEventSem(hevSound);

   // close play-semaphore
   DosCloseEventSem(hevPlay);

   // close WAVEAUDIO device
   // MCI: set generic parameters
   mciGenericParms.hwndCallback=(HWND) NULL;
   // MCI: close device
   rc=mciSendCommand(mciOpenParms.usDeviceID, MCI_CLOSE, MCI_WAIT,
      (PVOID) &mciGenericParms, 0);

   // free soundbuffer
   DosFreeMem(pSound);

   // mikmod: Exit
   VC_Exit();
}

// ***** OS2_MMPM2_LARGE_PlayStart *****
// starts playing the soundbuffer
VOID OS2_MMPM2_LARGE_PlayStart(VOID)
{
   ULONG ulPostCt; // counter for play-(semaphore-)events
/* reset soundbuffer(s) and rewind */
   // reset play-semaphore
   DosResetEventSem(hevPlay,&ulPostCt);
   // reset soundbuffer(s)
   memset(pSound,0,SOUNDBUFFERSIZE);
   // MCI: set generic parameters
   mciGenericParms.hwndCallback=(HWND) NULL;
   // MCI: rewind
   rc=mciSendCommand(mciOpenParms.usDeviceID,MCI_REWIND,MCI_WAIT,&mciGenericParms,0);
/* ------------------------------- */

// initialize timer here instead of in Init ? -> no error message possible ?

   // mikmod: start playing
   VC_PlayStart();

   // signal first block as next
   uiNext=0;
   // set play-semaphore
   DosPostEventSem(hevPlay);
//   // imitate playing of last block (necessary to update first block)
//   playList[SOUNDBUFFERBLOCKCOUNT-1].ulOperand3=(ULONG)SOUNDBUFFERBLOCKSIZE-1;
// s.t. warum auch immer es hilft - lassen wir's weg

   // MCI: set play-parameters
   mciPlayParms.hwndCallback=(HWND) NULL;
   mciPlayParms.ulFrom=0;
   mciPlayParms.ulTo=0;
   // MCI: start playing
   rc=mciSendCommand(mciOpenParms.usDeviceID,MCI_PLAY,0,&mciPlayParms,0);
}

// ***** OS2_MMPM2_LARGE_PlayStop *****
// stopps playing
VOID OS2_MMPM2_LARGE_PlayStop(VOID)
{
   ULONG ulPostCt; // counter for play-(semaphore-)events

   // MCI: set generic parameters
   mciGenericParms.hwndCallback=(HWND) NULL;
   // MCI: stop playing
   rc=mciSendCommand(mciOpenParms.usDeviceID,MCI_STOP,MCI_WAIT,&mciGenericParms,0);

   // reset play-semaphore
   DosResetEventSem(hevPlay,&ulPostCt);
/////   DosResetEventSem(hevPlay,NULL);
// s.t. da ulPostCt ein RÅckgabewert ist, mu· er doch nicht initialisiert werden, oder ?
   // mikmod: stop playing
   VC_PlayStop();

//   // stop sound-timer (if started in PlayStart)
//   DosStopTimer(htimerSound);
// s.t. wieso hast Du hier den Timer gestoppt ? es wird dann nur beim nÑchsten Lied weitergespielt,
// wenn im Thread nicht auf den Timer gewartet wird ! (oder seh' ich das falsch ?)
}

// ***** OS2_MMPM2_LARGE_Update *****
// dummy, does nothing, buffers are updated in the backgrounf
VOID OS2_MMPM2_LARGE_Update(VOID)
{
   // does nothing, buffers are updated in the background
   return;
// s.t. das return ist hier doch nicht notwendig ? oder ist das dann schneller ? ;-)
}

// ***** OS2_MMPM2_LARGE_SetAmpKnob *****
// do mixer settings
BOOL OS2_MMPM2_LARGE_SetAmpKnob(KNOB eKnob, ULONG ulLevel)
{                                                                
   MCI_SET_PARMS mciSetParms;
   ULONG ulSetWhich;

   // MCI: set parameters for MCI_SET
   // set window-handle for callback
   mciSetParms.hwndCallback=(HWND)NULL;
   // select channels
   mciSetParms.ulAudio=MCI_SET_AUDIO_ALL;
   // set volume level
   mciSetParms.ulLevel=ulLevel;
   // set delay time
   mciSetParms.ulOver=0;
   // others (not affected, just to reset structure)
   mciSetParms.ulItem=0;
   mciSetParms.ulValue=0;
   mciSetParms.ulTimeFormat=0;
   mciSetParms.ulSpeedFormat=0;

   switch (eKnob)
   {
      case VOLUME:
         ulSetWhich = MCI_SET_VOLUME;
         break;
      // not yet supported, reserved for future
      // version with mixer support 
      case BALANCE :
         ulSetWhich = 0; // MCI_AMP_SET_BALANCE
         break;
      case BASS :
         ulSetWhich = 0; // MCI_AMP_SET_BASS
         break;
      case TREBLE :
         ulSetWhich = 0; // MCI_AMP_SET_TREBLE
         break;
      // MCI_AMP_SET_PITCH, MCI_AMP_SET_GAIN
      default :
         ulSetWhich = 0;
         break;
   }
   // MCI: set device-parameters
   rc=mciSendCommand(mciOpenParms.usDeviceID,
      MCI_SET,
      MCI_WAIT | MCI_SET_AUDIO | MCI_SET_ON | ulSetWhich,
      &mciSetParms,0);
   if(rc)
   {
      myerr="Error setting mixer";
      return FALSE;
   }   
   return TRUE;
// AMPMIX-devices bieten noch weiter Einstellungsmîglichkeiten Åber MCI_SET_ITEM
}

DRIVER drv_os2_mmpm2_largebuffers= // driver-description for mikmod
{
   NULL, // next: pointer to next driver (used by mikmod?)
   "OS/2 MMPM/2 MCI (large buffers)", // Name: name of driver
   "Mikmod OS/2 MMPM/2 MCI driver v0.2· (large buffers)", // Version: driver-version
   OS2_MMPM2_LARGE_IsThere, // IsPresent: test presence of hardware
   VC_SampleLoad, // SampleLoad: default-function
   VC_SampleUnload, // SampleUnload: default-function
   OS2_MMPM2_LARGE_Init, // Init: initialize hardware and soundbuffer
   OS2_MMPM2_LARGE_Exit, // Exit: close hardware and soundbuffer
   OS2_MMPM2_LARGE_PlayStart, // PlayStart: start playing
   OS2_MMPM2_LARGE_PlayStop, // PlayStop: stop playing
   OS2_MMPM2_LARGE_Update, // Update: update soundbuffer
   VC_VoiceSetVolume, // VoiceSetVolume: default-function
   VC_VoiceSetFrequency, // VoiceSetFrequency: default-function
   VC_VoiceSetPanning, // VoiceSetPanning: default-function
   VC_VoicePlay // VoicePlay: default-function
//   OS2_MMPM2_LARGE_SetAmpKnob // SetAmpKnob: do mixer-output settings
};
