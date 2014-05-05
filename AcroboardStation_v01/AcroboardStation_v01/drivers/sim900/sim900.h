/*
* sim900.h
*
* Created: 16/03/2012 16:22:08
*  Author: fabio
*/


#ifndef SIM900_H_
#define SIM900_H_

#include "drivers/usart_interrupt/buffer_usart.h"
extern /* volatile */ USART_data_t sim900_usart_data;
extern volatile uint8_t usart_sync;



void sim900_power_on(void);
void sim900_power_off(void);


uint8_t sim900_send_sms(char * const p_sms_buf,const uint8_t isBufPGM, char * const p_sms_recipient,const uint8_t isRecipientPGM);

uint8_t sim900_send_sms_to_phone_book(const char * sms_book[],const uint8_t isBOOKPGM,char * const msg,const uint8_t isMSGPGM);

uint8_t sim900_init(void);

uint8_t sim900_GPRS_simple_open(void);
uint8_t sim900_GPRS_simple_close(void);

uint8_t sim900_GPRS_init( const char * const service_APN, const uint8_t isSZPGM );
uint8_t sim900_GPRS_check(const char* const Check_Response);
uint8_t sim900_GPRS_close(void);

uint16_t sim900_http_read(char * const return_value, uint8_t * const rv_len);

uint16_t sim900_http_get( const char * const get_URL, const uint8_t isURLPGM, char * const return_value, uint8_t * const rv_len );
uint16_t sim900_http_post( const char * const post_URL, const uint8_t isURLPGM, const char * const post_data, const uint16_t len_post_data,const uint8_t isPostPGM );

uint8_t sim900_http_close(void);

uint8_t sim900_ftp_init(
const char * const ftp_server,
const uint8_t isSZServerPGM,
const char * const ftp_username,
const uint8_t isSZUserNamePGM,
const char * const ftp_pwd,
const uint8_t isSZPWDPGM
);


uint8_t sim900_ftp_put(
const char * const file_name,
const uint8_t isSZFNamePGM,
const char * const file_path,
const uint8_t isSZFilePathPGM,
const char * const file_data,
const uint8_t isFileDataPGM
);





#endif /* SIM900_H_ */