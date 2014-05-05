/*
 * status_led.h
 *
 * Created: 16/03/2012 16:09:43
 *  Author: fabio
 */ 


#ifndef STATUS_LED_H_
#define STATUS_LED_H_


static inline void statusled_blink(uint8_t t)
{
	while( t-- ) {
		gpio_set_pin_high(STATUS_LED_PIN);
		delay_ms(100);
		gpio_set_pin_low(STATUS_LED_PIN);
		delay_ms(50);
	}		
	delay_ms(50);
	gpio_set_pin_high(STATUS_LED_PIN);
}

static inline void status_led_toggle(void)
{
	gpio_toggle_pin(STATUS_LED_PIN);
}



#endif /* STATUS_LED_H_ */