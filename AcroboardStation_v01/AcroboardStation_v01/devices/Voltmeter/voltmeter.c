/*
 * voltmeter.c
 *
 * Created: 24/07/2013 13:27:27
 *  Author: fabio
 */ 

#include <asf.h>
#include <stdio.h>
#include "voltmeter.h"

void voltmeter_init(void)
{
}

static void voltmeter_turn_on(void) 
{
	
}

static void voltmeter_turn_off(void) 
{
	
}


/**
 * \brief Read the board input voltage 
 *
 * the return value is in cents of volt
 *
 */
uint16_t voltmeter_getValue(void)
{
	struct adc_config adc_conf;
	struct adc_channel_config adcch_conf;

	adc_read_configuration(&BATTERY_VOLTMETER, &adc_conf);
	adcch_read_configuration(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH, &adcch_conf);

	adc_set_conversion_parameters(&adc_conf, ADC_SIGN_OFF, ADC_RES_12, ADC_REF_VCC);
	adc_set_conversion_trigger(&adc_conf, ADC_TRIG_MANUAL, 1, 0);
	adc_set_clock_rate(&adc_conf, 100000UL);

	adcch_set_input(&adcch_conf, BATTERY_VOLTMETER_PIN, ADCCH_NEG_NONE, 0);

	adc_write_configuration(&BATTERY_VOLTMETER, &adc_conf);
	adcch_write_configuration(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH, &adcch_conf);

	//

	ioport_configure_pin(BATTERY_VOLTMETER_SWITCH, IOPORT_DIR_OUTPUT | IOPORT_INIT_LOW);

	delay_ms(2);

	adc_enable(&BATTERY_VOLTMETER);
	adc_start_conversion(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	adc_wait_for_interrupt_flag(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	const int16_t result = adc_get_result(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	adc_disable(&BATTERY_VOLTMETER);

//	adcch_read_configuration(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH, &adcch_conf);
	adcch_set_input(&adcch_conf, ADCCH_POS_BANDGAP, ADCCH_NEG_NONE, 0);
	adcch_write_configuration(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH, &adcch_conf);

	adc_enable(&BATTERY_VOLTMETER);
	adc_start_conversion(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	adc_wait_for_interrupt_flag(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	adc_start_conversion(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	adc_wait_for_interrupt_flag(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	const int16_t gain = adc_get_result(&BATTERY_VOLTMETER, BATTERY_VOLTMETER_CH);
	adc_disable(&BATTERY_VOLTMETER);

	char szBUF[32];
	sprintf_P(szBUF,PSTR("ADC: %d\tgain: %d\r\n"),result,gain);
	debug_string(NORMAL,szBUF,RAM_STRING);
	
	ioport_configure_pin(BATTERY_VOLTMETER_SWITCH, IOPORT_DIR_INPUT);

	return result;
}

uint16_t thermometer_getValue(void)
{
	struct adc_config adc_conf;
	struct adc_channel_config adcch_conf;

	adc_read_configuration(&ADCA, &adc_conf);
	adcch_read_configuration(&ADCA, ADC_CH0, &adcch_conf);

	adc_set_conversion_parameters(&adc_conf, ADC_SIGN_OFF, ADC_RES_12, ADC_REF_BANDGAP);
	adc_set_clock_rate(&adc_conf, 200000UL);
	adc_set_conversion_trigger(&adc_conf, ADC_TRIG_MANUAL, 1, 0);
	adc_enable_internal_input(&adc_conf,ADC_INT_TEMPSENSE);

	adc_write_configuration(&ADCA, &adc_conf);


	adcch_set_input(&adcch_conf, ADCCH_POS_TEMPSENSE, ADCCH_NEG_NONE, 1);
	adcch_write_configuration(&ADCA, ADC_CH0, &adcch_conf);

	// Get measurement for 85 degrees C (358 kelvin) from calibration data.
	uint16_t tempsense = adc_get_calibration_data(ADC_CAL_TEMPSENSE);

	delay_ms(2);

	adc_enable(&ADCA);
	adc_start_conversion(&ADCA, ADC_CH0);
	adc_wait_for_interrupt_flag(&ADCA, ADC_CH0);
	const int16_t result = adc_get_result(&ADCA, ADC_CH0);
	adc_disable(&ADCA);

	uint32_t temperature = (uint32_t)result * 358;
	temperature /= tempsense;

   
	char szBUF[32];
	sprintf_P(szBUF,PSTR("temperature: %d\r\n"),temperature);
	debug_string(NORMAL,szBUF,RAM_STRING);
	
	return temperature;
}