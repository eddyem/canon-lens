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
// получить с компьютера два байта данных и записать их в SPI
#define	SPI_send	1
// режимы работы SPI
#define IMP_RISE	2
#define IMP_FALL	3
#define SPI_ON		4
#define SPI_OFF		5
#define SPI_ACTIVE	6
#define SPI_PASSIVE	7
#define MID_DATA	8
#define END_DATA	9


#define SPI_SHOW	14

// скорости порта (по умолчанию 9600)
#define MID_SPD		15
#define HIG_SPD		16
// записать в порт один полученный байт
#define SPI_send_one	17
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
