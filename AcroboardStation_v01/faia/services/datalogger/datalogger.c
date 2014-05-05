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

#include "globals.h"

#include "config/conf_usart_serial.h"
#include "drivers/sim900/sim900.h"
#include "drivers/usart_interrupt/cbuffer_usart.h"
#include "datalogger.h"
#include "devices/raingauge/pulse_raingauge.h"
#include "devices/statusled/status_led.h"
#include "config/conf_APN.h"
#include "devices/AT24CXX/AT24CXX.h"
#include "../config/config.h"
#include "devices/Voltmeter/voltmeter.h"


#define DL_T0 1350923400


typedef struct DL_LOG_ITEM {
	uint32_t data_timestamp;

#ifdef DATALOGGER_AUX_YES
	RAINGAUGE_STATS raingauge_stats[2];
#else
	RAINGAUGE_STATS raingauge_stats[1];
#endif
	uint16_t vBat;
	uint8_t flags;	
} __attribute__((packed)) DL_LOG_ITEM;



//TASKS
volatile uint8_t task_status_server_cmd_check = TASK_STOP;
volatile uint8_t task_status_send_data_prepare_RT = TASK_STOP;
volatile uint8_t task_status_send_data_RT = TASK_STOP;
volatile uint8_t task_status_store_data = TASK_STOP;
volatile uint8_t task_status_sync_time = TASK_STOP;
volatile uint8_t task_status_time_tick = TASK_STOP;

volatile uint8_t task_status_send_data_prepare = TASK_STOP;
volatile uint8_t task_status_send_data = TASK_STOP;


//EVENTS
volatile uint8_t dl_cycle_lock = true;
//volatile uint8_t dl_event_terminate_send = false;



//Memory pointers and partitions
#define DL_EEPROM_ITERATORS 2
#define LOG_BEGIN 0
#define LOG_END 1
//a unique partition holding
//the datalogger data
//except the first 256 bytes that are
//used to store setup info
#define LOG_LAST_VALID_ADDRESS (524288)
#define LOG_FIRST_VALID_ADDRESS (256)

static AT24CXX_iterator g_eeprom_iter[DL_EEPROM_ITERATORS];

typedef struct DL_SEND_PARAMS{
	AT24CXX_iterator iter_send;
	uint32_t dt_end;
	uint8_t  send_once;
	
} DL_SEND_PARAMS;

static DL_SEND_PARAMS dl_send_params_RT;
static DL_SEND_PARAMS dl_send_params;

#define POINTER_OK true
#define POINTER_INVALID false

//static volatile uint32_t g_timestart;

//SIM900 Status
static uint8_t g_is_GPRS_module_OK;

//Datalogger scheduler time accumulators
static volatile uint32_t g_acc_task_sync = 0;
static volatile uint32_t g_acc_task_store = 0;
static volatile uint32_t g_acc_task_send_RT = 280;

//Datalogger scheduler task schedule
static uint32_t  g_interv_task_store;
static uint32_t  g_interv_task_send_RT;
static uint32_t  g_interv_task_sync;

//Datalogger timer interval
static uint32_t  g_timespan;

static volatile uint32_t g_timePrev = DL_T0;
static uint8_t  datalogger_snapshot_data(const uint32_t ts);

static uint8_t send_data_with_get(DL_SEND_PARAMS * const pPara);
static uint16_t send_data_with_post(DL_SEND_PARAMS * const pPara);

static void dl_test_now(void);

//static uint8_t dl_get_record_from_eeprom(const struct AT24CXX_iterator * const it,)
//{
		//uint8_t pg,msb,lsb;
		//AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
		//uint8_t * const pBuf = (uint8_t *) &ds;
		//const uint8_t v = pPara->iter_send.byte[LSB_BYTE];
		////is it a plain page or a overlapping read ?
		//if ((v>=sizeof(DL_LOG_ITEM)) || (v==0)) {
			//AT24CXX_ReadSequential(pg,msb,lsb,pBuf,sizeof(DL_LOG_ITEM));
			//} else {
			//const uint8_t z = (sizeof(DL_LOG_ITEM) - v);
			//AT24CXX_ReadSequential(pg,msb,lsb,pBuf,z);
			//delay_ms(5);
			//AT24CXX_ReadSequential(pPara->iter_send.byte[PAGE_BYTE],pPara->iter_send.byte[MSB_BYTE],0,pBuf+z,v);
		//}
//
//}
//

static void datalogger_tick(uint32_t timeNow)
{
	
	rtc_set_alarm(timeNow+g_timespan);

	gpio_toggle_pin(STATUS_LED_PIN);
	
	//unlock the spin cycle of the process manager
	dl_cycle_lock = false;
	
	wdt_reset();

	const uint32_t deltaT = (timeNow - g_timePrev);
	g_timePrev = timeNow;
	
	g_acc_task_sync += deltaT;
	if((TASK_STOP==task_status_sync_time) && (g_acc_task_sync>=g_interv_task_sync)) { 
		g_acc_task_sync = 0;
		task_status_sync_time=TASK_READY;
	}

	g_acc_task_store += deltaT;
	if((TASK_STOP==task_status_store_data) && (g_acc_task_store>=g_interv_task_store)) 
	{
		g_acc_task_store = 0;
		//Snapshot of the memory that should be archived by the datalogger
		//The slow EEPROM reading procedure goes on the main loop
		datalogger_snapshot_data(timeNow); 
		task_status_store_data=TASK_READY;
	}

	g_acc_task_send_RT += deltaT;
	if((TASK_STOP==task_status_send_data_RT) && (g_acc_task_send_RT>=g_interv_task_send_RT)) 
	{
		g_acc_task_send_RT = 0;
		task_status_send_data_prepare_RT=TASK_READY;
	}

	//If any of the tasks is getting unscheduled for too long
	//means that that task is hanged somewhere...
	//we may decide to handle this case doing a system reset


	if(g_acc_task_send_RT>(3*g_interv_task_send_RT)) {
		wdt_reset_mcu();
	}

//	RAINGAUGE_STATS rs;
//	raingauge_get_stats(0,&rs);

	//char szBuf[24];
	//sprintf_P(szBuf,PSTR("\r\n@ %lu:%u\r\n"),timeNow,tc_read_count(&TCC1));
	//debug_string(NORMAL,szBuf,false);
	//dump_rainstats(0);
}


#include "devices/statusled/status_led.h"


void timer_overflow(void)
{
	usart_putchar(USART_DEBUG,'^');
}

//static uint8_t datalogger_reset_memory(void)
//{
	//
//}


void datalogger_dump_all()
{
	char szBuf[128];
	while(1) {
		debug_string(NORMAL,PSTR("Memory dump image BEGIN\r\n\r\n\r\n\r\n"),true);

		AT24CXX_iterator it = {.plain = 0};
		uint8_t buf[16];
		AT24CXX_Init();

		for(;it.plain<LOG_LAST_VALID_ADDRESS;it.plain+=16)
		{
			sprintf_P(szBuf,PSTR("\r\n%03d:%03d:%03d\t"),it.byte[PAGE_BYTE],it.byte[MSB_BYTE],it.byte[LSB_BYTE]);
			debug_string(NORMAL,szBuf,false);

			uint8_t pg,msb,lsb;
			AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);

			AT24CXX_ReadBlock(pg,msb,lsb,buf,16);
			for (uint8_t i=0;i<16;++i)
			{
				const char c = buf[i];
				sprintf_P(szBuf+(3*i),PSTR(" %02X"),c);
				if((c>31) && (c<127)) {
					szBuf[52+i] = c;
					} else {
					szBuf[52+i] = '.';
				}
			}
			szBuf[48] = 32;
			szBuf[49] = 32;
			szBuf[50] = 32;
			szBuf[51] = 32;

			szBuf[68] = 0;
			
			debug_string(NORMAL,szBuf,false);
			delay_ms(5);
		}

		debug_string(NORMAL,PSTR("\r\n\r\n\r\n\r\nMemory dump image END\r\n\r\n\r\n\r\n"),true);
	}

}



void datalogger_dump(AT24CXX_iterator itBeg,AT24CXX_iterator itEnd)
{
	char szBuf[128];
	while(1) {
		debug_string(NORMAL,PSTR("Memory dump image BEGIN\r\n\r\n\r\n\r\n"),true);

		AT24CXX_iterator it = {.plain = 0};
		uint8_t buf[16];
		AT24CXX_Init();

		for(;it.plain<LOG_LAST_VALID_ADDRESS;it.plain+=16)
		{
			sprintf_P(szBuf,PSTR("\r\n%03d:%03d:%03d\t"),it.byte[PAGE_BYTE],it.byte[MSB_BYTE],it.byte[LSB_BYTE]);
			debug_string(NORMAL,szBuf,false);

			uint8_t pg,msb,lsb;
			AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);

			AT24CXX_ReadBlock(pg,msb,lsb,buf,16);
			for (uint8_t i=0;i<16;++i)
			{
				const char c = buf[i];
				sprintf_P(szBuf+(3*i),PSTR(" %02X"),c);
				if((c>31) && (c<127)) {
					szBuf[52+i] = c;
					} else {
					szBuf[52+i] = '.';
				}
			}
			szBuf[48] = 32;
			szBuf[49] = 32;
			szBuf[50] = 32;
			szBuf[51] = 32;

			szBuf[68] = 0;
			
			debug_string(NORMAL,szBuf,false);
			delay_ms(5);
		}

		debug_string(NORMAL,PSTR("\r\n\r\n\r\n\r\nMemory dump image END\r\n\r\n\r\n\r\n"),true);
	}

}

//Power of 2
#define DL_ITER_CBUFFER_LEN 0x08

static uint8_t datalogger_read_iterators_from_eeprom(void)
{
	const static uint8_t S = sizeof(AT24CXX_iterator);
	const static AT24CXX_iterator TAG = {.plain = 0xFAFAFAFA};
	AT24CXX_iterator tmp;
	
	//Iterators are stored in the eeprom in a circular buffer
	//that allows a 1/8th of the writes on the singular address
	
	uint8_t i;
	uint8_t j;
	
	AT24CXX_Init();

	for(j=0;j<DL_EEPROM_ITERATORS;++j)	{
		const size_t idx = j*DL_ITER_CBUFFER_LEN;
		for(i=0;i<DL_ITER_CBUFFER_LEN;++i) {
			AT24CXX_iterator it = {.plain = (4+i*S)+idx};
			uint8_t msb,lsb,pg;
			AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
			AT24CXX_ReadBlock(pg,msb,lsb,(uint8_t*)&tmp,S);
			delay_ms(5);
			//If we have found the TAG that defines the end of the Buffer
			//it means that at the previous position we can find the
			//actual value of the variable
			if(tmp.plain==TAG.plain) {
				const uint8_t l = (i-1) & (DL_ITER_CBUFFER_LEN-1);
				AT24CXX_iterator it = {.plain = (4+l*S)+idx};
				AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
				AT24CXX_ReadBlock(pg,msb,lsb,(uint8_t*)&g_eeprom_iter[j],S);
				delay_ms(5);
			
				break;
			}
		}
	}

	return 1;
}

static uint8_t datalogger_write_iterators_to_eeprom(void)
{
	const static uint8_t S = sizeof(AT24CXX_iterator);
	const static AT24CXX_iterator TAG = {.plain = 0xFAFAFAFA};
	AT24CXX_iterator tmp;
	
	//Iterators are stored in the eeprom in a circular buffer
	//that allows a 1/8th of the writes on the singular address
	
	uint8_t i;
	uint8_t j;
	
	AT24CXX_Init();


	for(j=0;j<DL_EEPROM_ITERATORS;++j)	{
		const size_t idx = j*DL_ITER_CBUFFER_LEN;
		for(i=0;i<8;++i) {
			AT24CXX_iterator it = {.plain = (4+i*S)+idx};
			uint8_t msb,lsb,pg;
			AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
			AT24CXX_ReadBlock(pg,msb,lsb,(uint8_t*)&tmp,S);
			delay_ms(5);
			//If we have found the TAG that defines the end of the Buffer
			//it means that at the previous position we can find the
			//actual value of the variable
			if(tmp.plain==TAG.plain) {
				//It is a circular buffer so the previous
				//position may be at the end of the regular,linear, buffer
				const uint8_t l = (i-1) & (DL_ITER_CBUFFER_LEN-1);
				AT24CXX_iterator it = {.plain = (4+l*S)+idx};
				AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
				AT24CXX_ReadBlock(pg,msb,lsb,(uint8_t*)&tmp,S);
				delay_ms(5);
				//Verifies that the value stored in the eeprom
				//it is not equal to the one that we want to write
				//in that case we simply skip
				if(tmp.plain==g_eeprom_iter[j].plain) {
					break;
				}
				//Values are different proceed on writing it 
				AT24CXX_WriteBlock(pg,msb,lsb,(uint8_t*)&g_eeprom_iter[j],S);
				delay_ms(5);
				//Write the TAG in the next position
				const uint8_t m = (l+1) & (DL_ITER_CBUFFER_LEN-1);
				it.plain = (4+m*S)+idx;
				AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
				AT24CXX_WriteBlock(pg,msb,lsb,(uint8_t*)&TAG,S);
				delay_ms(5);
				break;
			}
		}
	}

	return 0;
}

static uint8_t datalogger_init_iterators_to_eeprom(void)
{
	const static uint8_t S = sizeof(AT24CXX_iterator);
	const static AT24CXX_iterator TAG = {.plain = 0xFAFAFAFA};
	
	//Iterators are stored in the eeprom in a circular buffer
	//that allows a 1/8th of the writes on the singular address
	
	uint8_t i;
	uint8_t j;
	
	AT24CXX_Init();

	for(j=0;j<DL_EEPROM_ITERATORS;++j)	{
		const size_t idx = j*DL_ITER_CBUFFER_LEN;
		
		AT24CXX_iterator it = {.plain = 4+idx};
		uint8_t msb,lsb,pg;
		AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
		AT24CXX_WriteBlock(pg,msb,lsb,(uint8_t*)&g_eeprom_iter[j],S);
		delay_ms(5);

		
		for(i=1;i<DL_ITER_CBUFFER_LEN;++i) {
			AT24CXX_iterator it = {.plain = (4+i*S)+idx};
			uint8_t msb,lsb,pg;
			AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
			AT24CXX_WriteBlock(pg,msb,lsb,(uint8_t*)&TAG,S);
			delay_ms(5);
		}
	}
	return 0;
}

static uint8_t datalogger_reset_eeprom(void)
{
	debug_string(NORMAL,PSTR("(datalogger_init) resetting eeprom\r\n"),true);

	char szBUF[4] = {'A','C','0','1'};
		
	AT24CXX_WriteBlock(0,0,0,(uint8_t*)szBUF,4);
	delay_ms(5);

	for (uint8_t i=0;i<DL_EEPROM_ITERATORS;++i)
	{
		g_eeprom_iter[i].plain = LOG_FIRST_VALID_ADDRESS;
	}
		

	datalogger_init_iterators_to_eeprom();


	return 0;	
}

uint8_t datalogger_init(void)
{
	char szBUF[64];

	debug_string(VERBOSE,PSTR("datalogger init\r\n"),true);
	
	rtc_set_time(DL_T0);

	if (!cfg_check())
	{
		debug_string(NORMAL,PSTR("\r\n\r\n[ERROR] Configuration is missing, check the internal eeprom image\r\n"),true);
		return 1;
	}

	cfg_get_datalogger_timings(&g_interv_task_send_RT,&g_interv_task_store,&g_interv_task_sync,&g_timespan);

	sprintf_P(szBUF,PSTR("DL_TASK_STORE_DT: %lu\r\n"),g_interv_task_store);
	debug_string(NORMAL,szBUF,false);
	sprintf_P(szBUF,PSTR("DL_TASK_SEND_DT: %lu\r\n"),g_interv_task_send_RT);
	debug_string(NORMAL,szBUF,false);
	sprintf_P(szBUF,PSTR("DL_TASK_SYNC_DT: %lu\r\n"),g_interv_task_sync);
	debug_string(NORMAL,szBUF,false);
	sprintf_P(szBUF,PSTR("DL_TIME_DT: %lu\r\n"),g_timespan);
	debug_string(NORMAL,szBUF,false);

	// USART options.
	static usart_rs232_options_t USART_SERIAL_OPTIONS = {
		.baudrate = USART_GPRS_BAUDRATE,
		.charlength = USART_CHAR_LENGTH,
		.paritytype = USART_PARITY,
		.stopbits = USART_STOP_BIT
	};

	// Initialize usart driver in RS232 mode
	usart_serial_init(USART_GPRS, &USART_SERIAL_OPTIONS);
	

	AT24CXX_Init();
	AT24CXX_ReadBlock(0,0,0,(uint8_t*)szBUF,4);
	delay_ms(5);
	
	uint8_t isundef = 0;
	//if the eeprom has to be initialized
	if((szBUF[0]!='A') || (szBUF[1]!='C') || (szBUF[2]!='0') || (szBUF[3]!='1'))
	{
		datalogger_reset_eeprom();
	}


	for (uint8_t i=0;i<DL_EEPROM_ITERATORS;++i)
	{
		g_eeprom_iter[i].plain = 0xFFFFFFFF;
	}
	
	datalogger_read_iterators_from_eeprom();

	debug_string(NORMAL,PSTR("(datalogger_init) eeprom iterators are :\r\n"),true);
	for (uint8_t i=0;i<DL_EEPROM_ITERATORS;++i)
	{
		if(g_eeprom_iter[i].plain == 0xFFFFFFFF) isundef = true;
		AT24CXX_iterator_report(&g_eeprom_iter[i]);
	}

	//if the eeprom has to be initialized 
	if(isundef)
	{
		
		debug_string(NORMAL,PSTR("(datalogger_init) resetting iterators\r\n"),true);
		

		for (uint8_t i=0;i<DL_EEPROM_ITERATORS;++i)
		{
			g_eeprom_iter[i].plain = LOG_FIRST_VALID_ADDRESS;
		}
		
		datalogger_init_iterators_to_eeprom();
	}

	dl_test_now();

	uint8_t ii=0;

	while(1) {	
		if(++ii==10) {
			wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_8CLK);
			wdt_enable();
			wdt_reset_mcu();
			delay_ms(1000);
		}
		if(0==sim900_init()) {
			break;
		}
	}
	
	g_is_GPRS_module_OK = true;


	ii=0;
	while(1) {
		if(++ii==10) {
			wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_8CLK);
			wdt_enable();
			wdt_reset_mcu();
			delay_ms(1000);
		}
		if( 0 == dl_task_sync_time() ) {
			//we finally got the time sync
			//so we can configure the scheduler time
			g_timePrev = rtc_get_time();
			break;
		}
	}

	tc_enable(&TCC0);
	tc_enable(&TCC1);

	tc_set_direction(&TCC0,TC_UP);
	tc_set_direction(&TCC1,TC_UP);

	TCC1.CTRLA = TC_CLKSEL_EVCH0_gc;

	//Setting the event channel
	//Channel0 is the TCC0 overflow event used to
	//clock the TCC1 16bit timer
	EVSYS.CH0MUX = EVSYS_CHMUX_TCC0_OVF_gc;

	TCC1.CTRLD = TC_EVSEL_CH1_gc | TC_EVACT_RESTART_gc;
	TCC0.CTRLD = TC_EVSEL_CH1_gc | TC_EVACT_RESTART_gc;

	//Timer TCC0 goes at 1/8 of the main clock
	//with a period of 30000 cycles
	//overflows will come at a rate of 100Hz approx
	//the TCC1 timer will count those overflows
	//using the event channel 0 as clock source
	tc_write_period(&TCC0,30000);
	tc_write_clock_source(&TCC0, TC_CLKSEL_DIV8_gc);

	const uint32_t res = tc_get_resolution(&TCC0);

	sprintf_P(szBUF,PSTR("TCC0 resolution = %lu\r\n"),res);
	debug_string(NORMAL,szBUF,false);

	rtc_set_callback(datalogger_tick);
	rtc_set_alarm_relative(5);
	
	wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_8KCLK);
	wdt_enable();
	
	return 0;

}

uint16_t dl_task_sync_time( void )
{
	//TODO: consider a better time sync mechanism


	task_status_sync_time = TASK_RUNNING;
	uint16_t err = 0;
	
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"DL_TASK_SYNC_TIME");

	if(!g_is_GPRS_module_OK) {
		debug_string(NORMAL,PSTR("(datalogger_sync_time) the GPRS status flag is not OK, this task will finish here\r\n"),true);
		err = 1;
		goto quit_synctime_task;
	}

	err = sim900_GPRS_simple_open();
	if(err!=0) {
		goto quit_synctime_task;
	}

	char buf[128];
	cfg_get_service_url_time(buf,128);
	uint8_t l = 30;
	err = sim900_http_get(buf,false,buf,&l);

	sim900_http_close();
	
	if(err!=0) {
		goto quit_synctime_task;
	}

	debug_string(VERY_VERBOSE,PSTR("(DL_synctime) service returned: "),true);
	debug_string(VERY_VERBOSE,&buf[0],false);
	debug_string(VERY_VERBOSE,szCRLF,true);

	uint32_t t = strtoul(&buf[0],NULL,10);
	rtc_set_time(t);

	struct calendar_date adate;
	calendar_timestamp_to_date(t,&adate);
	sprintf_P(buf,PSTR("%02d/%02d/%d %02d:%02d:%02d\r\n"),adate.date+1,adate.month+1,adate.year,adate.hour,adate.minute,adate.second);
	debug_string(NORMAL,PSTR("(DL_synctime) New date is: "),PGM_STRING);
	debug_string(NORMAL,buf,RAM_STRING);

	rtc_set_alarm_relative(g_timespan);

quit_synctime_task:

	sim900_GPRS_simple_close();
	

	task_status_sync_time = TASK_STOP;
	return err;
}

static uint8_t g_sendverb = NORMAL;

static uint16_t dl_task_cmd_check_prepare(void)
{
	//Datalogger is empty
	if(g_eeprom_iter[LOG_END].plain==g_eeprom_iter[LOG_BEGIN].plain) {
		debug_string(NORMAL,PSTR("(before_send_check) Datalogger is empty. Nothing to send\r\n"),true);
		return 0;
		
	}

	if(!g_is_GPRS_module_OK) {
		debug_string(NORMAL,PSTR("(before_send_check) The GPRS status is not OK this task will end here\r\n"),true);
		return -1;
	}


	if(STATUS_OK!= AT24CXX_Init())
	{
		debug_string(NORMAL,PSTR("(before_send_check) AT24CXX init went wrong, aborting procedure\r\n"),true);
		return -1;
	}

	return sim900_GPRS_simple_open();	
}

uint16_t dl_task_send_data_prepare( void )
{
	task_status_send_data_prepare = TASK_RUNNING;
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"DL_TASK_SEND_DATA_PREPARE");

	const uint16_t r = dl_task_cmd_check_prepare();

	if(r==0) {
		//g_sendverb = NORMAL;
		dl_send_params.iter_send.plain = g_eeprom_iter[LOG_END].plain;
		dl_send_params.dt_end = rtc_get_time() - 86400;
		dl_send_params.send_once = false;
	}

	task_status_send_data_prepare = TASK_STOP;
	return r;
}


uint16_t dl_task_send_data_prepare_RT( void )
{
	task_status_send_data_prepare_RT = TASK_RUNNING;
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"DL_TASK_SEND_DATA_PREPARE_RT");

	const uint16_t r = dl_task_cmd_check_prepare();

	if(r==0) {
		g_sendverb = VERBOSE;
		dl_send_params_RT.iter_send.plain = g_eeprom_iter[LOG_END].plain;
		dl_send_params_RT.dt_end = rtc_get_time() - 86400;
		dl_send_params_RT.send_once = true;
		//dl_send_params_RT.send_once = false;
	}

	task_status_send_data_prepare_RT = TASK_STOP;
	return r;
}


static uint8_t dl_get_record(const AT24CXX_iterator * const pIter,DL_LOG_ITEM * const pDS)
{

	AT24CXX_iterator it = *pIter;
	uint8_t pg,msb,lsb;
	AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
	uint8_t * const pBuf = (uint8_t *) pDS;
	const uint8_t v = it.byte[LSB_BYTE];
	//is it a plain page or a overlapping read ?
	if ((v>=sizeof(DL_LOG_ITEM)) || (v==0)) {
		AT24CXX_ReadBlock(pg,msb,lsb,pBuf,sizeof(DL_LOG_ITEM));
		//delay_ms(1);
	} else {
		const uint8_t z = (sizeof(DL_LOG_ITEM) - v);
		AT24CXX_ReadBlock(pg,msb,lsb,pBuf,z);
		delay_ms(3);
		it.plain += z;
		if(it.plain>LOG_LAST_VALID_ADDRESS) {
			it.plain = LOG_FIRST_VALID_ADDRESS;
		}
		AT24CXX_ReadBlock(pIter->byte[PAGE_BYTE],pIter->byte[MSB_BYTE],0,pBuf+z,v);
	}
	
	return 0;
}

static void dl_iterator_moveback(AT24CXX_iterator * pIter,uint16_t range)
{
	const int32_t n = range*sizeof(DL_LOG_ITEM);
	const int32_t x = LOG_FIRST_VALID_ADDRESS+n;
	const int32_t z = pIter->plain;
	const int32_t k = z-g_eeprom_iter[LOG_BEGIN].plain;

	if(n>k) {
		debug_string(NORMAL,PSTR("(dl_iterator_moveback) iterator reached the START position\r\n"),true);
		pIter->plain = g_eeprom_iter[LOG_BEGIN].plain;
		return;
	}

	if(z<x) {
		debug_string(NORMAL,PSTR("(dl_iterator_moveback) iterator wraps datalogger\r\n"),true);
		pIter->plain = LOG_LAST_VALID_ADDRESS - (x-z);
	} else {
		pIter->plain = z - n;
	}
}

static void dl_iterator_moveforward(AT24CXX_iterator * pIter,uint16_t range)
{
	const int32_t n = range*sizeof(DL_LOG_ITEM);
	const int32_t x = LOG_LAST_VALID_ADDRESS-n;
	const int32_t z = pIter->plain;
	const int32_t k = g_eeprom_iter[LOG_END].plain - z;

	if(n>k) {
		debug_string_P(NORMAL,PSTR("(dl_iterator_moveback) iterator reached the END position\r\n"));
		pIter->plain = g_eeprom_iter[LOG_END].plain;
		return;
	}

	if(z>x) {
		debug_string_P(NORMAL,PSTR("(dl_iterator_moveforward) iterator wraps datalogger\r\n"));
		pIter->plain = LOG_FIRST_VALID_ADDRESS + (z-x);
	} else {
		pIter->plain = z + n;
	}
}

enum DL_EEPROM_SEARCH_MODE {LEFT_BOUND,RIGHT_BOUND,EXACT_MATCH};
static uint8_t dl_search_record_by_date(uint32_t dt,enum DL_EEPROM_SEARCH_MODE search_mode,AT24CXX_iterator * pIter)
{
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"DL_SEARCH_RECORD_BY_DATE");

	uint8_t err = 0;

	if(pIter==NULL) {
		debug_string_P(NORMAL,PSTR("ERROR (dl_search_record_by_date) parameter pIter is NULL\r\n"));
		err = -1;
		goto quit_search;
	}
	
	//Datalogger is empty
	if(g_eeprom_iter[LOG_END].plain==g_eeprom_iter[LOG_BEGIN].plain) {
		debug_string_P(NORMAL,PSTR("(dl_search_record_by_date) Datalogger is empty. Nothing to search\r\n"));
		err = 1;
		goto quit_search;
	}

	if(STATUS_OK!= AT24CXX_Init())
	{
		debug_string_P(NORMAL,PSTR("(dl_search_record_by_date) AT24CXX init went wrong, aborting procedure\r\n"));
		err = -1;
		goto quit_search;
	}

	char szBuf[128];
	//first check the last record
	DL_LOG_ITEM ds;
	AT24CXX_iterator it = g_eeprom_iter[LOG_END];

	dl_iterator_moveback(&it,1);

	uint8_t reg_skip = 128;
	uint8_t reg_dir  = 0; //0 = backward, 1= forward
	while(1){
		

		if(0!=dl_get_record(&it,&ds))
		{
			err = 1;
			goto quit_search;
		}

		if(g_log_verbosity>=VERBOSE) {
			sprintf_P(szBuf,PSTR("Search called with dt=%lu found %lu\r\n"),dt,ds.data_timestamp);
			debug_string(NORMAL,szBuf,RAM_STRING);
		}
		
		if(reg_dir == 0) { //if we are searching backward
			if(ds.data_timestamp<dt) {
				debug_string_P(VERBOSE,PSTR("Going from backward to forward\r\n"));
				reg_skip >>= 1;
				reg_dir = 1;
			} else if(ds.data_timestamp==dt){
				break;
			}
		} else {//if we are searching forward
			if(ds.data_timestamp==dt){
				break;
			}else if(ds.data_timestamp>dt){
				debug_string_P(VERBOSE,PSTR("Going from forward to backward\r\n"));
				reg_skip >>= 1;
				reg_dir = 0;
			}
		}

		
		if(reg_skip==0) break;
		
		//Calculate the next search position
		if(reg_dir == 0) { //if we are going to search backward
			if(it.plain == LOG_FIRST_VALID_ADDRESS) {
				//scanned all the memory but did't find any match
				debug_string_P(VERBOSE,PSTR("Going backward - No match found\r\n"));
				break;
			}
			dl_iterator_moveback(&it,reg_skip);
		} else { //if we are going to search forward
			if(it.plain == LOG_LAST_VALID_ADDRESS) {
				//scanned all the memory but did't find any match
				debug_string_P(VERBOSE,PSTR("Going forward - No match found\r\n"));
				break;
			}
			dl_iterator_moveforward(&it,reg_skip);
		}
	}

	if(ds.data_timestamp<dt) {
		if(search_mode==LEFT_BOUND){
			err = 1;
			goto quit_search;
		} else if(search_mode==EXACT_MATCH) {
			err = 1;
			goto quit_search;
		}
		//else if RIGHT bound we have a match
		pIter->plain = it.plain;
	} else if(ds.data_timestamp==dt){
		pIter->plain = it.plain;
	}else if(ds.data_timestamp>dt){
		if(search_mode==RIGHT_BOUND){
			err = 1;
			goto quit_search;
		} else if(search_mode==EXACT_MATCH) {
			err = 1;
			goto quit_search;
		}
		//else if LEFT bound we have a match
		pIter->plain = it.plain;
	}

	if(g_log_verbosity>=NORMAL) {
			debug_string_P(NORMAL,PSTR("\tRecord found "));
			AT24CXX_iterator_report(&it);
			sprintf_P(szBuf,PSTR("\r\n\tRecord  dt=%lu\r\n"),ds.data_timestamp);
			debug_string(NORMAL,szBuf,RAM_STRING);
	}

	
quit_search:

	return err;
}

static uint16_t	dl_cmd_get_by_date_range(uint32_t dtBeg,uint32_t dtEnd)
{
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"DL_CMD_GET_BY_DATE_RANGE");

	AT24CXX_iterator itEnd = {0};
	uint16_t err = dl_search_record_by_date(dtEnd ,RIGHT_BOUND,&itEnd);

	if(err == 0) {
		dl_send_params.iter_send = itEnd;
		dl_send_params.dt_end = dtBeg;
		dl_send_params.send_once = false;
		task_status_send_data = TASK_READY;
		goto quit_cmd_get_by_date_range;
	}

//We didn't find the requested record


//////////////////////////////////////////////////////////////////////////
quit_cmd_get_by_date_range:

	return err;
}


//This procedure parses the command string from the server and 
//will call the internal procedure to accomplish
static uint16_t dl_task_cmd_check_2(char * const pBuf,uint16_t lenBuf)
{
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE);

	uint16_t err = 0;

	if(0==strncasecmp_P(pBuf,PSTR("OK"),2)){
		debug_string_P(VERBOSE,PSTR("\tgot OK\r\n"));
	
	} else if(0==strncasecmp_P(pBuf,PSTR("GET_BY_DATE_RANGE:"),18)){

		uint32_t dtBeg , dtEnd;
		
		char * psz;
		dtBeg = strtoul(pBuf+18,&psz,0);
		dtEnd = strtoul(psz+1,NULL,0);

		if(g_log_verbosity>=VERBOSE) {
			debug_string_P(VERBOSE,PSTR("\tGET_BY_DATE_RANGE parameters "));
			char szDbg[80];
			sprintf_P(szDbg,PSTR("dtBeg:%lu\tdtEnd:%lu\r\n"),dtBeg,dtEnd);
			debug_string(VERBOSE,szDbg,RAM_STRING);
		}
		
		err = dl_cmd_get_by_date_range(dtBeg,dtEnd);
	} else if(0==strncasecmp_P(pBuf,PSTR("FW_UPDATE:"),10)){
			debug_string_P(VERBOSE,PSTR("\tFW_UPDATE parameters "));
	} else 	if(0==strncasecmp_P(pBuf,PSTR("KO"),2)){
		debug_string_P(VERBOSE,PSTR("\t[WARNING] Server returned a KO message\r\n"));
	} else {
		debug_string_P(VERBOSE,PSTR("\t[WARNING] Server returned an unknown statement\r\n"));
	}
	
	return err;
}

static uint16_t dl_task_cmd_check_1(void)
{
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE);

	const uint16_t BUFFER_SIZE = 256;
	char szBuf[BUFFER_SIZE];

	cfg_get_service_url_send(szBuf,64);
	uint8_t l1 = strnlen(szBuf,64)-1;

	snprintf_P(szBuf+l1,BUFFER_SIZE-l1,PSTR("/CMD_CHK?AWSID="));
	l1 += strnlen(szBuf+l1,64);

	cfg_get_aws_id(szBuf+l1,64);
	l1 += strnlen(szBuf+l1,64);

	snprintf_P(szBuf+l1,BUFFER_SIZE-l1,PSTR("&API_VER=1"));

	char retBuf[64];
	uint8_t lenRetBuf=60;
	uint16_t err = sim900_http_get(szBuf,false,retBuf,&lenRetBuf);
	sim900_http_close();

	if(err!=0) {
		return err;
	}

	return dl_task_cmd_check_2(retBuf,lenRetBuf);
}

uint16_t dl_task_cmd_check(void)
{
	task_status_server_cmd_check = TASK_RUNNING;

	DEBUG_PRINT_FUNCTION_NAME(VERBOSE);

	uint16_t err = dl_task_cmd_check_prepare();

	if(err==0) {
		err = dl_task_cmd_check_1();
	}

quit_server_cmd_check:

	task_status_server_cmd_check = TASK_STOP;
	return err;
}


uint16_t dl_task_send_data_RT( void )
{
	task_status_send_data_RT = TASK_RUNNING;
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE);
	uint16_t err = send_data_with_post(&dl_send_params_RT);
	if(err==99) {
		task_status_send_data_RT = TASK_READY;
		err = 0;
	} else {
		sim900_GPRS_simple_close();
		task_status_send_data_RT = TASK_STOP;
		task_status_server_cmd_check = TASK_READY;
	}

	return err;
}


uint16_t dl_task_send_data(void)
{
	task_status_send_data = TASK_RUNNING;
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE);
	uint16_t err = send_data_with_post(&dl_send_params);
	if(err==99) {
		task_status_send_data = TASK_READY;
		err = 0;
	} else {
		sim900_GPRS_simple_close();
		task_status_send_data = TASK_STOP;
	}

	return err;
}


uint8_t send_data_with_get( DL_SEND_PARAMS * const pPara )
{

	uint8_t err = 0;

#ifdef DATALOGGER_AUX_YES
	const uint16_t BUFFER_SIZE = 256;
#else
	const uint16_t BUFFER_SIZE = 256;
#endif

	char szBuf[BUFFER_SIZE];
	
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE);

	//Datalogger is empty
	if(pPara->iter_send.plain==g_eeprom_iter[LOG_BEGIN].plain) {
		debug_string(NORMAL,PSTR("\tNothing left to send, quit the procedure\r\n"),true);
		err = 0;
		goto quit_senddata_task;
	}

	DL_LOG_ITEM ds;
	AT24CXX_iterator it;
	
	//calculating the reverse iterator
	const uint32_t x = LOG_FIRST_VALID_ADDRESS+sizeof(DL_LOG_ITEM);
	const uint32_t z = pPara->iter_send.plain;
	if(z<x) {
		debug_string(NORMAL,PSTR("\titerator wraps datalogger\r\n"),true);
		it.plain = LOG_LAST_VALID_ADDRESS - (x-z);
	} else {
		it.plain = z - sizeof(DL_LOG_ITEM);
	}

	if(NORMAL==g_sendverb) {
		debug_string(g_sendverb,PSTR("\tsending record "),true);
		AT24CXX_iterator_report(&it);
	}
	uint8_t pg,msb,lsb;
	AT24CXX_iterator_to_address(&it,&pg,&msb,&lsb);
	uint8_t * const pBuf = (uint8_t *) &ds;
	const uint8_t v = pPara->iter_send.byte[LSB_BYTE];
	//is it a plain page or a overlapping read ?
	if ((v>=sizeof(DL_LOG_ITEM)) || (v==0)) {
		AT24CXX_ReadBlock(pg,msb,lsb,pBuf,sizeof(DL_LOG_ITEM));
	} else {
		const uint8_t z = (sizeof(DL_LOG_ITEM) - v);
		AT24CXX_ReadBlock(pg,msb,lsb,pBuf,z);
		delay_ms(5);
		AT24CXX_ReadBlock(pPara->iter_send.byte[PAGE_BYTE],pPara->iter_send.byte[MSB_BYTE],0,pBuf+z,v);
	}

	const uint32_t min_date_boundary = dl_send_params_RT.dt_end;

	if(NORMAL==g_sendverb) {
		sprintf_P(szBuf,PSTR("\r\n\tSending record with timestamp %lu\r\n"),ds.data_timestamp);
		debug_string(NORMAL,szBuf,false);
	}

	//Record flag==0 means that is unsent
	//if the record is older than t and already sent
	//the procedure will end
	if (ds.data_timestamp<min_date_boundary)
	{
		debug_string(NORMAL,PSTR("\tMinimum date boundary reached\r\n"),true);
		//sim900_GPRS_close();

		goto quit_senddata_task;
	}

	if ((dl_send_params_RT.send_once)&&(ds.flags!=0))
	{
		debug_string(g_sendverb,PSTR("\tskips an already sent record\r\n"),true);
		goto skip1_senddata_task;
	}

	RAINGAUGE_STATS * rs = &ds.raingauge_stats[0];

	const uint16_t mxs0   = rs->maxSlope;
	
	cfg_get_service_url_send(szBuf,64);
	uint8_t l1 = strnlen(szBuf,64);
	szBuf[l1  ] = 'A';
	szBuf[l1+1] = 'W';
	szBuf[l1+2] = 'S';
	szBuf[l1+3] = 'I';
	szBuf[l1+4] = 'D';
	szBuf[l1+5] = '=';
	l1+=6;
	cfg_get_aws_id(szBuf+l1,64);
	l1 += strnlen(szBuf+l1,64);

	snprintf_P(szBuf+l1,BUFFER_SIZE-l1,PSTR("&TIME=%lu&B=%u&P=%u&S=%u"),ds.data_timestamp,ds.vBat,ds.raingauge_stats[0].tips,mxs0);


#ifdef DATALOGGER_AUX_YES
	uint8_t  i=strnlen(szBuf,BUFFER_SIZE);

	if(i==BUFFER_SIZE) {
		debug_string(VERY_VERBOSE,PSTR("\tThe buffer is not enough to contain the return string"),PGM_STRING);
		i=16;
	}
	rs = &dl_log.raingauge_stats[1];
	const float mxs1   = rs->maxSlope==MAXSLOPE_UNDEF_VALUE?0:rs->maxSlope;
	snprintf_P(szBuf+i,BUFFER_SIZE,PSTR("P=%u&S=%u"),rs->tips,mxs1);
#endif


	char retBuf[32];
	uint8_t lenRetBuf=30;
	//const uint8_t e = 0;
	err = sim900_http_get(szBuf,false,retBuf,&lenRetBuf);
	sim900_http_close();
//	debug_string(NORMAL,szBuf,false);
//	debug_string(NORMAL,szCRLF,true);
	if(err!=0) {
		goto quit_senddata_task;
	}


	//Flag the record as sent
	ds.flags = 1;
	
	//q points to ds.flags field, so only one byte will be written
	const AT24CXX_iterator q = {.plain = it.plain + ((&ds.flags)-((uint8_t*)&ds))};
	AT24CXX_iterator_to_address(&q,&pg,&msb,&lsb);
	AT24CXX_WriteBlock(pg,msb,lsb,(uint8_t*)&ds.flags,1);
	delay_ms(5);

skip1_senddata_task:

	if (pPara->iter_send.plain == g_eeprom_iter[LOG_BEGIN].plain)
	{
		goto quit_senddata_task;
	}

	pPara->iter_send = it;

	debug_string(g_sendverb,PSTR("\trequesting another iteration\r\n"),PGM_STRING);
	g_sendverb = g_log_verbosity;
	return 99;
		

quit_senddata_task:

	pPara->iter_send.plain = 0;
	return err;
}


uint16_t send_data_with_post( DL_SEND_PARAMS * const pPara )
{

	uint16_t err = 0;

#ifdef DATALOGGER_AUX_YES
	const uint16_t BUFFER_SIZE = 1024;
#else
	const uint16_t BUFFER_SIZE = 1024;
#endif

	char szBuf[BUFFER_SIZE];
	AT24CXX_iterator q[16] = {{0}}; 
	static const uint8_t QUEUE_LEN = {sizeof(q) / sizeof(AT24CXX_iterator)};

	DEBUG_PRINT_FUNCTION_NAME(VERBOSE);


	//Datalogger is empty
	if(pPara->iter_send.plain==g_eeprom_iter[LOG_BEGIN].plain) {
		debug_string(NORMAL,PSTR("\tNothing left to send, quit the procedure\r\n"),true);
		err = 0;
		goto quit_senddatapost_task;
	}

	DL_LOG_ITEM ds;
	
	cfg_get_service_url_send(szBuf,64);
	const uint16_t lz = strnlen(szBuf,64)-1;
	
	szBuf[lz] = 0;
	
	const uint16_t post_data_start = lz+1;
	uint16_t post_data_end = post_data_start;

	szBuf[post_data_end  ] = 'A';
	szBuf[post_data_end+1] = 'W';
	szBuf[post_data_end+2] = 'S';
	szBuf[post_data_end+3] = 'I';
	szBuf[post_data_end+4] = 'D';
	szBuf[post_data_end+5] = '=';
	post_data_end+=6;
	cfg_get_aws_id(szBuf+post_data_end,64);
	post_data_end += strnlen(szBuf+post_data_end,64);
	
	const uint32_t now = rtc_get_time();
	
	snprintf_P(szBuf+post_data_end,BUFFER_SIZE-post_data_end,PSTR("&TID=%lu"),now);
	post_data_end += strnlen(szBuf+post_data_end,64);

	uint8_t pg,msb,lsb;
	const uint32_t min_date_boundary = dl_send_params_RT.dt_end;
	const uint8_t send_once = pPara->send_once;

	AT24CXX_iterator it = pPara->iter_send;

	uint8_t iter=0;
	while(iter<QUEUE_LEN) {

		if(it.plain==g_eeprom_iter[LOG_BEGIN].plain) {
			debug_string(NORMAL,PSTR("\tBegin of memory reached\r\n"),true);
			break;
		}

		dl_iterator_moveback(&it,1);


		if(VERBOSE<=g_sendverb) {
			debug_string(g_sendverb,PSTR("\trecord "),true);
			AT24CXX_iterator_report(&it);
		}
		
		dl_get_record(&it,&ds);
		
		char szDebug[128];
		if(NORMAL<=g_sendverb) {
			sprintf_P(szDebug,PSTR("\tRecord timestamp %lu\r\n"),ds.data_timestamp);
			debug_string(NORMAL,szDebug,false);
		}

		if (ds.data_timestamp<min_date_boundary)
		{
			debug_string(NORMAL,PSTR("\tMinimum date reached\r\n"),true);
			break;
		}

		if ((send_once)&&(ds.flags!=0))
		{
			debug_string(g_sendverb,PSTR("\tflag sent is set, skip\r\n"),true);
			continue;
		}

		RAINGAUGE_STATS * rs = &ds.raingauge_stats[0];

		const uint16_t mxs0   = rs->maxSlope;
	
	//////////////////////////////
		snprintf_P(szBuf+post_data_end,BUFFER_SIZE-post_data_end,PSTR("#TIME=%lu&B=%u&P=%u&S=%u"),ds.data_timestamp,ds.vBat,ds.raingauge_stats[0].tips,mxs0);
		post_data_end += strnlen(szBuf+post_data_end,64);

	//////////////////////////////
	
		if(ds.flags==0) {
			const AT24CXX_iterator tmp = {.plain = it.plain + ((&ds.flags)-((uint8_t*)&ds))};
			q[iter] = tmp;
		} 
		
		iter++;
	}
	
	if(iter==0) goto quit_senddatapost_task;
	
	pPara->iter_send = it;


	err = sim900_http_post(szBuf,false,szBuf+post_data_start,post_data_end-post_data_start,false);

	if(err!=0) {
		goto quit_senddatapost_task;
	}

	char szRet[32];
	uint8_t len_szRet=32;

	sim900_http_read(szRet,&len_szRet);
	debug_string(NORMAL,PSTR("\tHTTP read got : "),PGM_STRING);
	szRet[31] = 0;
	debug_string(NORMAL,szRet,RAM_STRING);
	debug_string(NORMAL,szCRLF,PGM_STRING);

	sim900_http_close();


	//Flag the record as sent
	ds.flags = 1;

	for(iter=0;iter<QUEUE_LEN;iter++) {
		if(q[iter].plain == 0) continue;
		AT24CXX_iterator_to_address(&q[iter],&pg,&msb,&lsb);
		AT24CXX_WriteBlock(pg,msb,lsb,(uint8_t*)&ds.flags,1);
		delay_ms(5);
	}

//Check if there are other records to send
	if ((ds.data_timestamp<min_date_boundary) || (pPara->iter_send.plain == g_eeprom_iter[LOG_BEGIN].plain))
	{
		goto quit_senddatapost_task;
	}

	debug_string(g_sendverb,PSTR("\trequesting another iteration\r\n"),true);
	g_sendverb = NORMAL;
	return 99; //the procedure did't send all the requested data
	

quit_senddatapost_task:
	sim900_http_close();
	g_sendverb = g_log_verbosity;

	pPara->iter_send.plain = 0;
	return err;
}


#define SNAPSHOT_SIZE 16
static volatile DL_LOG_ITEM g_data_snapshot[SNAPSHOT_SIZE];
static volatile uint8_t g_data_snapshot_num = 0;

static uint8_t  datalogger_snapshot_data(const uint32_t ts)
{

	DL_LOG_ITEM * const p_item = &g_data_snapshot[g_data_snapshot_num++];
	if(SNAPSHOT_SIZE==g_data_snapshot_num) {
		debug_string(NORMAL,PSTR("(datalogger_snapshot_data) Snapshot memory is full something wrong is happening\r\n"),PGM_STRING);
		g_data_snapshot_num=SNAPSHOT_SIZE-1;
	}
	
	p_item->data_timestamp = (ts==-1)?rtc_get_time():ts;
	p_item->flags = 0;
	
	RAINGAUGE_STATS * const rs0 = &p_item->raingauge_stats[0];
	raingauge_get_stats(0,rs0);
	raingauge_reset_stats(0,0);


	#ifdef DATALOGGER_AUX_YES
	RAINGAUGE_STATS * const rs1 = &p_item->raingauge_stats[1];
	raingauge_get_stats(1,rs1);
	raingauge_reset_stats(1,0);
	#endif

	//This triggers the event that resets the timer
	//giving to the new DATALOGGER time-slot a proper value as T0
	EVSYS.STROBE = 0x02;

	return 0;		
}


uint8_t  dl_task_store_data()
{
	task_status_store_data = TASK_RUNNING;
	uint8_t err = 0;
	//	debug_string(NORMAL,PSTR("Store Data IN\r\n"),true);

	if(STATUS_OK!=AT24CXX_Init())
	{
		goto task_store_quit;
	}

	uint8_t msb,lsb,page;
	AT24CXX_iterator * const it = &g_eeprom_iter[LOG_END];
	
	//TODO: debug info
	char szBuf[32];
	voltmeter_init();
	uint16_t vBat = voltmeter_getValue();
	uint16_t temp = thermometer_getValue();

	
	uint8_t idx = g_data_snapshot_num;
	while(idx!=0) {
		DL_LOG_ITEM * const pd = &g_data_snapshot[--idx];
		pd->vBat = vBat; //Dirty trick assuming that in this application the snapshot should never be more then one item


		sprintf_P(szBuf,PSTR("VBat = %u\r\nrecID %lu "),vBat,pd->data_timestamp);
		debug_string(NORMAL,szBuf,false);
		AT24CXX_iterator_report(it);

		
		AT24CXX_iterator_to_address(it,&page,&msb,&lsb);
		AT24CXX_iterator_add(it,sizeof(DL_LOG_ITEM));

		//debug_string(NORMAL,PSTR("NEXT RAW Iterator position  :"),true);
		//AT24CXX_iterator_report(it);

		if( it->plain > LOG_LAST_VALID_ADDRESS ) {
			it->plain = LOG_FIRST_VALID_ADDRESS;
		}

		//debug_string(NORMAL,PSTR("NEXT Actual Iterator position  :"),true);
		//AT24CXX_iterator_report(it);

		AT24CXX_iterator * const itBeg = &g_eeprom_iter[LOG_BEGIN];
		if( (it->plain == itBeg->plain) ) {
			itBeg->plain = it->plain + sizeof(DL_LOG_ITEM);
		}

		//debug_string(NORMAL,PSTR("BEGIN RAW Iterator position  :"),true);
		//AT24CXX_iterator_report(itBeg);

		if( itBeg->plain > LOG_LAST_VALID_ADDRESS ) {
			itBeg->plain = LOG_FIRST_VALID_ADDRESS;
		}
	
		//debug_string(NORMAL,PSTR("BEGIN Actual Iterator position  :"),true);
		//AT24CXX_iterator_report(itBeg);

		uint8_t * const pBuf = (uint8_t * const) pd;
		const uint8_t v = it->byte[LSB_BYTE];
		delay_ms(5);//If we are looping wait a little bit before issuing commands on the TWI
		//Is it a plain page or a overlapping write
		if((v>=sizeof(DL_LOG_ITEM)) || (v==0)) { //inside the same chip page
			AT24CXX_WriteBlock(page,msb,lsb,pBuf,sizeof(DL_LOG_ITEM));
			delay_ms(5);
		} else { //overlaps on two different chip pages
			const uint8_t x = (sizeof(DL_LOG_ITEM) - v);
			AT24CXX_WriteBlock(page,msb,lsb,pBuf,x);
			delay_ms(5);
			AT24CXX_WriteBlock(it->byte[PAGE_BYTE],it->byte[MSB_BYTE],0,pBuf+x,v);
			delay_ms(5);
		}
	}

	AT24CXX_iterator_report(it);
	datalogger_write_iterators_to_eeprom();

	task_store_quit:

	g_data_snapshot_num = 0;
	//debug_string(NORMAL,PSTR("Store Data OUT\r\n"),true);
	//sprintf_P(szBuf,PSTR("DFLL: 0x%X%X CALA: 0x%X  CALB: 0x%X\r\n"),DFLLRC32M.COMP2,DFLLRC32M.COMP1,DFLLRC32M.CALA,DFLLRC32M.CALB);
	//debug_string(NORMAL,szBuf,false);
	task_status_store_data = TASK_STOP;
	return err;
}



void datalogger_run(void)
{
	while(1) {

		if(task_status_sync_time==TASK_READY) {
			dl_task_sync_time();
		}

		if(task_status_store_data==TASK_READY) {
			dl_task_store_data();
		}

		if(task_status_send_data_prepare_RT ==TASK_READY) {
			if(0==dl_task_send_data_prepare_RT())
			{
				task_status_send_data_RT = TASK_READY;
			} else { //Something went wrong during the prepare session
				sim900_init();
				g_acc_task_send_RT=g_interv_task_send_RT-30; //Schedule the SEND task after 30 seconds
			}
		}

		if(task_status_send_data_RT ==TASK_READY) {
			if(0!=dl_task_send_data_RT()) {
				sim900_init();
				g_acc_task_send_RT=g_interv_task_send_RT-30; //Schedule the SEND task after 30 seconds
				
			}
		}
		
		if(task_status_server_cmd_check==TASK_READY) {
			dl_task_cmd_check();
		}

		if(task_status_send_data ==TASK_READY) {
			if(0!=dl_task_send_data()) {
				sim900_init();
				task_status_send_data = TASK_STOP;
			}
		}


		if(	(task_status_send_data_RT==TASK_READY)		||
			(task_status_store_data==TASK_READY)		||
			(task_status_send_data==TASK_READY)			||
			(task_status_sync_time==TASK_READY)) continue;

		dl_cycle_lock = true;
		while(dl_cycle_lock) {
			sleepmgr_enter_sleep();
		};
			
	};

}


static void dl_test_now(void)
{
	return;
	g_log_verbosity = VERBOSE;
	AT24CXX_iterator it = {.plain = 256};
	
	DL_LOG_ITEM ds;
	dl_get_record(&it,&ds);
	char szBuf[128];
	snprintf_P(szBuf,128,PSTR("First record time = %lu\r\n"),ds.data_timestamp);
	debug_string(NORMAL,szBuf,RAM_STRING);
	dl_search_record_by_date(1394621921 ,RIGHT_BOUND,&it);
	
	while(1)
	{
			gpio_toggle_pin(STATUS_LED_PIN);
			delay_ms(200);

	}
}
