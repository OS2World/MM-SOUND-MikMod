/*

Name:
WILDFILE.C

Description:
Some routines to support wildcard filename handling.. Done by MikMak

	1-3-95 : Adapted so it compiles for both borland & watcom

Portability:

MSDOS:	BC(y)	Watcom(y)	DJGPP(?)
Win95:	BC(y)
Linux:	n

(y) - yes
(n) - no (not possible or not useful)
(?) - may be possible, but not tested

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <process.h>
#include <errno.h>
#include <dos.h>
#include "wildfile.h"

static char path[_MAX_PATH];
static char drive[_MAX_DRIVE];
static char dir[_MAX_DIR];
static char fname[_MAX_FNAME];		/* <- prevent TASM clash */
static char ext[_MAX_EXT];

static struct find_t ffblk;

static char **newargv;
static int count;


char *GetFirstName(char *wildname,int attrib)
/*
	Finds the first file in a directory that corresponds to the wildcard
	name 'wildname'.

	returns:        ptr to full pathname

				or

				NULL if file couldn't be found
*/
{
	_splitpath(wildname,drive,dir,fname,ext);
	if(!_dos_findfirst(wildname,attrib,&ffblk)){
		_splitpath(ffblk.name,NULL,NULL,fname,ext);
		_makepath(path,drive,dir,fname,ext);
		return path;
	}
	return NULL;
}



char *GetNextName(void)
/*
	Finds another file in a directory that corresponds to the wildcard
	name of the GetFirstName call.

	returns:        ptr to full pathname

				or

				NULL if file couldn't be found
*/
{
	if(!_dos_findnext(&ffblk)){
		_splitpath(ffblk.name,NULL,NULL,fname,ext);
		_makepath(path,drive,dir,ffblk.name,NULL);
		return path;
	}
	return NULL;
}


static char **newargv;
static int count;


void TackOn(char *s)
{
	newargv=realloc(newargv,(count+2)*sizeof(char *));

	if(newargv==NULL){
		perror("Glob");
		exit(-1);
	}

	newargv[count++]=strdup(s);
	newargv[count]=NULL;
}



void Expand(char *wildname,int attrib)
{
	char *s;

	s=(strpbrk(wildname,"*?")==NULL) ? NULL : GetFirstName(wildname,attrib);

	if(s==NULL){

		/* wildname is not a pattern, or there's no match for
		   this pattern -> add wildname to the list */

		TackOn(wildname);
	}
	else do{

		/* add all matches to the list */
		TackOn(s);

	} while((s=GetNextName()) != NULL);
}



int fcmp(const void *a,const void *b)
{
	return(strcmp(*(char **)a,*(char **)b));
}



void MyGlob(int *argc,char **argv[],int attrib)
{
	int i;
	int *idxarr;

	newargv=NULL;
	count=1;

	idxarr=calloc(*argc+1,sizeof(int));
	newargv=calloc(2,sizeof(char **));

	if(newargv==NULL || idxarr==NULL){
		errno=ENOMEM;
		perror("Glob");
		exit(-1);
	}

	/* init newargv[0] */

	newargv[0]=(*argv)[0];

	/* Try to expand all arguments except argv[0] */

	for(i=1;i<*argc;i++){

		/* remember position old arg -> new arg */

		idxarr[i]=count;

		/* expand the wildcard argument */

		Expand((*argv)[i],attrib);
	}

	idxarr[i]=count;

	for(i=1;i<*argc;i++){
		qsort(&newargv[idxarr[i]],
			  idxarr[i+1]-idxarr[i],
			  sizeof(char *),fcmp);
	}

	/* replace the old argc and argv values by the new ones */

	*argc=count;
	*argv=newargv;

	free(idxarr);
}
