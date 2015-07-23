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
#include "signals.h" // определения сигналов связи
#include "func.h" // функции обработки сигналов, глобальные переменные
#include "keys.h" // клавишные команды
#include "killbrothers.h" 
#include "queues.h"

#define CMSPAR 010000000000
extern int get_auth_level(); 
FILE* outfile;

double tm0, tm; // время в секундах

struct termio oldtty, tty; // флаги для UART
int comfd = -1; // Файловый дескриптор порта
static char *comdev = "/dev/ttyS1"; // Устройство порта (потом переделать для запроса)

unsigned char crc(unsigned char data){
	unsigned char crc = data & 1;
	unsigned int i;
	for(i = 1; i<8; i++) crc ^= (data >> i) & 1;
	return crc;
}

double dtime(){
  struct timeval tv;
  struct timezone tz;
  int ret;
  double t;
  ret = gettimeofday(&tv, &tz);
  t = (double)tv.tv_sec + (double)tv.tv_usec*0.000001;
  t -= timezone;
  return(t);
}

void quit(int ex_stat){ // выход из программы
	unsigned char i;
	err("Отключаюсь...");
	if(comfd > 0){
		for(i=0; i<8; i++)
			stop_motor(i<<5, "0"); // останавливаем двигатель
		ioctl(comfd, TCSETA, &oldtty); // восстанавливаем режим работы com
		close(comfd); // закрываем соединение
	}
	rm_queues();
	exit(ex_stat);
}

void tty_init(){
	if ((comfd = open(comdev,O_RDWR|O_NOCTTY|O_NONBLOCK)) < 0)
		die("Can't use port %s\n",comdev);
	ioctl(comfd,TCGETA,&oldtty); // Узнаем текущие параметры порта
	tty = oldtty;
	tty.c_lflag    = 0;
	tty.c_iflag    = BRKINT;//|PARENB|CMSPAR;
	tty.c_oflag    = 0;
	tty.c_cflag    = B9600|CS8|CREAD|CLOCAL|PARENB;//|CMSPAR;
	tty.c_cflag &= ~PARODD;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 1;
	ioctl(comfd,TCSETA,&tty);
}

unsigned char read_tty(){ // считываем 1 байт (с ожиданием сигнала)
	unsigned char rb; // то, что считываем
	tty.c_iflag &= ~PARODD; // принимаем с девятым битом = 0
	ioctl(comfd,TCSETA,&tty);
	if(read(comfd, &rb, 1) < 1) return 0; // ошибка считывания
	tm = dtime() - tm0;
	tm0 = dtime();
	fprintf(stderr,"считан сигнал: %d с контроллера %d, время от предыдущего считывания: %f с\n", rb&0x1F, rb&0xE0, tm);	
	return rb;
}

void write_tty_raw(unsigned char wb){
	if(crc(wb)) // нечетная сумма
		tty.c_cflag |=PARODD; // 9-й бит = 0
	else
		tty.c_cflag &= ~PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // перенастраиваем порт
	if(write(comfd, &wb, 1) < 0) err("Ошибка! Запись не удалась :(\n"); // и пишем в него байт
}

void write_tty(unsigned char wb, unsigned char cno){
	unsigned char tmp = wb;
	tmp &= 0x1f; // на всякий случай очищаем от мусора
	tmp |= cno; // добавляем к команде 
	write_tty_raw(tmp);
}

int send_cmd(unsigned char cmd, unsigned char flag, unsigned char cno){ // посылка контроллеру команды с контролем
	// flag = 1 - команда отсылается текущему контроллеру, 0 - по запросу
	int ret = -1;
	unsigned char rtn=0, i=0, cmd1;
	cmd1 = (flag)? ((cmd&0x1F)|cno) : cmd;
	while(i<10 && rtn != cmd1){ // 10 попыток
		usleep(1000);
		if(flag) write_tty(cmd, cno);
		else write_tty_raw(cmd);
		usleep(100000);
		rtn = read_tty();
		i++;
	}
	if(rtn != cmd1)
		err("!!!!!!  Ошибка! Контроллер не отвечает  !!!!!");
	else ret = (i<2)?1:0;
	return ret;
}

void sendword(unsigned int data){
	unsigned char tmp,i=0,rr=0;
	tmp = (data >> 8) & 0xFF;
	if(crc(tmp)) // нечетная сумма
		tty.c_cflag &= ~PARODD; // 9-й бит = 0
	else
		tty.c_cflag |= PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // перенастраиваем порт
	do{ i++; 
	if(write(comfd, &tmp, 1) < 0){continue;}
	usleep(10000);
	rr = read_tty() & 0x1F;
	}while(rr != OK && i < 10);
	if(rr != OK){ err("Ошибка записи"); return;}
	i = 0;
	tmp = data & 0xFF;
	if(crc(tmp)) // нечетная сумма
		tty.c_cflag &= ~PARODD; // 9-й бит = 0
	else
		tty.c_cflag |= PARODD; // = 1
	ioctl(comfd,TCSETA,&tty); // перенастраиваем порт
	do{ i++; 
	if(write(comfd, &tmp, 1) < 0){continue;}
	usleep(10000);
	rr = read_tty() & 0x1F;
	}while(rr != OK && i < 10);
	if(rr != OK){err("Неудача"); return;}
}

/*
int getl(int fd, char* data, int N){
	int rb = 0, i=0;
	char *ptr = data;
	do{
		if((rb = read(fd, ptr, 1)) != 1) break;
		if(*ptr == '\0' || *ptr == '\n') break;
		ptr++;
	}while(++i < N);
	if(rb != 1 && i == 0) return (-1);
	*ptr = '\0';
	return i;
}*/

void tmrset(unsigned char cno){
	unsigned char t_h,t_l;
	float period;
	if(send_cmd(TMR_SETTINGS, 1, cno)<0) return;
	usleep(10000);
	read(comfd, &t_h, 1);
	usleep(10000);
	read(comfd, &t_l, 1);
	period = (65536.0 - 256.0*(float)t_h - (float)t_l)/125000.0;
	warn("Период таймера: T=%f", period);
}

void init_ctrlr(unsigned char cno){
	unsigned char rb = ERR_CMD;
	unsigned char ini = INIT | cno;
	int i=0;
	warn("Инициализация...");
	do{
	    write_tty(INIT, cno); // Инициализация контроллера
	    rb = read_tty();
	    usleep(100000);
	    i++;
	}
	while(i<10 && rb != ini); // проверка связи
	if(i > 9 || rb != ini){
		err("!!!!!!  Ошибка: контроллер не отвечает  !!!!!!");
	}
	else{
		warn("... успешно.");
	}
}

void con_sig(unsigned char cno, unsigned char rb, char *val){ // обработка сигналов с консоли
	//fprintf(stderr, "key: %c, cno: %d, val:%s\n", rb, cno, val);
	switch(rb){
		case KEY_QUIT:		quit(0); break;
		case KEY_DEVICE:	set_dev(cno, val); break;
		case KEY_LEFT:		rotate_left(cno, val); break;
		case KEY_RIGHT:		rotate_right(cno, val); break;
		case KEY_NR:		steps_(cno,RIGHT, val); break;
		case KEY_NL:		steps_(cno,LEFT, val); break;
		case KEY_STOP:		stop_motor(cno, val); break;
		case KEY_VOLTAGE:	set_voltage(cno, val); break;
		case KEY_TMR:		set_timer(cno, val); break;
		case KEY_TMR_STOP:	send_cmd(STOP_TIMER, 1, cno); break;
		case KEY_TMR_STNGS:	tmrset(cno); break;
		case KEY_INIT:		init_ctrlr(cno); break;
		case KEY_Xplus:		corrector(cno,0, val); break;
		case KEY_Xminus:	corrector(cno,1, val); break;
		case KEY_Yplus:		corrector(cno,2, val); break;
		case KEY_Yminus:	corrector(cno,3, val); break;
		case KEY_MIDDLE:	goto_middle(cno); break;
		case KEY_TEST:		send_cmd(TEST,1, cno); break;
		
/*		default:		send_cmd(rb, 0); // спец. команда (для выделенных контроллеров)
					data = read_int();
					if(data != -1) sendword(data); */
	}
}

void tty_sig(unsigned char rb){ // обработка сигналов с контроллера
	unsigned char cno = rb & 0xE0;
	rb &= 0x1F;
	switch(rb){
		case TERMINAL_: terminals(cno); break; // обработка концевиков
		case STACK_OVERFLOW: error(STACK_OVERFLOW, cno); break; // переполнение стека RX контроллера
		case NO_STOP_BIT: error(NO_STOP_BIT, cno); break;
	}
}

int main(int argc, char *argv[]){
	unsigned char rb, key;// считанные данные, счетчик
	char *val; 
	setbuf(stdout, NULL);
	printf("Content-type: multipart/form-data; charset=koi8-r\n\n");
	if(!killbrothers()) die("Не могу гарантировать монопольный доступ..");
	printf("Перехожу в режим демона\n");
	umask(0);
	mk_queues();
	val = calloc(512, 1);
	tty_init();
	close(0); close(1); close(2); // отключаемся от терминала
	if(fork() != 0) exit(0); // закрываем родительский поток
	if(argc > 1){ //comdev = argv[1];
	if(strcmp(argv[1], "XY") == 0){
		warn("Режим XY-актуаторов\n");
		if(init_motor(MOTOR_X)==0){ // двигатель Y
			err("Y: Ошибка !");
		}
		if(init_motor(MOTOR_Y)==0){ // двигатель X
			err("X: Ошибка!");
		}
	}
	}
	else
	warn("Обычный режим работы");
	signal(SIGTERM, quit);		// kill (-15)
	signal(SIGINT, quit);		// ctrl+C
	signal(SIGQUIT, SIG_IGN);	// ctrl+\   .
	signal(SIGTSTP, SIG_IGN);	// ctrl+Z
	tm0 = dtime();
	while(1){ // бесконечно зацикливаемся
		if(read_queue(&rb, &key, val) > 0){
			if(key != 0) con_sig(rb, key, val); // если что-то появилось - обрабатываем
		}
		rb = read_tty();
		if(rb != 0) tty_sig(rb); // если есть сигнал с контроллера - обрабатываем
	}
}

