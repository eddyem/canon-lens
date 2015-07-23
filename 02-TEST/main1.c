#include	<pic16f873a.h>
#include "signals.h"
//#define CLRWDT	_asm clrwdt _endasm;
typedef unsigned int word;
word at 0x2007 CONFIG = 0x3F72; // для WDT: 0x3F76
unsigned char
	cmd,		// полученная команда
	d_H, d_L,	// старший и младший байты двухбайтной посылки
	T1H, T1L,	// значения регистров счетчика таймера 1
	mC_addr;	// физ. адрес контроллера (константа, устанавливается функцией init)

unsigned char SPI_buf[64], SPI_cntr;

void send9bit(unsigned char something){
	unsigned char tmp;
	something &= 0x1F; // сброс старших трех бит (там будет адрес)
	tmp = mC_addr | something;
	TXEN = 1; // готов к передаче
	TX9D = 0; // 0 - передает контроллер
	TXREG = tmp;	// послать команду
}

unsigned char get9bit(){
	unsigned char err1, err2, flag9bit, tmp;
	while(!RCIF);
	flag9bit = RX9D;
	err1 = FERR; err2 = OERR; // считать 9-й бит и ошибки
	cmd = RCREG; // очистить буфер данных
	RX9D = 0;
	if(err1 == 1){
//		send9bit(NO_STOP_BIT); // не обнаружен стоповый бит
		return NO_STOP_BIT;
	}
	if(err2 == 1){
//		send9bit(STACK_OVERFLOW); // переполнение приемных регистров
		CREN = 0; CREN = 1; // сбросить флаг ошибки
		return STACK_OVERFLOW;
	}
	if(flag9bit) return TWOBYTE; // данные - часть двухбайтной посылки (неизвестно еще чьей :) )
	tmp = cmd & 0xE0; // выделение адреса из команды
	cmd &= 0x1F; // обнуление адресных битов
	if(tmp != mC_addr) return ERR_CMD; // если адресован чужому
	send9bit(cmd); // Эхо принятой команды
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
			ret = 1;}} // если оба байта считали правильно
	TMR1IE = 1;
	RCIE = 1; // enable USART in interrupt
	return ret;
}

unsigned char ten_times_read(){ // 10 попыток чтения для двухбайтного приема
	unsigned char i=0;
	do i++;
	while(get9bit() != TWOBYTE && i < 10);
	if(i > 9){send9bit(ERR_CMD); return 0;}
	send9bit(OK);
	return 1; 
}

void init(){ // инициализация
// Настройка USART'a
// TXSTA: | CSRC | TX9 | TXEN | SYNC | N/A | BRGH | TRMT | TX9D |
	TXSTA = 0x66; // (11000110): master, 9-ти битный ввод/вывод, async, hi-speed, ready
// SPBRG - скорость передачи
	SPBRG = 25; // 9.6 кб/с
// RCSTA: | SPEN | RX9 | SREN | CREN | ADDEN | FERR | OERR | RX9D |
	RCSTA = 0xD0; // ( 11010000): enable, 9bit, continuous mode
// настройка портов:
	PORTA = 0; // 6-ти битный аналогово/цифровой порт (0..5 биты)
// ADCON1: | ADFM | N/A | N/A | N/A | PCFG3 | PCFG2 | PCFG1 | PCFG0 |
	ADCON1 = 0x06; // Аналогово/цифровой порт работает в полностью цифровом режиме
	TRISA = 0; // направление порт А (1-вход, 0-выход) 
	TRISB = 0xff; //           --/ B /--
// OPTION_REG: | !RBPU | INTEDG | TOCS | TOSE | PSA | PS2 | PS1 | PS0 |
	OPTION_REG = 0x7f; /* (01111111) 0 - подключение подтяжек на порт B (уст. лог. 1),
				прерывание по нарастающему фронту RB0,
				таймер 0 работает по сигналу с RA4
				таймер 0 увеличивается при спаде сигнала на RA4
				предделитель подключен к сторожевому таймеру
				режим prescaler:  1:128 */ 
	TRISC = 0xC0; // (11000000) - 0..5 биты как выходы
	INTCON = 0; // отключить все прерывания
	T1CON = 0;
// PIE1: | PSPIE | ADIE | RCIE | TXIE | SSPIE | CCP1IE | TMR2IE | TMR1IE |
	PIE1 = 0x20; // (00100000): enable USART(in)
// PIE2: все N/A, кроме EEIE (PIE2.4)
	PIE2 = 0; //                & disable other int.s
	PORTB = 0;
// INTCON: | GIE | PEIE | T0IE | INTE | RBIE | T0IF | INTF | RBIF |
	INTCON = 0xC0; // (1100000) - включить глобальные прерывания, прерывания по периферии
	PORTC = 0; // без напряжения
	PORTA = 0xF; // (00001111)
// получение адреса
	mC_addr = PORTB; // физический адрес устройства
	mC_addr &= 0xE0; // выделение физического адреса
	SSPEN = 0; SSPIE = 0;
	SPI_cntr = 0;
}

void timer1set(){ // установка таймера
//	unsigned int tmp = 0xffff - usec/8;
	T1CON = 0; // выключить таймер
	TMR1IF = 0; // сбросить флаг прерывания
	TMR1IE = 1; // разр/запр прерывание
	TMR1H = T1H = d_H; //(tmp >> 8) & 0xff;
	TMR1L = T1L = d_L; //tmp & 0xff;  // установить счетчики
// T1CON: | - | - | T1CPS1 | T1CPS0 | T1OSCEN | T1SYNC | TMR1CS | TMR1ON |
	T1CON = 0x31; // (00110001) - включить таймер 1, предделитель на 1/8 (250 кГц)
} 

void timer1int(){ // обработка прерываний первого таймера
	T1CON = 0;
	TMR1H = T1H; TMR1L = T1L; 
//	send9bit(TEST);
	T1CON = 0x31; // снова запускаем таймер
}

void SPI_int(){ //	в пассивном режиме принимаемые данные сохраняются в
// 					буфер, при заполнении буфера он отсылается на ПК
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
	if(SSPOV == 1) // ошибка переполнения буфера
		return;
	if(BF == 0) return; // буфер не заполнен
	RCIE = 0;
	TMR1IE = 0;
	TXEN = 1;
	TXREG = SSPBUF; // отправляем полученный байт
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
	//PORTA &= 0xEF; // -SS = 0 - передаем данные по SPI ведомому
	while(!SSPIF); // ждем окончания передачи
//	SPI_int();

	RCIE = 0;
	TMR1IE = 0;
	TXEN = 1;
	TXREG = SSPBUF; // отправляем полученный байт
	RCIE = 1;
	TMR1IE = 1;
	BF = 0;
	SSPIF = 0;
}

void  on_interrupt() __interrupt 0{ // обработка прерываний
	if(RCIF == 1){	// поступило прерывание от USART
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
			case IMP_RISE:		CKE = 1; break;//данные передаются по заднему фронту
			case IMP_FALL:		CKE = 0; break; //данные передаются по переднему фронту
			case MID_DATA:		SMP = 0; break;
			case END_DATA:		SMP = 1; break;
			case SPI_ON:		// SPI
	// SSPCON: | WCOL | SSPOV | SSPEN | CKP | SSPM3 | SSPM2 | SSPM1 | SSPM0 |
	SSPEN = 1; // (00110010) - выключить SPI, высокий уровень CLK (CKP=1), частота Fosc/64
	// SSPSTAT: | SMP | CKE | - | - | - | - | - | BF |
//	SSPSTAT = 0; // режим работы SPI:  SMP=0 - опрос входа в середине периода
					// CKE=0 - данные передаются по заднему фронту
								SSPIE = 0; break;
			case SPI_OFF:		SSPEN = 0; SSPIE = 0; break;
			case SPI_ACTIVE:	SSPCON = 0x32; TRISC = 0x10; CKE = 0; SSPIE = 0; break; //TRISC = 0xC0;
			case SPI_PASSIVE:	SSPCON = 0x35; TRISC = 0x18; CKE = 0; SSPIE = 1; break;//TRISC = 255;
			case HIG_SPD:		SPBRG = 1; break; // 115200
			case MID_SPD:		SPBRG = 12; break; // 19200
			case TEST:			T1CON = 0; TMR1IF = 0; break;
		}
	}
	if(TMR1IF == 1) // поступило прерывание от таймера
		timer1int(); // обработать прерывание
	if(SSPIF == 1) // прерывание от SPI
		SPI_int(); // обрабатываем
}

void main(){ // основной цикл
	init();
	while(1){};
}

