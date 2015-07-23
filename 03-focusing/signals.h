/*********** Функции ***********************/
void send9bit(unsigned char something);
unsigned char get9bit();
void sendword(unsigned char data_H, unsigned char data_L);
unsigned char getword();
unsigned char ten_times_read();
void init();
void timer1set();
void timer1int();
void SPI_int();

/***********	Перечень команд ************/
// получить с компьютера байт данных и записать в SPI
#define	SPI_send_one	1
// режимы работы SPI
#define MID_DATA	2
#define END_DATA	3
// скорости порта (по умолчанию 9600)
#define MID_SPD		4	// 19200
#define LOW_SPD		5	// 9600
// скорости объектива (время, через которое будет послан сигнал "стоп")
#define SPEED1		6	// .3c
#define SPEED2		7	// .05c
#define SPEED3		8	// .01c
// направления движения
#define FORW		9
#define BACK		10
#define INFTY		11
#define ZERO		12
// узнать фокус
#define FOCUS		19
// ручной режим управления
#define HANDS		13
#define TMR_ON		14
#define TMR_OFF		15
#define SPI_ON		16
#define SPI_OFF		17
// текущий период таймера
#define TMR_SETTINGS	18
// установить значение таймера
#define SET_TIMER	26
// сброс
#define INIT		28
#define TEST		29
/************** Ошибки и сигналы контроллера ******************/
// все в порядке
#define OK		22
// нет стопового бита
#define NO_STOP_BIT	24
// переполнение регистров
#define STACK_OVERFLOW	25
// ошибочкая команда
#define ERR_CMD		31

#define TWOBYTE		33	// двухбайтная посылка
