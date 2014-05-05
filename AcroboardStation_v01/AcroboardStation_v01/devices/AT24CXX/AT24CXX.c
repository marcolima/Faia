/*
 * AT24CXX.c
 *
 * Created: 6/27/2013 4:42:50 PM
 *  Author: fabio
 */ 


#include <asf.h>
#include <stdio.h>
#include "board.h"
#include "sysclk.h"
#include "twi_master.h"
//#include "led.h"

#include "conf_board.h"
#include "AT24CXX.h"

//static volatile uint8_t g_is_busy = 0;
/*
uint8_t AT24CXX_busy (void)
{
	return g_is_busy;
}

*/
int AT24CXX_Init()
{
		// TWI master initialization options.
		
		twi_master_options_t opt = {
			.speed = EEPROM_TWI_SPEED,
			.chip  = TWI_MASTER_ADDRESS,
		};
		
//		while(g_is_busy) {};
		
		// Initialize the TWI master driver.
//		g_is_busy = 1;
		const int e = twi_master_setup(EEPROM_TWI_PORT, &opt); 
//		g_is_busy = 0;
		return e;
}

static status_code_t AT24CXX_internalWrite(twi_package_t * ppak)
{
	//debug_string("Before write\r\n");
	const status_code_t r = twi_master_write(EEPROM_TWI_PORT, ppak);

	switch(r) {
		case TWI_SUCCESS:
		//debug_string_P(NORMAL,PSTR("Write Succeeded\r\n"));
		return r;
		break;
		case ERR_IO_ERROR:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_IO_ERROR\r\n"));
		break;
		case ERR_FLUSHED:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_FLUSHED\r\n"));
		break;
		case ERR_TIMEOUT:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_TIMEOUT\r\n"));
		break;
		case ERR_BAD_DATA:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_BAD_DATA\r\n"));
		break;
		case ERR_PROTOCOL:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_PROTOCOL\r\n"));
		break;
		case ERR_UNSUPPORTED_DEV:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_UNSUPPORTED_DEV\r\n"));
		break;
		case ERR_NO_MEMORY:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_NO_MEMORY\r\n"));
		break;
		case ERR_INVALID_ARG:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_INVALID_ARG\r\n"));
		break;
		case ERR_BAD_ADDRESS:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_BAD_ADDRESS\r\n"));
		break;
		case ERR_BUSY:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_BUSY\r\n"));
		break;
		case ERR_BAD_FORMAT:
		debug_string_P(NORMAL,PSTR("Write Failed: ERR_BAD_FORMAT\r\n"));
		break;
		default:
		debug_string_P(NORMAL,PSTR("Write Failed: UNKONWN ERROR\r\n"));
	}
	return r;
}

static status_code_t AT24CXX_internalRead(twi_package_t * ppak)
{
	const status_code_t r = twi_master_read(EEPROM_TWI_PORT, ppak);
	switch(r) {
		case TWI_SUCCESS:
		//debug_string_P(NORMAL,PSTR("READ Succeeded\r\n"));
		return r;
		break;
		case ERR_IO_ERROR:
		debug_string_P(NORMAL,PSTR("READ Failed: ERR_IO_ERROR\r\n"));
		break;
		case ERR_FLUSHED:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_FLUSHED\r\n"));
		break;
		case ERR_TIMEOUT:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_TIMEOUT\r\n"));
		break;
		case ERR_BAD_DATA:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_BAD_DATA\r\n"));
		break;
		case ERR_PROTOCOL:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_PROTOCOL\r\n"));
		break;
		case ERR_UNSUPPORTED_DEV:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_UNSUPPORTED_DEV\r\n"));
		break;
		case ERR_NO_MEMORY:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_NO_MEMORY\r\n"));
		break;
		case ERR_INVALID_ARG:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_INVALID_ARG\r\n"));
		break;
		case ERR_BAD_ADDRESS:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_BAD_ADDRESS\r\n"));
		break;
		case ERR_BUSY:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_BUSY\r\n"));
		break;
		case ERR_BAD_FORMAT:
		debug_string_P(NORMAL,PSTR("READ  Failed: ERR_BAD_FORMAT\r\n"));
		break;
		default:
		debug_string_P(NORMAL,PSTR("READ  Failed: UNKONWN ERROR\r\n"));

	}
	return r;
}

status_code_t AT24CXX_WriteBlockA( const uint8_t addr_page, const uint8_t addr_msb,const uint8_t addr_lsb,uint8_t outbuf[],const uint8_t buflen )
{
	twi_package_t packet = {
		.addr[0]	  = addr_msb,
		.addr[1]	  = addr_lsb,
		.addr_length  = 2,     // TWI slave memory address data size
		.chip         = EEPROM_CHIP_ADDR + addr_page,      // TWI slave bus address
		.buffer       = (void *)outbuf, // transfer data source buffer
		.length       = buflen// transfer data size (bytes)
	};

//	while(g_is_busy) {};

//	g_is_busy = 1;
	const status_code_t e = AT24CXX_internalWrite(&packet);
//	g_is_busy = 0;

	return e;
	
}

status_code_t AT24CXX_WriteBlockB( const uint8_t addr_page, const uint8_t addr_msb,const uint8_t addr_lsb,uint8_t outbuf[],const uint8_t buflen )
{
	uint8_t buf[buflen+2];

	for(int i=0;i<buflen;++i) {
		buf[i+2] = outbuf[i];
	}
	
	buf[0] = addr_msb;
	buf[1] = addr_lsb;

	twi_package_t packet = {
		.addr_length  = 0,     // TWI slave memory address data size
		.chip         = EEPROM_CHIP_ADDR + addr_page,      // TWI slave bus address
		.buffer       = (void *)buf, // transfer data source buffer
		.length       = buflen+2  // transfer data size (bytes)
	};

//	while(g_is_busy) {};

//	g_is_busy = 1;
	const status_code_t e = AT24CXX_internalWrite(&packet);
//	g_is_busy = 0;

	return e;

}

status_code_t AT24CXX_ReadBlockA( const uint8_t addr_page, const uint8_t addr_msb,const uint8_t addr_lsb,uint8_t inbuf[],const uint8_t buflen )
{
	twi_package_t packet = {
		.addr[0]	  = addr_msb,
		.addr[1]	  = addr_lsb,
		.addr_length  = 2,     // TWI slave memory address data size
		.chip         = EEPROM_CHIP_ADDR + addr_page,      // TWI slave bus address
		.buffer       = (void *)inbuf, // transfer data source buffer
		.length       = buflen// transfer data size (bytes)
	};

//	while(g_is_busy) {};

//	g_is_busy = 1;
	const status_code_t e = AT24CXX_internalRead(&packet);
//	g_is_busy = 0;

	return e;
	
}

status_code_t AT24CXX_ReadBlockB( const uint8_t addr_page, const uint8_t addr_msb,const uint8_t addr_lsb,uint8_t inbuf[],const uint8_t buflen )
{
	uint8_t add_buff[2] = {addr_msb,addr_lsb};

	twi_package_t pak = {
		.addr_length  = 0,     // TWI slave memory address data size
		.chip         = EEPROM_CHIP_ADDR + addr_page,      // TWI slave bus address
		.buffer       = add_buff,        // transfer data destination buffer
		.length       = 2   // transfer data size (bytes)
	};

//	while(g_is_busy) {};

//	g_is_busy = 1;
	
	AT24CXX_internalWrite(&pak);

	//debug_string(NORMAL,PSTR("After write\r\n"));
	
	pak.addr_length = 0;
	pak.chip = EEPROM_CHIP_ADDR + addr_page;
	pak.buffer = inbuf;
	pak.length = buflen;

	// Perform a multi-byte read access then check the result.
	const status_code_t e = AT24CXX_internalRead(&pak);
	
//	g_is_busy = 0;

	return e;
}

void AT24CXX_iterator_report(const AT24CXX_iterator * const it) 
{
	char numbuf[16];
	
	debug_string(NORMAL,PSTR("@ "),PGM_STRING);
	itoa(it->byte[PAGE_BYTE],numbuf,10);
	debug_string(NORMAL,numbuf,RAM_STRING);
	debug_string(NORMAL,PSTR(" : "),PGM_STRING);
	itoa(it->byte[MSB_BYTE],numbuf,10);
	debug_string(NORMAL,numbuf,RAM_STRING);
	debug_string(NORMAL,PSTR(" : "),PGM_STRING);
	itoa(it->byte[LSB_BYTE],numbuf,10);
	debug_string(NORMAL,numbuf,RAM_STRING);
	debug_string(NORMAL,PSTR("\r\n"),PGM_STRING);
	
}

