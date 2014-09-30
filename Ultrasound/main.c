/*
 *
 * 	The MIT License (MIT)
 *
 *	Copyright (c) 2014 Phillip Nash
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a copy
 *	of this software and associated documentation files (the "Software"), to deal
 *	in the Software without restriction, including without limitation the rights
 *	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *	copies of the Software, and to permit persons to whom the Software is
 *	furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *	THE SOFTWARE.
 *
 * */


/**
 *	This program captures the data obtained from multiple sonar modules (HC-SR04) and stores the measured distances.
 *	The program requires to have NUM_SENSORS connected, otherwise the program will halt
 *	(due to not generating an interrupt and resuming the CPU's execution).
 *
 *	You have to connect the ultrasound sensors to the msp430 (msp430g2553 in this case) like this:
 *
 *
 *			 	 msp430g2553
 *				 ___________
 *				|			|
 *			S1T |P1.0	P2.0|S1E
 *			S2T |P1.1	P2.1|S2E
 *			S3T |P1.2	P2.2|S3E
 *			S4T |P1.3	P2.3|S4E
 *				|P1.4	P2.4|
 *				|P1.5	P2.5|
 *				|P1.6	P2.6|
 *				|P1.7	P2.7|
 *				|___________|
 *
 *	SxT = Sensor trigger
 *	SxE	= Sensor echo
 * */

#include <msp430.h>

#define SENSOR_TOP_PIN BIT0
#define SENSOR_BOTTOM_PIN BIT1
#define SENSOR_LEFT_PIN BIT2
#define SENSOR_RIGHT_PIN BIT3
#define NUM_SENSORS 4

short sensor_id = 0;
float sensors_distances[NUM_SENSORS];
unsigned int rising_edge = 0;
unsigned int falling_edge = 0;

short sensors_pinouts[NUM_SENSORS] = {SENSOR_TOP_PIN, SENSOR_BOTTOM_PIN, SENSOR_LEFT_PIN, SENSOR_RIGHT_PIN};

void init_ports();
void init_timers();

float sensor_echo_pulse_diff = 0;
float sensor_distance_cm = 0;

void main(void) {

	init_timers();
	init_ports();

    while (1) {
    	_BIS_SR(LPM0_bits + GIE);	//Turn off CPU w/interrupts
		sensor_echo_pulse_diff = falling_edge - rising_edge;
		sensor_distance_cm = sensor_echo_pulse_diff / 58;
		sensors_distances[sensor_id] = sensor_distance_cm;

		if (sensor_distance_cm <= 10) {
			P1OUT |= BIT6;
		} else {
			P1OUT &= ~BIT6;
		}

		TACTL = TACLR;
		TA1CTL = TACLR;
		__delay_cycles(20000);
		init_timers();
		//Move to the next sensor
    	sensor_id = ++sensor_id == NUM_SENSORS ? 0 : sensor_id;
    }
}


void init_ports()
{
    int i;

    P1DIR |= BIT6;

    //Setup port A for triggering pulses and port B for retrieving the replies from the sensors (via isr).
    for (i = 0; i < NUM_SENSORS; i++) {
    	P1DIR |= sensors_pinouts[i];
    	P2DIR &= ~sensors_pinouts[i];
		P2IE |= sensors_pinouts[i];
		P2IFG &= ~sensors_pinouts[i];
    }
    P1OUT = 0;
}

void init_timers()
{
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
	TACTL = TASSEL_1 | MC_1;
	TACCR0 = 20; //1.30ms pulse
	TACCTL1 = OUTMOD_3 | CCIE; //Reset / Set (see page 364 of the user guide)
	TA1CTL = TASSEL_2 | MC_2;
}


#pragma vector=TIMER0_A1_VECTOR
__interrupt void generate_trigger_pulse_isr(void)
{
	switch (__even_in_range(TAIV,0x02)) {
		case TA0IV_TACCR1:
			P1OUT ^= sensors_pinouts[sensor_id];
			TACCTL1 &= ~CCIFG;
			break;
	}
}

#pragma vector=PORT2_VECTOR
__interrupt void echo_reply_isr(void)
{
	const unsigned short sensor = sensors_pinouts[sensor_id]; //Obtain current sensor

	if (P2IFG & sensor) { //Has an interrupt occurred?
		if (P2IN & sensor) { //High transition
			rising_edge = TA1R;
		} else {	//Low transistion
			falling_edge = TA1R;
			TA1R = 0;	//Clear to prevent overflow from messing up the next calculations
			__bic_SR_register_on_exit(LPM0_bits + GIE); //Wake up CPU (resume main execution)
		}
		P2IES ^= sensor;  //This enables the ISR to be called on both low and high transitions
		P2IFG &= ~sensor; //Clear the pin that generated the interrupt
	}
}
