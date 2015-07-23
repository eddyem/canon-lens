#include	<pic16f873a.h>
#include "signals.h"
#define BUFSIZE	95
//#define CLRWDT	_asm clrwdt _endasm;
typedef unsigned int word;
word at 0x2007 CONFIG = 0x3F72; // ��� WDT: 0x3F76
unsigned char
	cmd,		// ���������� �������
	d_H, d_L,	// ������� � ������� ����� ����������� �������
	T1H, T1L,	// �������� ��������� �������� ������� 1
	mC_addr,	// ���. ����� ����������� (���������, ��������������� �������� init)
	tmr_on;		// ==1 - ������ �������

unsigned char write_SPI(unsigned char);
void hands(){
	unsigned char i;
	write_SPI(10); // �������������
	write_SPI(0);
/*	write_SPI(128);
	for(i=0; i<20; i++)
		write_SPI(0);*/
	write_SPI(94); // ����
	for(i=0; i<5; i++)
		write_SPI(0);
	T1CON = 0;
}

void focus(){
	unsigned char i;
	write_SPI(194);
	TX9D = 0; 
	for(i=0; i<5; i++){
		TXEN = 1;
		TXREG = write_SPI(0);
		while(!TRMT);
	}
}

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
		return NO_STOP_BIT;
	}
	if(err2 == 1){
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
	TMR1IF = 0;
	TMR1IE = 0;
	TXEN = 1;
	TX9D = 1;
	TXREG = data_H;
	while(!TRMT);
	TXEN = 1;
	TX9D = 1;
	TXREG = data_L;
	RCIE = 1;
	if(tmr_on) TMR1IE = 1;
}

unsigned char getword(){
	unsigned char ret = 0;
	RCIE = 0; // disable USART in interrupt
	TMR1IF = 0;
	TMR1IE = 0;
	if(ten_times_read()){
		d_H = cmd;
		if(ten_times_read()){
			d_L = cmd;
			ret = 1;}} // ���� ��� ����� ������� ���������
	if(tmr_on) TMR1IE = 1;
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
	// SSPCON: | WCOL | SSPOV | SSPEN | CKP | SSPM3 | SSPM2 | SSPM1 | SSPM0 |
	SSPCON = 0x32; // (00110010) - �������� SPI, ������� ������� CLK (CKP=1), ������� Fosc/64
	// SSPSTAT: | SMP | CKE | - | - | - | - | - | BF |
	SSPSTAT = 0; // ����� ������ SPI:  SMP=0 - ����� ����� � �������� �������
					// CKE=0 - ������ ���������� �� ������� ������
	TRISC = 0xD0; SSPIE = 1;
	tmr_on = 0;
	hands();
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
	tmr_on = 1;
} 

unsigned char write_SPI(unsigned char byte){
	unsigned char ans;
	TMR1IF = 0;
	if(tmr_on){
		T1CON = 0; // ��������� ������
		TMR1IE = 0;
	}
	SSPBUF = byte;
	while(!SSPIF); // ���� ��������� ��������
	ans = SSPBUF; // ������������ ���������� ����
	SSPIF = 0;
	SSPIE = 1;
	if(tmr_on){
		TMR1H = T1H; TMR1L = T1L;
		T1CON = 0x31; // ��������� ������
		TMR1IE = 1;
	}
	return ans;
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
			case SPI_send_one:	while(!RCIF); 
				TX9D = 0; TXEN = 1; TXREG = write_SPI(RCREG); break;
			case SPI_ON:		SSPEN = 1; SSPIE = 1; break;
			case SPI_OFF:		SSPEN = 0; SSPIE = 0; break;
			case MID_DATA:		SMP = 0; break;
			case END_DATA:		SMP = 1; break;
			case MID_SPD:		SPBRG = 12; break; // 19200
			case LOW_SPD:		SPBRG = 25; break; // 9600
			case SPEED1:		d_H = 0x6D; d_L = 0x84; timer1set(); break; // 0.3�
			case SPEED2:		d_H = 0xE7; d_L = 0x96; timer1set(); break; // 0.05�
			case SPEED3:		d_H = 0xFB; d_L = 0x1E; timer1set(); break; // 0.01�
			case FORW:			write_SPI(5); break;
			case BACK:			write_SPI(6); break;
			case INFTY:			write_SPI(37); break;
			case ZERO:			write_SPI(22); break;
			case HANDS:			hands(); break;
			case TMR_OFF:		tmr_on = 0; T1CON = 0; TMR1IE = 0; break;
			case TMR_ON:		tmr_on = 1; T1CON = 0x31; TMR1IE = 1; break;
			case FOCUS:			focus(); break;
		}
	}
	if(TMR1IF == 1 && tmr_on){ // ��������� ���������� �� �������
		write_SPI(4); // �������� ������ ���� � ������������� ������
		write_SPI(0);
	}
}

void main(){ // �������� ����
	init();
	while(1){};
}

