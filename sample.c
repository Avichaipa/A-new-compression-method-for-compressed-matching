/***************************************************************************
*                     Sample Program
*
*   File    : sample.c
*   Purpose : Demonstrate usage of New Compression Method for Compressed Matching library
*   Author  : Avichai and Omer
*   Date    : 2016
*   Based on : Michael Dipperstein implementation Version 0.7
*			   http://michael.dipperstein.com/lzss/
*
****************************************************************************
*
* SAMPLE: Sample usage of LZSS Library
* Copyright (C) 2004, 2006, 2007, 2014 by
* Michael Dipperstein (mdipper@alumni.engr.ucsb.edu)
*
* This file is part of the lzss library.
*
* The lzss library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the
* License, or (at your option) any later version.
*
* The lzss library is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lzss.h"

#include "lzlocal.h"
/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/
typedef enum
{
	ENCODE,
	DECODE
} modes_t;



int main(int argc, char *argv[])
{
	FILE *org;             /* pointer to  input file */
	FILE *comp;            
	FILE *compWithSlide;   
	FILE *compProject;
	FILE *compBackToLzss;
	FILE *decomp;            
	FILE *print;
	FILE *slideP;
	unsigned int i;
	printf("WINDOW_SIZE: %d  MAX_CODED: %d \n",WINDOW_SIZE,MAX_CODED);
	
	/* initialize data */
	org = NULL;
	comp = NULL;
	decomp = NULL;
/***************************************************************************
*                                Encoding
***************************************************************************/

	
	org = fopen("org.txt", "rb"); 
	if (org == NULL)
	{
		perror("Opening input file");
	}

	
	comp = fopen("comp.txt", "wb"); 
	
	if (comp == NULL)
	{
		perror("Opening output file");
	}
	printf("Encoding.....\n");
	EncodeLZSS(org, comp); 
	printf("\n");
	printf("\n");
	/* remember to close files */
	fclose(org);
	fclose(comp);

/***************************************************************************
*                                Add Slide
***************************************************************************/

	
	comp = fopen("comp.txt", "rb"); 
	
	if (comp == NULL)
	{
		perror("Opening input file");
	}


	compWithSlide = fopen("compWithSlide", "wb"); 

	if (compWithSlide == NULL)
	{
		perror("Opening output file");
	}
	printf("\n");
	printf("Add Slide.....\n");
	AddSlide(comp, compWithSlide); 
	printf("\n");
	printf("\n");
	

	fclose(comp);
	fclose(compWithSlide);


	
/***************************************************************************
*                         Cast lzss(with slide) to Project format
***************************************************************************/

	
	 
	compWithSlide = fopen("compWithSlide", "rb"); 
	
	if (compWithSlide == NULL)
	{
		perror("Opening input file");
	}

	
	compProject = fopen("compProject", "wb"); 
	
	if (compProject == NULL)
	{
		perror("Opening output file");
	}
	printf("\n");
	printf("Cast.....\n");
	CastEncodeLZSS(compWithSlide,compProject); 
	printf("\n");
	printf("\n");
		
	fclose(compWithSlide);
	fclose(compProject);

	
/***************************************************************************
*                         Cast back to lzss
***************************************************************************/
	
	
	 
	compProject = fopen("compProject", "rb"); 
	
	if (compProject == NULL)
	{
		perror("Opening input file");
	}

	
	compBackToLzss = fopen("compBackToLzss", "wb"); 
	
	if (compBackToLzss == NULL)
	{
		perror("Opening output file");
	}
	printf("\n");
	printf("Cast Back To Lzss.....\n");
	CastBack(compProject,compBackToLzss); 
	printf("\n");
	printf("\n");
		
	fclose(compProject);
	fclose(compBackToLzss);

	

/***************************************************************************
*                                Decoding
***************************************************************************/
	
	compBackToLzss = fopen("compBackToLzss", "rb"); 
	
	if (compBackToLzss == NULL)
	{
		perror("Opening input file");
	}

	
	decomp = fopen("decomp.txt", "wb"); 
	
	if (decomp == NULL)
	{
		perror("Opening output file");
	}
	printf("\n");
	printf("Decoding.....\n");
	DecodeLZSS(compBackToLzss, decomp); 
	
	printf("\n");
	printf("\n");
	
	fclose(compBackToLzss);
	fclose(decomp);

	

/***************************************************************************
*                                Diff
***************************************************************************/
	
	
	org = fopen("org.txt", "rb"); 
	
	if (org == NULL)
	{
		perror("Opening input file");
	}

	
	decomp = fopen("decomp.txt", "rb"); 
	
	if (decomp == NULL)
	{
		perror("Opening output file");
	}
	printf("Checking.....\n");
	diff(org, decomp); 

	
	fclose(comp);
	fclose(decomp);

	printf("\n");
	

	return 0;
}


