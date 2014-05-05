/*
 * taskman.h
 *
 * Created: 05/04/2012 18:09:49
 *  Author: fabio
 */ 


#ifndef TASKMAN_H_
#define TASKMAN_H_


#define MAXTASK 8
#define TASKDATABUFSIZE 8

typedef void (*TASKMAIN)(void *);

bool TM_CreateTask(void (*_taskmain)(void *),uint8_t * _taskdata);
bool TM_RunNextTask(void);

#endif /* TASKMAN_H_ */