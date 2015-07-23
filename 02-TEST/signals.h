/*********** ������� ***********************/
void send9bit(unsigned char something);
unsigned char get9bit();
void sendword(unsigned char data_H, unsigned char data_L);
unsigned char getword();
unsigned char ten_times_read();
void init();
void timer1set();
void timer1int();
void SPI_int();

/***********	�������� ������ ************/
// �������� � ���������� ��� ����� ������ � �������� �� � SPI
#define	SPI_send	1
// ������ ������ SPI
#define IMP_RISE	2
#define IMP_FALL	3
#define SPI_ON		4
#define SPI_OFF		5
#define SPI_ACTIVE	6
#define SPI_PASSIVE	7
#define MID_DATA	8
#define END_DATA	9


#define SPI_SHOW	14

// �������� ����� (�� ��������� 9600)
#define MID_SPD		15
#define HIG_SPD		16
// �������� � ���� ���� ���������� ����
#define SPI_send_one	17
// ������� ������ �������
#define TMR_SETTINGS	18
// ���������� �������� �������
#define SET_TIMER	26
// �����
#define INIT		28
#define TEST		29
/************** ������ � ������� ����������� ******************/
// ��� � �������
#define OK		22
// ��� ��������� ����
#define NO_STOP_BIT	24
// ������������ ���������
#define STACK_OVERFLOW	25
// ��������� �������
#define ERR_CMD		31

#define TWOBYTE		33	// ����������� �������
