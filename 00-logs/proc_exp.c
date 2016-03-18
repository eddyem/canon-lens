#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv){
	char *inm = argv[1], *onm = argv[2];
	FILE *fin = fopen(inm, "r");
	char *buf = malloc(256);
	printf("# time   MOSI  MISO\n");
	double told = 0.;
	do{
		size_t len = 256;
		if(getline(&buf, &len, fin) < 1) break;
		char *brace;
		if(!(brace = strchr(buf, '('))) continue;
		double t; int mosi, miso;
		t = atof(buf);
		++brace;
		mosi = strtol(brace, NULL, 16);
		if(!(brace = strchr(brace, '('))) continue;
		miso = strtol(brace+1, NULL, 16);
		if(t - told > 0.001) printf("\n"); // next data portion
		printf("%7.5f  %.2x  %.2x\n", t, mosi >> 1, miso >> 1);
		told = t;
	}while(1);
	fclose(fin);
}
//0.00015375,0,'1' (0x001),'17' (0x011)
