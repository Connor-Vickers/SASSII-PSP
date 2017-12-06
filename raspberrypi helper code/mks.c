#include <wiringPi.h>
#include <wiringSerial.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>

FILE *fp;
int fp2;

void intHandler(int dummy) {
    fclose(fp);
    exit(1);
}

void format(FILE *fd, uint raw){
	char csign;
	if(raw & 0x8000){
		csign = '-';
	}else{
		csign = '+';
	}
	raw = raw & 0x7FFF;
	int cones = raw / 1000;
	int cremander = (raw / 10) - cones*100;
	int cexp = raw % 10;
	fprintf(fd,"%d.%02de%c%d,", cones, cremander, csign, cexp);
}

int main (){

	signal(SIGINT, intHandler);
	char path[100];
	getcwd(path, 100);
	char time_s[100];
	time_t rawtime;
 	struct tm * timeinfo;
 	time(&rawtime);
 	timeinfo = localtime(&rawtime);
 	strftime(time_s, 100, "/%a:%T.csv", timeinfo);
	strcat(path, time_s);

	fp = fopen(path,"w+");

	struct timespec tim, tim2;
    	tim.tv_sec = 0;
    	tim.tv_nsec = 100000000;

    	double elapsed_time;
    	struct timespec tw1, tw2;

	fp2 = serialOpen("/dev/ttyS0", 9600);
	wiringPiSetup();
	clock_gettime(CLOCK_MONOTONIC, &tw1);
	int i;
	int j;
	char string[100];
	char buff[30];

	serialFlush(fp2);
	while(1){

		serialPutchar(fp2, 'Q');
		delay(1000);
		clock_gettime(CLOCK_MONOTONIC, &tw2);
		elapsed_time = 1000.0*tw2.tv_sec + 1e-6*tw2.tv_nsec - (1000.0*tw1.tv_sec + 1e-6*tw1.tv_nsec);
		fprintf(fp, "%f,", elapsed_time);
		int first = 1;
		j = 0;
//		printf(":%d", serialDataAvail(fp2));
		while(serialDataAvail(fp2)){
			
			buff[j] = serialGetchar(fp2);
			//printf(":%d", buff[j]);
			j++;
		}
		
	format(fp,buff[9]*0x100+buff[10]);	
	format(fp,buff[11]*0x100+buff[12]);
	format(fp,buff[13]*0x100+buff[14]);
	fprintf(fp,"\n");
//	printf("\n", string);

//	fprintf(fp, "%s\n", string);


	}
	return 0;
}
