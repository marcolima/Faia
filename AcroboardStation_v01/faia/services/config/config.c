/*
 * config.c
 *
 * Created: 6/22/2013 7:47:58 PM
 *  Author: fabio
 */ 

#include <asf.h>
#include <stdio.h>
#include "progmem.h"
#include <conf_board.h>
#include <conf_usart_serial.h>

#include "globals.h"

#include "config/conf_usart_serial.h"
#include "drivers/sim900/sim900.h"
#include "drivers/usart_interrupt/cbuffer_usart.h"
#include "devices/raingauge/pulse_raingauge.h"
#include "devices/statusled/status_led.h"
#include "config/conf_APN.h"
#include "devices/AT24CXX/AT24CXX.h"
#include "config.h"
#include "devices/Voltmeter/voltmeter.h"

static void cfg_get(uint8_t it, char szVal[],const uint8_t len)
{
	uint16_t idx;
	nvm_eeprom_read_buffer(it,&idx,sizeof(uint16_t));
	
	uint8_t bs = min(EEPROM_SIZE-idx,len);
	nvm_eeprom_read_buffer(idx,szVal,bs);
}


uint8_t cfg_check(void)
{
	uint16_t v;
	nvm_eeprom_read_buffer(0,&v,sizeof(uint16_t));
	
	return (v==0x01AC);
}


void cfg_get_sim_pin(char szVal[],const uint8_t len)
{
	cfg_get(8,szVal,len);
}

void cfg_get_gprs_apn(char szVal[],const uint8_t len)
{
	cfg_get(6,szVal,len);
}

void cfg_get_aws_id(char szVal[],const uint8_t len)
{
	cfg_get(4,szVal,len);
}

void cfg_get_service_url_time(char szVal[],const uint8_t len)
{
	cfg_get(10,szVal,len);
}

void cfg_get_service_url_send(char szVal[],const uint8_t len)
{
	cfg_get(12,szVal,len);
}

void cfg_get_datalogger_timings(uint32_t * const sendDT,uint32_t * const storeDT,uint32_t * const syncDT,uint32_t * const tickDT)
{
	struct {uint32_t a,b,c,d;} buf;
	cfg_get(0x0e,&buf,sizeof(buf));
	*sendDT  = buf.a;
	*storeDT = buf.b;
	*syncDT  = buf.c;
	*tickDT  = buf.d;
}
