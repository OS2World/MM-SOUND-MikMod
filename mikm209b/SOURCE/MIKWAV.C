/*

Name:
MIKWAV.C

Description:
WAV playing example source.

Portability:

MSDOS:	BC(y)	Watcom(y)	DJGPP(?)
Win95:	BC(n)
Linux:	n

(y) - yes
(n) - no (not possible or not useful)
(?) - may be possible, but not tested

*/
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <dos.h>
#include "mikmod.h"


void InitWasOkay(void)
{
	int t;
	SAMPLE *s1,*s2;

	if((s1=MW_LoadWavFN("b1.wav"))==NULL){
		printf("Wavload error: %s.\n",myerr);
		return;
	}

	if((s2=MW_LoadWavFN("b2.wav"))==NULL){
		printf("Wavload error: %s.\n",myerr);
		MW_FreeWav(s1);
		return;
	}

	md_numchn=4;

	MD_PlayStart();

	puts("Press key 1,2,3,4 to play the wav files, press 'q' to quit.");

	while(1){

		t=kbhit() ? getch() : 0;

		if(t=='q') break;

		if(t=='1'){
			MD_VoiceSetVolume(0,64);
			MD_VoiceSetPanning(0,0);
			MD_VoiceSetFrequency(0,12000);
			MD_VoicePlay(0,s1->handle,0,s1->length,0,0,s1->flags);
		}

		if(t=='2'){
			MD_VoiceSetVolume(1,64);
			MD_VoiceSetPanning(1,64);
			MD_VoiceSetFrequency(1,13000);
			MD_VoicePlay(1,s1->handle,0,s1->length,0,0,s1->flags);
		}

		if(t=='3'){
			MD_VoiceSetVolume(2,64);
			MD_VoiceSetPanning(2,128);
			MD_VoiceSetFrequency(2,10000);
			MD_VoicePlay(2,s2->handle,0,s2->length,0,0,s2->flags);
		}

		if(t=='4'){
			MD_VoiceSetVolume(3,64);
			MD_VoiceSetPanning(3,200);
			MD_VoiceSetFrequency(3,12000);
			MD_VoicePlay(3,s2->handle,0,s2->length,0,0,s2->flags);
		}

		delay(10);
		MD_Update();
	}

	MD_PlayStop();
	MW_FreeWav(s1);
	MW_FreeWav(s2);
}


int main(int argc,char *argv[])
{
	puts(mikbanner);

	/*
		Initialize soundcard parameters.. you _have_ to do this
		before calling MD_Init(), and it's illegal to change them
		after you've called MD_Init()
	*/

	md_mixfreq              =44100;                         /* standard mixing freq */
	md_dmabufsize   		=8000;                          /* standard dma buf size */
	md_mode                 =DMODE_16BITS|DMODE_STEREO;             /* standard mixing mode */
	md_device               =0;                                     /* standard device: autodetect */

	/*
		Register the drivers we want to use:
	*/

	MD_RegisterDriver(&drv_sb);
	MD_RegisterDriver(&drv_gus);

	/*      initialize soundcard */

	if(!MD_Init()){
		printf("Driver error: %s.\n",myerr);
		return 0;
	}

	printf("Using %s for %d bit %s sound at %u Hz\n\n",
			md_driver->Name,
			(md_mode&DMODE_16BITS) ? 16:8,
			(md_mode&DMODE_STEREO) ? "stereo":"mono",
			md_mixfreq);

	/* call main program */

	InitWasOkay();

	/* and clean up */

	MD_Exit();
	return 0;
}

