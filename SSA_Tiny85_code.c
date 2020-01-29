//StartStopAutomatic
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

uint8_t EEPvalue;
uint8_t EEPchange = 0;
volatile uint8_t LEDN_now;
volatile uint8_t LEDN_before;
volatile uint16_t LEDcounter = 0;

#define LED_N  PB2
#define SSAout PB3
#define SSAin  PB4
#define EEPReadByte(addr) eeprom_read_byte((uint8_t *)addr)
#define EEPWriteByte(addr, val) eeprom_write_byte((uint8_t *)addr, val)

void press_button (void)
{
	PORTB = PORTB | (1<<SSAout);
	_delay_ms(150);
	PORTB = PORTB & ~(1<<SSAout);
	_delay_ms(150); //avoiding two button presses following each other but recognized by the car just as one due to late interrupt execution after press_button
}

int main (void)
{
	DDRB   = DDRB | (1<<SSAout);              // set SSAout as output
	PORTB  = PORTB | (1<<SSAin) | (1<<LED_N); // set Pull-Up-Resistor at SSAin and LED_N
	TCCR0A = 0b00000010;                      // Timer CTC mode
	TCCR0B = 0b00000010;                      // set prescaler 8
	OCR0A  = 250;                             // overflow at 250 * prescaler 8 = divider 2000 = 2ms-interrupt at 1MHz
	TIMSK  = 0b00010000;                      // CTC compare A and timer interrupt enable

	EEPvalue = EEPReadByte(0); // read stored on/off status in EEPROM

	// initial (first use of THIS tiny85) setup of EEPROM if not 1/0 already
	if (!(EEPvalue == 1 || EEPvalue == 0)) {EEPWriteByte(0, 1); EEPvalue = 1;}

	// toggle EEPROM if SSA is pressed during startup
	if (!(PINB & (1<<SSAin)))
	{
		while (!(PINB & (1<<SSAin))); // wait until button is not pressed anymore
		_delay_ms(250);
		if (EEPvalue == 1) {EEPWriteByte(0, 0); EEPvalue = 0; press_button();}
		else {EEPWriteByte(0, 1); EEPvalue = 1; EEPchange = 1;}
	}

	// if toggled off then do nothing but deep sleep
	if (EEPvalue == 0) {MCUCR = 0b00110000; sleep_mode();}

	sei();                  // enable interrupts and start creating LEDN_now status
	_delay_ms(5000);        // waiting for the car electronics to be ready
	LEDN_before = LEDN_now; // set initial condition taken from ISR

	// press SSA once at startup if active, not on NEUTRAL gear and not pressed during startup
	if (LEDN_before == 0 && EEPchange == 0) {press_button();} //standard case at startup
	if (LEDN_before == 1 && EEPchange == 1) {press_button();} //super special case if started in N position AND being toggled

	// endless loop pushing SSA-button on LED_N status change
	while (1)
	{
		if (LEDN_before != LEDN_now)
		{
			cli();
			LEDN_before = LEDN_now;
			press_button();
			sei();
		}
	}
}

ISR(TIMER0_COMPA_vect)
{
	if (!(PINB & (1<<LED_N))) {LEDcounter = 0;}
	else                      {LEDcounter = LEDcounter + 1;}
	if (LEDcounter >= 200)    {LEDN_now = 1; LEDcounter = 200;} //keep at 200 to avoid overflow and jump back to 0
	else                      {LEDN_now = 0;}
}
