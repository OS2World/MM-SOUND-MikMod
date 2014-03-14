/*

Name:
DRV_VOX.C

Description:
Mikmod driver for output on linux voxware (/dev/dsp)

Portability:
Linux only.

*/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "mikmod.h"

static int sndfd;
static int fragmentsize;

BOOL Vox_IsThere(void)
{
	return (access("/dev/dsp",W_OK)==0);
}


BOOL Vox_Init(void)
{
	int play_precision,play_stereo,play_rate;

	if((sndfd=open("/dev/dsp",O_WRONLY))<0){
		myerr="Cannot open sounddevice";
		return 0;
	}

	play_precision = (md_mode & DMODE_16BITS) ? 16 : 8;
	play_stereo= (md_mode & DMODE_STEREO) ? 1 : 0;
	play_rate=md_mixfreq;
	
	if(ioctl(sndfd, SNDCTL_DSP_SAMPLESIZE, &play_precision) == -1 || 
	   ioctl(sndfd, SNDCTL_DSP_STEREO, &play_stereo) == -1 ||
	   ioctl(sndfd, SNDCTL_DSP_SPEED, &play_rate) == -1){
		myerr = "Device can't play sound in this format";
		close(sndfd);
		return 0;
	}

	fragmentsize=0x0004000c;
	if(ioctl(sndfd, SNDCTL_DSP_SETFRAGMENT, &fragmentsize)<0){
		myerr = "Buffer fragment failed";
		close(sndfd);
		return 0;
	}

	ioctl(sndfd, SNDCTL_DSP_GETBLKSIZE, &fragmentsize);

	printf("Fragment size is %ld\n",fragmentsize);

	if(!VC_Init()){
		close(sndfd);
		return 0;
	}
	return 1;
}


void Vox_Exit(void)
{
	VC_Exit();
	close(sndfd);
}


void Vox_PlayStart(void)
{
	VC_PlayStart();
}


void Vox_PlayStop(void)
{
	VC_PlayStop();
}


char audiobuffer[15000];

void Vox_Update(void)
{
	VC_WriteBytes(audiobuffer,fragmentsize);
	write(sndfd,audiobuffer,fragmentsize);
}


DRIVER drv_vox={
	NULL,
	"Linux Voxware",
	"MikMod Voxware Driver v1.0",
	Vox_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	Vox_Init,
	Vox_Exit,
	Vox_PlayStart,
	Vox_PlayStop,
	Vox_Update,
	VC_VoiceSetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceSetPanning,
	VC_VoicePlay
};
