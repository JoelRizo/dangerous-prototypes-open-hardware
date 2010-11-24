/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project and http://dangerousprototypes.com
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* spanner 2010-10 v0.31
 * some minor tidy up of text, finished implementation of a v minimal verbose display option
 * Added two automated / macro play modes to the original play mode:
 * Three play modes:
 * 1. -p f NAME
 *     This is the unchanged method form the origianl release.
 *     Plays all bin files matching NAME_nnn.bin, where nnn starts at 000 and consists of SEQUENTIAL numbers. Stops at first gap in numbers, or last file.
 *     In this mode user MUST press any key, except 'x', to play next file/command.
 * 2. -p f NAME - a nnn
 *     Plays as above, except does NOT wait for any key to be pressed, instead delays nnn miliseconds between sending each file.
 *     500 is recommended as a starting delay. On old P4 computer, no delay always hangs the IRToy (have to uplug & plug in again to reset).
 * 3. -q -f NAME
 *     Play command files listed in the file indicated in -f parameter (requires -f )
 *
 *         Note the file names can be random, ie the numbered sequential rule does not apply.
 *         Sample file content:
 *         sanyo_000.bin
 *         sanyo_010.bin
 *         sanyo_020.bin
 *         sanyo_500.bin
 *
 *         Files can be in a subdirectory if use the following syntax:
 *         .\\dvd\\sanyo_000.bin
 *         .\\dvd\\sanyo_010.bin
 *         .\\dvd\\sanyo_020.bin
 *         .\\dvd\\sanyo_500.bin
 */

// added: enhanced to support continous stream of data from irtoy.
//        support for conversion to OLS format, via commandline option -o
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <time.h>
#ifdef _WIN32
#include <conio.h>
#include <windef.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <ncurses.h>
#include <stdbool.h>
#include <unistd.h>

//#define int BOOL

#endif

#include "serial.h"
#include "txt.h"
#include "ols.h"
#include "bin.h"
#include "queue.h"


#define FREE(x) if(x) free(x)
#define IRTOY_VERSION "v0.05"




int modem =FALSE;   //set this to TRUE of testing a MODEM
int verbose = 0;

#ifndef _WIN32
static int peek = -1;
static struct termios orig,new;


int kbhit()
{

  char ch;
  int nread;

  if(peek != -1) return 1;
  new.c_cc[VMIN]=0;
  tcsetattr(0, TCSANOW, &new);
  nread = read(0,&ch,1);
  new.c_cc[VMIN]=1;
  tcsetattr(0, TCSANOW, &new);

  if(nread == 1) {
   peek = ch;
   return 1;
  }

  return 0;
}

int readch()
{

  char ch;

  if(peek != -1) {
    ch = peek;
    peek = -1;
    return ch;
  }

  read(0,&ch,1);
  return ch;
}



#endif

int print_usage(char * appname)
	{

		//print usage
		printf("\n\n");
        printf(" IRToy version: %s\n", IRTOY_VERSION);
        printf(" Usage:              \n");
		printf("   IRtoy.exe  -d device [-s speed]\n ");
		printf("\n");
		printf("   Example Usage:   IRtoy.exe  -d COM1 -s speed -f outputfile  -r -p \n");
		printf("                    IRtoy.exe  -d COM1 -s speed -f outputfile  -r -p -t -o\n");
		printf("\n");
#ifdef _WIN32
		printf("           Where: -d device is port e.g.  COM1  \n");
#else
		printf("           Where: -d device is port e.g.  /dev/ttyS0  \n");
#endif
		printf("                  -s Speed is port Speed  default is 115200 \n");
		printf("                  -f Output/input file is a base filename for recording/playing");
        printf("                     pulses  \n");
        printf("                  -r Record into a file indicated in -f parameter (requires -f) \n");
		printf("                  -p Play the file/sequence of file indicated in -f parameter");
		printf("                     (requires -f \n");
		printf("                  -q Play command files listed in the file indicated in -f");
		printf("                     parameter (requires -f )\n");
		printf("                  -a Optional automatic play (does not wait for keypress). \n");
		printf("                     You must specify delay in milliseconds between sending \n");
		printf("                     each command.\n");
		printf("                  -v Display verbose output, have to specify level 0, 1 etc,\n");
		printf("                     although at present it is only on or off :).\n");
		printf("                  -o Create OLS file based on the filename format \n");
		printf("                     ext. \"ols\" (Requires -f)  \n");
		printf("                  -t Create or Play text files based on the filename format\n");
        printf("                     ext. \"txt\" (Requires -f)  \n");
		printf("\n");
#ifdef _WIN32
		printf("           IRtoy.exe -d com3    - default used is: -s 115200, displays data from IRTOY\n");
#else
		printf("           IRtoy.exe -d /dev/ttyS2    - default used is: -s 115200, displays data from IRTOY\n");

#endif
		printf("\n");
		printf(" To record and play a text file test_000.txt, use \n");
        printf("           IRtoy.exe -d  com3  -f test -p -t -r\n\n");
        printf(" NOTE:     Except for -q command, Use only the base name of the\n");
        printf("           output/input file, without the numeric sequence:\n");
        printf("           use -f test instead of -f test_000.bin \n");
        printf("           _000 to _999 will be supplied by this utility. \n");
        printf("           You may also edit the resulting text file and replace \n");
        printf("           it with your own values, and should end with FFFF.\n");
        printf("\n\n");

	    printf("-------------------------------------------------------------------------\n");


		return 0;
	}


int main(int argc, char** argv)
{
	int cnt, i,flag;
	int opt;
	char buffer[255] = {0};  //   buffer

	int fd,fcounter;
	int res,c;
	char *param_port = NULL;
	char *param_speed = NULL;
	char *param_fname=NULL;
	char *param_delay=NULL;



	#ifndef _WIN32
	typedef bool BOOL;
	#endif
	BOOL record=FALSE, play=FALSE, queue=FALSE, OLS =FALSE,textfile=FALSE;

	#ifndef _WIN32
//just to get the display need required on linux
  tcgetattr(0, &orig);
  new = orig;
  new.c_lflag &= ~ICANON;
  new.c_lflag &= ~ECHO;
  new.c_lflag &= ~ISIG;
  new.c_cc[VMIN] = 1;
  new.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &new);

#endif

	printf("-------------------------------------------------------------------------\n");
	printf("\n");
	printf(" IR TOY Recorder/Player utility %s (CC-0)\n", IRTOY_VERSION);
	printf(" http://dangerousprototypes.com\n");
	printf("\n");
	printf("-------------------------------------------------------------------------\n");
	if (argc <= 1)  {

			print_usage(argv[0]);
			exit(-1);
		}

	while ((opt = getopt(argc, argv, "tomrpqsv:a:d:f:")) != -1) {
       // printf("%c  \n",opt);
		switch (opt) {

			case 'v':  // verbose output
                verbose=1;
				break;
			case 'a':  // delay in miliseconds
				if ( param_delay != NULL){
					printf(" delay error!\n");
					exit(-1);
				}
				param_delay = strdup(optarg);
				//printf("delay %s - %d \n", param_delay, atoi(param_delay));
				break;
			case 'd':  // device   eg. com1 com12 etc
				if ( param_port != NULL){
					printf(" Device/PORT error!\n");
					exit(-1);
				}
				param_port = strdup(optarg);
				break;
            case 's':
				if (param_speed != NULL) {
					printf(" Speed should be set: eg  115200 \n");
					exit(-1);
				}
				param_speed = strdup(optarg);

				break;
            case 'f':  // device   eg. com1 com12 etc
				if ( param_fname != NULL){
					printf(" Error: File Name parameter error!\n");
					exit(-1);
				}
				param_fname = strdup(optarg);

				break;
 			case 'm':    //modem debugging for testing
                   modem =TRUE;   // enable modem mode, for testing only
				break;
            case 'p':    //play
                play =TRUE;
				break;
            case 'q':    //play command queue from file (ie macro command list)
                queue =TRUE;
				break;
			case 'r':    //record
                record =TRUE;
                break;
			case 't':    //text file: record or play text file
                textfile =TRUE;
				break;
            case 'o':    //write to OLS format file
                OLS =TRUE;
				break;
			default:
				printf(" Invalid argument %c", opt);
				print_usage(argv[0]);
				//exit(-1);
				break;
		}
	}

 //defaults here --------------
    if (param_delay==NULL)
		param_delay=strdup("-1");
    if (param_port==NULL){
        printf(" No serial port set\n");
		print_usage(argv[0]);
		exit(-1);
    }

    if (param_speed==NULL)
		param_speed=strdup("115200");
    if (record==TRUE){
        if ((param_fname==NULL) && (record==TRUE)){    //either 'r' or 'p' or both should be used  witout filename it will just display it
            printf(" Error: -f Parameter is required\n");
            print_usage(argv[0]);
            exit(-1);
        }
    }
    if(OLS==TRUE) {  // -f must be specified + r or p  or both
            if (param_fname==NULL) {
               printf(" Error: -f Parameter is required, ignoring OLS writing.\n");
                OLS=FALSE;
            }
    }

  	printf(" Opening IR Toy on %s at %sbps...\n", param_port, param_speed);
	fd = serial_open(param_port);
	if (fd < 0) {
		fprintf(stderr, " Error opening serial port\n");
		return -1;
	}
    serial_setup(fd,(speed_t) param_speed);
 printf(" Entering IR sample mode ....\n ");
    for (i=0;i<5;i++) {
        //send 5x, just to make sure it exit the sump mode too
        serial_write( fd, "\x00", 1);
    }

    serial_write( fd, "S", 1);
    res= serial_read(fd, buffer, sizeof(buffer));  //get version

    if (res>0){
        printf("IR Toy Protocol version: ");
        for (i=0;i<res;i++)
            printf(" %c ",buffer[i]);
        printf("\n");
    }
    else{
        fprintf(stderr," Error: IR Toy doesn't have a reply\n ");
        exit(-1);
    }
    cnt=0;
    c=0;
    flag=0;
    fcounter=0;
    if(record==TRUE){ //open the file

         if ( IRrecord(param_fname,fd)==-1) {
            FREE(param_port);
            FREE(param_speed);
            FREE(param_fname);
            exit(-1);
         }
	}

    if (play==TRUE){
        IRplay(	param_fname,fd,param_delay);

    } // play=true
    if (textfile==TRUE) {
         if (record==TRUE){
             IRtxtrecord(param_fname);

         }

    if (play==TRUE){
           IRtxtplay(param_fname,fd,param_delay);
        }

    }
    if (OLS==TRUE) {

       create_ols(param_fname);

    }  //OLS==true

    if (queue==TRUE){

        IRqueue(param_fname,fd);
    } // queue=true


    printf(" Resetting IRtoy ....\n ");
    serial_write( fd, "\xFF\xFF", 2);
    for (i=0;i<5;i++) {
        //send 5x, just to make sure it exit the sump mode too
        serial_write( fd, "\x00", 1);
    }
    serial_close(fd);
	FREE(param_port);
	FREE(param_speed);
	FREE(param_fname);
    printf("\n Thank you for playing with the IRToy version: %s. \n", IRTOY_VERSION);
    return 0;
}  //main