/*
 * SHT21.h
 *
 * Created: 22/07/2013 16:36:17
 *  Author: fabio
 */ 


#ifndef SHT21_H_
#define SHT21_H_

typedef union {
	int16_t wval;
	int8_t bval[2];
} SHTVAL;


int SHT21_Init(void);

status_code_t SHT21_TriggerReadTemp_hold(SHTVAL * val);
status_code_t SHT21_TriggerReadRH_hold(SHTVAL * val);

status_code_t SHT21_TriggerReadTemp_noHold(SHTVAL * val);
status_code_t SHT21_TriggerReadRH_noHold(SHTVAL * val);




#endif /* SHT21_H_ */