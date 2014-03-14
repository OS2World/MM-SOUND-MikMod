/*

Name:
DRV_W95.C

Description:
Experimental Mikmod driver for output on Windows 95 (Win-NT even?)

Method:
Single buffer, automatic refill using a high-resolution timer callback routine.
This one is probably the best.

Portability:
Microsoft Windows 95 - Borland C 4.5

*/
#include <windows.h>
#include <stdio.h>
#include "mikmod.h"

static WAVEOUTCAPS woc;
static HWAVEOUT hWaveOut;
static LPVOID mydata;
static HGLOBAL hglobal;
static WAVEHDR WaveOutHdr;
static UINT gwID;						/* timer handle */
static UWORD last;    		/* last recorded playing position */
static char *mydma;

extern DRIVER drv_w95;

#define WIN95BUFFERSIZE 30000


BOOL W95_IsThere(void)
{
	UINT numdevs=waveOutGetNumDevs();
	return numdevs>0;
}


BOOL W95_Init(void)
{
	MMRESULT err;
	PCMWAVEFORMAT wf;

	hglobal=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE,WIN95BUFFERSIZE);
	if(hglobal==NULL){
		myerr="Globalalloc failed";
		return 0;
	}

	mydata=GlobalLock(hglobal);

	/* get audio device name and put it into the driver structure: */
	waveOutGetDevCaps(0,&woc,sizeof(WAVEOUTCAPS));
	drv_w95.Name=woc.szPname;

	if((md_mode & DMODE_STEREO) && (woc.wChannels<2) ){
		/* switch output mode to mono if device
			doesn't support stereo */
		md_mode&=~DMODE_STEREO;
	}

	if(md_mode & DMODE_16BITS){
		/* switch output mode to 8 bit if device
			doesn't support 16 bit output */
		if(!(woc.dwFormats &
			 (WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
			  WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
			  WAVE_FORMAT_4M16 | WAVE_FORMAT_4S16))){
			  md_mode&=~DMODE_16BITS;
		}
	}

	wf.wf.wFormatTag=WAVE_FORMAT_PCM;
	wf.wf.nChannels=(md_mode & DMODE_STEREO) ? 2 : 1;
	wf.wf.nSamplesPerSec=md_mixfreq;
	wf.wf.nAvgBytesPerSec=md_mixfreq;
	if(md_mode & DMODE_STEREO) wf.wf.nAvgBytesPerSec<<=1;
	if(md_mode & DMODE_16BITS) wf.wf.nAvgBytesPerSec<<=1;
	wf.wf.nBlockAlign=1;
	if(md_mode & DMODE_STEREO) wf.wf.nBlockAlign<<=1;
	if(md_mode & DMODE_16BITS) wf.wf.nBlockAlign<<=1;
	wf.wBitsPerSample=(md_mode & DMODE_16BITS) ? 16 : 8;

	err=waveOutOpen(&hWaveOut,0,(WAVEFORMAT *)&wf,NULL,NULL,0);

	if(err){
		if(err==WAVERR_BADFORMAT)
			myerr="This output format is not supported (Try another sampling rate?)";
		else if(err==MMSYSERR_ALLOCATED)
			myerr="Audio device already in use";
		else
			myerr="Can't open audio device";
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
		return 0;
	}

	if(!VC_Init()){
		waveOutClose(hWaveOut);
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
		return 0;
	}

	return 1;
}


void W95_Exit(void)
{
	while(waveOutClose(hWaveOut)==WAVERR_STILLPLAYING) Sleep(20);
	GlobalUnlock(hglobal);
	GlobalFree(hglobal);
	VC_Exit();
}


ULONG GetPos(void)
{
	MMTIME mmt;
	mmt.wType=TIME_BYTES;
	waveOutGetPosition(hWaveOut,&mmt,sizeof(MMTIME));
	return(mmt.u.cb&0xfffffff0);
}


void CALLBACK TimeProc(
	 UINT  IDEvent,	/* identifies timer event */
	 UINT  uReserved,	/* not used */
	 DWORD  dwUser,	/* application-defined instance data */
	 DWORD  dwReserved1,	/* not used */
	 DWORD  dwReserved2	/* not used */
)
{
	UWORD todo,curr;
	static volatile int timersema=0;

	/* use semaphore to prevent entering
		the mixing routines twice.. do we need this ? */

	if(++timersema==1){

		curr=GetPos()%WIN95BUFFERSIZE;

		todo=(curr>last) ? curr-last : (WIN95BUFFERSIZE-last)+curr;

		if(todo<(WIN95BUFFERSIZE/2)){

			/* only come here is the number of bytes we have to fill isn't
				too big.. this prevents lockups if the system can't handle
				the load.
			*/

			if(curr>last){
				VC_WriteBytes(&mydma[last],curr-last);
			}
			else{
				VC_WriteBytes(&mydma[last],WIN95BUFFERSIZE-last);
				VC_WriteBytes(mydma,curr);
			}
		}
		last=curr;
	}
	timersema--;
}


void W95_PlayStart(void)
{
	VC_PlayStart();
	waveOutSetVolume(0,0xffffffff);

	VC_WriteBytes(mydata,WIN95BUFFERSIZE);		/* fill audio buffer with data */

	WaveOutHdr.lpData=mydata;
	WaveOutHdr.dwBufferLength=WIN95BUFFERSIZE;
	WaveOutHdr.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP;
	WaveOutHdr.dwLoops=0xffffffff;
	WaveOutHdr.dwUser=0;
	waveOutPrepareHeader(hWaveOut,&WaveOutHdr,sizeof(WAVEHDR));
	waveOutWrite(hWaveOut,&WaveOutHdr,sizeof(WAVEHDR));
	mydma=mydata;
	last=0;

	timeBeginPeriod(20);      /* set the minimum resolution */

	/*  Set up the callback event.  The callback function
	 *  MUST be in a FIXED CODE DLL!!! -> not in Win95
	 */
	gwID = timeSetEvent(40,   				/* how often                 */
							  40,   				/* timer resolution          */
							  TimeProc,  		/* callback function         */
							  NULL,    			/* info to pass to callback  */
							  TIME_PERIODIC); /* oneshot or periodic?      */
}


void W95_PlayStop(void)
{
	/* stop the timer */
	timeKillEvent(gwID);
	timeEndPeriod(20);
	/* stop playing the wave */
	waveOutReset(hWaveOut);
	waveOutUnprepareHeader(hWaveOut,&WaveOutHdr,sizeof(WAVEHDR));
	VC_PlayStop();
}


void W95_Update(void)
{
	/* does nothing, buffers are updated in the background */
}


DRIVER drv_w95={
	NULL,
	"Win95",
	"MikMod Win95 Driver v2.0 - To boldly go..",
	W95_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	W95_Init,
	W95_Exit,
	W95_PlayStart,
	W95_PlayStop,
	W95_Update,
	VC_VoiceSetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceSetPanning,
	VC_VoicePlay
};

