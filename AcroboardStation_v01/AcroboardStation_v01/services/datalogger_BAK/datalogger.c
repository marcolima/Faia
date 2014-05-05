/*
* datalogger.c
*
* Created: 16/03/2012 16:32:59
*  Author: fabio
*/
#include <asf.h>
#include <stdio.h>
#include "progmem.h"
#include <conf_board.h>
#include <conf_usart_serial.h>
#include "config/conf_usart_serial.h"
#include "drivers/sim900/sim900.h"
#include "drivers/usart_interrupt/cbuffer_usart.h"
#include "services/datalogger/datalogger.h"
#include "devices/raingauge/pulse_raingauge.h"
#include "devices/statusled/status_led.h"


#define BUFFER_SIZE 96

typedef struct {
	int32_t data_time_stamp;
	RAINGAUGE_STATS raingauge_stats[2];
} DL_LOG_STRUCTURE;


typedef struct {
	uint16_t address;
	uint8_t p0_and_chip;
} DL_LOG_POINTER ;

//TASKS
volatile uint8_t dl_task_send_data = TASK_STOP;
volatile uint8_t dl_task_store_data = TASK_STOP;
volatile uint8_t dl_task_sync_time = TASK_STOP;


//Memory
static DL_LOG_POINTER g_eeprom_pmem[4];

#define POINTER_OK 0
#define POINTER_INVALID 1


static void logwriter_uint32(char * const pb,const uint32_t val)
{
	static const char HEXCHAR[] PROGMEM = "0123456789ABCDEF";

	const uint32_t H = (val & 0xF0F0F0F0)>>4;
	pb[0] =  pgm_read_byte(HEXCHAR+(H>>24));
	pb[2] =  pgm_read_byte(HEXCHAR+((H<<8)>>24));
	pb[4] =  pgm_read_byte(HEXCHAR+((H<<16)>>24));
	pb[6] =  pgm_read_byte(HEXCHAR+((H<<24)>>24));

	const uint32_t L = val & 0x0F0F0F0F;
	pb[1] =  pgm_read_byte(HEXCHAR+(L>>24));
	pb[3] =  pgm_read_byte(HEXCHAR+((L<<8)>>24));
	pb[5] =  pgm_read_byte(HEXCHAR+((L<<16)>>24));
	pb[7] =  pgm_read_byte(HEXCHAR+((L<<24)>>24));
	pb[8] =  0;

}

static void logwriter_uint16(char * const pb,const uint16_t val)
{
	utoa(val,pb,10);
}

static void logwriter_uint8(char * const pb,const uint8_t val)
{
	utoa(val,pb,10);
}



static uint8_t eeprom_pointer_add(DL_LOG_POINTER * const pnt,int16_t offset )
{
	int16_t address = pnt->address;
	int8_t p0_and_chip = pnt->p0_and_chip;

	char buf[16];


	debug_string(PANIC,PSTR("calling eeprom_pointer_add with offset = "));
	itoa(offset,buf,10);
	debug_string(PANIC,buf);
	debug_string(PANIC,szCRLF);
	debug_string(PANIC,PSTR("pointer values :\r\naddress : "));
	itoa(address,buf,10);
	debug_string(PANIC,buf);
	debug_string(PANIC,PSTR("\r\np0_and_chip : "));
	itoa(p0_and_chip,buf,10);
	debug_string(PANIC,buf);
	debug_string(PANIC,szCRLF);


	address+=offset;

	if(offset<0) {
		//Underflow check
		if(address>pnt->address) {
			//Underflow check
			if(!(p0_and_chip&0x07)==0) {
				return POINTER_INVALID;
			}
			--p0_and_chip;
		}

		} else {

		//Overflow check
		if(address<pnt->address) {
			++p0_and_chip;
			//Overflow check
			if((p0_and_chip&0x07)==0x07) {
				return POINTER_INVALID;
			}
		}

	}

	debug_string(PANIC,PSTR("after eeprom_pointer_add  \r\n"));
	debug_string(PANIC,PSTR("pointer values :\r\naddress : "));
	itoa(address,buf,10);
	debug_string(PANIC,buf);
	debug_string(PANIC,PSTR("\r\np0_and_chip : "));
	itoa(p0_and_chip,buf,10);
	debug_string(PANIC,buf);
	debug_string(PANIC,szCRLF);

	pnt->address = address;
	pnt->p0_and_chip = p0_and_chip;
	return POINTER_OK;

}

//Datalogger temporary object
static DL_LOG_STRUCTURE dl_log_item ;



static char log_buffer[BUFFER_SIZE] = {0};

//static void flush_log_buffer(void)
//{
//uint8_t * lb = log_buffer;
//uint8_t len = BUFFER_SIZE;
//
//while (len) {
//const uint8_t c = *lb++;
//if(c==0)
//break;
//
//udi_cdc_putc(c);
////usart_putchar(USART_SERIAL_USB, c);
//len--;
//}
//log_buffer[0]=0;
//statusled_blink(1);
//}
//
static void store_log_buffer(void)
{

}




static void make_date_string(uint32_t t,char buf[32],uint8_t * const l)
{
	const 	uint32_t ts = (t==-1)?rtc_get_time():t;
	struct calendar_date acd;
	calendar_timestamp_to_date(ts,&acd);

	uint8_t  i = 0;
	buf[i++] = 'D';
	buf[i++] = 'A';
	buf[i++] = 'T';
	buf[i++] = 'E';
	buf[i++] = ':';
	buf[i++] = ' ';
	const uint8_t d = acd.date+1;
	buf[i++] = '0' + (d / 10);
	buf[i++] = '0' + (d % 10);
	buf[i++] = '-';
	const uint8_t m = acd.month+1;
	buf[i++] = '0' + (m / 10);
	buf[i++] = '0' + (m % 10);
	buf[i++] = '-';
	buf[i++] = '0' + (acd.year / 1000);
	buf[i++] = '0' + (acd.year / 100) % 10;
	buf[i++] = '0' + (acd.year / 10)  % 10;
	buf[i++] = '0' + (acd.year % 10);
	buf[i++] = ' ';
	buf[i++] = '0' + (acd.hour / 10);
	buf[i++] = '0' + (acd.hour % 10);
	buf[i++] = ':';
	buf[i++] = '0' + (acd.minute / 10);
	buf[i++] = '0' + (acd.minute % 10);
	buf[i++] = ':';
	buf[i++] = '0' + (acd.second / 10);
	buf[i++] = '0' + (acd.second % 10);

	if(l) *l = i;


}


static void alarm(uint32_t time)
{

	RAINGAUGE_STATS * const rs0 = &dl_log_item.raingauge_stats[0];
	RAINGAUGE_STATS * const rs1 = &dl_log_item.raingauge_stats[1];
	raingauge_get_stats(0,rs0);
	raingauge_get_stats(1,rs1);
	raingauge_reset_stats(0,0);
	raingauge_reset_stats(1,0);

	dl_log_item.data_time_stamp = time;


	gpio_toggle_pin(STATUS_LED_PIN);
	debug_string(PANIC,"RTC callback @ ");

	uint8_t  i=0;
	make_date_string(time,log_buffer,&i);
	log_buffer[i++] = '\r';
	log_buffer[i++] = '\v';
	log_buffer[i++] = 0;

	debug_string(PANIC,log_buffer);


	rtc_set_alarm(time+DL_TIMESPAN);

	//dl_task_send_data = TASK_READY;
	volatile static uint8_t ratio1 = 8;
	if((--ratio1==0) && (TASK_STOP==dl_task_sync_time)) {
		ratio1=8;
		//dl_task_sync_time=TASK_READY;
	}


	if(TASK_STOP==dl_task_store_data) {
		dl_task_store_data=TASK_READY;
	}

	volatile static uint8_t ratio2 = 8;
	if((--ratio2==0) && (TASK_STOP==dl_task_send_data)) {
		ratio2=8;
		dl_task_send_data=TASK_READY;
	}


}


#include "devices/statusled/status_led.h"


void datalogger_init(void)
{

	sim900_power_off();
	delay_ms(1500);
	sim900_power_on();
	delay_ms(1500);

	debug_string(PANIC,PSTR("datalogger init\r\n"));

	
	sim900_init();
	return;
	//delay_ms(2000);

	twi_master_options_t opt = {
		.speed = TWI_SPEED,
		.chip  = EEPROM_CHIP_ADDR
	};

	datalogger_sync_time();

	char szBuf[128];
	uint8_t l=0;
	//rtc_get_time();
	sprintf_P(szBuf,PSTR("Acronet: Power ON @ "));
	make_date_string(-1,szBuf+20,&l);
	szBuf[20+l+1]=0;

	sim900_send_sms(szBuf,PSTR("3480186511"));
	//	sim900_send_sms(szBuf,PSTR("3466883898"));

	// Initialize the TWI master driver.
	twi_master_setup(TWI_EXEEPROM, &opt);



	rtc_set_callback(alarm);
	rtc_set_alarm_relative(2);

}

void datalogger_sync_time(void)
{
	dl_task_sync_time = TASK_RUNNING;
	debug_string(PANIC,PSTR("(datalogger_sync_time) IN\r\n"));

	sim900_GPRS_init(PSTR("web.omnitel.it"));

	char buf[32];

	sim900_http_get(PSTR("130.251.104.23/getTime/"),buf,30);
	buf[31]=0;
	int i,j;
	debug_string(PANIC,PSTR("finding the return\r\n"));
	debug_string(PANIC,buf);

	for(i=0; i<30; ++i) {
		if(buf[i]=='\n') break;
	}

	for(j=++i; j<30; ++j) {
		if(buf[j]=='\r') {
			buf[j]=0;
			break;
		}
	}

	debug_string(PANIC,PSTR("ready to change RTC\r\n"));

	if(i!=31) {
		debug_string(PANIC,"Using ");
		debug_string(PANIC,&buf[i]);
		uint32_t t = strtoul(&buf[i],NULL,10);//atoi(&buf[i]);
		rtc_set_time(t);
		rtc_set_alarm_relative(2);
	}

	sim900_http_close();

	sim900_GPRS_close();
	debug_string(PANIC,PSTR("(datalogger_sync_time) OUT\r\n"));

	dl_task_sync_time = TASK_STOP;
}

void datalogger_sendData(void)
{
	dl_task_send_data = TASK_RUNNING;

	//g_eeprom_pmem[0]

	debug_string(PANIC,"IN datalogger_sendData\r\n");


	DL_LOG_POINTER dl_pnt = g_eeprom_pmem[0];
	if(POINTER_OK!= eeprom_pointer_add(&dl_pnt,-8*sizeof(DL_LOG_STRUCTURE ))) {
		debug_string(PANIC,PSTR("In datalogger sendData: Invalid pointer calculating the end of the current block\r\n"));
		return;
	}

	twi_master_enable(TWI_EXEEPROM);


	for(int i=0; i<8; ++i) {

		const int16_t address = dl_pnt.address;
		const int8_t p0 =  dl_pnt.p0_and_chip;

		DL_LOG_STRUCTURE dl_log;
		twi_package_t packet = {
			.addr[0]      = (uint8_t) address >> 8,			// TWI slave memory address data MSB
			.addr[1]      = (uint8_t) address & 0xFF,      // TWI slave memory address data LSB
			.addr_length  = 2*sizeof (uint8_t),     // TWI slave memory address data size
			.chip         = EEPROM_CHIP_ADDR | p0,      // TWI slave bus address
			.buffer       = (void *)&dl_log, // transfer data source buffer
			.length       = sizeof(DL_LOG_STRUCTURE)   // transfer data size (bytes)
		};

		//while (twi_master_read(TWI_EXEEPROM, &packet) != TWI_SUCCESS);
		switch(twi_master_read(TWI_EXEEPROM, &packet)) {
			case TWI_SUCCESS:
			debug_string(PANIC,PSTR("Write Succeeded\r\n"));
			break;
			case ERR_IO_ERROR:
			debug_string(PANIC,PSTR("Write Failed: ERR_IO_ERROR\r\n"));
			break;
			case ERR_FLUSHED:
			debug_string(PANIC,PSTR("Write Failed: ERR_FLUSHED\r\n"));
			break;
			case ERR_TIMEOUT:
			debug_string(PANIC,PSTR("Write Failed: ERR_TIMEOUT\r\n"));
			break;
			case ERR_BAD_DATA:
			debug_string(PANIC,PSTR("Write Failed: ERR_BAD_DATA\r\n"));
			break;
			case ERR_PROTOCOL:
			debug_string(PANIC,PSTR("Write Failed: ERR_PROTOCOL\r\n"));
			break;
			case ERR_UNSUPPORTED_DEV:
			debug_string(PANIC,PSTR("Write Failed: ERR_UNSUPPORTED_DEV\r\n"));
			break;
			case ERR_NO_MEMORY:
			debug_string(PANIC,PSTR("Write Failed: ERR_NO_MEMORY\r\n"));
			break;
			case ERR_INVALID_ARG:
			debug_string(PANIC,PSTR("Write Failed: ERR_INVALID_ARG\r\n"));
			break;
			case ERR_BAD_ADDRESS:
			debug_string(PANIC,PSTR("Write Failed: ERR_BAD_ADDRESS\r\n"));
			break;
			case ERR_BUSY:
			debug_string(PANIC,PSTR("Write Failed: ERR_BUSY\r\n"));
			break;
			case ERR_BAD_FORMAT:
			debug_string(PANIC,PSTR("Write Failed: ERR_BAD_FORMAT\r\n"));
			break;
			default:
			debug_string(PANIC,PSTR("Write Failed: UNKONWN ERROR\r\n"));
		}


		RAINGAUGE_STATS * rs = &dl_log.raingauge_stats[0];

		uint8_t  i=0;
		make_date_string(dl_log.data_time_stamp,log_buffer,&i);
		log_buffer[i++] = '\r';
		log_buffer[i++] = '\v';

		log_buffer[i++] = 'P';
		logwriter_uint32(log_buffer+i,rs->tips);
		i+=8;
		const uint32_t mxs0   = rs->maxSlope==MAXSLOPE_UNDEF_VALUE?0:rs->maxSlope;
		log_buffer[i++] = 'S';
		logwriter_uint32(log_buffer+i,mxs0);
		i+=8;

		rs = &dl_log.raingauge_stats[1];

		log_buffer[i++] = 'P';
		logwriter_uint32(log_buffer+i,rs->tips);
		i+=8;
		const uint32_t mxs1   = rs->maxSlope==MAXSLOPE_UNDEF_VALUE?0:rs->maxSlope;
		log_buffer[i++] = 'S';
		logwriter_uint32(log_buffer+i,mxs1);
		i+=8;

		//
		log_buffer[i++] = '\v';
		log_buffer[i++] = 0;
		debug_string(PANIC,log_buffer);
		eeprom_pointer_add(&dl_pnt,sizeof(DL_LOG_STRUCTURE));
	}
	//flush_log_buffer();

	//	sim900_send_sms(log_buffer,"\"3480186511\"\r\n");

	dl_task_send_data = TASK_STOP;
}

void datalogger_store_data(void)
{

	dl_task_store_data = TASK_RUNNING;
	debug_string(PANIC,PSTR("Store Data IN\r\n"));

	const int16_t address = g_eeprom_pmem[0].address;
	const int8_t p0 =  g_eeprom_pmem[0].p0_and_chip;


	twi_package_t packet = {
		.addr[0]      = (uint8_t) address >> 8,			// TWI slave memory address data MSB
		.addr[1]      = (uint8_t) address & 0xFF,      // TWI slave memory address data LSB
		.addr_length  = 2*sizeof (uint8_t),     // TWI slave memory address data size
		.chip         = EEPROM_CHIP_ADDR | p0,      // TWI slave bus address
		.buffer       = (void *)&dl_log_item, // transfer data source buffer
		.length       = sizeof(DL_LOG_STRUCTURE)   // transfer data size (bytes)
	};


	//twi_master_enable(TWI_EXEEPROM);

	//PORTCFG.MPCMASK = 0x03; // Configure several PINxCTRL registers at the same time
	//PORTC.PIN0CTRL = (PORTC.PIN0CTRL & ~PORT_OPC_gm) | PORT_OPC_PULLUP_gc; //Enable pull-up to get a defined level on the switches


	// Perform a multi-byte write access then check the result.
	switch(twi_master_write(TWI_EXEEPROM, &packet)) {
		case TWI_SUCCESS:
		debug_string(PANIC,PSTR("Write Succeeded\r\n"));
		break;
		case ERR_IO_ERROR:
		debug_string(PANIC,PSTR("Write Failed: ERR_IO_ERROR\r\n"));
		break;
		case ERR_FLUSHED:
		debug_string(PANIC,PSTR("Write Failed: ERR_FLUSHED\r\n"));
		break;
		case ERR_TIMEOUT:
		debug_string(PANIC,PSTR("Write Failed: ERR_TIMEOUT\r\n"));
		break;
		case ERR_BAD_DATA:
		debug_string(PANIC,PSTR("Write Failed: ERR_BAD_DATA\r\n"));
		break;
		case ERR_PROTOCOL:
		debug_string(PANIC,PSTR("Write Failed: ERR_PROTOCOL\r\n"));
		break;
		case ERR_UNSUPPORTED_DEV:
		debug_string(PANIC,PSTR("Write Failed: ERR_UNSUPPORTED_DEV\r\n"));
		break;
		case ERR_NO_MEMORY:
		debug_string(PANIC,PSTR("Write Failed: ERR_NO_MEMORY\r\n"));
		break;
		case ERR_INVALID_ARG:
		debug_string(PANIC,PSTR("Write Failed: ERR_INVALID_ARG\r\n"));
		break;
		case ERR_BAD_ADDRESS:
		debug_string(PANIC,PSTR("Write Failed: ERR_BAD_ADDRESS\r\n"));
		break;
		case ERR_BUSY:
		debug_string(PANIC,PSTR("Write Failed: ERR_BUSY\r\n"));
		break;
		case ERR_BAD_FORMAT:
		debug_string(PANIC,PSTR("Write Failed: ERR_BAD_FORMAT\r\n"));
		break;
		default:
		debug_string(PANIC,PSTR("Write Failed: UNKONWN ERROR\r\n"));
	}

	//twi_master_disable(TWI_EXEEPROM);

	eeprom_pointer_add(&g_eeprom_pmem[0],sizeof(DL_LOG_STRUCTURE));
	debug_string(PANIC,"g_eeprom_pmem :\r\n");
	char buf[16];
	debug_string(PANIC,"address : ");
	logwriter_uint16(buf,g_eeprom_pmem[0].address);
	debug_string(PANIC,buf);
	debug_string(PANIC,"\r\np0_and_chip : ");
	logwriter_uint8(buf,g_eeprom_pmem[0].p0_and_chip);
	debug_string(PANIC,buf);

	debug_string(PANIC,"Store Data OUT\r\n");

	dl_task_store_data = TASK_STOP;
}

