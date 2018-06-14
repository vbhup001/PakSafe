/*
 * PakSafe.c
 *
 * 
 * Author : Vishal
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "io.c"
#include "rc522.h"

int cnt = 0;

//-------------------BEGIN Timer Code-----------------------
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

unsigned char tmpB = 0x00;
unsigned char lock = 0x00;
unsigned char print = 0x00;
unsigned char code;
unsigned char piccpresent;
unsigned char pcount = 0x00;

//RFID reading

enum rfid{wait1, getcode} state;

int tick2(int state){
	switch(state){
		case wait1:
			state = getcode;
			break;
		case getcode:
			state = wait1;
			break;
		default:
			state = wait1;
			break;
	}
	switch(state){
		case wait1:
			code = 0x00;
			//PORTA = 0xFF;
			break;
		case getcode:
			piccpresent = rc522_wakeup(); //check if tag is present
			if(piccpresent == 0x04){
				code = rc522id(); //get id from tag
			}
			else{
				code = 0x00;
			}
			//PORTA = code; //test printing
			break;
		default:
			//code = 0x00;//test printing
			break;
	}
	return state;
}

//end RFID reading

enum PakStates { Init, locked_np, unlocked_rfid, locked_p, unlocked_keypad } State;

void PakSafe() {
	switch(State) { // Transitions
		case Init:
			State = locked_np;
			break;

		case locked_np: //no package
			if(code == 0xD0){ //white card
				State = unlocked_rfid;
			}
			else if(code == 0x1B){ //keychain
				State = unlocked_keypad;
			}
			else{
				State = locked_np;
			}
			break;

		case unlocked_rfid:
			/*if(IR == 1){
				State = locked_p;
			}
			else{
				State = unlocked_rfid;
			}
			*/
			State = locked_p;
			pcount++;
			break;

		case locked_p: //package
			if(code == 0xD0){ //white card
				State = unlocked_rfid;
			}
			else if(code == 0x1B){ //keychain
				State = unlocked_keypad;
			}
			else{
				State = locked_p;
			}
			break;
		
		case unlocked_keypad:
			/*if(IR == 1){
				State = locked_np;
			}
			else{
				State = unlocked_keypad;
			}*/
			State = locked_np;
			pcount = 0x00;
			break;
		
		default:
			State = Init;
			break;
	} // END Transitions

	switch(State) { // State actions
		case Init:
			break;

		case locked_np:
			tick2(wait1);
			lock = 0; //closed
			//print = 0; //"No Package";
			tmpB = SetBit(tmpB, 0, lock);
			PORTA = 0x01;//power and no package
			PORTD = tmpB;
			break;

		case unlocked_rfid:
			lock = 1; //open
			//print = 1; //"Package";
			tmpB = SetBit(tmpB, 0, lock);
			PORTD = tmpB;
			//-------------------nothing
			break;

		case locked_p:
			tick2(wait1);
			lock = 0; //closed
			//print = 1; //"Package";
			tmpB = SetBit(tmpB, 0, lock);
			PORTD = tmpB;
			if(pcount == 1){
				PORTA = 0x03;//Power and 1 package
			}
			else if(pcount == 2){
				PORTA = 0x07;//Power and 2
			}
			else if(pcount == 3){
				PORTA = 0x0F;//Power and 3
			}
			else{
				PORTA = 0x1F;//Power and 4
			}
			break;

		case unlocked_keypad:
			lock = 1; //open
			//print = 0; //"No Package";
			tmpB = SetBit(tmpB, 0, lock);
			PORTD = tmpB;
			//-------------------nothing
			break;
		
		default:
			break;
	} // END State actions
}


int main(void){
	
	SPI_MasterInit();
	rc522init();
	DDRA = 0xFF; //sets to 1 for output
	PORTA = 0x00; //sets to all 0s
	//DDRB = 0xFF; //sets to 1 for output
	//PORTB = 0x00; //sets to all 0s
	DDRC = 0x00; //sets to 0 for input
	PORTC = 0xFF; //initializes to all 1s
	DDRD = 0xFF; //sets to 1 for output
	PORTD = 0x00; //sets to all 0s

	TimerSet(80);
	TimerOn();
	
	State = Init; // Initial state
	while (1){
		tmpA = ~PINC;
		
		PakSafe();
		
		while (!TimerFlag);	// Wait 1 sec
		TimerFlag = 0;
	}
	return 0;
}
