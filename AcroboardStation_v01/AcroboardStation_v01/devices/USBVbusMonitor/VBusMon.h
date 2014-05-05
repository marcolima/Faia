/*
 * VBusMon.h
 *
 * Created: 6/23/2013 3:40:00 PM
 *  Author: fabio
 */ 


#ifndef VBUSMON_H_
#define VBUSMON_H_


static inline uint8_t is_USB_cable_plugged(void)
{
	return ioport_pin_is_high(USB_PROBE_PIN);
}

void VBusMon_init(void);
void VBusMon_check(void);


#endif /* VBUSMON_H_ */