/* $Id: msp_adc.c 2192 2008-10-07 09:13:06Z matejk $ */

//! \file msp_adc.c 
//! Implements MSP voltage monitor for MSP430 target

/*
MSP430 voltage monitor
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

/* 
 * CLK = n/a, MCLK = SMCLK = DCO ~ 800kHz, ULCK = external
*/

#include <msp430x13x.h>
#include <signal.h>
  
// genral setup 

/*
 * 16-bit data stored in 8-bit table for SPI communication
 *
 * no_data ... used as "mutex" ... 1 if data not ready
 *                                 0 if in middle of conversion
 *  
*/
static unsigned char byte_data[16];
volatile unsigned int no_data=1;
unsigned short *short_data = (unsigned short *) byte_data;


/*
 *
 * brief_pause delay from mspgcc manual (16-bit wait)
 *
 */

static void __inline__ brief_pause(register unsigned int n)
{
    __asm__ __volatile__ (
		    "1: \n"
		    " dec	%[n] \n"
		    " jne	1b \n"
		    : [n] "+r"(n));
}

/* SPI_setup */

void SPI_setup(void)  // SPI setup function //
{
    
	P3SEL |= 0x0F;                     // Setup P3 for SPI mode, lower 4-bits 
	U0CTL |= SWRST;                    // Set SWRST, must be set while configuration 
	U0CTL |= CHAR | SYNC;              // 8-bit, SPI, Slave (MM=0)
	
	U0TCTL |= CKPH;    // testing phase=1
	
	URCTL0 = 0x00;   // 0b00000000;  Recieve control Register (FE & OE = 0)
	UBR1_0 = 0x00;   // 0b00000000; 
	UBR0_0 = 0x02;   // 0b00000010;  // clock freq, useless, unpredictable SPI operation if < 2
	UMCTL0 = 0x00;   //  not used  in SPI and should be set to 0x00
	ME1 |= USPIE0;                             // Module enable
	U0CTL &= ~SWRST;                          // SPI enable
	IE1 |= URXIE0;                             // Recieve interrupt enable
}


/* Converter setup */

void Converter_setup(void)
{         // Converter setup function 
	
	P6SEL |= 0xff; // 0b11111111;        All selected 
	
	// Register definitions //
	
	ADC12CTL0 = ADC12ON | REF2_5V | REFON | MSC | SHT0_0; 
			// original -> 0b1111 1111 1111 0011;
	ADC12CTL1 =  SHP | CONSEQ_1; // sequence of chanelles
    			// original  0b0000 0010 0000 0111;     All MCTL with 2.5V ref
	
	ADC12MCTL0 = SREF_1 | INCH_0;
	    		// 0b00010000;                   channel 0, Vref	                
	ADC12MCTL1 = SREF_1 | INCH_1; 
	    		// 0b00010001;                  channel 1, Vref 
	ADC12MCTL2 = SREF_1 | INCH_2;
			//0b00010010;                  channel 2, Vref
	ADC12MCTL3 = SREF_1 | INCH_3;
			//0b00010011;                  channel 3, Vref
	ADC12MCTL4 = SREF_1 | INCH_4;
    			//0b00010100;                  channel 4, Vref
	ADC12MCTL5 = SREF_1 | INCH_5;
			//0b00010101;                  channel 5, Vref
	ADC12MCTL6 = SREF_1 | INCH_6;
			//0b00010110;                  channel 6, Vref
	ADC12MCTL7 = SREF_1 | INCH_7 | EOS;
			//0b00010111;                  channel 7, Vref
} 

//main routine: 
//	1. Hold Watchdog
//	2. Initialize SPI 
//	3. Initialize ADC12
//	4. Start Conversion
//	5. Go to sleep Low Power Mode and wait for XCEP SPI-master

/*
 * Protocol in use: 
 * 				XCEP (master)	|	MSP (slave)
 *				
 * Start conversion		0x10		|	xx ( not care )
 * Coversion Finished??		0xa5 		|	0xef (not ready)
 * ( code !=0xee and >0x10)
 *
 * 				0xa5		|	0xef (not ready)
 * 						.
 * 						.
 * 						.
 * 				0xa5		|	0xef (not ready)
 * 				0xa5		|	0xa6 (Data ready)
 * 				0x00		|	0xa6	
 * 				0x01		|	Data[00] (LSB of Voltage 0)
 * 				0x02		|	Data[01] (MSB of Voltage 0)
 * 				0x03		|	Data[02] (LSB of Voltage 1)
 * 						.
 * 						.
 * 						.
 * 				0x0f		|	Data[14] (LSB of Voltage 7)
 * {dummy read}			0xa5		|	Data[15] (MSB of Voltage 7)
 *
 *
 */


int main(void) 
{
	WDTCTL = WDTPW | WDTHOLD;

	SPI_setup();   

	Converter_setup();
	brief_pause(0x3600);  // Delay ~17ms for Reference Voltage generator 

	ADC12CTL0 |= ENC;   // Enable ADC12


    U0TXBUF= 0xaa; // 0xaa initial state
    no_data=1;
   
    while(1)
    {
	    ADC12CTL0 |= ADC12SC;
	    while((ADC12IFG & BIT7) == 0);

	    short_data[0]= ADC12MEM0;
	    short_data[1]= ADC12MEM1;
	    short_data[2]= ADC12MEM2;
	    short_data[3]= ADC12MEM3;
	    short_data[4]= ADC12MEM4;
	    short_data[5]= ADC12MEM5;
	    short_data[6]= ADC12MEM6;
	    short_data[7]= ADC12MEM7;
	    
	    no_data=0;
	    _BIS_SR(LPM0_bits | GIE);
	    no_data=1;
    }
    
    return 0;
} 


interrupt (USART0RX_VECTOR) rx_isr(void)
{
	while ((IFG1 & UTXIFG0) == 0);   // safety loop, before writing in U0TXBUF


	if (no_data) U0TXBUF=0xef;
	else 
		switch(U0RXBUF)
		{
			case 0 ... 15: U0TXBUF=byte_data[U0RXBUF];
				       break;
			case 16: U0TXBUF=0xef;
				 _BIC_SR_IRQ(LPM0_bits);
				 break;
			default: U0TXBUF=U0RXBUF+1;
				  break;
		}
}
