/*
 * PakSafe.c
 *
 * 
 * Author : Vishal
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "io.c"

int cnt = 0;
volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s
	// AVR output compare register OCR1A.
	OCR1A = 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}
//--------------------END Timer Code------------------------


// Bit-access function
unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
	return (b ? x | (0x01 << k) : x & ~(0x01 << k));
}
unsigned char GetBit(unsigned char x, unsigned char k) {
	return ((x & (0x01 << k)) != 0);
}

enum LCDStates { lcdInit, no_package, package } lcdState;
	
void LCDFSM(unsigned char print){
	switch(lcdState) { //Transitions
		case lcdInit: 
			lcdState = no_package;
			break;
			
		case no_package:
			if(print == 1){
				lcdState = package;
			}
			else{
				lcdState = no_package;
			}
			break;
			
		case package:
			if(print == 0){
				lcdState = no_package;
			}
			else{
				lcdState = package;
			}
			break;
			
		default:
			lcdState = lcdInit;
			break;
	}//END Transitions
	
	switch(lcdState) { //State actions
		case lcdInit:
			break;
		
		case no_package:
			LCD_ClearScreen();
			LCD_DisplayString(1, "PakSafe is empty");
			break;
		
		case package:
			LCD_ClearScreen();
			LCD_DisplayString(1, "Package inside");
			break;
		
		default:
			break;
	}//END State actions
	
}


unsigned char tmpB = 0x00;
unsigned char lock = 0x00;
unsigned char print = 0x00;

enum PakStates { Init, locked_np, unlocked_rfid, locked_p, unlocked_keypad } State;

void PakSafe(unsigned char RFID, unsigned char IR, unsigned char keypad) {
	switch(State) { // Transitions
		case Init:
			State = locked_np;
			break;

		case locked_np: //no package
			if(RFID == 1){
				State = unlocked_rfid;
			}
			else if(keypad == 1){
				State = unlocked_keypad;
			}
			else{
				State = locked_np;
			}
			break;

		case unlocked_rfid:
			if(IR == 1){
				State = locked_p;
			}
			else{
				State = unlocked_rfid;
			}
			break;

		case locked_p: //package
			if(RFID == 1){
				State = unlocked_rfid;
			}
			else if(keypad == 1){
				State = unlocked_keypad;
			}
			else{
				State = locked_p;
			}
			break;
		
		case unlocked_keypad:
			if(IR == 1){
				State = locked_np;
			}
			else{
				State = unlocked_keypad;
			}
			break;
		
		default:
			State = Init;
			break;
	} // END Transitions

	switch(State) { // State actions
		case Init:
			break;

		case locked_np:
			lock = 0; //closed
			print = 0; //"No Package";
			tmpB = SetBit(tmpB, 0, lock);
			tmpB = SetBit(tmpB, 1, print);
			LCDFSM(print);
			PORTB = tmpB;
			break;

		case unlocked_rfid:
			lock = 1; //open
			print = 1; //"Package";
			tmpB = SetBit(tmpB, 0, lock);
			tmpB = SetBit(tmpB, 1, print);
			PORTB = tmpB;
			LCDFSM(print);
			break;

		case locked_p:
			lock = 0; //closed
			print = 1; //"Package";
			tmpB = SetBit(tmpB, 0, lock);
			tmpB = SetBit(tmpB, 1, print);
			PORTB = tmpB;
			LCDFSM(print);
			break;

		case unlocked_keypad:
			lock = 1; //open
			print = 0; //"No Package";
			tmpB = SetBit(tmpB, 0, lock);
			tmpB = SetBit(tmpB, 1, print);
			PORTB = tmpB;
			LCDFSM(print);
			break;
		
		default:
			break;
	} // END State actions
}


int main(void){
	//input
	DDRC = 0x00; //sets to 0 for input
	PORTC = 0xFF; //initializes to all 1s

	//output
	DDRA = 0xFF; //sets to 1 for output
	PORTA = 0x00; //sets to all 0s
	DDRB = 0xFF; //sets to 1 for output
	PORTB = 0x00; //sets to all 0s
	DDRD = 0xFF; //sets to 1 for output
	PORTD = 0x00; //sets to all 0s

	unsigned char tmpA = 0x00;
	unsigned char RFID = 0x00;
	unsigned char IR = 0x00;
	unsigned char keypad = 0x00;
	
	// Initializes the LCD display
	LCD_init();
	
	TimerSet(10);
	TimerOn();
	
	State = Init; // Initial state
	lcdState = lcdInit;
	// Replace with your application code
	while (1){
		tmpA = ~PINC;
		
		RFID = GetBit(tmpA, 0);
		IR = GetBit(tmpA, 1);
		keypad = GetBit(tmpA, 2);
		
		PakSafe(RFID, IR, keypad);
		
		while (!TimerFlag);	// Wait 1 sec
		TimerFlag = 0;
	}
	return 0;
}


	
	