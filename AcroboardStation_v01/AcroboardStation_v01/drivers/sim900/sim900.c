/*
 * sim900.c
 *
 * Created: 16/03/2012 16:21:56
 *  Author: fabio
 */ 
#include <asf.h>


#include "globals.h"
#include <stdio.h>
#include <conf_board.h>
#include <conf_usart_serial.h>
#include "config/conf_usart_serial.h"
#include "drivers/sim900/sim900.h"
#include "../caribbean/services/config/config.h"
#include "devices/raingauge/pulse_raingauge.h"
#include "devices/statusled/status_led.h"
#include "drivers/usart_interrupt/cbuffer_usart.h"

#include "config/conf_SMS_book.h"
#include "config/conf_sim_pin_code.h" 

#define UNLOCK_SIM 0
#define MESSAGE_FORMAT 1
#	define PDU  0
#	define TEXT 1
#define MESSAGE_SEND 2

#define LITTLE_DELAY delay_ms(500)

//IMEI
#define IMEI_CODE_LEN 24
char g_szIMEI[IMEI_CODE_LEN];


// string in flash
static const char version_P[] PROGMEM = "v0.1 Caribbean";
static const char name_P[] PROGMEM = "sim900 driver";

//USART buffer
/*volatile */USART_data_t sim900_usart_data;
volatile uint8_t usart_sync = false;


//GPRS AT command & return codes
static const char sz_AT[]		PROGMEM = "AT\r\n";
static const char sz_OK[]		PROGMEM = "OK";
static const char sz_ERROR[]	PROGMEM = "ERROR";

static const char * const tbl_returns[] PROGMEM = {	sz_OK,
													sz_ERROR };
													
#define NUM_OF_RETURNS (sizeof(tbl_returns)/sizeof(char *))


//GPRS AT+CPIN answer strings
static const char szCPIN_SIM_READY[]	PROGMEM = "READY";
static const char szCPIN_SIM_PIN[]		PROGMEM = "SIM PIN";
static const char szCPIN_SIM_PUK[]		PROGMEM = "SIM PUK";
static const char szCPIN_PHSIM_PIN[]	PROGMEM = "PH_SIM PIN";
static const char szCPIN_PHSIM_PUK[]	PROGMEM = "PH_SIM PUK";
static const char szCPIN_SIM_PIN2[]		PROGMEM = "SIM PIN2";
static const char szCPIN_SIM_PUK2[]		PROGMEM = "SIM PUK2";

static const char * const tbl_CPIN_rets[] PROGMEM = {	sz_OK,
														sz_ERROR,
														szCPIN_SIM_READY,
														szCPIN_SIM_PIN,
														szCPIN_SIM_PUK,
														szCPIN_PHSIM_PIN,
														szCPIN_PHSIM_PUK,
														szCPIN_SIM_PIN2,
														szCPIN_SIM_PUK2 };

#define NUM_OF_CPIN_RETURNS (sizeof(tbl_CPIN_rets)/sizeof(char *))

//GPRS AT+CREG answer strings

/////////////////////////////////////////////////////////////////


static void sim900_power_toggle(void)
{
	gpio_set_pin_high(GPRS_SWITCH);
	delay_ms(1200);
	gpio_set_pin_low(GPRS_SWITCH);
	delay_ms(2500);
}


void sim900_power_off(void)
{
	const bool status = gpio_pin_is_high(GPRS_STATUS);
	if(true==status) {
		debug_string_P(NORMAL,PSTR("(sim900_power_off) module is ON so power cycling\r\n"));
		sim900_power_toggle();
	} else {
		debug_string_P(NORMAL,PSTR("(sim900_power_off) module is already OFF\r\n"));
		
	}
}
													
void sim900_power_on(void)
{
	const bool status = gpio_pin_is_high(GPRS_STATUS);
	if(true!=status) {
		debug_string_P(NORMAL,PSTR("(sim900_power_on) module is OFF so power cycling\r\n"));
		sim900_power_toggle();
	} else {
		debug_string_P(NORMAL,PSTR("(sim900_power_on) module is already ON\r\n"));
	}
}
													

static void sim900_put_string(const char * const sz,const uint8_t isPGM)
{
	
//	debug_string(NORMAL,PSTR("(sim900_put_string) IN\r\n"),true);

	const char * p = sz;


	if(isPGM) {
		while(1) {
			const char c = nvm_flash_read_byte((flash_addr_t)p++);
			if(0==c) break;
			while (usart_data_register_is_empty(USART_GPRS) == false) {}
			usart_put(USART_GPRS, c);
			if(g_log_verbosity>NORMAL) usart_putchar(USART_DEBUG,c);
		}
		} else {
		while(1) {
			const char c = *p++;
			if(0==c) break;
			while (usart_data_register_is_empty(USART_GPRS) == false) {}
			usart_put(USART_GPRS, c);
			if(g_log_verbosity>NORMAL) usart_putchar(USART_DEBUG,c);
		}
	}

//	debug_string(NORMAL,PSTR("(sim900_put_string) OUT\r\n"),true);
		
}


static uint8_t sim900_wait_data_on_usart(uint8_t seconds)
{

	for(uint8_t d1=seconds;d1>0;--d1)
	{
		for(uint16_t d2=10000;d2>0;--d2)
		{
#ifdef SIM900_USART_POLLED
			if(usart_rx_is_complete(USART_GPRS)) {
#else
			if(USART_RX_CBuffer_Data_Available(&sim900_usart_data)) {
#endif				
				return 0xFF;
			}
			delay_us(100);
		}
	}
	
	debug_string(VERY_VERBOSE,PSTR("(sim900_wait_data_on_usart) timed-out\r\n"),PGM_STRING);
	return 0;
}


static uint8_t sim900_read_string(char * const szBuf,uint8_t * const lenBuf)
{
	const uint8_t ec = *lenBuf;

	uint8_t l = 0;
	uint8_t r = 0;
	char c;

#ifndef SIM900_USART_POLLED
	usart_buffer_flush(&sim900_usart_data);
#endif

	while(1) {
		
		if(!sim900_wait_data_on_usart(10)) {
			debug_string(NORMAL,PSTR("(sim900_read_string) got timeout waiting for a character\r\n"),true);
			r=1;
			break;
		}
		
#ifdef SIM900_USART_POLLED
		c = usart_get(USART_GPRS);
#else
		c = USART_RX_CBuffer_GetByte(&sim900_usart_data);
#endif

		if(c=='\r') {
			if(g_log_verbosity > NORMAL) usart_putchar(USART_DEBUG,'@');
		} else	if(c=='\n') {
			if(g_log_verbosity > NORMAL) usart_putchar(USART_DEBUG,'#');
		} else break;
	}

	if(r==0) while(l<ec) {
		
		szBuf[l++] = c;

		if(!sim900_wait_data_on_usart(10)) {
			debug_string(NORMAL,PSTR("(sim900_read_string) got timeout waiting for a character\r\n"),true);
			r=1;
			break;
		}
		
#ifdef SIM900_USART_POLLED
		c = usart_get(USART_GPRS);
#else
		c = USART_RX_CBuffer_GetByte(&sim900_usart_data);
#endif

		if(c=='\r') {
			//if(g_log_verbosity > NORMAL) usart_putchar(USART_DEBUG,'@');
			break;
		} else	if(c=='\n') {
			//if(g_log_verbosity > NORMAL) usart_putchar(USART_DEBUG,'#');
			break;
		} 
		//if(g_log_verbosity > NORMAL) usart_putchar(USART_DEBUG,'.');

	}  

	if(ec==l) {
		r = 2;
		debug_string(NORMAL,PSTR("(sim900_read_string) Provided buffer is not big enough. Discarding chars\r\n"),true);
		l -= 1;
	}

	szBuf[l] = 0;

	//debug_string(VERBOSE,szBuf,RAM_STRING);
	//debug_string(VERBOSE,szCRLF,PGM_STRING);

	//debug_string(VERBOSE,PSTR("(sim900_read_string) out\r\n"),true);

	*lenBuf = l;
	return r;
}


static const char * sim900_wait4dictionary(const char * const dictionary[],uint8_t len)
{
	
	uint8_t num = len;
	const char * p[len];

#ifndef SIM900_USART_POLLED
	usart_buffer_flush(&sim900_usart_data);
#endif

	//debug_string(VERBOSE,PSTR("(sim900_wait4dictionary) IN\r\n"),true);


	while(num--) {
		p[num] = nvm_flash_read_word(dictionary+num);
	}

	while(1) {
		
		
		if(!sim900_wait_data_on_usart(20)) {
			debug_string(NORMAL,PSTR("(sim900_wait4dictionary) timeout waiting for sim900 response\r\n(sim900_wait4dictionary) OUT\r\n"),true);
			return NULL;
		}
		
#ifdef SIM900_USART_POLLED
		const char c1 = usart_get(USART_GPRS);
#else
		const char c1=USART_RX_CBuffer_GetByte(&sim900_usart_data);
#endif

		for(uint8_t i=0;i<len;++i) {

			const char c2 = nvm_flash_read_byte(p[i]);
			if(c1!=c2)
			{
				p[i]=(char *) nvm_flash_read_word(dictionary+i);
				continue;
			}

			p[i]++;

			if(nvm_flash_read_byte(p[i])==0)
			{
				const char * const r = nvm_flash_read_word(dictionary+i);
				//debug_string(VERY_VERBOSE,PSTR("(sim900_wait4dictionary) got: "),true);
				//debug_string(VERY_VERBOSE,r,true);
				//debug_string(VERY_VERBOSE,szCRLF,true);
				//debug_string(NORMAL,PSTR("(sim900_wait4dictionary) OUT\r\n"),true);
				return r;
			}
		}
	}
}



static const char * sim900_wait_string(const char * const sz,const uint8_t isPGM)
{
	
#ifndef SIM900_USART_POLLED
	usart_buffer_flush(&sim900_usart_data);
#endif

	const char * p=sz;
	char c2 = 0;

	while(c2=((isPGM)?(nvm_flash_read_byte(p++)):(*p++) )) {

		if(!sim900_wait_data_on_usart(20)) {
			debug_string_P(NORMAL,PSTR("(sim900_wait_string) timeout waiting for sim900 response\r\n(sim900_wait_string) OUT\r\n"));
			return NULL;
		}

#ifdef SIM900_USART_POLLED
		const char c1=usart_get(USART_GPRS);
#else
		const char c1=USART_RX_CBuffer_GetByte(&sim900_usart_data);
#endif
		//if the sequence is not the same we start over again
		if(c1!=c2) {
			p = sz;
			continue;
		}
	}
	return sz;

}



static const char * sim900_wait_retstring(void)
{
	return sim900_wait4dictionary(tbl_returns,NUM_OF_RETURNS);
}



static uint8_t sim900_cmd_with_read_string(const char * const szCMD,const uint8_t isCMDPGM,char * const szBuf, uint8_t * lenBuf)
{
	for(uint8_t i=0;i<2;++i) {
		sim900_put_string(szCMD,isCMDPGM);

		uint8_t l = *lenBuf;
		if (sim900_read_string(szBuf,&l)==0)
		{
			*lenBuf = l;
			return 0;
		}
		debug_string_P(NORMAL,PSTR("(sim900_cmd_with_read_string) got timeot retring\r\n"));
	}
	debug_string_P(NORMAL,PSTR("(sim900_cmd_with_read_string) failed\r\n"));
	return 1;
	
}

static const char * sim900_cmd_with_retstring(const char * const szCMD,const uint8_t isPGM)
{
	
	const char * szRet;
	for(uint8_t i=0;i<2;++i) {

		sim900_put_string(szCMD,isPGM);
		szRet=sim900_wait_retstring();

		if(NULL==szRet) {
			continue;
			} else {
			break;
		}

	}
	return szRet;
}




static uint8_t sim900_check_status(void)
{
	return 0;
}





uint8_t sim900_init()
{
	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"SIM900_INIT");

	
#ifdef SIM900_USART_POLLED
	
	debug_string_P(NORMAL,PSTR("(sim900_init) code compiled with polled usart\r\n"));
#else
	usart_interruptdriver_initialize(&sim900_usart_data,USART_GPRS,USART_INT_LVL_LO);
	usart_set_rx_interrupt_level(USART_GPRS,USART_INT_LVL_LO);
	usart_set_tx_interrupt_level(USART_GPRS,USART_INT_LVL_OFF);

	debug_string_P(NORMAL,PSTR("(sim900_init) code compiled with interrupt usart\r\n"));

#endif

init_sim:

	usart_rx_enable(USART_GPRS);

	sim900_power_off();
	
	const char * szRET = NULL;
	int t=2;
	while(t--) {
		sim900_power_toggle();
		uint8_t i=3;
		while(i--) {
			statusled_blink(1);
			sim900_put_string(sz_AT,PGM_STRING);
			szRET = sim900_wait_retstring();
			if(szRET!=sz_OK) {
				debug_string_P(NORMAL,PSTR("(SIM900_init) trying to set a serial line speed\r\n"));
			} else break;
		}
		if(szRET==sz_OK) break;
		debug_string_P(NORMAL,PSTR("(SIM900_init) Not being able to connect to SIM900. Try to power it on again\r\n"));
	}

	if (szRET!=sz_OK)
	{
		debug_string_P(NORMAL,PSTR("(SIM900_init) failed to synchronize with GPRS UART. The process will end here\r\n"));
		return -1;
	}


	//disabling echo back from the modem
	LITTLE_DELAY;
	szRET = sim900_cmd_with_retstring(PSTR("ATE0\r\n"),PGM_STRING);
	//sim900_wait_retstring();
	
	if(szRET!=sz_OK) {
		debug_string_P(NORMAL,PSTR("(SIM900_init) this sim900 doesn't support ATE0\r\n"));
	} else {
		debug_string_P(VERBOSE,PSTR("(SIM900_init) correctly issued ATE0\r\n"));
	}
	

	LITTLE_DELAY;
	szRET = sim900_cmd_with_retstring(PSTR("AT+CREG=0\r\n"),PGM_STRING);

	if(szRET!=sz_OK) {
		debug_string_P(NORMAL,PSTR("(SIM900_init) this sim900 doesn't support AT+CREG\r\n"));
		} else {
		debug_string_P(VERBOSE,PSTR("(SIM900_init) correctly issued AT+CREG=0\r\n"));
	}

	
	//get the IMEI code
	LITTLE_DELAY;
	szRET = sim900_cmd_with_retstring(PSTR("AT+GSN=?\r\n"),true);
	
	if(szRET!=sz_OK) {
		debug_string_P(NORMAL,PSTR("(SIM900_init) this sim900 doesn't support AT+GSN\r\n"));
	}

	

	uint8_t i=IMEI_CODE_LEN;
	LITTLE_DELAY;
	sim900_cmd_with_read_string(PSTR("AT+GSN\r\n"),PGM_STRING,g_szIMEI,&i);
	sim900_wait_retstring();

	debug_string_P(NORMAL,PSTR("IMEI code is : "));
	debug_string(NORMAL,g_szIMEI,RAM_STRING);
	debug_string_P(NORMAL,szCRLF);

	if(szRET!=sz_OK) {
		debug_string_P(NORMAL,PSTR("(SIM900_init) AT+GSN messed up\r\n"));
	} else {
		debug_string_P(NORMAL,PSTR("(SIM900_init) AT+GSN GOT OK\r\n"));
	}


	LITTLE_DELAY;
	sim900_put_string(PSTR("AT+CPIN=?\r\n"),PGM_STRING);
	szRET = sim900_wait_retstring();
	
	if(szRET!=sz_OK) {
		debug_string_P(NORMAL,PSTR("(SIM900_init) SIM is not present\r\n"));
		return 1;
	}
	

	LITTLE_DELAY;
	sim900_put_string(PSTR("AT+CPIN?\r\n"),PGM_STRING);
	szRET = sim900_wait4dictionary(tbl_CPIN_rets,NUM_OF_CPIN_RETURNS);

	char szBuf[16];

	
	if(szRET==szCPIN_SIM_PIN) {
		debug_string_P(NORMAL,PSTR("(SIM900_init) SIM present and is PIN locked\r\n"));

		cfg_get_sim_pin(szBuf,8);
		if((szBuf[0]==0xFF) || (szBuf[0]==0x00)) {		
			debug_string_P(NORMAL,PSTR("(SIM900_init) PIN code is not present in memory SIM will remain locked\r\n"));
			return 3;
		} else {
			debug_string_P(NORMAL,PSTR("(SIM900_init) PIN code is present in memory trying to unlock\r\nPIN: "));
			debug_string(NORMAL,szBuf,RAM_STRING);
			debug_string_P(NORMAL,szCRLF);
			LITTLE_DELAY;
			sim900_put_string(PSTR("AT+CPIN="),PGM_STRING);
			sim900_put_string(szBuf,RAM_STRING);
			sim900_put_string(szCRLF,PGM_STRING);
			const char * ret = sim900_wait_retstring();
			if(ret!=sz_OK) {
				debug_string_P(NORMAL,PSTR("(sim900_init) WARNING PIN is WRONG\r\n"));
				return 3;
			}
		}
	} else if (szRET==szCPIN_SIM_PUK)
	{
		debug_string_P(NORMAL,PSTR("(SIM900_init) SIM present and is PUK locked\r\n"));
		debug_string_P(NORMAL,PSTR("(SIM900_init) no PUK code, SIM init will fail here\r\n"));
		return 4;
	}
	

	for(i=0;i<18;++i) {
		statusled_blink(1);
		
		LITTLE_DELAY;
		sim900_put_string(PSTR("AT+CREG?\r\n"),PGM_STRING);

		uint8_t l = 16;
		memset(szBuf,0,16);
		sim900_read_string(szBuf,&l);
		if(strncasecmp_P(szBuf,PSTR("+CREG: 1,1"),5)!=0) {
			debug_string_P(NORMAL,PSTR("(SIM900_init) device is not answering as it should restarting it\r\n"));
			goto init_sim;
		}
		if(szBuf[9]=='1') {
			debug_string_P(NORMAL,PSTR("(SIM900_init) device correctly registered in the network\r\n"));
			break;
		}
		debug_string_P(NORMAL,PSTR("(SIM900_init) device answered "));
		debug_string(NORMAL,szBuf,RAM_STRING);
		debug_string_P(NORMAL,PSTR(" seems not registered in the network\r\n"));
		debug_string_P(NORMAL,PSTR("(SIM900_init) will check again in 5 second\r\n"));
		delay_ms(5000);
	}


	return 0;
}


uint8_t sim900_send_sms(char * const p_sms_buf,const uint8_t isBufPGM, char * const p_sms_recipient,const uint8_t isRecPGM)
{

	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"SIM900_SEND_SMS");

	usart_rx_enable(USART_GPRS);


	const char * szret = sim900_cmd_with_retstring(PSTR("AT+CMGF=1\r\n"),true);
	if(szret!=sz_OK) {
		debug_string_P(NORMAL,PSTR ("(sim900_send_sms) [ERROR] got an error from AT+CMGF=1\r\n(sim900_send_sms) [ERROR] Instead of OK got : "));
		debug_string_P(NORMAL,(NULL==szret) ? PSTR("NULL"):szret); //szret points to a flash storage string
		debug_string_P(NORMAL,szCRLF);
		return -1;
	}		
	
	LITTLE_DELAY;
	sim900_put_string(PSTR("AT+CMGS=\""),PGM_STRING);
	sim900_put_string(p_sms_recipient,isRecPGM);
	sim900_put_string(PSTR("\"\r\n"),PGM_STRING);
	
	if(!sim900_wait_string(PSTR(">"),PGM_STRING)) {
		debug_string_P(NORMAL,PSTR ("(sim900_send_sms) [ERROR] got an error from AT+CMGS\r\n"));
		return -1;
	}
	

	LITTLE_DELAY;
	sim900_put_string(p_sms_buf,isBufPGM);
	sim900_put_string(PSTR("\x1A"),PGM_STRING);


	debug_string_P(NORMAL,PSTR ("(sim900_send_sms) out\r\n"));
	
	return 0;
}


uint8_t sim900_send_sms_to_phone_book(const char * sms_book[],const uint8_t isBOOKPGM,char * const msg,const uint8_t isMSGPGM)
{
	const size_t n = sizeof(sms_book)/sizeof(char *);
	uint8_t e = 0;
	for(size_t i=0;i<n;++i) {
		if(sim900_send_sms(msg,isMSGPGM,sms_book[i],isBOOKPGM)!=0) e=-1;
	}
	
	return e;
}

//static uint8_t g_gprs_ref = 0;
uint8_t sim900_GPRS_simple_open(void)
{
	//if(g_gprs_ref++) {
		//return 0;
	//}
	
	char szAPN[64];
	cfg_get_gprs_apn(szAPN,60);

	debug_string_P(VERBOSE,PSTR("(sim900_GPRS_simple_open) Using APN "));
	debug_string(VERBOSE,szAPN,RAM_STRING);
	debug_string_P(VERBOSE,szCRLF);


	return sim900_GPRS_init(szAPN,RAM_STRING);
}

uint8_t sim900_GPRS_simple_close(void)
{
	return sim900_GPRS_close();

	//if(g_gprs_ref==0) {
		//return 0;
	//} else {
		//--g_gprs_ref;
	//}
	//
	//if(g_gprs_ref==0) {
		//return sim900_GPRS_close();
	//} else return 0;
}


uint8_t sim900_GPRS_init( const char * const service_APN, const uint8_t isSZPGM )
{

	const char * szRet = NULL;
	char szBuf[32];
	uint8_t len = 32;
	uint8_t i;

	DEBUG_PRINT_FUNCTION_NAME(VERBOSE,"SIM900_GPRS_INIT");


	//CHECK THE STATUS OF THE BEARER
	LITTLE_DELAY;
	if(0!=sim900_cmd_with_read_string(PSTR("AT+SAPBR=2,1\r\n"),true,szBuf,&len))
	{
		debug_string(NORMAL,PSTR("(sim900_gprs_init) the SAPBR #1 query went wrong for some reason, procedure aborted\r\n"),true);
		return 1;
	}
	
	szRet = sim900_wait_retstring();


	debug_string(VERBOSE,PSTR("(sim900_gprs_init) bearer is : "),true);
	debug_string(VERBOSE,szBuf,false);
	debug_string(VERBOSE,szCRLF,true);

	
	//check if the OK was at the end of the answer and
	//the status of the bearer
	
	if(sz_OK!=szRet) { //Something went wrong with the sapbr query, we exit from the init function
		debug_string(NORMAL,PSTR("(sim900_gprs_init) the SAPBR #2 query went wrong for some reason, procedure aborted\r\n"),true);
		return 1;
	}
	

	//check if the bearer is not closed 
	if (strncasecmp_P(szBuf,PSTR("+SAPBR: 1,3,\"0.0.0.0\""),11)!=0)
	{
		//The bearer is not closed maybe we can accept it
		//and try to use it anyway

		const char r = szBuf[10];
		if(r=='2') { //this bearer is closing
			debug_string(NORMAL,PSTR("(sim900_gprs_init) the requested bearer is in closing status (WARNING)\r\n"),true);
			return 2;
		} else {
			debug_string(NORMAL,PSTR("(sim900_gprs_init) the requested bearer is already open(WARNING)\r\n"),true);
			return 0;
		}
	}


	//configure the bearer to be used
	LITTLE_DELAY;
	if(NULL==sim900_cmd_with_retstring(PSTR("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n"),true))
	{
		//Something went wrong with the sapbr query, we exit from the init function
		debug_string(NORMAL,PSTR("(sim900_gprs_init) the SAPBR=3,1,\"Contype\",\"GPRS\""  \
								" query went wrong for some reason, procedure aborted\r\n"),true);
		return 1;
		
	}
	

	//Set the APN
	//We don't use the facility function because of the buffer handling
	LITTLE_DELAY;
	for (i=0;i<2;i++)
	{
		sim900_put_string(PSTR("AT+SAPBR=3,1,\"APN\",\""),PGM_STRING);
		sim900_put_string(service_APN,isSZPGM);
		sim900_put_string(PSTR("\"\r\n"),PGM_STRING);
		szRet=sim900_wait_retstring();

		if (sz_OK==szRet)
		{
			break;
		}
		else if(NULL==szRet)
		{
			continue;
		}

		//Something went wrong with the sapbr query, we exit from the init function
		debug_string_P(NORMAL,PSTR("(sim900_gprs_init) the SAPBR=3,1,\"APN\""  \
								" query went wrong for some reason, procedure aborted\r\n"));
		return 1;
	}


	//Finally open the bearer
	LITTLE_DELAY;
	szRet = sim900_cmd_with_retstring(PSTR("AT+SAPBR=1,1\r\n"),true);
	if(sz_OK!=szRet)
	{
		//Something went wrong with the sapbr query, we exit from the init function
		debug_string_P(NORMAL,PSTR("(sim900_gprs_init) the SAPBR=1,1"  \
					" query went wrong for some reason, procedure aborted\r\n"));
		return 1;
	}



	//Check if the bearer is correctly open
	for(i=0;i<10;++i) {

		len = 32;		
		LITTLE_DELAY;
		if(0!=sim900_cmd_with_read_string(PSTR("AT+SAPBR=2,1\r\n"),true,szBuf,&len))
		{
			debug_string_P(NORMAL,PSTR("(sim900_gprs_init) the SAPBR query went wrong for some reason, procedure aborted\r\n"));
			return 1;
		}

		
		szRet = sim900_wait_retstring();

		debug_string_P(VERBOSE,PSTR("(sim900_gprs_init) bearer is : "));
		debug_string(VERBOSE,szBuf,RAM_STRING);
		debug_string_P(VERBOSE,szCRLF);

	
		//check if the OK was at the end of the answer and
		//the status of the bearer
	
		if(sz_OK!=szRet) { //Something went wrong with the sapbr query, we exit from the init function
			debug_string_P(NORMAL,PSTR("(sim900_gprs_init) the SAPBR query went wrong, procedure aborted\r\n"));
			return 1;
		}

		const char r = szBuf[10];
			
		//check bearer status 0=connecting 1=connected 2=closing 3=closed
		if (r=='0')
		{
			debug_string_P(NORMAL,PSTR("(sim900_gprs_init) the requested bearer is in 'connecting' status. Check again in 2 seconds\r\n"));
			delay_s(2);
			continue;
		} else if (r=='1')
		{
			debug_string_P(VERBOSE,PSTR("(sim900_gprs_init) the requested bearer is open\r\n"));
			break;
		} 
		else if((r=='2') || (r=='3'))
		{
			debug_string_P(NORMAL,PSTR("(sim900_gprs_init) the requested bearer is closed or in closing status (ERROR)\r\n"));
			return 1;
		}
	}

	return 0;
}
/*
uint8_t sim900_GPRS_check( const char* const Check_Response )
{
	
	return 0;
}
*/

uint8_t sim900_GPRS_close(void)
{
	LITTLE_DELAY;

	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+SAPBR=0,1\r\n"),PGM_STRING))
	{
		return 1;
	}

	return 0;
}

uint16_t sim900_http_read(char * const return_value, uint8_t * const rv_len)
{
	char szBuf[64];
	uint8_t len_szBuf = 64;

	//Gets the size of the answer buffer
	uint8_t lb = len_szBuf;

	LITTLE_DELAY;
	sim900_put_string(PSTR("AT+HTTPREAD\r\n"),PGM_STRING);


	if(NULL == sim900_wait_string(PSTR("+HTTPREAD:"),PGM_STRING))
	{
		return 1;
	}

	uint16_t r = sim900_read_string(szBuf,&lb);

		
	if(r==1) {
		debug_string_P(NORMAL,PSTR("(sim900_http_read1) #1 got timeout waiting for sim900 response\r\n"));
		return 1;
	}
		
	if(r==2) {
		debug_string_P(NORMAL,PSTR("(sim900_http_read1) #1 provided buffer is not big enough\r\n"));
		return 2;
	}
		
	r = sim900_read_string(return_value,rv_len);

	if(r==1) {
		debug_string_P(NORMAL,PSTR("(sim900_http_read2) #2 got timeout waiting for sim900 response\r\n"));
		return 3;
	}
		
	if(r==2) {
		debug_string_P(NORMAL,PSTR("(sim900_http_read2) #2 provided buffer is not big enough\r\n"));
		return 4;
	}

	return 0;

}

uint16_t sim900_http_get(	const char * const get_URL, 
							const uint8_t isURLPGM, 
							char * const return_value, 
							uint8_t * const rv_len )
{

	LITTLE_DELAY;
	if(NULL==sim900_cmd_with_retstring(PSTR("AT\r\n"),PGM_STRING))
	{
		debug_string_P(NORMAL,PSTR("(sim900_http_get) SIM900 not responding \r\n"));
		return 1;
	}

	LITTLE_DELAY;
	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+HTTPINIT\r\n"),PGM_STRING))
	{
		debug_string_P(NORMAL,PSTR("(sim900_http_get) AT+HTTPINIT not OK\r\n"));
		return 1;
	}

	LITTLE_DELAY;
	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+HTTPPARA=\"CID\",1\r\n"),PGM_STRING))
	{
		debug_string_P(NORMAL,PSTR("(sim900_http_get) AT+HTTPPARA=\"CID\" not OK\r\n"));
		return 1;
	}

	LITTLE_DELAY;
	sim900_put_string(PSTR("AT+HTTPPARA=\"URL\",\""),PGM_STRING);
	sim900_put_string(get_URL,isURLPGM);
	sim900_put_string(PSTR("\"\r\n"),PGM_STRING);
	sim900_wait_retstring();

	LITTLE_DELAY;
	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+HTTPACTION=0\r\n"),PGM_STRING))
	{
		debug_string_P(NORMAL,PSTR("(http_get) HTTPACTION didn't return OK\r\n"));
		return 1;
	}
		
	if(NULL == sim900_wait_string(PSTR("+HTTPACTION:"),true))
	{
		debug_string_P(NORMAL,PSTR("(http_get) didn't got +httpaction aborting\r\n"));
		return 1;
	}
		
	//HTTPACTION result code in the form of <method>,<status>,<datalen>
	const uint8_t len_szBuf = 16;
	char szBuf[len_szBuf];
	uint8_t lb = len_szBuf;
	uint8_t r = sim900_read_string(szBuf,&lb);
	if(1==r) {
		debug_string_P(NORMAL,PSTR("(sim900_http_get) got timeout waiting for httpaction response\r\n"));
		return 1;
	}

	debug_string_P(NORMAL,PSTR("(sim900_http_get) httpaction returned: "));
	debug_string(NORMAL,szBuf,RAM_STRING);
	debug_string_P(NORMAL,szCRLF);
	
	if(szBuf[2]!='2' || szBuf[3]!='0' || szBuf[4]!='0') {
		//HTTPACTION didn't return OK
		debug_string_P(NORMAL,PSTR("(sim900_http_get) httpget response was not OK\r\n"));
		return 1;
	}
	
	LITTLE_DELAY;
	sim900_put_string(PSTR("AT+HTTPREAD\r\n"),PGM_STRING);


	if(NULL == sim900_wait_string(PSTR("+HTTPREAD:"),PGM_STRING))
	{
		return 1;
	}

	//Gets the size of the answer buffer
	lb = len_szBuf;
	r = sim900_read_string(szBuf,&lb);

	
	if(r==1) {
		debug_string(NORMAL,PSTR("(sim900_http_get) #1 got timeout waiting for sim900 response\r\n"),true);
		return 1;
	} 
	
	if(r==2) {
		debug_string(NORMAL,PSTR("(sim900_http_get) #1 provided buffer is not big enough\r\n"),true);
		return 2;
	}
	
	sim900_read_string(return_value,rv_len);

	if(r==1) {
		debug_string(NORMAL,PSTR("(sim900_http_get) #2 got timeout waiting for sim900 response\r\n"),true);
		return 3;
	}
	
	if(r==2) {
		debug_string(NORMAL,PSTR("(sim900_http_get) #2 provided buffer is not big enough\r\n"),true);
		return 4;
	}

	return 0;
}


uint16_t sim900_http_post(	const char * const post_URL, 
							const uint8_t isURLPGM, 
							const char * const post_data, 
							const uint16_t len_post_data,
							const uint8_t isPostPGM )
{

	LITTLE_DELAY;
	if(NULL==sim900_cmd_with_retstring(PSTR("AT\r\n"),true))
	{
		debug_string(VERY_VERBOSE,PSTR("(sim900_http_post) SIM900 not responding \r\n"),true);
		return 1;
	}


	LITTLE_DELAY;
	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+HTTPINIT\r\n"),true))
	{
		return 1;
	}


	LITTLE_DELAY;
	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+HTTPPARA=\"CID\",1\r\n"),true))
	{
		return 1;
	}

	
	LITTLE_DELAY;
	sim900_put_string(PSTR("AT+HTTPPARA=\"URL\",\""),true);
	sim900_put_string(post_URL,isURLPGM);
	sim900_put_string(PSTR("\"\r\n"),true);
	sim900_wait_retstring();


	LITTLE_DELAY;
	const uint8_t SZBUF_LEN = 64;
	char szBuf[SZBUF_LEN];
	sprintf_P(szBuf,PSTR("AT+HTTPDATA=%d,%d\r\n"),len_post_data,30000);
	sim900_put_string(szBuf,false);

	if(NULL==sim900_wait_string(PSTR("DOWNLOAD"),true))
	{
		return 1;
	}
	
	LITTLE_DELAY;
	sim900_put_string(post_data,isPostPGM);
	sim900_wait_retstring();
	

	LITTLE_DELAY;
	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+HTTPACTION=1\r\n"),true))
	{
		debug_string(NORMAL,PSTR("(http_post) HTTPACTION didn't return OK\r\n"),true);
		return 1;
	}

	
	if(NULL == sim900_wait_string(PSTR("+HTTPACTION:"),true))
	{
		debug_string(NORMAL,PSTR("(http_post) +httpaction is taking a while\r\n"),true);
		if(NULL == sim900_wait_string(PSTR("+HTTPACTION:"),true))
		{
			debug_string(NORMAL,PSTR("(http_post) +httpaction is taking a lot\r\n"),true);
			if(NULL == sim900_wait_string(PSTR("+HTTPACTION:"),true))
			{
				debug_string(NORMAL,PSTR("(http_post) didn't got +httpaction aborting\r\n"),true);
				return 1;
			}
		}
	}

/////////////
// We need to know the return code of the post
		
	uint8_t len_szBuf = SZBUF_LEN;
	uint8_t r = sim900_read_string(szBuf,&len_szBuf);


	debug_string(NORMAL,PSTR("(sim900_http_post) httpaction returned: "),true);
	debug_string(NORMAL,szBuf,false);
	debug_string(NORMAL,szCRLF,true);
	
	if(szBuf[2]!='2' || szBuf[3]!='0' || szBuf[4]!='0') {
		//HTTPACTION didn't return OK
		debug_string(NORMAL,PSTR("(sim900_http_get) httppost response was not OK\r\n"),true);
		return 1;
	}

	
	return 0;
}

uint8_t sim900_http_close(void)
{
	LITTLE_DELAY;
	if(sz_OK!=sim900_cmd_with_retstring(PSTR("AT+HTTPTERM\r\n"),true))
	{
		return 1;
	}
	
	return 0;
}

uint8_t sim900_ftp_init(	const char * const ftp_server, 
							const uint8_t isSZServerPGM,
							const char * const ftp_username, 
							const uint8_t isSZUserNamePGM,
							const char * const ftp_pwd,
							const uint8_t isSZPWDPGM)
{
	sim900_put_string(szCRLF,true);
	sim900_put_string(PSTR("AT+FTPCID=1"),true);
	sim900_put_string(szCRLF,true);
	
	sim900_put_string(PSTR("AT+FTPSERV=\""),true);
	sim900_put_string(ftp_server,isSZServerPGM);
	sim900_put_string(PSTR("\""),true);
	sim900_put_string(szCRLF,true);
	
	sim900_put_string(PSTR("AT+FTPUN=\""),true);
	sim900_put_string(ftp_username,isSZUserNamePGM);
	sim900_put_string(PSTR("\""),true);
	sim900_put_string(szCRLF,true);
	
	sim900_put_string(PSTR("AT+FTPW=\""),true);
	sim900_put_string(ftp_pwd,isSZPWDPGM);
	sim900_put_string(PSTR("\""),true);
	sim900_put_string(szCRLF,true);
	
	return 0;
}
uint8_t sim900_ftp_put(const char * const file_name,
						const uint8_t isSZFNamePGM,
						const char * const file_path,
						const uint8_t isSZFilePathPGM,
						const char * const file_data,
						const uint8_t isFileDataPGM)
{
	char buf[64];
	LITTLE_DELAY;
	sim900_put_string(PSTR("\""),true);
	sim900_put_string(PSTR("AT+FTPPUTNAME=\""),true);
	sim900_put_string(file_name,isSZFNamePGM);
	sim900_put_string(PSTR("\""),true);
	sim900_put_string(szCRLF,true);
	LITTLE_DELAY;
	
	sim900_put_string(szCRLF,true);
	sim900_put_string(PSTR("AT+FTPPUTPATH=\""),true);
	sim900_put_string(file_path,isSZFilePathPGM);
	sim900_put_string(PSTR("\""),true);
	sim900_put_string(szCRLF,true);
	LITTLE_DELAY;
	
	sim900_put_string(PSTR("AT+FTPPUT=1"),true);
	sim900_put_string(szCRLF,true);
	LITTLE_DELAY;

	sim900_put_string(PSTR("AT+FTPPUT=2,"),true);
	itoa(strlen(file_data),buf,10);
	sim900_put_string(buf,false);
	sim900_put_string(szCRLF,true);
	LITTLE_DELAY;
	
	sim900_put_string(file_data,isFileDataPGM);
	sim900_put_string(PSTR("AT+FTPPUT=2,0"),true);
	sim900_put_string(szCRLF,true);
	LITTLE_DELAY;
	
	return 0;
}

#ifndef SIM900_USART_POLLED
ISR(USART_GPRS_RX_Vect)
{
	USART_RX_CBuffer_Complete(&sim900_usart_data);
}

//ISR(USART_GPRS_TX_Vect)
//{
	//
//}

//ISR(USART_GPRS_DRE_Vect)
//{
	//USART_DataRegEmpty(&usart_data);
//}

//ISR(USART_GPRS_TX_Vect)
//{
	//udi_cdc_putc((uint8_t)((USART_SERIAL_GPRS)->DATA));
//}

#endif