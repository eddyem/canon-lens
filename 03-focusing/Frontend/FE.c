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
#include "signals.h" // определения сигналов связи

#define C_NO	224 // номер контроллера (старшие 3 бита)
int BAUD_RATE = B9600;

struct termio oldtty, tty; // флаги для UART
struct termios oldt, newt; // флаги для консоли
//struct serial_struct old_extra_term;
int comfd; // Файловый дескриптор порта
char *comdev = "/dev/ttyS0"; // Устройство порта (потом переделать для запроса)

void tty_sig(unsigned char rb);

unsigned char crc(unsigned char data){
	unsigned char crc = data & 1;
	unsigned int i;
	for(i = 1; i<8; i++) crc ^= (data >> i) & 1;
	return crc;
}

void quit(int ex_stat){ // выход из программы
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt ); // возвращаем консоль в начальное состояние (на всякий случай)
	ioctl(comfd, TCSANOW, &oldtty ); // восстанавливаем режим работы com
	close(comfd); // закрываем соединение
	printf("Выход... (сигнал %d)\n", ex_stat);
	exit(ex_stat);
}

void tty_init(){
	printf("\nОткрываю порт...\n");
	if ((comfd = open(comdev,O_RDWR|O_NOCTTY|O_NONBLOCK)) < 0){
		fprintf(stderr,"Can't use port %s\n",comdev);
		quit(1);
	}
	printf(" OK\nПолучаю текущие настройки порта...\n");
	ioctl(comfd,TCGETA,&oldtty); // Узнаем текущие параметры порта
	tty = oldtty;
	tty.c_lflag    = 0; // ~(ICANON | ECHO | ECHOE | ISIG)
	tty.c_iflag    = BRKINT; // подготовка к 9битной передаче 
	tty.c_oflag    = 0;
	tty.c_cflag    = BAUD_RATE|CS8|CREAD|CLOCAL|PARENB; // 9.6к, 8N1, RW, игнорировать контр. линии
	tty.c_cc[VMIN] = 0;  // не канонический режим
	tty.c_cc[VTIME] = 5; // (без символов ВК и пр.)
	if(ioctl(comfd,TCSETA,&tty)<0) exit(-1); // Устанавливаем текущие параметры порта
	printf(" OK\n");
}

unsigned char read_tty(unsigned char *byte){
	*byte = 0;
	fd_set rfds; // набор файловых дескрипторов
	struct timeval tv; // время ожидания
	int retval; // возвращаемое ф-й select значение
	tty.c_iflag &= ~PARODD;
	ioctl(comfd,TCSETA,&tty);
// Ждем, сигнала с порта
	FD_ZERO(&rfds); // очищаем набор 
	FD_SET(comfd, &rfds); // теперь это - свойства порта
	tv.tv_sec = 0; tv.tv_usec = 50000; // ждем
	retval = select(comfd + 1, &rfds, NULL, NULL, &tv);
	if (!retval) return 0; // если сигнала нет, возвращаем ноль
	if(FD_ISSET(comfd, &rfds)){
//		printf("ready  ");
		if(read(comfd, byte, 1) < 1) return 0; // ошибка считывания
	} 
	else return 0; // ошибка
/*	if(*byte == 8){
		return 0;
		
	}
	else*/
//		printf("считан: %d (команда %d)\n", *byte, *byte & 0x1f);
	return 1;
}

unsigned char read_console(){ // считываем данные с консоли
	unsigned char rb;
	struct timeval tv;
	int retval;
	tcgetattr( STDIN_FILENO, &oldt ); // открываем терминал для реакции на клавиши без эха
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds); // 0 - стандартный вход
	tv.tv_sec = 0; tv.tv_usec = 10000; // ждем 0.01с
	retval = select(1, &rfds, NULL, NULL, &tv);
	if (!retval) rb = 0;
	else {
		if(FD_ISSET(STDIN_FILENO, &rfds)) rb = getchar();
		else rb = 0;
	}
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );	
	return rb;
}

unsigned char mygetchar(){ // аналог getchar() без необходимости жать Enter
	unsigned char ret;
	do ret = read_console();
	while(ret == 0);
	return ret;
}

void write_tty_raw(unsigned char wb){
	if(crc(wb)) // нечетная сумма
		tty.c_cflag |=PARODD; // 9-й бит = 0
	else
		tty.c_cflag &= ~PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // перенастраиваем порт
	if(write(comfd, &wb, 1) < 0) fprintf(stderr, "Ошибка! Запись не удалась :(\n"); // и пишем в него байт
}

void write_tty(unsigned char wb){
	unsigned char tmp = wb;
	tmp &= 0x1f; // на всякий случай очищаем от мусора
	tmp |= C_NO; // добавляем к команде 
	write_tty_raw(tmp);
}

int send_cmd(unsigned char cmd){ // посылка контроллеру команды с контролем
	unsigned char rtn=ERR_CMD, byte=0, i=0;
	while(i<2 && rtn != 1){ 
		write_tty(cmd);
		usleep(100000);
		rtn = read_tty(&byte);
		byte &= 0x1f;
		i++;
	}
	if(byte != cmd){
		printf("\n\n!!!!!!  Ошибка! Контроллер не отвечает (ответ %d на команду %d)  !!!!!\n\n", byte, cmd);
		return 0;
	}
	else return 1;
}
	
void sendword(unsigned int data){
	unsigned char tmp,i=0,rr=0, ret;
	tmp = (data >> 8) & 0xFF;
	if(crc(tmp)) // нечетная сумма
		tty.c_cflag &= ~PARODD; // 9-й бит = 0
	else
		tty.c_cflag |= PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // перенастраиваем порт
	write(comfd, &tmp, 1);
	do{ i++; 
		usleep(10000);
		ret = read_tty(&rr); rr &= 0x1F;
	}while((ret == 0 || rr != OK) && i < 10 );
	if(rr != OK){ fprintf(stderr, "Ошибка записи"); return;}
	i = 0;
	tmp = data & 0xFF;
	if(crc(tmp)) // нечетная сумма
		tty.c_cflag &= ~PARODD; // 9-й бит = 0
	else
		tty.c_cflag |= PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // перенастраиваем порт
	write(comfd, &tmp, 1);
	do{ i++; 
		usleep(10000);
		ret = read_tty(&rr); rr &= 0x1F;
	}while((ret == 0 || rr != OK) && i < 10 );
	if(rr != OK){fprintf(stderr, "Неудача"); return;}
}

void send_rand(){
	int i, j = 0; unsigned char byte, rb;
	srand(time(NULL));
	for(i=0; i<128; i++)
		if(send_cmd(SPI_send_one)){
			write_tty_raw(25);
			printf("%d: передан байт \e[1;32;40m18\e[0m\n",i);
			if(read_tty(&byte))
				tty_sig(byte);
			rb = read_console();
			if(rb == 'q') break;
			//byte = (rand()|rand()) & 0xff;
			byte = j++;
			if(send_cmd(SPI_send_one)){
				write_tty_raw(byte);
				printf("%d: передан байт \e[1;32;40m%d\e[0m\n", i, byte);
				if(read_tty(&byte))
					tty_sig(byte);
				rb = read_console();
				if(rb == 'q') break;	
			}
			if(send_cmd(SPI_send_one)){
				write_tty_raw(0);
				printf("%d: передан байт \e[1;32;40m0\e[0m\n", i);
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
			printf("%d: передан байт \e[1;32;40m%d\e[0m\n", i, byte);
			if(read_tty(&byte))
				tty_sig(byte);
			rb = read_console();
			if(rb == 'q') return;
			for(j=0; j<5; j++)
				if(send_cmd(SPI_send_one)){
				write_tty_raw(0);
				printf("%d: передан байт \e[1;32;40m0\e[0m\n", i);
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
	printf("\nВводите числа по одному в строке, окончание - слово end\n");
	do{
		n = getline(&line, &sz, stdin);
		if(n == 0 || strstr(line, "end") != NULL) break;
		seq[cntr++] = (unsigned char)atoi(line);
	}while(cntr < 1024);
	printf("\nНачинаю передачу\n");
	for(n=0; n<cntr; n++){
		if(send_cmd(SPI_send_one)){
			write_tty_raw(seq[n]);
			printf("Передан байт \e[1;32;40m%d\e[0m\n", seq[n]);
			if(read_tty(&byte))
				tty_sig(byte);
		}
	}
	free(line);
	printf("\nПередача окончена\n");
}


void send_same(){
	int i; unsigned char cmd, byte, rb;
	printf("\nВведите команду: ");
	scanf("%d", &i);
	cmd = i;
	for(i=0; i<1024; i++)
		if(send_cmd(SPI_send_one)){
			byte = (i%16) ? 0:cmd;
			write_tty_raw(byte);
			printf("%d: передан байт \e[1;32;40m%d\e[0m\n", i, byte);
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
	printf("Период таймера: T=%f", period);
}

void help(){
	printf("\nh\tПомощь\n"
		"t\tУзнать текущий период таймера\n"
		"T\tСменить период таймера\n"
		"s\tОтослать введенную с клавиатуры команду по SPI\n"
		"i\t(Ре)инициализировать контроллер\n"
		"m\tДанные считываются в середине такта\n"
		"e\tДанные считываются в конце такта\n"
		"R\tОтсылать случайные числа (1024 штуки)\n"
		"L\tСкорость 9600\n"
		"M\tСкорость 19200\n"
		"1..3\tУстановить скорость изменения F\n"
		"f\tПереместиться в сторону бесконечности\n"
		"b\tПереместиться в сторону нуля\n"
		"0\tПереместиться в начало\n"
		"9\tПереместиться в бесконечность\n"
		"H\tВключить ручное управление\n"
		"p\tОстановить таймер\n"
		"P\tЗапустить таймер\n"
		"y\tОтключить SPI\n"
		"Y\tВключить SPI\n"
		"R\tПосылать случайные числа\n"
		"F\tФокусное расстояние\n"
		"S\tОтослать последовательность байт (не более 1024, завершение последовательности - end)\n"
		"w\tОтсылать последовательность одинаковых введенных команд\n"
		"W\tОтсылать последовательность 0..255 с промежуточными 5 нулями\n"
		"\n");
}

void set_speed(int spd){
	//B115200 или B9600
	BAUD_RATE = spd;
	ioctl(comfd, TCSANOW, &oldtty ); // восстанавливаем режим работы com
	close(comfd); // закрываем соединение
	tty_init();
}

void con_sig(unsigned char rb){ // обработка сигналов с консоли
	unsigned int tmp; unsigned char cmd;
	if(rb == 'q') quit(0); // выходим по нажатию q
	switch(rb){
		case 'h':	help(); break;
		case 't':	tmrset(); break;
		case 'T':	printf("\nПериод в мкс:\n"); scanf("%u", &tmp);
					if(send_cmd(SET_TIMER)) sendword(65536 - tmp/8); break;
		case 'i':	send_cmd(INIT); break;
		case 's':	printf("\nДанные:\n"); scanf("%d", &cmd);
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

void tty_sig(unsigned char rb){ // обработка сигналов с контроллера
	char bin[9];
	dec2bin(rb, bin);
	switch(rb){
		case ERR_CMD: printf("Ошибка (\e[1;31;40m%d\e[0m?)\n", ERR_CMD); break;
		case OK: printf("OK (\e[1;31;40m%d\e[0m?)\n", OK); break;
		default: printf("\e[1;31;40m%d\e[0m\t(%s)\n", rb, bin);
	}
}

int main(int argc, char *argv[]){
	unsigned char rb, byte, i = 0; // считанные данные, счетчик
	tty_init();
	rb = ERR_CMD;
	signal(SIGTERM, quit);		// kill (-15)
	signal(SIGINT, quit);		// ctrl+C
	signal(SIGQUIT, SIG_IGN);	// ctrl+\   .
	signal(SIGTSTP, SIG_IGN);	// ctrl+Z
	setbuf(stdout, NULL);
	printf("\nИнициализация...\n");
	do{
	    write_tty(INIT); // Инициализация контроллера
	    rb = read_tty(&byte);
	    usleep(100000);
	    i ++;
	}
	while(i<3 && rb != 1); // проверка связи
	if(i > 2 || (byte&0x1f) != INIT){
		fprintf(stderr,"\n!!!!!!  Ошибка: контроллер не отвечает (%d) !!!!!!\n", byte);
	}
	else printf(" OK\n");
	while(1){ // бесконечно зацикливаемся
		rb = read_console();
		if(rb != 0) con_sig(rb); // если что-то появилось - обрабатываем
		rb = read_tty(&byte);
		if(rb != 0) tty_sig(byte); // если есть сигнал с контроллера - обрабатываем

	}
}
