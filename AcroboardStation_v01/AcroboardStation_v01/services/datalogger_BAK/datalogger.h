/*
 * datalogger.h
 *
 * Created: 16/03/2012 16:33:12
 *  Author: fabio
 */ 


#ifndef DATALOGGER_H_
#define DATALOGGER_H_

#define AT24CXX_ReadSequential  AT24CXX_ReadSequentialA
#define AT24CXX_WriteSequential AT24CXX_WriteSequentialA
#define AT24CXX_ReadSequential  AT24CXX_ReadSequentialA


uint8_t datalogger_init(void);

//TASK functions
void datalogger_sendData(void);
void datalogger_sync_time(void);
void datalogger_store_data(const uint32_t ts);




#endif /* DATALOGGER_H_ */