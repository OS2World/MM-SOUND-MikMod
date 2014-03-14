/*

Name:
MIKCVT.C

Description:
Program to convert any module into a .UNI module

Portability:
All systems - all compilers

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wildfile.h"
#include "mikmod.h"

/*
	Declare external loaders:
 */

extern LOADER load_mtm, load_s3m, load_ult, load_mod, load_uni, load_xm, load_stm, load_m15;


FILE *fpi, *fpo;


UWORD numsamples;
ULONG samplepos[128];
ULONG samplesize[128];
UBYTE buf[8000];


BOOL CopyData(FILE * fpi, FILE * fpo, ULONG len)
{
	 ULONG todo;

	 while (len) {
	todo = (len > 8000) ? 8000 : len;
	if (!fread(buf, todo, 1, fpi))
		 return 0;
	fwrite(buf, todo, 1, fpo);
	len -= todo;
	 }
	 return 1;
}


/***************************************************************************
****************************************************************************
***************************************************************************/


BOOL TrkCmp(UBYTE * t1, UBYTE * t2)
{
	UWORD l1, l2;

	if (t1 == NULL || t2 == NULL)
	return 0;

	l1 = TrkLen(t1);
	l2 = TrkLen(t2);

	if (l1 != l2)
	return 0;

	 return (MyCmp(t1, t2, l1));
}



void ReplaceTrack(UNIMOD * mf, int t1, int t2)
{
	 int t;

	 for (t = 0; t < mf->numpat * mf->numchn; t++) {
	if (mf->patterns[t] == t1)
		 mf->patterns[t] = t2;
	 }
}



void Optimize(UNIMOD * mf)
/*
	Optimizes the number of tracks in a modfile by removing tracks with
	identical contents.
 */
{
	int t, u, done = 0, same, newcnt = 0;
	 UBYTE *ta;
	 UBYTE **newtrk;

	 if (!(newtrk = malloc(mf->numtrk * sizeof(UBYTE *))))
	return;

	 for (t = 0; t < mf->numtrk; t++) {

	/* ta is track to examine */

	ta = mf->tracks[t];

	/* does ta look familiar ? */

	for (same = u = 0; u < newcnt; u++) {
		 if (TrkCmp(ta, newtrk[u])) {
		same = 1;
		break;
		 }
	}

	if (same) {
		 ReplaceTrack(mf, t, u);
		 done++;
	} else {
		 ReplaceTrack(mf, t, newcnt);
		newtrk[newcnt++] = ta;
	}

	printf("\rOptimizing: %d\%", (t * 100L) / mf->numtrk);
	}

	printf("\rOptimized : %d tracks\n", done);

	free(mf->tracks);
	mf->tracks = newtrk;
	mf->numtrk = newcnt;
}

/***************************************************************************
****************************************************************************
***************************************************************************/


SWORD MD_SampleLoad(FILE * fp, ULONG length, ULONG loopstart, ULONG loopend, UWORD flags)
{
	 /* record position of sample */

	samplepos[numsamples] = ftell(fp);

	 /* determine it's bytesize */

	 if (flags & SF_16BITS)
	length <<= 1;

	 /* record bytesize and skip the sample */

	 samplesize[numsamples++] = length;
	 fseek(fp, length, SEEK_CUR);
	 return 1;
}


void MD_SampleUnLoad(SWORD handle)
{
}


void StrWrite(char *s)
/*
	Writes a null-terminated string as a pascal string to fpo.
 */
{
	 UWORD len;

	 len = (s != NULL) ? strlen(s) : 0;
	 _mm_write_I_UWORD(len, fpo);
	 if (len)
	fwrite(s, len, 1, fpo);
}


void TrkWrite(UBYTE * t)
/*
	Writes a track to fpo.
 */
{
	 UWORD len;
	 if (t == NULL)
	printf("NULL track");
	 len = TrkLen(t);
	_mm_write_I_UWORD(len, fpo);
	 fwrite(t, len, 1, fpo);
}



char *stripname(char *path, char *ext)
/*
	Strips the filename from a path, and replaces or adds
	a new extension to it.
*/
{
	char *n, *m;
	static char newname[256];

	/* extract the filename from the path */

#ifdef unix
	n = ((n = strrchr(path, '/')) == NULL) ? path : n + 1;
#else
	n = ((n = strrchr(path, '\\')) == NULL) ? path : n + 1;
	if(m = strrchr(n, ':')) n=m+1;
#endif

	/* copy the filename into 'newname' */
	strncpy(newname,n,255);
	newname[255]=0;

	/* remove the extension */
	if (n = strrchr(newname, '.'))
	*n = 0;

	/* and tack on the new extension */
	return strcat(newname, ext);
}


int main(int argc, char *argv[])
{
	int t, v, w;
	char *outname;

	puts(mikbanner);

	/* Expand wildcards on commandline (NoT on unix systems please): */

#ifndef unix
	MyGlob(&argc, &argv, 0);
#endif

	/*
		Register the loaders we want to use..
	*/

	ML_RegisterLoader(&load_m15);
	ML_RegisterLoader(&load_mod);
	ML_RegisterLoader(&load_mtm);
	ML_RegisterLoader(&load_s3m);
	ML_RegisterLoader(&load_stm);
	ML_RegisterLoader(&load_ult);
	ML_RegisterLoader(&load_uni);
	ML_RegisterLoader(&load_xm);

	if (argc < 2) {

		/* display a usage message */

		puts("Usage: MIKCVT <fletch.mod> ... ");
		puts("Converts your modules to .UNI modules\n");
		exit(-1);
	}

	for (t = 1; t < argc; t++) {

		UNIMOD *mf;

		printf("In file : %s\n", argv[t]);

		numsamples = 0;

		if ((fpi = fopen(argv[t], "rb")) == NULL) {
			printf("MikCvt Error: Error opening input file\n");
			break;
		}
		outname = stripname(argv[t], ".uni");

		printf("Out file: %s\n", outname);

		if ((fpo = fopen(outname, "wb")) == NULL) {
			printf("MikCvt Error: Error opening output file\n");
			break;
		}
		mf = ML_LoadFP(fpi);

		/*      didn't work -> exit with error */

		if (mf == NULL) {
			printf("MikCvt Error: %s\n", myerr);
			fclose(fpi);
			break;
		}
		printf("Songname: %s\n"
				"Modtype : %s\n",
				mf->songname,
				mf->modtype);

		/* Optimize the tracks */

		Optimize(mf);

		/* Write UNI header */

		fwrite("UN05", 4, 1, fpo);
		_mm_write_UBYTE(mf->numchn, fpo);
		_mm_write_I_UWORD(mf->numpos, fpo);
		_mm_write_I_UWORD(mf->reppos, fpo);
		_mm_write_I_UWORD(mf->numpat, fpo);
		_mm_write_I_UWORD(mf->numtrk, fpo);
		_mm_write_I_UWORD(mf->numins, fpo);
		_mm_write_UBYTE(mf->initspeed, fpo);
		_mm_write_UBYTE(mf->inittempo, fpo);
		_mm_write_UBYTES(mf->positions, 256, fpo);
		_mm_write_UBYTES(mf->panning, 32, fpo);
		_mm_write_UBYTE(mf->flags, fpo);

		StrWrite(mf->songname);
		StrWrite(mf->modtype);
		StrWrite(mf->comment);

		/* Write instruments */

		for (v = 0; v < mf->numins; v++) {

			INSTRUMENT *i = &mf->instruments[v];

			_mm_write_UBYTE(i->numsmp, fpo);
			_mm_write_UBYTES(i->samplenumber, 96, fpo);

			_mm_write_UBYTE(i->volflg, fpo);
			_mm_write_UBYTE(i->volpts, fpo);
			_mm_write_UBYTE(i->volsus, fpo);
			_mm_write_UBYTE(i->volbeg, fpo);
			_mm_write_UBYTE(i->volend, fpo);

			for (w = 0; w < 12; w++) {
				_mm_write_I_SWORD(i->volenv[w].pos, fpo);
				_mm_write_I_SWORD(i->volenv[w].val, fpo);
			}

			_mm_write_UBYTE(i->panflg, fpo);
			_mm_write_UBYTE(i->panpts, fpo);
			_mm_write_UBYTE(i->pansus, fpo);
			_mm_write_UBYTE(i->panbeg, fpo);
			_mm_write_UBYTE(i->panend, fpo);

			for (w = 0; w < 12; w++) {
				_mm_write_I_SWORD(i->panenv[w].pos, fpo);
				_mm_write_I_SWORD(i->panenv[w].val, fpo);
			}

			_mm_write_UBYTE(i->vibtype, fpo);
			_mm_write_UBYTE(i->vibsweep, fpo);
			_mm_write_UBYTE(i->vibdepth, fpo);
			_mm_write_UBYTE(i->vibrate, fpo);
			_mm_write_I_UWORD(i->volfade, fpo);

			StrWrite(i->insname);

			for (w = 0; w < i->numsmp; w++) {
				SAMPLE *s = &i->samples[w];

				_mm_write_I_UWORD(s->c2spd, fpo);
				_mm_write_SBYTE(s->transpose, fpo);
				_mm_write_UBYTE(s->volume, fpo);
				_mm_write_UBYTE(s->panning, fpo);
				_mm_write_I_ULONG(s->length, fpo);
				_mm_write_I_ULONG(s->loopstart, fpo);
				_mm_write_I_ULONG(s->loopend, fpo);
				_mm_write_I_UWORD(s->flags, fpo);
				StrWrite(s->samplename);
			}
		}

		/* Write patterns */

		_mm_write_I_UWORDS(mf->pattrows, mf->numpat, fpo);
		_mm_write_I_UWORDS(mf->patterns, mf->numpat * mf->numchn, fpo);

		/* Write tracks */

		for (v = 0; v < mf->numtrk; v++) {
			TrkWrite(mf->tracks[v]);
		}

		printf("Writing samples.. ");

		/* Write sample-data */

		for (v = 0; v < numsamples; v++) {
			 fseek(fpi, samplepos[v], SEEK_SET);
			 CopyData(fpi, fpo, samplesize[v]);
		}

		puts("Done.");

		/* and clean up */

		fclose(fpo);
		fclose(fpi);
		ML_Free(mf);
	}
	return 0;
}

