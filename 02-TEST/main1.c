#include	<pic16f873a.h>
#include "signals.h"
//#define CLRWDT	_asm clrwdt _endasm;
typedef unsigned int word;
word at 0x2007 CONFIG = 0x3F72; // ��� WDT: 0x3F76
unsigned char
	cmd,		// ���������� �������
	d_H, d_L,	// ������� � ������� ����� ����������� �������
	T1H, T1L,	// �������� ��������� �������� ������� 1
	mC_addr;	// ���. ����� ����������� (���������, ��������������� �������� init)

unsigned char SPI_buf[64], SPI_cntr;

void send9bit(unsigned char something){
	unsigned char tmp;
	something &= 0x1F; // ����� ������� ���� ��� (��� ����� �����)
	tmp = mC_addr | something;
	TXEN = 1; // ����� � ��������
	TX9D = 0; // 0 - �������� ����������
	TXREG = tmp;	// ������� �������
}

unsigned char get9bit(){
	unsigned char err1, err2, flag9bit, tmp;
	while(!RCIF);
	flag9bit = RX9D;
	err1 = FERR; err2 = OERR; // ������� 9-� ��� � ������
	cmd = RCREG; // �������� ����� ������
	RX9D = 0;
	if(err1 == 1){
//		send9bit(NO_STOP_BIT); // �� ��������� �������� ���
		return NO_STOP_BIT;
	}
	if(err2 == 1){
//		send9bit(STACK_OVERFLOW); // ������������ �������� ���������
		CREN = 0; CREN = 1; // �������� ���� ������
		return STACK_OVERFLOW;
	}
	if(flag9bit) return TWOBYTE; // ������ - ����� ����������� ������� (���������� ��� ���� :) )
	tmp = cmd & 0xE0; // ��������� ������ �� �������
	cmd &= 0x1F; // ��������� �������� �����
	if(tmp != mC_addr) return ERR_CMD; // ���� ��������� ������
	send9bit(cmd); // ��� �������� �������
	return OK;
}

void sendword(unsigned char data_H, unsigned char data_L){
	RCIE = 0; // disable USART in interrupt
	TMR1IE = 0;
	TXEN = 1;
	TX9D = 1;
	TXREG = data_H;
	while(!TRMT);
	TXEN = 1;
	TX9D = 1;
	TXREG = data_L;
	RCIE = 1;
	TMR1IE = 1;
}

unsigned char getword(){
	unsigned char ret = 0;
	RCIE = 0; // disable USART in interrupt
	TMR1IE = 0;
	if(ten_times_read()){
		d_H = cmd;
		if(ten_times_read()){
			d_L = cmd;
			ret = 1;}} // ���� ��� ����� ������� ���������
	TMR1IE = 1;
	RCIE = 1; // enable USART in interrupt
	return ret;
}

unsigned char ten_times_read(){ // 10 ������� ������ ��� ������������ ������
	unsigned char i=0;
	do i++;
	while(get9bit() != TWOBYTE && i < 10);
	if(i > 9){send9bit(ERR_CMD); return 0;}
	send9bit(OK);
	return 1; 
}

void init(){ // �������������
// ��������� USART'a
// TXSTA: | CSRC | TX9 | TXEN | SYNC | N/A | BRGH | TRMT | TX9D |
	TXSTA = 0x66; // (11000110): master, 9-�� ������ ����/�����, async, hi-speed, ready
// SPBRG - �������� ��������
	SPBRG = 25; // 9.6 ��/�
// RCSTA: | SPEN | RX9 | SREN | CREN | ADDEN | FERR | OERR | RX9D |
	RCSTA = 0xD0; // ( 11010000): enable, 9bit, continuous mode
// ��������� ������:
	PORTA = 0; // 6-�� ������ ���������/�������� ���� (0..5 ����)
// ADCON1: | ADFM | N/A | N/A | N/A | PCFG3 | PCFG2 | PCFG1 | PCFG0 |
	ADCON1 = 0x06; // ���������/�������� ���� �������� � ��������� �������� ������
	TRISA = 0; // ����������� ���� � (1-����, 0-�����) 
	TRISB = 0xff; //           --/ B /--
// OPTION_REG: | !RBPU | INTEDG | TOCS | TOSE | PSA | PS2 | PS1 | PS0 |
	OPTION_REG = 0x7f; /* (01111111) 0 - ����������� �������� �� ���� B (���. ���. 1),
				���������� �� ������������ ������ RB0,
				������ 0 �������� �� ������� � RA4
				������ 0 ������������� ��� ����� ������� �� RA4
				������������ ��������� � ����������� �������
				����� prescaler:  1:128 */ 
	TRISC = 0xC0; // (11000000) - 0..5 ���� ��� ������
	INTCON = 0; // ��������� ��� ����������
	T1CON = 0;
// PIE1: | PSPIE | ADIE | RCIE | TXIE | SSPIE | CCP1IE | TMR2IE | TMR1IE |
	PIE1 = 0x20; // (00100000): enable USART(in)
// PIE2: ��� N/A, ����� EEIE (PIE2.4)
	PIE2 = 0; //                & disable other int.s
	PORTB = 0;
// INTCON: | GIE | PEIE | T0IE | INTE | RBIE | T0IF | INTF | RBIF |
	INTCON = 0xC0; // (1100000) - �������� ���������� ����������, ���������� �� ���������
	PORTC = 0; // ��� ����������
	PORTA = 0xF; // (00001111)
// ��������� ������
	mC_addr = PORTB; // ���������� ����� ����������
	mC_addr &= 0xE0; // ��������� ����������� ������
	SSPEN = 0; SSPIE = 0;
	SPI_cntr = 0;
}

void timer1set(){ // ��������� �������
//	unsigned int tmp = 0xffff - usec/8;
	T1CON = 0; // ��������� ������
	TMR1IF = 0; // �������� ���� ����������
	TMR1IE = 1; // ����/���� ����������
	TMR1H = T1H = d_H; //(tmp >> 8) & 0xff;
	TMR1L = T1L = d_L; //tmp & 0xff;  // ���������� ��������
// T1CON: | - | - | T1CPS1 | T1CPS0 | T1OSCEN | T1SYNC | TMR1CS | TMR1ON |
	T1CON = 0x31; // (00110001) - �������� ������ 1, ������������ �� 1/8 (250 ���)
} 

void timer1int(){ // ��������� ���������� ������� �������
	T1CON = 0;
	TMR1H = T1H; TMR1L = T1L; 
//	send9bit(TEST);
	T1CON = 0x31; // ����� ��������� ������
}

void SPI_int(){ //	� ��������� ������ ����������� ������ ����������� �
// 					�����, ��� ���������� ������ �� ���������� �� ��
	unsigned char i;
	TMR1IE = 0;
	RCIE = 0;
	SPI_buf[SPI_cntr++] = SSPBUF;
	if(SPI_cntr == 64){
		for(i = 0; i < 64; i++){
			TXEN = 1;
			TXREG = SPI_buf[i]; 
			while(!TRMT);
		}
		SPI_cntr = 0;
	}
	RCIE = 1;
	TMR1IE = 1;
	BF = 0;
	SSPIF = 0;
}



/*
void SPI_int(){
	SSPIE = 0;
	if(SSPOV == 1) // ������ ������������ ������
		return;
	if(BF == 0) return; // ����� �� ��������
	RCIE = 0;
	TMR1IE = 0;
	TXEN = 1;
	TXREG = SSPBUF; // ���������� ���������� ����
//	while(!TRMT);
	RCIE = 1;
	TMR1IE = 1;
	BF = 0;
	SSPIF = 0;
	SSPIE = 1;
}
*/

void write_SPI(unsigned char byte){
	SSPBUF = byte;
	//PORTA &= 0xEF; // -SS = 0 - �������� ������ �� SPI ��������
	while(!SSPIF); // ���� ��������� ��������
//	SPI_int();

	RCIE = 0;
	TMR1IE = 0;
	TXEN = 1;
	TXREG = SSPBUF; // ���������� ���������� ����
	RCIE = 1;
	TMR1IE = 1;
	BF = 0;
	SSPIF = 0;
}

void  on_interrupt() __interrupt 0{ // ��������� ����������
	if(RCIF == 1){	// ��������� ���������� �� USART
		if(get9bit() != OK) return;
		switch(cmd){
			case INIT:			init(); break;
			case SET_TIMER:		if(getword()) timer1set();
								else send9bit(ERR_CMD);
								break;
			case TMR_SETTINGS:	sendword(T1H, T1L); break;
			case SPI_send:		if(getword()){
									write_SPI(d_H);
									write_SPI(d_L);}
								break;
			case SPI_send_one:	while(!RCIF); write_SPI(RCREG); break;
			case IMP_RISE:		CKE = 1; break;//������ ���������� �� ������� ������
			case IMP_FALL:		CKE = 0; break; //������ ���������� �� ��������� ������
			case MID_DATA:		SMP = 0; break;
			case END_DATA:		SMP = 1; break;
			case SPI_ON:		// SPI
	// SSPCON: | WCOL | SSPOV | SSPEN | CKP | SSPM3 | SSPM2 | SSPM1 | SSPM0 |
	SSPEN = 1; // (00110010) - ��������� SPI, ������� ������� CLK (CKP=1), ������� Fosc/64
	// SSPSTAT: | SMP | CKE | - | - | - | - | - | BF |
//	SSPSTAT = 0; // ����� ������ SPI:  SMP=0 - ����� ����� � �������� �������
					// CKE=0 - ������ ���������� �� ������� ������
								SSPIE = 0; break;
			case SPI_OFF:		SSPEN = 0; SSPIE = 0; break;
			case SPI_ACTIVE:	SSPCON = 0x32; TRISC = 0x10; CKE = 0; SSPIE = 0; break; //TRISC = 0xC0;
			case SPI_PASSIVE:	SSPCON = 0x35; TRISC = 0x18; CKE = 0; SSPIE = 1; break;//TRISC = 255;
			case HIG_SPD:		SPBRG = 1; break; // 115200
			case MID_SPD:		SPBRG = 12; break; // 19200
			case TEST:			T1CON = 0; TMR1IF = 0; break;
		}
	}
	if(TMR1IF == 1) // ��������� ���������� �� �������
		timer1int(); // ���������� ����������
	if(SSPIF == 1) // ���������� �� SPI
		SPI_int(); // ������������
}

void main(){ // �������� ����
	init();
	while(1){};
}

