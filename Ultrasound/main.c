#include <msp430.h> 

void init_ports();
void init_trigger_pulse();
void init_echo_pulse();

#define RED_LED BIT0 //Red led on the texas launchpad
#define TRIGGER_PIN BIT1 //Port 2
#define ECHO_PIN BIT2 //Port 1


float echo_pulse_diff;
/*
 * main.c
 */
void main(void) {
	float distance_cm;
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    init_ports();
    init_trigger_pulse();
    init_echo_pulse();

    while (1) {
    	_BIS_SR(LPM0_bits + GIE); //Shut of CPU. Wake up on echo ISR
    	distance_cm = echo_pulse_diff / 58;

    	if (distance_cm <= 10) {
    		P1OUT |= RED_LED;
    	} else {
    		P1OUT &= ~RED_LED;
    	}
    }
}

void init_ports() {
	//Echo
	P1DIR |= ECHO_PIN | RED_LED; // Pin 1.2 output
	P1SEL |= ECHO_PIN; // Route Timer_A to P1.2
	//Trigger
	P2DIR &= ~TRIGGER_PIN;// P2.1/TA1.1 Input Capture
	P2SEL |= TRIGGER_PIN;
}

void init_trigger_pulse() {
	//ACLK, Up mode
	TACTL = TASSEL_1 | MC_1;
	TACCR0 = 100; //30.5ms pulse
	TACCTL1 = OUTMOD_3; //Reset / Set (see page 364 of the user guide)
	TACCR1 = 100;
}

void init_echo_pulse() {
	//SMCLK, continuous mode
	TA1CTL = TASSEL_2 | MC_2;
	TA1CCTL1 = CAP + CM_3 + CCIE + SCS + CCIS_0;
}

#pragma vector = TIMER1_A1_VECTOR
__interrupt void echo_pulse_capture_isr(void) {
	static unsigned short rising_edge;

	switch(__even_in_range(TA1IV,0x01)) {
		case TA1IV_NONE: break;              // Vector  0:  No interrupt
		case TA1IV_TACCR1:
			if ((TA1CCTL1 & CCI) == CCI) {
				rising_edge = TA1CCR1;
			} else { //Falling edge
				echo_pulse_diff = TA1CCR1 - rising_edge;
				TA1CCR1 = 0; //Reset for next sample
				__bic_SR_register_on_exit(LPM0_bits + GIE);
			}
		break;
		default:break;
	}
}
