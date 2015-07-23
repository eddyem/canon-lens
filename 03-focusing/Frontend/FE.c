#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termio.h>
#include <termios.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include <linux/serial.h>
#include <signal.h>
#include "signals.h" // ����������� �������� �����

#define C_NO	224 // ����� ����������� (������� 3 ����)
int BAUD_RATE = B9600;

struct termio oldtty, tty; // ����� ��� UART
struct termios oldt, newt; // ����� ��� �������
//struct serial_struct old_extra_term;
int comfd; // �������� ���������� �����
char *comdev = "/dev/ttyS0"; // ���������� ����� (����� ���������� ��� �������)

void tty_sig(unsigned char rb);

unsigned char crc(unsigned char data){
	unsigned char crc = data & 1;
	unsigned int i;
	for(i = 1; i<8; i++) crc ^= (data >> i) & 1;
	return crc;
}

void quit(int ex_stat){ // ����� �� ���������
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt ); // ���������� ������� � ��������� ��������� (�� ������ ������)
	ioctl(comfd, TCSANOW, &oldtty ); // ��������������� ����� ������ com
	close(comfd); // ��������� ����������
	printf("�����... (������ %d)\n", ex_stat);
	exit(ex_stat);
}

void tty_init(){
	printf("\n�������� ����...\n");
	if ((comfd = open(comdev,O_RDWR|O_NOCTTY|O_NONBLOCK)) < 0){
		fprintf(stderr,"Can't use port %s\n",comdev);
		quit(1);
	}
	printf(" OK\n������� ������� ��������� �����...\n");
	ioctl(comfd,TCGETA,&oldtty); // ������ ������� ��������� �����
	tty = oldtty;
	tty.c_lflag    = 0; // ~(ICANON | ECHO | ECHOE | ISIG)
	tty.c_iflag    = BRKINT; // ���������� � 9������ �������� 
	tty.c_oflag    = 0;
	tty.c_cflag    = BAUD_RATE|CS8|CREAD|CLOCAL|PARENB; // 9.6�, 8N1, RW, ������������ �����. �����
	tty.c_cc[VMIN] = 0;  // �� ������������ �����
	tty.c_cc[VTIME] = 5; // (��� �������� �� � ��.)
	if(ioctl(comfd,TCSETA,&tty)<0) exit(-1); // ������������� ������� ��������� �����
	printf(" OK\n");
}

unsigned char read_tty(unsigned char *byte){
	*byte = 0;
	fd_set rfds; // ����� �������� ������������
	struct timeval tv; // ����� ��������
	int retval; // ������������ �-� select ��������
	tty.c_iflag &= ~PARODD;
	ioctl(comfd,TCSETA,&tty);
// ����, ������� � �����
	FD_ZERO(&rfds); // ������� ����� 
	FD_SET(comfd, &rfds); // ������ ��� - �������� �����
	tv.tv_sec = 0; tv.tv_usec = 50000; // ����
	retval = select(comfd + 1, &rfds, NULL, NULL, &tv);
	if (!retval) return 0; // ���� ������� ���, ���������� ����
	if(FD_ISSET(comfd, &rfds)){
//		printf("ready  ");
		if(read(comfd, byte, 1) < 1) return 0; // ������ ����������
	} 
	else return 0; // ������
/*	if(*byte == 8){
		return 0;
		
	}
	else*/
//		printf("������: %d (������� %d)\n", *byte, *byte & 0x1f);
	return 1;
}

unsigned char read_console(){ // ��������� ������ � �������
	unsigned char rb;
	struct timeval tv;
	int retval;
	tcgetattr( STDIN_FILENO, &oldt ); // ��������� �������� ��� ������� �� ������� ��� ���
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds); // 0 - ����������� ����
	tv.tv_sec = 0; tv.tv_usec = 10000; // ���� 0.01�
	retval = select(1, &rfds, NULL, NULL, &tv);
	if (!retval) rb = 0;
	else {
		if(FD_ISSET(STDIN_FILENO, &rfds)) rb = getchar();
		else rb = 0;
	}
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );	
	return rb;
}

unsigned char mygetchar(){ // ������ getchar() ��� ������������� ���� Enter
	unsigned char ret;
	do ret = read_console();
	while(ret == 0);
	return ret;
}

void write_tty_raw(unsigned char wb){
	if(crc(wb)) // �������� �����
		tty.c_cflag |=PARODD; // 9-� ��� = 0
	else
		tty.c_cflag &= ~PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // ��������������� ����
	if(write(comfd, &wb, 1) < 0) fprintf(stderr, "������! ������ �� ������� :(\n"); // � ����� � ���� ����
}

void write_tty(unsigned char wb){
	unsigned char tmp = wb;
	tmp &= 0x1f; // �� ������ ������ ������� �� ������
	tmp |= C_NO; // ��������� � ������� 
	write_tty_raw(tmp);
}

int send_cmd(unsigned char cmd){ // ������� ����������� ������� � ���������
	unsigned char rtn=ERR_CMD, byte=0, i=0;
	while(i<2 && rtn != 1){ 
		write_tty(cmd);
		usleep(100000);
		rtn = read_tty(&byte);
		byte &= 0x1f;
		i++;
	}
	if(byte != cmd){
		printf("\n\n!!!!!!  ������! ���������� �� �������� (����� %d �� ������� %d)  !!!!!\n\n", byte, cmd);
		return 0;
	}
	else return 1;
}
	
void sendword(unsigned int data){
	unsigned char tmp,i=0,rr=0, ret;
	tmp = (data >> 8) & 0xFF;
	if(crc(tmp)) // �������� �����
		tty.c_cflag &= ~PARODD; // 9-� ��� = 0
	else
		tty.c_cflag |= PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // ��������������� ����
	write(comfd, &tmp, 1);
	do{ i++; 
		usleep(10000);
		ret = read_tty(&rr); rr &= 0x1F;
	}while((ret == 0 || rr != OK) && i < 10 );
	if(rr != OK){ fprintf(stderr, "������ ������"); return;}
	i = 0;
	tmp = data & 0xFF;
	if(crc(tmp)) // �������� �����
		tty.c_cflag &= ~PARODD; // 9-� ��� = 0
	else
		tty.c_cflag |= PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // ��������������� ����
	write(comfd, &tmp, 1);
	do{ i++; 
		usleep(10000);
		ret = read_tty(&rr); rr &= 0x1F;
	}while((ret == 0 || rr != OK) && i < 10 );
	if(rr != OK){fprintf(stderr, "�������"); return;}
}

void send_rand(){
	int i, j = 0; unsigned char byte, rb;
	srand(time(NULL));
	for(i=0; i<128; i++)
		if(send_cmd(SPI_send_one)){
			write_tty_raw(25);
			printf("%d: ������� ���� \e[1;32;40m18\e[0m\n",i);
			if(read_tty(&byte))
				tty_sig(byte);
			rb = read_console();
			if(rb == 'q') break;
			//byte = (rand()|rand()) & 0xff;
			byte = j++;
			if(send_cmd(SPI_send_one)){
				write_tty_raw(byte);
				printf("%d: ������� ���� \e[1;32;40m%d\e[0m\n", i, byte);
				if(read_tty(&byte))
					tty_sig(byte);
				rb = read_console();
				if(rb == 'q') break;	
			}
			if(send_cmd(SPI_send_one)){
				write_tty_raw(0);
				printf("%d: ������� ���� \e[1;32;40m0\e[0m\n", i);
				if(read_tty(&byte))
					tty_sig(byte);
				rb = read_console();
				if(rb == 'q') break;	
			}
		}
	
}

void send_nseq(){
	int i, j; unsigned char byte, rb;
	for(i=0; i<256; i++)
		if(send_cmd(SPI_send_one)){
			byte = (unsigned char)i;
			write_tty_raw(byte);
			printf("%d: ������� ���� \e[1;32;40m%d\e[0m\n", i, byte);
			if(read_tty(&byte))
				tty_sig(byte);
			rb = read_console();
			if(rb == 'q') return;
			for(j=0; j<5; j++)
				if(send_cmd(SPI_send_one)){
				write_tty_raw(0);
				printf("%d: ������� ���� \e[1;32;40m0\e[0m\n", i);
				if(read_tty(&byte))
					tty_sig(byte);
				rb = read_console();
				if(rb == 'q') return;
			}
		}
	
}

void send_seq(){
	unsigned char seq[1024], byte;
	char *line;
	int cntr = 0, n, sz = 9;
	line = (char*) malloc(10);
	printf("\n������� ����� �� ������ � ������, ��������� - ����� end\n");
	do{
		n = getline(&line, &sz, stdin);
		if(n == 0 || strstr(line, "end") != NULL) break;
		seq[cntr++] = (unsigned char)atoi(line);
	}while(cntr < 1024);
	printf("\n������� ��������\n");
	for(n=0; n<cntr; n++){
		if(send_cmd(SPI_send_one)){
			write_tty_raw(seq[n]);
			printf("������� ���� \e[1;32;40m%d\e[0m\n", seq[n]);
			if(read_tty(&byte))
				tty_sig(byte);
		}
	}
	free(line);
	printf("\n�������� ��������\n");
}


void send_same(){
	int i; unsigned char cmd, byte, rb;
	printf("\n������� �������: ");
	scanf("%d", &i);
	cmd = i;
	for(i=0; i<1024; i++)
		if(send_cmd(SPI_send_one)){
			byte = (i%16) ? 0:cmd;
			write_tty_raw(byte);
			printf("%d: ������� ���� \e[1;32;40m%d\e[0m\n", i, byte);
			if(read_tty(&rb))
				tty_sig(rb);
			rb = read_console();
			if(rb == 'q') break;
		}
	
}

void tmrset(){
	unsigned char t_h,t_l;
	float period;
	if(send_cmd(TMR_SETTINGS) == 0) return;
	usleep(10000);
	read(comfd, &t_h, 1);
	usleep(10000);
	read(comfd, &t_l, 1);
	period = (65536.0 - 256.0*(float)t_h - (float)t_l)/125000.0;
	printf("������ �������: T=%f", period);
}

void help(){
	printf("\nh\t������\n"
		"t\t������ ������� ������ �������\n"
		"T\t������� ������ �������\n"
		"s\t�������� ��������� � ���������� ������� �� SPI\n"
		"i\t(��)���������������� ����������\n"
		"m\t������ ����������� � �������� �����\n"
		"e\t������ ����������� � ����� �����\n"
		"R\t�������� ��������� ����� (1024 �����)\n"
		"L\t�������� 9600\n"
		"M\t�������� 19200\n"
		"1..3\t���������� �������� ��������� F\n"
		"f\t������������� � ������� �������������\n"
		"b\t������������� � ������� ����\n"
		"0\t������������� � ������\n"
		"9\t������������� � �������������\n"
		"H\t�������� ������ ����������\n"
		"p\t���������� ������\n"
		"P\t��������� ������\n"
		"y\t��������� SPI\n"
		"Y\t�������� SPI\n"
		"R\t�������� ��������� �����\n"
		"F\t�������� ����������\n"
		"S\t�������� ������������������ ���� (�� ����� 1024, ���������� ������������������ - end)\n"
		"w\t�������� ������������������ ���������� ��������� ������\n"
		"W\t�������� ������������������ 0..255 � �������������� 5 ������\n"
		"\n");
}

void set_speed(int spd){
	//B115200 ��� B9600
	BAUD_RATE = spd;
	ioctl(comfd, TCSANOW, &oldtty ); // ��������������� ����� ������ com
	close(comfd); // ��������� ����������
	tty_init();
}

void con_sig(unsigned char rb){ // ��������� �������� � �������
	unsigned int tmp; unsigned char cmd;
	if(rb == 'q') quit(0); // ������� �� ������� q
	switch(rb){
		case 'h':	help(); break;
		case 't':	tmrset(); break;
		case 'T':	printf("\n������ � ���:\n"); scanf("%u", &tmp);
					if(send_cmd(SET_TIMER)) sendword(65536 - tmp/8); break;
		case 'i':	send_cmd(INIT); break;
		case 's':	printf("\n������:\n"); scanf("%d", &cmd);
					if(send_cmd(SPI_send_one)) write_tty_raw(cmd);; break;
		case 'm':	send_cmd(MID_DATA); break;
		case 'e':	send_cmd(END_DATA); break;
		case 'L':	send_cmd(LOW_SPD); set_speed(B9600); break;
		case 'M':	send_cmd(MID_SPD); set_speed(B19200); break;
		case '1':	send_cmd(SPEED1); break;
		case '2':	send_cmd(SPEED2); break;
		case '3':	send_cmd(SPEED3); break;
		case 'f':	send_cmd(FORW); break;
		case 'b':	send_cmd(BACK); break;
		case '0':	send_cmd(ZERO); break;
		case '9':	send_cmd(INFTY); break;
		case 'H':	send_cmd(HANDS); break;
		case 'p':	send_cmd(TMR_OFF); break;
		case 'P':	send_cmd(TMR_ON); break;
		case 'y':	send_cmd(SPI_OFF); break;
		case 'Y':	send_cmd(SPI_ON); break;
		case 'R':	send_rand(); break;
		case 'F':	send_cmd(FOCUS); break;
		case 'S':	send_seq(); break;
		case 'w':	send_same(); break;
		case 'W':	send_nseq(); break;
	}
}

void dec2bin(unsigned char ii, char* bin){
	int i;
	for(i=0; i<8; i++){
		bin[7-i] = (ii & 1) ? '1' : '0';
		ii>>=1;
	}
	bin[8]=0;
}

void tty_sig(unsigned char rb){ // ��������� �������� � �����������
	char bin[9];
	dec2bin(rb, bin);
	switch(rb){
		case ERR_CMD: printf("������ (\e[1;31;40m%d\e[0m?)\n", ERR_CMD); break;
		case OK: printf("OK (\e[1;31;40m%d\e[0m?)\n", OK); break;
		default: printf("\e[1;31;40m%d\e[0m\t(%s)\n", rb, bin);
	}
}

int main(int argc, char *argv[]){
	unsigned char rb, byte, i = 0; // ��������� ������, �������
	tty_init();
	rb = ERR_CMD;
	signal(SIGTERM, quit);		// kill (-15)
	signal(SIGINT, quit);		// ctrl+C
	signal(SIGQUIT, SIG_IGN);	// ctrl+\   .
	signal(SIGTSTP, SIG_IGN);	// ctrl+Z
	setbuf(stdout, NULL);
	printf("\n�������������...\n");
	do{
	    write_tty(INIT); // ������������� �����������
	    rb = read_tty(&byte);
	    usleep(100000);
	    i ++;
	}
	while(i<3 && rb != 1); // �������� �����
	if(i > 2 || (byte&0x1f) != INIT){
		fprintf(stderr,"\n!!!!!!  ������: ���������� �� �������� (%d) !!!!!!\n", byte);
	}
	else printf(" OK\n");
	while(1){ // ���������� �������������
		rb = read_console();
		if(rb != 0) con_sig(rb); // ���� ���-�� ��������� - ������������
		rb = read_tty(&byte);
		if(rb != 0) tty_sig(byte); // ���� ���� ������ � ����������� - ������������

	}
}
