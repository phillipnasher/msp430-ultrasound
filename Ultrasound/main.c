#include <msp430.h>

#define SENSOR_TOP_TRIGGER_PIN BIT0
#define SENSOR_DOWN_TRIGGER_PIN BIT1
#define SENSOR_LEFT_TRIGGER_PIN BIT2
#define SENSOR_RIGHT_TRIGGER_PIN BIT3
#define NUM_SENSORS 4

//short sensor_triggers[NUM_SENSORS] = {SENSOR_TOP_TRIGGER_PIN,SENSOR_DOWN_TRIGGER_PIN};

short sensor_triggers[NUM_SENSORS] = {SENSOR_TOP_TRIGGER_PIN,SENSOR_DOWN_TRIGGER_PIN,
		SENSOR_LEFT_TRIGGER_PIN,SENSOR_RIGHT_TRIGGER_PIN};
#define GREEN_LED BIT6
short sensor_trigger_id = 0;
float echo_pulse_diff = 0;
float distance_cm = 0;
float sensor_distance[NUM_SENSORS];
unsigned int rising_edge;
unsigned int falling_edge;

void main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
    int i;

    for (i = 0; i < NUM_SENSORS; i++) {
    	P1DIR |= sensor_triggers[i];
    	P2DIR &= ~sensor_triggers[i];
		P2IE |= sensor_triggers[i];
		P2IFG &= ~sensor_triggers[i];
    }

    P1DIR |= GREEN_LED;
    P1OUT = 0;
    P1OUT |= GREEN_LED;

    TACTL = TASSEL_1 | MC_1;
	TACCR0 = 20; //1.30ms pulse
	TACCTL1 = OUTMOD_3 | CCIE; //Reset / Set (see page 364 of the user guide)
	TA1CTL = TASSEL_2 | MC_2;

    while (1) {
    	_BIS_SR(LPM0_bits + GIE);
    	__no_operation();
		echo_pulse_diff = falling_edge - rising_edge;
		distance_cm = echo_pulse_diff / 58;
		sensor_distance[sensor_trigger_id] = distance_cm;

		if (sensor_trigger_id == NUM_SENSORS) {
			sensor_trigger_id = 0;
		} else {

			sensor_trigger_id++;
		}

    	//sensor_trigger_id = sensor_trigger_id++ == 2 ? 0 : sensor_trigger_id;
    }
}


#pragma vector=TIMER0_A1_VECTOR
__interrupt void trigger_pulse(void)
{
	switch (__even_in_range(TAIV,0x02)) {
		case 0x02:
			P1OUT ^= sensor_triggers[sensor_trigger_id];
			//P1OUT ^= SENSOR_DOWN_TRIGGER_PIN;
			TACCTL1 &= ~CCIFG;
			//__bic_SR_register_on_exit(LPM0_bits + GIE);
			break;
	}
}

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
{
	unsigned short sensor = sensor_triggers[sensor_trigger_id];

	if (P2IFG & sensor) {
		if (P2IN & sensor) { //High transition
			rising_edge = TA1R;
		} else {
			falling_edge = TA1R;
			//TA1R = 0;
			__bic_SR_register_on_exit(LPM0_bits + GIE);
		}
		P2IES ^= sensor;
		P2IFG &= ~sensor; //Clear the pin that generated the interrupt
	}
}
