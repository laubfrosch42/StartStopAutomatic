// StartStopAutomatic
// licensed under GNU General Public License v3.0 or later

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

uint8_t EEPvalue;
volatile uint8_t LED_N_status;
volatile uint8_t LED_SSA_status;
volatile uint16_t LEDcounter = 0;

#define LED_SSA_IN PB1
#define LED_N_IN PB2
#define BUTTON_SSA_IN PB3
#define BUTTON_SSA_OUT PB4
#define EEPReadByte(addr) eeprom_read_byte((uint8_t *)addr)
#define EEPWriteByte(addr, val) eeprom_write_byte((uint8_t *)addr, val)

void press_button (void)
{
	cli();
	PORTB = PORTB | (1<<BUTTON_SSA_OUT);
	_delay_ms(150);
	PORTB = PORTB & ~(1<<BUTTON_SSA_OUT);
	_delay_ms(150); //avoiding two button presses following each other but recognized by the car just as one due to late interrupt execution after press_button
	sei();
}

int main (void)
{
	DDRB   = DDRB | (1<<BUTTON_SSA_OUT);                 // set BUTTON_SSA_OUT as output
	PORTB  = PORTB | (1<<BUTTON_SSA_IN) | (1<<LED_N_IN); // set Pull-Up-Resistor at BUTTON_SSA_IN and LED_N_IN
	TCCR0A = 0b00000010;                                 // Timer CTC mode
	TCCR0B = 0b00000010;                                 // set prescaler 8
	OCR0A  = 250;                                        // overflow at 250 * prescaler 8 = divider 2000 = 2ms-interrupt at 1MHz
	TIMSK  = 0b00010000;                                 // CTC compare A and timer interrupt enable

	EEPvalue = EEPReadByte(0); // read stored on/off status in EEPROM
	if (!(EEPvalue == 1 || EEPvalue == 0)) {EEPWriteByte(0, 1); EEPvalue = 1;} // if not 0/1 then set 1

	// toggle EEPROM if SSA is pressed during startup
	if (!(PINB & (1<<BUTTON_SSA_IN)))
	{
		while (!(PINB & (1<<BUTTON_SSA_IN))); // wait until button is not pressed anymore
		if (EEPvalue == 1) {EEPWriteByte(0, 0); EEPvalue = 0;}
		else {EEPWriteByte(0, 1); EEPvalue = 1;}
	}

	// sleep if toggled off
	if (EEPvalue == 0) {MCUCR = 0b00110000; sleep_mode();}

	sei();         // enable interrupts and start creating LED_X_status
	_delay_ms(50); // a bit of time to collect LED_X_status

	// endless loop to check and act if LED_N_status equals LED_SSA_status
	// by logic the status needs to be different (if LED_N is ON, then LED_SSA shall be OFF)
	while (1)
	{
		if (LED_SSA_status == LED_N_status) {press_button(); _delay_ms(50);}
	}
}

ISR(TIMER0_COMPA_vect)
{
	//check status LED_SSA
	if (!(PINB & (1<<LED_SSA_IN))) {LED_SSA_status = 0;}
	else {LED_SSA_status = 1;}

	//check status LED_N
	if (!(PINB & (1<<LED_N_IN)))
	{
		LEDcounter = 0;
		LED_N_status = 0;
	}
	else
	{
		LEDcounter = LEDcounter + 1;
		if (LEDcounter >= 200) // with 2ms-interrupt this leads to a minimum of 400ms to switch LED_N_status to ON
		{
			LEDcounter = 200; // keep at 200 to avoid overflow of variable
			LED_N_status = 1;
			}
		else
		{
			LED_N_status = 0;
		}
	}
}
// end of file
