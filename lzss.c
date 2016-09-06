/***************************************************************************
*  A New Compression Method for Compressed Matching Encoding and Decoding
*
*   File    : lzss.c
*   Purpose : Header for encode and decode implementation
*	Author  : Avichai and Omer
*   Date    : 2016
*   Based on : Michael Dipperstein implementation Version 0.7
*			   http://michael.dipperstein.com/lzss/
*
*
*
****************************************************************************
*
* LZss: An ANSI C LZSS Encoding/Decoding Routines
* Copyright (C) 2003 - 2007, 2014 by
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
#include <string.h>
#include <errno.h>
#include "lzlocal.h"
#include "bitfile.h"

//  #define DEBUG

#ifdef DEBUG
# define DEBUG_PRINT() toPrintOutput = 1
#else
# define DEBUG_PRINT() toPrintOutput = 0
#endif

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/

/***************************************************************************
*                                CONSTANTS
***************************************************************************/

/***************************************************************************
*                            GLOBAL VARIABLES
***************************************************************************/
/* cyclic buffer sliding window of already read characters */
unsigned char slidingWindow[WINDOW_SIZE];
unsigned char uncodedLookahead[MAX_CODED];

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/****************************************************************************
*   Function   : EncodeLZSS
*   Description: This function reads an input file and writes an encoded
*				 output file according to the traditional LZSS algorithm using Brute force
*				 matching algorithm. The function doesn’t accept “self-references”.
*   Parameters : fpIn - pointer to the open binary file to encode
*                fpOut - pointer to the open binary file to write encoded
*                       output
*   Effects    : fpIn is encoded and written to fpOut.  Neither file is
*                closed after exit.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int EncodeLZSS(FILE *fpIn, FILE *fpOut) 
{

	bit_file_t *bfpOut;
	encoded_string_t matchData;
	int c;
	unsigned int i,toPrintOutput,len;
	

	/* head of sliding window and lookahead */
	unsigned int windowHead, uncodedHead;

	/* validate arguments */
	if ((NULL == fpIn) || (NULL == fpOut))
	{
		errno = ENOENT;
		return -1;
	}

	/* convert output file to bitfile */
	bfpOut = MakeBitFile(fpOut, BF_WRITE);

	if (NULL == bfpOut)
	{
		perror("Making Output File a BitFile");
		return -1;
	}

	windowHead = 0;
	uncodedHead = 0;
	DEBUG_PRINT();
	/************************************************************************
	* Fill the sliding window buffer with some known vales.  DecodeLZSS must
	* use the same values.  If common characters are used, there's an
	* increased chance of matching to the earlier strings.
	************************************************************************/
	memset(slidingWindow, '~', WINDOW_SIZE * sizeof(unsigned char)); // space -> metilda 

	/************************************************************************
	* Copy MAX_CODED bytes from the input file into the uncoded lookahead
	* buffer.
	************************************************************************/
	for (len = 0; len < MAX_CODED && (c = getc(fpIn)) != EOF; len++)
	{
		uncodedLookahead[len] = c;
	}

	if (0 == len)
	{
		return 0;   /* inFile was empty */
	}

	/* Look for matching string in sliding window */
	i = InitializeSearchStructures();

	if (0 != i)
	{
		return i;       /* InitializeSearchStructures returned an error */
	}

	matchData = FindMatch(windowHead, uncodedHead);

	/* now encoded the rest of the file until an EOF is read */
	while (len > 0)
	{
		if (matchData.length > len)
		{
			/* garbage beyond last data happened to extend match length */
			matchData.length = len;
		}

		if (matchData.length <= MAX_UNCODED)
		{
			/* not long enough match.  write uncoded flag and character */
			BitFilePutBit(UNCODED, bfpOut);
			BitFilePutChar(uncodedLookahead[uncodedHead], bfpOut);		
			matchData.length = 1;   /* set to 1 for 1 byte uncoded */
			if(toPrintOutput == 1)
				printf("%c,",uncodedLookahead[uncodedHead]);
		}
		else
		{

			/* change the offset format */ 
			if(windowHead > matchData.offset)
			{
				matchData.offset = windowHead - matchData.offset;// Wrap((windowHead - matchData.offset),WINDOW_SIZE);
			}
			else
			{
				matchData.offset = windowHead + WINDOW_SIZE - matchData.offset  ;
			}



			/* match length > MAX_UNCODED.  Encode as offset and length. */
			BitFilePutBit(ENCODED, bfpOut);
			BitFilePutBitsNum(bfpOut, &matchData.offset, OFFSET_BITS, sizeof(unsigned int));
			BitFilePutBitsNum(bfpOut, &matchData.length, LENGTH_BITS, sizeof(unsigned int));

			if(toPrintOutput == 1)
			{
				printf("(%d,",matchData.offset);
				printf("%d),",matchData.length);             
			}
			if(matchData.length > matchData.offset)
				printf("length is %d bigger than offset %d\n",matchData.length,matchData.offset);
		}

		/********************************************************************
		* Replace the matchData.length worth of bytes we've matched in the
		* sliding window with new bytes from the input file.
		********************************************************************/
		i = 0;
		while ((i < matchData.length) && ((c = getc(fpIn)) != EOF))
		{
			/* add old byte into sliding window and new into lookahead */
			ReplaceChar(windowHead, uncodedLookahead[uncodedHead]);
			uncodedLookahead[uncodedHead] = c;
			windowHead = Wrap((windowHead + 1), WINDOW_SIZE);
			uncodedHead = Wrap((uncodedHead + 1), MAX_CODED);
			i++;
		}

		/* handle case where we hit EOF before filling lookahead */
		while (i < matchData.length)
		{
			ReplaceChar(windowHead, uncodedLookahead[uncodedHead]);
			/* nothing to add to lookahead here */
			windowHead = Wrap((windowHead + 1), WINDOW_SIZE);
			uncodedHead = Wrap((uncodedHead + 1), MAX_CODED);
			len--;
			i++;
		}

		/* find match for the remaining characters */
		matchData = FindMatch(windowHead, uncodedHead);
	}

	/* we've encoded everything, free bitfile structure */
	BitFileToFILE(bfpOut);

	return 0;
}


/****************************************************************************
*   Function   : DecodeLZSS
*   Description: This function decodes a LZSS encoded file.
*				 The function replaces every pointer with the
*				 substring the pointer points to.
*   Parameters : fpIn - pointer to the open binary file to decode
*                fpOut - pointer to the open binary file to write decoded
*                       output
*   Effects    : fpIn is decoded and written to fpOut.  Neither file is
*                closed after exit.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int DecodeLZSS(FILE *fpIn, FILE *fpOut)
{
	bit_file_t *bfpIn;
	int c;
	unsigned int i, nextChar,toPrintOutput;
	encoded_string_t code;              

	/* use stdin if no input file */
	if ((NULL == fpIn) || (NULL == fpOut))
	{
		errno = ENOENT;
		return -1;
	}

	/* convert input file to bitfile */
	bfpIn = MakeBitFile(fpIn, BF_READ);

	if (NULL == bfpIn)
	{
		perror("Making Input File a BitFile");
		return -1;
	}
	
	DEBUG_PRINT();

	/************************************************************************
	* Fill the sliding window buffer with some known vales.  EncodeLZSS must
	* use the same values.  If common characters are used, there's an
	* increased chance of matching to the earlier strings.
	************************************************************************/
	memset(slidingWindow, '~', WINDOW_SIZE * sizeof(unsigned char)); // space -> metilda 

	nextChar = 0;

	while (1)
	{
		if ((c = BitFileGetBit(bfpIn)) == EOF)
		{
			/* we hit the EOF */
			break;
		}

		if (c == UNCODED)
		{
			/* uncoded character */
			if ((c = BitFileGetChar(bfpIn)) == EOF)
			{
				break;
			}

			/* write out byte and put it in sliding window */
			putc(c, fpOut);
			slidingWindow[nextChar] = c;
			nextChar = Wrap((nextChar + 1), WINDOW_SIZE);
			if(toPrintOutput == 1)
				printf("%c",c);
		}
		else
		{
			/* offset and length */
			code.offset = 0;
			code.length = 0;

			if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS,
				sizeof(unsigned int))) == EOF)
			{
				break;
			}

			if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS,
				sizeof(unsigned int))) == EOF)
			{
				break;
			}
			

			/* change the offset format */ 
			if(nextChar > code.offset)
			{
				code.offset = nextChar - code.offset;//Wrap((nextChar - code.offset),WINDOW_SIZE);
			}
			else
			{
				code.offset = nextChar + WINDOW_SIZE - code.offset  ;
			}

			/****************************************************************
			* Write out decoded string to file and lookahead.  It would be
			* nice to write to the sliding window instead of the lookahead,
			* but we could end up overwriting the matching string with the
			* new string if abs(offset - next char) < match length.
			****************************************************************/
			for (i = 0; i < code.length; i++)
			{
				c = slidingWindow[Wrap((code.offset + i), WINDOW_SIZE)];
				putc(c, fpOut);
				uncodedLookahead[i] = c;
				if(toPrintOutput == 1)
					printf("%c",c);
			}

			/* write out decoded string to sliding window */
			for (i = 0; i < code.length; i++)
			{
				slidingWindow[Wrap((nextChar + i), WINDOW_SIZE)] =
					uncodedLookahead[i];
			}

			nextChar = Wrap((nextChar + code.length), WINDOW_SIZE);
		}
	}

	/* we've decoded everything, free bitfile structure */
	BitFileToFILE(bfpIn);

	return 0;
}

/****************************************************************************
*   Function   : diff
*   Description: The function gets two files, and checks whether the files are the same.
*   Parameters : fpIn1 - pointer to first file
*                fpIn2 - pointer to second file
*   Effects    : print to screen the result
*   Returned   : void
****************************************************************************/
void diff(FILE *fpIn1, FILE *fpIn2){
	unsigned int C1 , C2;
	unsigned int flag=0;

	while((C1 = getc(fpIn1)) != EOF){
		C2 = getc(fpIn2);
		if(C1 != C2){
			flag = 1;
			break;
		}
	}
	C2 = getc(fpIn2);
	if(C1 != C2){
		flag = 1;
	}
	if(flag == 0){
		printf("The files are the same\n");
	}
	else{
		printf("The files are not the same !!\n");
	}

}

/****************************************************************************
*   Function   : AddSlide
*   Description: This function gets a LZSS compressed file and adapts it to include a slide parameter in every pointer. 
*   Parameters : fpIn - pointer to the LZSS encoded file
*                fpOut - pointer to the open binary file to write encoded
*                       output with slide
*   Effects    : fpIn get slide parameter for every poiner and written to fpOut.  Neither file is
*                closed after exit.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int AddSlide(FILE *fpIn, FILE *fpOut)// remove pointer with length = 0
{
	bit_file_t *bfpIn;
	bit_file_t *bfpOut;
	unsigned int i,j,slide,tempIndex,toPrintOutput;
	int c,BufferIndex;
	item Buffer[BUFFER_SIZE];
	encoded_string_t code;
	int temp;

	DEBUG_PRINT();
	/* convert input file to bitfile */
	bfpIn = MakeBitFile(fpIn, BF_READ);

	if (NULL == bfpIn)
	{
		perror("Making Input File a BitFile");
		return -1;
	}

	/* convert output file to bitfile */
	bfpOut = MakeBitFile(fpOut, BF_WRITE);

	if (NULL == bfpOut)
	{
		perror("Making Output File a BitFile");
		return -1;
	}

	/************************************************************************
	* Initialize Buffer and Text arrays  
	************************************************************************/
	for(i = 0; i < BUFFER_SIZE; i++)
	{
		Buffer[i].ch = 0;
		Buffer[i].Encoded = -1;
	}
	BufferIndex = 0;
	/************************************************************************
	* pass over the whole file and insert slide. *  
	************************************************************************/

	while((c = BitFileGetBit(bfpIn)) != EOF)
	{
		if( c == UNCODED)
		{
			if ((c = BitFileGetChar(bfpIn)) == EOF)
			{
				break;
			}
			Buffer[BufferIndex].Encoded = UNCODED;
			BitFilePutBit(UNCODED, bfpOut);
			BitFilePutChar(c, bfpOut);
			BufferIndex = Wrap ((BufferIndex + 1) , BUFFER_SIZE);
			if(toPrintOutput == 1)
				printf("%c,",c);
		}

		else // c== CODED
		{
			/* offset and length */
			code.offset = 0;
			code.length = 0;
			if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
			{
				break;
			}

			if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
			{
				break;
			}
			
			//find slide
			if(code.length > 0 )
			{
				slide = 0;
				temp = BufferIndex - code.offset + code.length - 1;
				tempIndex = Wrap ((temp) , BUFFER_SIZE); // give the index of the last char.
				while(Buffer[tempIndex].Encoded == ENCODED && slide < (SLIDE_SIZE - 1))
				{
					slide++;
					temp --;
					tempIndex = Wrap ((temp) , BUFFER_SIZE); // give the index of the first char.
				}
				//update Buffer
				for(j = 0; j < code.length; j++)//find how many chars are ENCODED
				{
					Buffer[BufferIndex].Encoded = ENCODED;
					BufferIndex = Wrap ((BufferIndex + 1) , BUFFER_SIZE);
				}
				//write to file
				if(slide == 0)
				{
					BitFilePutBit(ENCODED, bfpOut);
					BitFilePutBit(PAIR, bfpOut);
					BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
				}
				else
				{
					BitFilePutBit(ENCODED, bfpOut);
					BitFilePutBit(TRIPLE, bfpOut);
					BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &slide, SLIDE_BITS, sizeof(unsigned int));
				}

				if(toPrintOutput == 1)
				{
					printf("(%d,",code.offset); 
					printf("%d,",code.length); 
					printf("%d),",slide); 
				}
			}
		}
	}

	/* we've encoded everything, free bitfile structure */
	BitFileToFILE(bfpIn);
	BitFileToFILE(bfpOut);

	return 0;

}

/****************************************************************************
*   Function   : CastEncodeLZSS
*   Description: This function gets a LZSS compressed file with a SLIDE parameter in all pointers, and adjusts it according to project format v2. 
*   Parameters : fpIn - pointer to the LZSS encoded file with SLIDE parameter
*                fpOut - pointer to the open binary file to write encoded
*                       output according to project format. 
*   Effects    : fpIn moves every pointer to the right place according to the OFFSET and SLIDE parameters and written to fpOut.
*                Neither file is closed after exit.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int CastEncodeLZSS(FILE *fpIn, FILE *fpOut)
{
	bit_file_t *bfpIn;
	bit_file_t *bfpOut;
	int c;
	unsigned int i,j,distance,temp_index,toPrintOutput;
	encoded_string_t code;              
	encoded Buffer[WINDOW_SIZE];
	unsigned int head ,tail,bool_EOF,len;
	
	/* use stdin if no input file */
	if ((NULL == fpIn) || (NULL == fpOut))
	{
		errno = ENOENT;
		return -1;
	}

	/* convert input file to bitfile */
	bfpIn = MakeBitFile(fpIn, BF_READ);

	if (NULL == bfpIn)
	{
		perror("Making Input File a BitFile");
		return -1;
	}

	/* convert output file to bitfile */
	bfpOut = MakeBitFile(fpOut, BF_WRITE);

	if (NULL == bfpOut)
	{
		perror("Making Output File a BitFile");
		return -1;
	}
	DEBUG_PRINT();
	/************************************************************************
	* Initialize Buffer and Text arrays  
	************************************************************************/
	for(i = 0; i < WINDOW_SIZE; i++)
	{
		Buffer[i].ch = 0;
		Buffer[i].length = 0;
		Buffer[i].offset = 0;
		Buffer[i].slide = 0;
		Buffer[i].bool_writed = 1; /// change to 1 !!!!!!
	}

	head = 0;
	tail = 0;
	bool_EOF = 0;

	/************************************************************************
	*					Fill the Buffer array  
	************************************************************************/
	for (len = 0; len < WINDOW_SIZE && (c = BitFileGetBit(bfpIn)) != EOF; len++)
	{
		if( c == UNCODED)
		{
			if ((c = BitFileGetChar(bfpIn)) == EOF)
			{
				break;
			}
			Buffer[tail].ch = c;
			Buffer[tail].length = 1;
			Buffer[tail].bool_writed = 0;
		}
		else
		{
			if ((c = BitFileGetBit(bfpIn)) == EOF)
			{
				break;
			}
			code.offset = 0;
			code.length = 0;
			code.slide = 0;
			if (c == PAIR)
			{
				if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
				{
					c = EOF;
					break;
				}

				if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}

			}
			else
			{
				if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
				{
					c = EOF;
					break;
				}

				if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}
				if ((BitFileGetBitsNum(bfpIn, &code.slide, SLIDE_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}
			}

			Buffer[tail].offset = code.offset;
			Buffer[tail].length = code.length;
			Buffer[tail].slide  = code.slide;
			Buffer[tail].bool_writed = 0;
		}
		tail = Wrap((tail + 1), WINDOW_SIZE);
	}

	/************************************************************************
	*					while there is charcters on file  
	************************************************************************/
	if(len == WINDOW_SIZE)// buffer full
	{
		while(1)
		{

			/************************************************************************
			* Part A - check if exist pointer to BufferIndex  
			************************************************************************/
			//A.1 check if exist pointer to BufferIndex

			temp_index = head; 
			
			distance = 0;
			
			for(j = 0; j < len; j++)
			{
				

				if(Buffer[temp_index].bool_writed == 0 && 
					Buffer[temp_index].length > 1 &&
					distance == (Buffer[temp_index].offset - Buffer[temp_index].length + Buffer[temp_index].slide) ) 
				{
					// and insert the new pointer
					code.offset = Buffer[temp_index].offset - Buffer[temp_index].length; 
					code.length = Buffer[temp_index].length;
					code.slide  = Buffer[temp_index].slide;
					if(code.slide == 0)
					{
						BitFilePutBit(ENCODED, bfpOut);
						BitFilePutBit(PAIR, bfpOut);
						BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
						BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
					}
					else
					{
						BitFilePutBit(ENCODED, bfpOut);
						BitFilePutBit(TRIPLE, bfpOut);
						BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
						BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
						BitFilePutBitsNum(bfpOut, &code.slide, SLIDE_BITS, sizeof(unsigned int));

					}

					if(toPrintOutput == 1)
					{
						printf("(%d,",code.offset);
						printf("%d,",code.length);
						printf("%d),",code.slide);             
					}
					distance+= Buffer[temp_index].length;
					Buffer[temp_index].bool_writed = 1;
				}
				else
				{
					distance+= Buffer[temp_index].length;
				}
				temp_index = Wrap((temp_index + 1), WINDOW_SIZE);
			}

			/************************************************************************
			* Part B - write the first item (if it is not have been alredy writed
			************************************************************************/

			if(Buffer[head].length == 1) // if char
			{
				BitFilePutBit(UNCODED, bfpOut);
				BitFilePutChar(Buffer[head].ch, bfpOut);

				if(toPrintOutput == 1)
					printf("%c,",Buffer[head].ch);
			}

			else if(Buffer[head].bool_writed == 0 ) // pointer that we didnt write yet
			{
				code.offset = Buffer[head].offset - Buffer[head].length; 
				code.length = Buffer[head].length;
				code.slide  = Buffer[head].slide;
				if(code.slide == 0)
				{
					BitFilePutBit(ENCODED, bfpOut);
					BitFilePutBit(PAIR, bfpOut);
					BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
				}
				else
				{
					BitFilePutBit(ENCODED, bfpOut);
					BitFilePutBit(TRIPLE, bfpOut);
					BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.slide, SLIDE_BITS, sizeof(unsigned int));

				}

				if(toPrintOutput == 1)
				{
					printf("Ww:(%d,",code.offset);
					printf("%d,",code.length);
					printf("%d),",code.slide);             
				}
			}

			head = Wrap((head + 1), WINDOW_SIZE);
			len--;



			/************************************************************************
			* Part C - replace item, insert the next item    
			************************************************************************/
			c = BitFileGetBit(bfpIn);
			if( c == UNCODED)
			{
				if ((c = BitFileGetChar(bfpIn)) == EOF)
				{
					break;
				}
				Buffer[tail].ch = c;
				Buffer[tail].bool_writed = 0;
				Buffer[tail].offset = 0;
				Buffer[tail].length = 1;
				Buffer[tail].slide  = 0;
			}
			else
			{

				code.offset = 0;
				code.length = 0;
				code.slide = 0;
				if ((c = BitFileGetBit(bfpIn)) == EOF)
				{
					break;
				}
				if (c == PAIR)
				{
					if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
					{
						break;
					}

					if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
					{
						break;
					}

				}
				else
				{
					if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
					{
						break;
					}

					if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
					{
						break;
					}
					if ((BitFileGetBitsNum(bfpIn, &code.slide, SLIDE_BITS, sizeof(unsigned int))) == EOF)
					{
						break;
					}
				}

				Buffer[tail].offset = code.offset;
				Buffer[tail].length = code.length;
				Buffer[tail].slide  = code.slide;
				Buffer[tail].bool_writed = 0;
				Buffer[tail].ch = 0;
			}
			len++;
			tail = Wrap((tail + 1), WINDOW_SIZE);
		}
	}

	/************************************************************************
	* handle the itemes that remains in the buffer
	************************************************************************/
	while(len > 0)
	{
		/************************************************************************
		* Part A - check if exist pointer to BufferIndex  
		************************************************************************/
		//A.1 check if exist pointer to BufferIndex

		temp_index = head; 
		distance = 0;
		
		for(j = 0; j < len; j++)
		{
			if(Buffer[temp_index].bool_writed == 0 && 
				Buffer[temp_index].length > 1 &&
				distance == (Buffer[temp_index].offset - Buffer[temp_index].length + Buffer[temp_index].slide) ) 
			{
				// and insert the new pointer
				code.offset = Buffer[temp_index].offset - Buffer[temp_index].length; 
				code.length = Buffer[temp_index].length;
				code.slide  = Buffer[temp_index].slide;
				if(code.slide == 0)
				{
					BitFilePutBit(ENCODED, bfpOut);
					BitFilePutBit(PAIR, bfpOut);
					BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
				}
				else
				{
					BitFilePutBit(ENCODED, bfpOut);
					BitFilePutBit(TRIPLE, bfpOut);
					BitFilePutBitsNum(bfpOut, &code.offset, OFFSET_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.length, LENGTH_BITS, sizeof(unsigned int));
					BitFilePutBitsNum(bfpOut, &code.slide, SLIDE_BITS, sizeof(unsigned int));

				}

				if(toPrintOutput == 1)
				{
					printf("(%d,",code.offset);
					printf("%d,",code.length);
					printf("%d),",code.slide);             
				}
				distance+= Buffer[temp_index].length;
				Buffer[temp_index].bool_writed = 1;
			}
			else
			{
				distance+= Buffer[temp_index].length;
			}

			temp_index = Wrap((temp_index + 1), WINDOW_SIZE);
		}

		/************************************************************************
		* Part B - write the first item (if it is not have been alredy writed
		************************************************************************/

		if(Buffer[head].length == 1) // if char
		{
			BitFilePutBit(UNCODED, bfpOut);
			BitFilePutChar(Buffer[head].ch, bfpOut);

			if(toPrintOutput == 1)
				printf("%c,",Buffer[head].ch);
		}

		else if(Buffer[head].bool_writed == 0 ) // pointer that we didnt write yet
		{
			code.offset = 0;
			code.length = 0;
			code.slide = 0;
			if ((c = BitFileGetChar(bfpIn)) == EOF)
			{
				break;
			}
			if (c == PAIR)
			{
				if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}

				if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}

			}
			else
			{
				if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}

				if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}
				if ((BitFileGetBitsNum(bfpIn, &code.slide, SLIDE_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}
			}

			Buffer[tail].offset = code.offset;
			Buffer[tail].length = code.length;
			Buffer[tail].slide  = code.slide;
			Buffer[tail].bool_writed = 0;
			Buffer[tail].ch = 0;
		}

		head = Wrap((head + 1), WINDOW_SIZE);
		len--;
	}




	/* we've decoded everything, free bitfile structure */
	BitFileToFILE(bfpIn);
	BitFileToFILE(bfpOut);

	return 0;
}

/****************************************************************************
*   Function   : CastBack
*   Description: This function gets the encoded file according to the format V2 of the project, and transforms it to the traditional LZSS encoded file 
*   Parameters : fpIn - pointer to the project encoded file
*                fpOut - pointer to the open binary file to write encoded output according to LZSS format. 
*   Effects    : fpIn moves every pointer to the right place according to the OFFSET and SLIDE parameters and written to fpOut.  Neither file is
*                closed after exit.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int CastBack(FILE *fpIn, FILE *fpOut)
{
	bit_file_t *bfpIn;
	bit_file_t *bfpOut;
	int c;
	unsigned int i,toPrintOutput;
	encoded_string_t code;              
	encoded Buffer[BUFFER_SIZE];
	unsigned int head ,index;
	encoded Pointer;

	/* use stdin if no input file */
	if ((NULL == fpIn) || (NULL == fpOut))
	{
		errno = ENOENT;
		return -1;
	}

	/* convert input file to bitfile */
	bfpIn = MakeBitFile(fpIn, BF_READ);

	if (NULL == bfpIn)
	{
		perror("Making Input File a BitFile");
		return -1;
	}

	/* convert output file to bitfile */
	bfpOut = MakeBitFile(fpOut, BF_WRITE);

	if (NULL == bfpOut)
	{
		perror("Making Output File a BitFile");
		return -1;
	}
	//toPrintOutput = 1;// debug
	DEBUG_PRINT();
	/************************************************************************
	* Initialize Buffer and Text arrays  
	************************************************************************/
	for(i = 0; i < BUFFER_SIZE; i++)
	{
		Buffer[i].ch = 0;
		Buffer[i].length = 0;
		Buffer[i].offset = 0;
		Buffer[i].slide = 0;
		Buffer[i].bool_writed = 1;
	}

	head = 0;

	while (1)
	{
		if ((c = BitFileGetBit(bfpIn)) == EOF)
		{
			/* we hit the EOF */
			break;
		}
		if (c == UNCODED)
		{
			if ((c = BitFileGetChar(bfpIn)) == EOF)//replace if and while
			{
				break;
			}
			
			while(Buffer[head].bool_writed ==  0 ) // write pointers that should appear
			{
				Pointer.offset = Buffer[head].offset; 
				Pointer.length = Buffer[head].length;

				BitFilePutBit(ENCODED, bfpOut);
				BitFilePutBitsNum(bfpOut, &Pointer.offset, OFFSET_BITS, sizeof(unsigned int));
				BitFilePutBitsNum(bfpOut, &Pointer.length, LENGTH_BITS, sizeof(unsigned int));

				if(toPrintOutput == 1)
				{
					printf("(%d,",Pointer.offset);
					printf("%d),",Pointer.length);

				}

				Buffer[head].bool_writed = 1;// sign the pointer
				head = Wrap((head + Buffer[head].length ), BUFFER_SIZE);
			}
			//write the new characters
			BitFilePutBit(UNCODED, bfpOut);
			BitFilePutChar(c, bfpOut);

			if(toPrintOutput == 1)
				printf("%c,",c);

			head = Wrap((head + 1), BUFFER_SIZE);

		}
		else // c==CODED
		{
			//insert the pointer to the right place at buffer
			code.offset = 0;
			code.length = 0;
			code.slide = 0;

			if ((c = BitFileGetBit(bfpIn)) == EOF)
			{
				/* we hit the EOF */
				break;
			}
			if( c == PAIR)
			{
				if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
				{
					c = EOF;
					break;
				}

				if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}

			}
			else
			{
				if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS, sizeof(unsigned int))) == EOF)
				{
					c = EOF;
					break;
				}

				if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}
				if ((BitFileGetBitsNum(bfpIn, &code.slide, SLIDE_BITS, sizeof(unsigned int))) == EOF)
				{
					break;
				}
			}

			index = head + code.offset + code.slide;// -1
			index = Wrap((index), BUFFER_SIZE);
			Buffer[index].offset = code.offset + code.length;// no need Wrap, i think
			Buffer[index].length = code.length;
			Buffer[index].bool_writed = 0;
		}
	}

	/************************************************************************
	* handle the itemes that remains in the buffer
	************************************************************************/

	for(i = 0; i < BUFFER_SIZE; i++)
	{
		if(Buffer[head].bool_writed == 0 )
		{
			Pointer.offset = Buffer[head].offset; 
			Pointer.length = Buffer[head].length;

			BitFilePutBit(ENCODED, bfpOut);
			BitFilePutBitsNum(bfpOut, &Pointer.offset, OFFSET_BITS, sizeof(unsigned int));
			BitFilePutBitsNum(bfpOut, &Pointer.length, LENGTH_BITS, sizeof(unsigned int));

			if(toPrintOutput == 1)
			{
				printf("(%d,",Pointer.offset);
				printf("%d),",Pointer.length);

			}

			Buffer[head].bool_writed = 1;// sign the pointer

		}

		head = Wrap((head + 1), BUFFER_SIZE);
	}

	/* we've decoded everything, free bitfile structure */
	BitFileToFILE(bfpIn);
	BitFileToFILE(bfpOut);

	return 0;
}



