/* $Id: mspload.c 2287 2008-11-12 10:40:15Z matejk $ */

//! \file mspload.c
//! Implements MSP bootloader

/*
MSP430 bootloader
Copyright (C) 2004-2006 Instrumentation Technologies

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
or visit http://www.gnu.org
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "debug.h"


#define CPLD_ADD 0x10000000
#define REG2 0x0E

#define PGM       0x04
#define TCK       0x02
#define RST_NMI   0x01

#define REG2_MASK 0x0f
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)


/* Bootstrap commands */
#define     BSL_SYNC                 0x80  // Sync character
#define     BSL_TXPWORD              0x10  // RX passwd
#define     BSL_TXBLK                0x12  // Transmit block to msp
#define     BSL_RXBLK                0x14  // Receive  block from msp
#define	    BSL_ERASE                0x16  // Erase one segment
#define     BSL_MERAS                0x18  // Erase complete FLASH memory
#define	    BSL_CHANGEBAUD           0x20  // Change baudrate
#define     BSL_LOADPC               0x1A  // Load PC and start execution
#define     BSL_TXVERSION            0x1E  // Get BSL version


/* Forward decl's */
int checksum(void);
int frame_gen(char *p);
int read_line(void);


volatile unsigned char StreamData;
unsigned char Reg2;
static volatile unsigned char *Reg2Address;
unsigned char *mapBase;
volatile unsigned int len,fori;
static volatile char cks_l;  // Checksum low byte
static volatile char cks_h;  // Checksum hight byte
volatile static int frame_lenght; // Lenght of frame
volatile static int frame_done;   // Switch
volatile char number_of_data_bytes; // Number of data bytes in file.a43 for each row
volatile char address_h; // high byte of msp ram address
volatile char address_l; // low byte of msp ram address
volatile int file_over, end_of_line;
char S[1024];
volatile char FRAME_BYTES[270];
int l;

// CPLD register description
// cpld register  /  7   /  6  /  5  /  4  /  3  /  2  /  1  /  0  /
//                 unused  free bits               PGM  TCK  RST_NMI

// BSL ENTRY SEQUENCE

//         ____________________________________________
// PGM  __/            programming signal             \_____________
//
//                            _______________________________
//RST    ____________________|       BSL LOADING
//
//       _________   ______      _____________________
//TCK            |___|     |____|
//          1    2   3     4  5 6                      7



// function that allows to start writing MSP430
int Bootstrap_ON(){

   TRACE("ENTERING MSP330 Programming mode over bsl \n\n");
   Reg2 = (PGM | TCK | RST_NMI ) ;                         // sequence 1
   *Reg2Address=Reg2;
   usleep(500);

   Reg2 = ((PGM | TCK) & ~RST_NMI ) ;                         // sequence 1
   *Reg2Address=Reg2;
   usleep(500);
   TRACE("sequence 1  \n");
   TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);


   Reg2 = (PGM & ~TCK & ~RST_NMI ) ;                        // sequence 2
   *Reg2Address=Reg2;
   usleep(500);
   TRACE("sequence 2  \n");
   TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);

   Reg2 =  ((PGM | TCK) & ~RST_NMI )  ;                      // sequence 3
   *Reg2Address=Reg2;
   usleep(500);
   TRACE("sequence 3  \n");
   TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);


   Reg2 = (PGM & ~TCK & ~RST_NMI ) ;                        // sequence 4
   *Reg2Address=Reg2;
   usleep(500);
   TRACE("sequence 4  \n");
   TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);


   Reg2 = ((PGM & ~TCK) | RST_NMI) ;                         // sequence 5
   *Reg2Address=Reg2;
   usleep(500);
   TRACE("sequence 5  \n");
   TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);


   Reg2 = (PGM | TCK | RST_NMI ) ;                              // sequence 6
   *Reg2Address=Reg2;
   usleep(500);
   TRACE("sequence 6 \n");
   TRACE1("Value at address 0x1000000E : 0x%X \n\n", *Reg2Address);

   return (0);
}

// bootstrap logic shut down

int Bootstrap_OFF(){


   Reg2 = (~PGM & (TCK | RST_NMI )) ;                          // sequence 7
   *Reg2Address=Reg2;
   usleep(500);
   TRACE("sequence 7 \n");
   TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);
   TRACE("BSL mode is OFF \n\n\n");

   return(0);
}






 	// MSP430 reset, then the uProc starts the program at 0FFFFEh

 	//         ___________________________________________
 	// PGM  __/            programming signal             \_____________
 	//
 	//                            _______________________________________
 	//RST    ____________________|reset msp and start at FFFFEh
 	//
 	//       ____________________________________________________________
  //TCK
  //                   1        2                         3

int Reset_msp() {

    TRACE("Reseting MSP430  \n");

    Reg2 = ((PGM | TCK) & ~RST_NMI ) ;                         // sequence 1
    *Reg2Address=Reg2;
    usleep(500);
    TRACE("sequence 1  \n");
    TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);

    Reg2 = (PGM | TCK |  RST_NMI ) ;                           // sequence 2
    *Reg2Address=Reg2;
    usleep(500);
    TRACE("sequence 2  \n");
    TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);

    Reg2 = ((RST_NMI | TCK) & ~PGM );                         // sequence 3

    *Reg2Address=Reg2;
    usleep(500);
    TRACE("sequence 3  \n");
    TRACE1("Value at address 0x1000000E : 0x%X \n", *Reg2Address);


    TRACE("MSP restarted!  \n");
    return (0);
}





 //                        ________________
 //        A[i]     -->   |                |
 //                       | MSP_WRITE      | -> ttyS2
 //   frame_lenght --->   |                | ->screen downwrite copy
 //   frame_done   --->   |________________| ->frame_done = 0 (new frame please!)

 // frame_done is a switch, == 1 {write frame to ttyS2}/////   if ..== 0 ; frame has been sent
 // This function works soleily as a Byte output to ttSy2


int MSP_WRITE(){
    FILE *pFile;

    //pFile = fopen ("/dev/ttyS2","wr");

    if(frame_done == 1){
        int i;

        pFile = fopen ("/dev/ttyS2","wr");
        fputc( 0x80, pFile);
        fclose (pFile);

        for(i=0; i<frame_lenght; i++){
            pFile = fopen ("/dev/ttyS2","wr");
            fputc( FRAME_BYTES[i], pFile);
            //for(counter=0; counter<10000; counter++){}
            TRACE1("%x ", FRAME_BYTES[i]);
            fclose (pFile);
        }
        TRACE("\n");
    }
    //fclose (pFile);

    usleep(1000);
    frame_done=0; // frame sent, -> frame_gen, generate new frame please

    // Progress indicator.
    printf("."), fflush(stdout);
    return 0;
}






//                 ________________
//                |                |
//                | erase_mem()    | --->frame_lenght
//                |                | ---> A[i] (frame, erase all memory in msp430)
//                |________________| --->frame_done = 1;

// this is a fixed command, stand alone call will produce mass erase on MSP`s ram!
int erase_mem() {

    frame_lenght=8;

    volatile char B[]={BSL_SYNC,BSL_MERAS,0x04,0x04,0x00,0x00,0x00,0x00};

    for (l=0; l<frame_lenght; l++){
	FRAME_BYTES[l]=B[l];
    }
    checksum();
    frame_lenght=10;

    frame_done=1;

    MSP_WRITE();
    return(0);
}






//                 ________________
//                |                |
//                | passwd()       | --->frame_lenght
//                |                | ---> A[i] (frame, send password to unlock commands)
//                |________________| --->frame_done = 1;

// this is a fixed command, unlock msp430
int passwd(){

    frame_lenght=40;
    char B[]={BSL_SYNC , BSL_TXPWORD , 0x24 , 0x24 , 0x00, 0x00 , 0x00 , 0x00,
              0xFF, 0xFF,0xFF, 0xFF,
              0xFF, 0xFF,0xFF, 0xFF,
              0xFF, 0xFF,0xFF, 0xFF,
              0xFF, 0xFF,0xFF, 0xFF,
              0xFF, 0xFF,0xFF, 0xFF,
              0xFF, 0xFF,0xFF, 0xFF,
              0xFF, 0xFF,0xFF, 0xFF,
              0xFF, 0xFF,0xFF, 0xFF};
    for (l=0; l<frame_lenght; l++){
	FRAME_BYTES[l]=B[l];
    }

    checksum();

    frame_done=1;
    MSP_WRITE();
    return(0);
}



//                 ________________
//                |                |
//                | load_PC_patch()| --->frame_lenght
//                |                | ---> A[i] (frame, load PC to specific value)
//                |________________| --->frame_done = 1;
// this is a fixed command, unlock msp430
int load_PC_patch(){

    frame_lenght=8;
    char B[]={BSL_SYNC , BSL_LOADPC , 0x4 , 0x4 , 0x20, 0x02 , 0x00 , 0x00};
    for (l=0; l<frame_lenght; l++){
	FRAME_BYTES[l]=B[l];
    }
    checksum();

    frame_done=1;
    MSP_WRITE();
    return(0);
}


//                 ________________
//                |                |
//                | load_pc        | --->frame_lenght
//                |                | ---> A[i] (frame, load PC to specific value)
//                |________________| --->frame_done = 1;
// this is a fixed command, unlock msp430
int load_PC(){

    frame_lenght=8;
    char B[]={BSL_SYNC , BSL_LOADPC , 0x4 , 0x4 , 0x22, 0x0C , 0x00 , 0x00};
    for (l=0; l<frame_lenght; l++){
	FRAME_BYTES[l]=B[l];
    }
    checksum();

    frame_done=1;
    MSP_WRITE();
    return(0);
}



//                 ________________
//                |                |
//                | load_pc        | --->frame_lenght
//                |                | ---> A[i] (frame, load PC to specific value)
//                |________________| --->frame_done = 1;
// this is a fixed command, unlock msp430
int load_PC2(){

    frame_lenght=8;
    char B[]={BSL_SYNC , BSL_LOADPC , 0x4 , 0x4 , 0x20, 0x02 , 0x00 , 0x00};
    for (l=0; l<frame_lenght; l++){
	FRAME_BYTES[l]=B[l];
    }
    checksum();

    frame_done=1;
    MSP_WRITE();
    return(0);
}



//// load PATCH.TXT  ///

//                 ________________
//                |                |
//                |load_PATCH.TXT()| --->frame_lenght
//                |                | ---> A[i] (frame for erase all memory in msp430)
//                |________________| --->frame_done = 1;

// this is a fixed command, PATCH loader is  used to update msp bootloader options (always use this function)//
int load_PATCH(){

    //1. line
    frame_lenght=202;
    char B[]={BSL_SYNC , BSL_TXBLK , 0xC6 , 0xC6 ,0x20,0x02,0xC2,0,

              0x31 ,0x40 ,0x1A ,0x02 ,0x09 ,0x43 ,0xB0 ,0x12 ,0x2A ,0x0E ,0xB0 ,0x12 ,0xBA ,0x0D ,0x55 ,0x42,
              0x0B ,0x02 ,0x75 ,0x90 ,0x12 ,0x00 ,0x1F ,0x24 ,0xB0 ,0x12 ,0xBA ,0x02 ,0x55 ,0x42 ,0x0B ,0x02,
              0x75 ,0x90 ,0x16 ,0x00 ,0x16 ,0x24 ,0x75 ,0x90 ,0x14 ,0x00 ,0x11 ,0x24 ,0xB0 ,0x12 ,0x84 ,0x0E,
              0x06 ,0x3C ,0xB0 ,0x12 ,0x94 ,0x0E ,0x03 ,0x3C ,0x21 ,0x53 ,0xB0 ,0x12 ,0x8C ,0x0E ,0xB2 ,0x40,
              0x10 ,0xA5 ,0x2C ,0x01 ,0xB2 ,0x40 ,0x00 ,0xA5 ,0x28 ,0x01 ,0x30 ,0x40 ,0x42 ,0x0C ,0x30 ,0x40,
              0x76 ,0x0D ,0x30 ,0x40 ,0xAC ,0x0C ,0x16 ,0x42 ,0x0E ,0x02 ,0x17 ,0x42 ,0x10 ,0x02 ,0xE2 ,0xB2,
              0x08 ,0x02 ,0x14 ,0x24 ,0xB0 ,0x12 ,0x10 ,0x0F ,0x36 ,0x90 ,0x00 ,0x10 ,0x06 ,0x28 ,0xB2 ,0x40,
              0x00 ,0xA5 ,0x2C ,0x01 ,0xB2 ,0x40 ,0x40 ,0xA5 ,0x28 ,0x01 ,0xD6 ,0x42 ,0x06 ,0x02 ,0x00 ,0x00,
              0x16 ,0x53 ,0x17 ,0x83 ,0xEF ,0x23 ,0xB0 ,0x12 ,0xBA ,0x02 ,0xD3 ,0x3F ,0xB0 ,0x12 ,0x10 ,0x0F,
              0x17 ,0x83 ,0xFC ,0x23 ,0xB0 ,0x12 ,0xBA ,0x02 ,0xD0 ,0x3F ,0x18 ,0x42 ,0x12 ,0x02 ,0xB0 ,0x12,
              0x10 ,0x0F ,0xD2 ,0x42 ,0x06 ,0x02 ,0x12 ,0x02 ,0xB0 ,0x12 ,0x10 ,0x0F ,0xD2 ,0x42 ,0x06 ,0x02,
              0x13 ,0x02 ,0x38 ,0xE3 ,0x18 ,0x92 ,0x12 ,0x02 ,0xBF ,0x23 ,0xE2 ,0xB3 ,0x08 ,0x02 ,0xBC ,0x23,
              0x30 ,0x41};

    for (l=0; l<frame_lenght; l++){
	FRAME_BYTES[l]=B[l];
    }
    checksum();

    frame_done=1;
    MSP_WRITE();

    return(0);
}




int read_file(){

    load_PC_patch();
    do {
	load_PC_patch();
    } while (read_line() == 0);

    return(0);
}


 //                        ________________
 //  frame_lenght   --->  |                |      ( global variables)
 //                       | checksum()     | -> cks_h (high)   Xor over all elements and invert
 //  B[i]           --->  |                | -> cks_l (low)
 //                       |________________|
 //
int checksum(){
    int i;
    cks_l = 0;
    cks_h = 0;

    for(i=0; i<frame_lenght; i++){

        if (i % 2 == 0){
            cks_l ^= FRAME_BYTES[i];
        }
        else{

            cks_h ^= FRAME_BYTES[i];
        }
    }
    FRAME_BYTES[frame_lenght]   = ~cks_l;  // high byte
    FRAME_BYTES[frame_lenght+1] = ~cks_h;  // low byte
    frame_lenght = frame_lenght+2;

    return(0);
}


/** Two nibble = Byte reading helper */
inline unsigned char read_byte() {
    unsigned int two_nibbles;

    scanf("%02x",&two_nibbles);
    return (unsigned char)two_nibbles;
}


int read_line(){

    end_of_line=0;

    int i=0;
    do{
        if(i==0)
	  do { scanf("%c",&StreamData); } while (StreamData!=':');
        if(i==1) {
            //scanf("%02x",&StreamData);
            StreamData = read_byte();
            number_of_data_bytes=StreamData;
            if (number_of_data_bytes  == 0)
              return(1);
	}
        if(i==2) {
            //scanf("%02x",&StreamData);
            StreamData = read_byte();
            address_h=StreamData;
	}

        if(i==3) {
            //scanf("%02x",&StreamData);
            StreamData = read_byte();
            address_l=StreamData;
	}

	if(i==4) {
            //scanf("%02x",&StreamData);
            StreamData = read_byte();
	}

	if(i==5){
            int k;

            for(k=0; k<number_of_data_bytes; k++){ /// number_of data_bytes
                //scanf("%02x",&StreamData);
                StreamData = read_byte();

                S[k]=StreamData;

            }
            //scanf("%02x",&StreamData);
            StreamData = read_byte();
        }
	i++;

    }
    while(i<6);

    frame_gen(S);
    return(0);
}


//                        ________________
//         StreamData->  | frame_gen()    |
//                       | {checksum(); } | -> A[i]
//                       |                | -> frame_done
//                       |________________|
//

// for frame structure read documentation
// Frame Generator, output to MSP_WRITE()



int frame_gen(char *p){

    char A[8+number_of_data_bytes];
    int i;

    A[0]=BSL_SYNC;
    A[1]=BSL_TXBLK;
    A[2]=number_of_data_bytes+4;
    A[3]=number_of_data_bytes+4;
    A[4]=address_l;
    A[5]=address_h;
    A[6]=number_of_data_bytes;  // number of data bytes + 4 -4 = number of data bytes
    A[7]=0;



    for(i=0; i<number_of_data_bytes; i++){     // copy DATA bytes from file
        A[i+8] = p[i]; //  B[i];
    }

    frame_lenght=8+number_of_data_bytes;
    for (l=0; l<frame_lenght; l++){
	FRAME_BYTES[l]=A[l];
    }
    checksum();    // calculates cks_l, cks_h


    frame_done=1;  // switch, frame is ready, send to MSP_WRITE
    MSP_WRITE();
    return(0);

}


/// main function ///
int main(int argc, char **argv) {
    int fd;

    if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1){
        fprintf(stderr,"can't open /dev/mem \n");
        exit(-1);
    }

    mapBase = ( unsigned char * )mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd, CPLD_ADD  & ~MAP_MASK);
    if(mapBase == (void *) -1){
        fprintf(stderr,"can't mmap REG2 \n");
        exit(-1);
    }

    Reg2Address = mapBase + (REG2 & MAP_MASK);

    if (argc > 1) {

        switch (*argv[1]){


        case('e'):

            printf("\nStarting MSP erasing.\n");
            Bootstrap_ON();  // enter bootstrap mode
            usleep(2500);
            erase_mem();     // starts routine for erase memory
            Bootstrap_OFF();
            Reset_msp();
            printf("MSP erased.\n");
            break;

        case('r'):
            Reset_msp();
            break;


        case('w'):

            printf("\nStarting MSP programming.\n");
            // Progress indicator.
            printf("*"), fflush(stdout);
            Bootstrap_ON();  // enter bootstrap mode
            usleep(2500);
            printf("*"), fflush(stdout);
            erase_mem();     // starts routine for erase memory
            printf("*"), fflush(stdout);
            Bootstrap_OFF();
            printf("*"), fflush(stdout);
            Reset_msp();
            sleep(1);
            printf("*"), fflush(stdout);
            Bootstrap_ON();
            printf("*"), fflush(stdout);
            passwd();
            printf("*"), fflush(stdout);
            load_PC();
            printf("*"), fflush(stdout);
            passwd();
            printf("*"), fflush(stdout);
            load_PATCH();
            printf("*"), fflush(stdout);
            load_PC_patch();
            printf("*"), fflush(stdout);
            read_file();     // starts transcription of .a43 file into msp 430
            printf("*"), fflush(stdout);
            Bootstrap_OFF(); // exit bootstrap mode
            printf("*"), fflush(stdout);
            Reset_msp();     // reset msp
            printf("\nCompleted MSP programming.\n");
            break;

        case('h'):
            printf("\n");
            printf("\n");
            printf("TI MSP430 bootloader for Libera \n");
            printf("This program writes files into MSP430 on Libera digital board \n");
            printf("Usage:                                 \n");
            printf("      -Erase memory: ./bootloader e    \n");
            printf("      -  Write file: ./bootloader w < file_to_write.a43\n");
            printf("      -   Reset msp: ./bootloader r     \n");
            printf("      -        help: ./bootloader h     \n");
            printf("\n");
            break;


        default:
            printf("\n");
            printf("\n");
            printf("Error, see help. Type: ./bootloader h \n");
            printf("\n");
            printf("\n");
        }

        close(fd);
        return 0;
    }

    else {
        printf("\n");
        printf("\n");
        printf("Error, see help. Type: ./bootloader h \n");
        printf("\n");
        printf("\n");

        close(fd);
        return 0;
    }

}
