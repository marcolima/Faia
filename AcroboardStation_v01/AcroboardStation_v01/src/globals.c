/*
 * globals.c
 *
 * Created: 22/03/2013 10:50:10
 *  Author: fabio
 */ 

#include <stdio.h>
#include <asf.h>
#include "globals.h"
#include "devices/raingauge/pulse_raingauge.h"
//Extern delarations
const char szCRLF[] PROGMEM = "\r\n";
uint8_t g_log_verbosity = VERY_VERBOSE;


//Static declarations
static volatile bool g_cdc_enable = false;

void debug_string_P(uint8_t level,const char * const sz)
{
	debug_string(level,sz,PGM_STRING);
}

void debug_string(uint8_t level,const char * const sz,const uint8_t isPGM)
{
	if(level>g_log_verbosity) return;

	const char * p = sz;

	if(isPGM) {
		nvm_wait_until_ready();
		while(1) {
			const uint8_t c = nvm_flash_read_byte( p++ );
			if(c==0) break;
//			while (udi_cdc_is_tx_ready()) {}
//			udi_cdc_putc(c);
			while (usart_data_register_is_empty(USART_DEBUG) == false) {}
			usart_put(USART_DEBUG, c);
		}
		
	} else {
		while(*p) {
			while (usart_data_register_is_empty(USART_DEBUG) == false) {}
			usart_put(USART_DEBUG, *p++);
			//while (udi_cdc_is_tx_ready()) {}
			//udi_cdc_putc(*p++);

		}
	}
}


void debug_function_out_name_print(const char * const fname[])
{
	debug_string(VERBOSE,PSTR("  [OUT]\t/\\ "),PGM_STRING);
	debug_string(VERBOSE,*fname,RAM_STRING);
	debug_string(VERBOSE,PSTR("\r\n"),PGM_STRING);
}

void debug_function_in_name_print(const uint8_t level,const char fname[])
{
	debug_string(level,PSTR("  [IN]\t\\/ "),PGM_STRING);\
	debug_string(level,fname,RAM_STRING);\
	debug_string(level,PSTR("\r\n"),PGM_STRING);
}

void debug_function_in_name_print_P(const uint8_t level,const char fname[])
{
	debug_string(VERBOSE,PSTR("  [IN]\t\\/ "),PGM_STRING);
	debug_string(VERBOSE,fname,PGM_STRING);
	debug_string(VERBOSE,PSTR("\r\n"),PGM_STRING);

}

void debug_function_out_name_print_P(const char * const fname[])
{
	debug_string(VERBOSE,PSTR("  [OUT]\t/\\ "),PGM_STRING);
	debug_string(VERBOSE,*fname,PGM_STRING);
	debug_string(VERBOSE,PSTR("\r\n"),PGM_STRING);
}

void dump_rainstats_to_log(const uint8_t id)
{
	RAINGAUGE_STATS rs;
	raingauge_get_stats(id,&rs);

	char szBuf[256];
	sprintf_P(szBuf,PSTR("\tfirst tip cents: %u\r\n\tlast tip cents:%u\r\n\tmax slope cents: %u\r\n\ttips: %u\r\n\tmax slope %u\r\n")
	,rs.firstTip_cents
	,rs.lastTip_cents
	,rs.maxSlope_cents
	,rs.tips
	,rs.maxSlope);

	debug_string(NORMAL,szBuf,false);
}

void vbus_action(bool b_high)
{
	if (b_high) {
		// Attach USB Device
		udc_attach();
	} else {
		// VBUS not present
		udc_detach();
	}
}


bool cdc_enable(void)
{
	g_cdc_enable = true;
	return true;
}

void cdc_disable(void)
{
	g_cdc_enable = false;
}

void cdc_set_dtr(bool b_enable)
{
	if (b_enable) {
		g_cdc_enable = true;
	}else{
		g_cdc_enable = false;
	}
}


