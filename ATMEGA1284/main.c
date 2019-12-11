#include <avr/io.h>
#include "usart_ATmega1284.h"
#include "scheduler.h"
#include "lcd.h"
#include "bit.h"
#include <string.h>
#include <stdlib.h>

volatile long long TimerOverflow = 0;
unsigned char joystick = -1;
unsigned char direction = 0;
unsigned char speed = 1;

long count;
char sonar_string[16];
char joy_string[16];
char speed_string[16];
double distance;

void Output_LCD() {
	LCD_ClearScreen();
	for (unsigned int j = 0; j < strlen(sonar_string); j++) {		//Output Sonar Distance
		LCD_Cursor(j + 1);
		LCD_WriteData(sonar_string[j]);
	}
	for (unsigned int j = 0; j < strlen(joy_string); j++) {			//Output Joystick Magnitude and Speed
		LCD_Cursor(j + 17);
		LCD_WriteData(joy_string[j]);
	}
}
ISR(USART0_RX_vect) {
	UCSR0B &= ~(1 << RXCIE0);										//Disable UART Interrupt
	tasks[2].elapsedTime = 0;										//Re-enable UART Interrupt after 25 ms
	direction = (PIND & 0x0C) >> 2;									//Read Direction
	joystick = USART_Receive(0);									//Read joystick 0-255
	dtostrf((double)(joystick), 2, 0, joy_string);					//Create LCD Output String
	dtostrf((double)(6 - (joystick / 65)), 2, 0, speed_string);
	strcat(joy_string, "/256JOY ");
	strcat(joy_string, speed_string); strcat(joy_string, " SPD");
	
	tasks[1].period = 6 - (joystick / 65);							//Set Drive_TickFct Period 3-6 ms
	Output_LCD();													//Output to LCD Every Time a New Value is Received
}
ISR(TIMER3_OVF_vect) {												//TimerOverflow variable is necessary because Timer3 may overflow (2^16 values) since Sonar waits for Input Capture Flag 3
	TimerOverflow++;
}
void Timer3On() {
	TIMSK3 |= (1 << TOIE3);											//Enable Timer Overflow Interrupt
	TCNT3 = 0;														//Reset Timer3 Count
	SREG |= 0x80;													//Enable Global Interrupts
	TCCR3B |= (1 << CS30);											//Timer3 no prescale (2^16 values)
	TCCR3A = 0x00;													//Normal Mode
}
void Sonar_Pulse() {
	TCCR3B |= (1 << WGM32);											//CTC Mode
	OCR3A = 120;													//8Mhz * 15us = 120
	PORTB |= (1 << 4);												//Trigger High
	Timer3On();
	while ((TIFR3 & (1 << OCF3A)) == 0);							//Count to 15us
	PORTB &= ~(1 << 4);												//Trigger Low
	TCCR3B &= ~(1 << WGM32);										//CTC Off (Normal Mode)
}
void Sonar_Read() {
	//Rising Edge
	TCCR3B |= (1 << ICES3);											//Input Capture Flag trigger on RISING EDGE
	TIFR3 |= (1 << ICF3) | (1 << TOV3);								//Clear Overflow and Input Capture Flags
	Timer3On();
	while ((TIFR3 & (1 << ICF3)) == 0);								//Wait for Input Capture Flag RISING EDGE
	
	//Falling Edge
	TCCR3B &= ~(1 << ICES3);										//Input Capture Flag trigger on FALLING EDGE
	TIFR3 |= (1 << ICF3) | (1 << TOV3);								//Clear Overflow and Input Capture Flags
	Timer3On();
	TimerOverflow = 0;												//Reset TimerOverflow
	while ((TIFR3 & (1 <<ICF3)) == 0);								//Wait for Input Capture Flag FALLING EDGE
}
void Sonar_Analyze() {
	count = ICR3 + (65535 * TimerOverflow);							//65535 = (2^16 - 1)
	distance = (double)count / 466.47;
	dtostrf(distance, 2, 2, sonar_string);
	strcat(sonar_string, "cm DIST");
}

enum tick_state {read} tick_states;
int Sonar_TickFct(int state) {
	Sonar_Pulse();
	Sonar_Read();
	Sonar_Analyze();
	Output_LCD();
	return state;
}

const unsigned char angles[4] = {0x05, 0x06, 0x0A, 0x09};
int Drive_TickFct(int state) {
	static unsigned int Left_Index = 0;
	static unsigned int Right_Index = 0;
	unsigned char tempA = 0x00;
	tempA = (angles[Left_Index] << 4) | angles[Right_Index];
	if (direction == 0x00) {	 //Forward
		if (Left_Index >= 3)	{Left_Index = 0;}
		else					{Left_Index++;}
		if (Right_Index >= 3)	{Right_Index = 0;}
		else					{Right_Index++;}
	}
	else if (direction == 0x01) {//Backward
		if (Left_Index <= 0)	{Left_Index = 3;}
		else					{Left_Index--;}
		if (Right_Index <= 0)	{Right_Index = 3;}
		else					{Right_Index--;}
	}
	else if (direction == 0x02) {//Left
		if (Left_Index <= 0)	{Left_Index = 3;}
		else					{Left_Index--;}
		if (Right_Index >= 3)	{Right_Index = 0;}
		else					{Right_Index++;}
	}
	else if (direction == 0x03) {//Right
		if (Left_Index >= 3)	{Left_Index = 0;}
		else					{Left_Index++;}
		if (Right_Index <= 0)	{Right_Index = 3;}
		else					{Right_Index--;}
	}
	
	if (joystick > 0) {
		if (direction == 0x00) {
			if (distance > 20) {PORTA = tempA;}
		}
		else {PORTA = tempA;}
	}
	
	return state;
}

int Enable_UART_Int(int state) {
	UCSR0B |= (1 << RXCIE0);
	return state;
}

int main(void) {
	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0xDF; PORTB = 0x20;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xC0; PORTD = 0x3F;
	
	LCD_init();
	LCD_ClearScreen();
	initUSART(0);
	
	tasksNum = 3;
	task tsks[3];
	tasks = tsks;
	
	unsigned char i = 0;
	tasks[i].state = -1;
	tasks[i].period = 333;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &Sonar_TickFct;
	i++;
	tasks[i].state = -1;
	tasks[i].period = 8;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &Drive_TickFct;
	i++;
	tasks[i].state = -1;
	tasks[i].period = 25;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &Enable_UART_Int;
	
	TimerSet(1);
	Timer3On();
	TimerOn();
	
	while (1)
	{
		
	}
}

