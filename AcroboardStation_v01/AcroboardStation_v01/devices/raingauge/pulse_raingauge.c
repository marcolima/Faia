/*
 * pulse_raingauge.c
 *
 * Created: 13/03/2012 17:28:39
 *  Author: fabio
 */ 
#include <asf.h>
#include "globals.h"
#include "devices/raingauge/pulse_raingauge.h"
#include "devices/statusled/status_led.h"

#define RAINGAUGE1 0
#define RAINGAUGE2 1

volatile RAINGAUGE_STATS raingauge_stats[2];

//! \name raingauge tilt manager
//@{
/**
 * \fn void tilt(uint32_t time)
 * \brief handles the tilt and manages the statistics.
 *
 */
//@}

static void raingauge_tip(uint8_t id,uint16_t cents)
{
	volatile RAINGAUGE_STATS * const ps = &raingauge_stats[id];

	if(ps->lastTip_cents!=CENTS_UNDEF_VALUE) {
		//Before modifying this please take a minute 
		//to read how the cents of seconds are handled by the data logger
		//it is described in the datalogger_init function
		const uint16_t c = cents - ps->lastTip_cents;
		if (c<20) //this sets a maximum rate of tips per cent of seconds
		{
			return;
		}


		if(c < ps->maxSlope) {
			ps->maxSlope = c;
			ps->maxSlope_cents = cents;
		}			
	} else {
		ps->firstTip_cents = cents;
	}

	ps->lastTip_cents = cents;
	ps->tips++;

//	usart_putchar(USART_DEBUG,'!');
	status_led_toggle();	
}

//@{
/**
 * \fn raingauge_reset_stats(struct RAINGAUGE_STATS * const ps)
 * \brief resets staistics to a given status.
 *  if ps==NULL reset to the default value
 *
 */
//@}
void raingauge_reset_stats(uint8_t id,RAINGAUGE_STATS * const ps)
{
	
	const static RAINGAUGE_STATS z PROGMEM = {
		.firstTip_cents	= CENTS_UNDEF_VALUE,
		.lastTip_cents	= CENTS_UNDEF_VALUE,
		.maxSlope_cents	= CENTS_UNDEF_VALUE,
		.maxSlope		= MAXSLOPE_UNDEF_VALUE,
		.tips			= 0
	};
	
	volatile RAINGAUGE_STATS * const s = raingauge_stats+id;

	if(ps==NULL) {
		nvm_flash_read_buffer(&z,s,sizeof(RAINGAUGE_STATS));
	} else {
		memcpy_ram2ram(s,ps,sizeof(RAINGAUGE_STATS));
	}		
}

//@{
/**
 * \fn raingauge_get_stats(struct RAINGAUGE_STATS * const ps)
 * \brief statistics getter.
 *
 */
//@}

void raingauge_get_stats(uint8_t id,RAINGAUGE_STATS * const ps)
{
	volatile RAINGAUGE_STATS * const s = raingauge_stats+id;

	memcpy_ram2ram(ps,s,sizeof(RAINGAUGE_STATS));
}


//! \name raingauge tilt manager
//@{
/**
 * \fn raingauge_init(void) 
 * \brief initialize the raingauge.
 *
 */
//@}

void raingauge_init(void) 
{
	//both Rain1 and Rain2 issue interrupts so the bitmask is 0x03
	PORT_ConfigureInterrupt0( &PORTR, PORT_INT0LVL_MED_gc, 0x03 ); 


	sleepmgr_lock_mode(SLEEPMGR_IDLE);

	raingauge_reset_stats(RAINGAUGE1,NULL);
#ifdef DATALOGGER_AUX_YES
	raingauge_reset_stats(RAINGAUGE2,NULL);
#endif	
}

ISR(PORTR_INT0_vect)
{
	raingauge_tip(RAINGAUGE1,tc_read_count(&TCC1));
}

#ifdef DATALOGGER_AUX_YES

ISR(PORTR_INT1_vect)
{
	raingauge_tip(RAINGAUGE2,tc_read_count(&TCC1));
}

#endif