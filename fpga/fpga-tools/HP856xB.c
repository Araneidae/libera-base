//$Id: HP856xB.c 2192 2008-10-07 09:13:06Z matejk $
#include <ni488.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//gcc HP856xB.c Wall -lgpibapi -oHP856xB

#define VERSION 1.1
#define DFLT_RF_LEVEL	-100.0
#define MIN_RF_FREQ 	0.099999
#define DFLT_GPIB_ADDR	0

int main(int argc, char **argv) {
	
	char	cmdText[1024];
	char	tmp_cmdText[256];  
	int 	ud;
	int 	len;
	
	int GPIB_addr = DFLT_GPIB_ADDR;
	double RF_level = DFLT_RF_LEVEL;
	double RF_freq = MIN_RF_FREQ;
	int ch = -1;
	int reset = 0;
	int RF_off = 0;	
	
  	
  	printf("\r\nHP856xB control program");
  	printf("\r\n(c) Instrumentation Technologies 2006\r\n");
  
	while ( (ch = getopt( argc, argv, "a:f:hl:orv" )) != -1 ) {

		switch ( ch ) {
			case 'a':
				GPIB_addr = atoi(optarg);
				break;			

			case 'f':
				RF_freq = atof(optarg);
				break;

			case 'h':
				printf("\r\nusage: HP856x [-a HPIB_addr] [-f RF_frequency] [-h] [-l RF_level] [-r] [-v]");
				printf("\r\n -h help");
				printf("\r\n -o RF off (overrides all comands except reset)");				
				printf("\r\n -r generator reset");
				printf("\r\n -v software version");
				printf("\r\nDefault GPIB address is 0\n");
				exit( 0 );

			case 'l':
				RF_level = atof(optarg);
				break;

			case 'o':
				RF_off = 1;
				break;

			case 'r':
				reset = 1;
				break;

			case 'v':
				printf("\r\nGPIB HP856x control program, version %.1f\r\n",VERSION);
				break;				

			default:
				printf("\r\nUnknown parameter: %s", optarg);			
				exit( 1 );
		}
	}

  
	ud = ibdev(0,GPIB_addr,0,T10s,1,0);
	if((ibsta&ERR) != 0)
    	printf("%x %x error\n",ud,ibsta&ERR);
    	
	if (reset) {
		ibclr(ud);
		printf("\nRF generator reset");
	}
  
  
	if (RF_off) {
		sprintf(cmdText,"R2");
		printf("\nRF off");		
	}
	else {
		sprintf(cmdText,"");
    	
		if (RF_freq > MIN_RF_FREQ) {
  			sprintf(cmdText,"FR%10.6fMZ", RF_freq);
			printf("\nFREQ: %10.6lf MHz", RF_freq);
		}
    	
		if (RF_level > DFLT_RF_LEVEL) {
 			sprintf(tmp_cmdText,"AP%2.1fDM", RF_level);
			strcat(cmdText,tmp_cmdText);
			printf("\nLEVEL: %2.1lf dBm", RF_level);
		}
	}
	
  	len = strlen(cmdText);
	if (len > 0) {
  		ibwrt(ud,cmdText,len);
		printf("\nGPIB addr: %d", GPIB_addr);
		printf("\nCMD string: %s\n", cmdText);
	}
  	else
		printf("\r\nNo RF generator parameter was set!\r\n");
	
	return (0);	
}



