/*
 * taskman.c
 *
 * Created: 05/04/2012 18:09:31
 *  Author: fabio
 */ 

#include <asf.h>
#include "services/taskman/taskman.h"



static TASKMAIN taskMain[MAXTASK] = {NULL};
static uint8_t taskData[MAXTASK][TASKDATABUFSIZE];
	

bool TM_CreateTask(void (*_taskmain)(void * p),uint8_t * _taskdata)
{
	for(uint8_t i=0;i<MAXTASK;++i)
	{
		if(taskMain[i]==NULL) {
			taskMain[i]=_taskmain;
			memcmp_ram2ram(taskData[i],_taskdata,TASKDATABUFSIZE);
			return true;
		}
		
	}	
	return false;
}

bool TM_RunNextTask(void)
{
	uint8_t i=MAXTASK;
	do 
	{
		const TASKMAIN t = taskMain[--i];
		if(t!=NULL) {
			t(taskData+i);
			return true;
		}
	} while (i>0);
	
	return false;
}
