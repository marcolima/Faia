/*
 * simple_logger.c
 *
 * Created: 19/03/2013 17:03:06
 *  Author: fabio
 */ 


#include "asf.h"
#include <stdio.h>
#include "services/taskman/taskman.h"
#include "services/datalogger/datalogger.h"
#include "devices/raingauge/pulse_raingauge.h"
#include "devices/statusled/status_led.h"
#include "devices/USBVbusMonitor/VBusMon.h"

#include <conf_usart_serial.h>
#include "config/conf_usart_serial.h"
#include "drivers/sim900/sim900.h"
#include "services/config/config.h"
#include "calendar.h"
//#include "uart.h"
#include "globals.h"


int main (void)
{

	//Setting this variable we can decide the amount of info to send
	//at the Debug facility. Compiling the firmware in Release may
	//disable it
	g_log_verbosity = VERBOSE;


	/* Initialize the board.
	 * The board-specific conf_board.h file contains the configuration of
	 * the board initialization.
	 */
	sleepmgr_init();
	sysclk_init();

	board_init();
	VBusMon_init();
	pmic_init(); 


	//Enables the Event module
	sysclk_enable_module(SYSCLK_PORT_GEN, SYSCLK_EVSYS); 

	// USART options.
	static usart_rs232_options_t USART_SERIAL_OPTIONS = {
		.baudrate = USART_DEBUG_BAUDRATE,
		.charlength = USART_CHAR_LENGTH,
		.paritytype = USART_PARITY,
		.stopbits = USART_STOP_BIT
	};


	//Configure the Debug facilty
	sysclk_enable_module(SYSCLK_PORT_F,PR_USART0_bm);
	usart_init_rs232(USART_DEBUG, &USART_SERIAL_OPTIONS);


	//After that the Debug facilty has been configured
	//report the Power On event on it
	debug_string(NORMAL,PSTR ("Power On\r\n"),true);


	if(g_log_verbosity>=NORMAL) {
		debug_string(NORMAL,PSTR ("MCU Revision : "),true);
		usart_putchar(USART_DEBUG,'A'+(MCU.REVID & 0x0F));
		debug_string(NORMAL,szCRLF,true);
	}

	//Initialize the RTC 
	// Workaround for known issue: Enable RTC32 sysclk
	sysclk_enable_module(SYSCLK_PORT_GEN, SYSCLK_RTC);
	while (RTC32.SYNCCTRL & RTC32_SYNCBUSY_bm) {
		// Wait for RTC32 sysclk to become stable
	}

	// If we have battery power and RTC is running, don't initialize RTC32
	if (rtc_vbat_system_check(false) != VBAT_STATUS_OK) {
		rtc_init();
		// Set current time to default
		rtc_set_time(0);
	}


	debug_string(NORMAL,PSTR("Enabling oscilator for DFLL\r\n"),true);

	irqflags_t flags = cpu_irq_save();
	OSC.XOSCCTRL |= 0x02;
	OSC.CTRL |= 0x08;
	cpu_irq_restore(flags);

	do {} while (!(OSC.STATUS & 0x08));
	debug_string(NORMAL,PSTR("oscilator ok\r\n"),true);




//the code in the DEBUG definition block is
//here just for testing purposes
#ifdef DEBUG

	char buf[64];
	
	debug_string(NORMAL,PSTR("\r\nsizeof(void *) = "),true);
	itoa(sizeof(void *),buf,10);
	debug_string(NORMAL,buf,false);

	debug_string(NORMAL,PSTR("\r\nsizeof(flash_addr_t) = "),true);
	itoa(sizeof(flash_addr_t),buf,10);
	debug_string(NORMAL,buf,false);


#ifndef SIM900_USART_POLLED
	debug_string(NORMAL,PSTR ("\r\nSince you compiled in DEBUG we start the GPRS UART in main\r\n"),true);
	usart_interruptdriver_initialize(&sim900_usart_data,USART_GPRS,USART_INT_LVL_LO);
	//usart_set_dre_interrupt_level(USART_SERIAL_GPRS,USART_INT_LVL_LO);
	usart_set_rx_interrupt_level(USART_GPRS,USART_INT_LVL_LO);
	usart_set_tx_interrupt_level(USART_GPRS,USART_INT_LVL_OFF);
#endif

#endif


	cpu_irq_enable();
	VBusMon_check();

	raingauge_init();
	dump_rainstats_to_log(0);

	if(0==datalogger_init()) {
		datalogger_run();
	}

	while (1)
	{
		gpio_toggle_pin(STATUS_LED_PIN);
		delay_ms(500);
	}

	
}
