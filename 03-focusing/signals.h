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
// �������� � ���������� ���� ������ � �������� � SPI
#define	SPI_send_one	1
// ������ ������ SPI
#define MID_DATA	2
#define END_DATA	3
// �������� ����� (�� ��������� 9600)
#define MID_SPD		4	// 19200
#define LOW_SPD		5	// 9600
// �������� ��������� (�����, ����� ������� ����� ������ ������ "����")
#define SPEED1		6	// .3c
#define SPEED2		7	// .05c
#define SPEED3		8	// .01c
// ����������� ��������
#define FORW		9
#define BACK		10
#define INFTY		11
#define ZERO		12
// ������ �����
#define FOCUS		19
// ������ ����� ����������
#define HANDS		13
#define TMR_ON		14
#define TMR_OFF		15
#define SPI_ON		16
#define SPI_OFF		17
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
