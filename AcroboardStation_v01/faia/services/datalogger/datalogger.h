/*
 * datalogger.h
 *
 * Created: 16/03/2012 16:33:12
 *  Author: fabio
 */ 


#ifndef DATALOGGER_H_
#define DATALOGGER_H_

extern volatile uint8_t dl_cycle_lock;

void datalogger_run(void);

uint8_t datalogger_init(void);

//TASK functions

uint16_t dl_task_send_data_prepare(void);
uint16_t dl_task_send_data_prepare_RT(void);
uint16_t dl_task_send_data_RT(void);
uint16_t dl_task_sync_time(void);
uint8_t dl_task_store_data(void);




#endif /* DATALOGGER_H_ */