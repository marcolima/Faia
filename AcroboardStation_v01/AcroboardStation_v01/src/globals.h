/*
 * globals.h
 *
 * Created: 16/03/2012 16:15:18
 *  Author: fabio
 */


#ifndef GLOBALS_H_
#define GLOBALS_H_



#define TASK_STOP			0
#define TASK_READY			1
#define TASK_RUNNING		2

//#define SIM900_USART_POLLED

#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <compiler.h>

#include "string.h"
#include "progmem.h"

#include "lowpower_macros.h"
//Strings
extern const char szCRLF[];



struct Sensor_API
{

    int dummy;

};


//DEBUG facilities

extern uint8_t g_log_verbosity;
#define RAM_STRING false
#define PGM_STRING true

enum {QUIET=0,NON_MASKERABLE=0,NORMAL,VERBOSE,VERY_VERBOSE};
void debug_string(const uint8_t level,const char * const sz,const uint8_t isPGM);
void debug_string_P(const uint8_t level,const char * const sz);


#define DEBUG_FUNCTION_NAME_PRINT_1(x) 		const char * const func_name __attribute__((__cleanup__(debug_function_out_name_print))) = __func__; \
											debug_function_in_name_print(x,func_name);
#define DEBUG_FUNCTION_NAME_PRINT_2(x,y)	const char * const func_name __attribute__((__cleanup__(debug_function_out_name_print_P))) = PSTR( y );\
											debug_function_in_name_print_P(x,func_name);


#define GET_3RD_ARG(a1,a2,a3,...) a3
#define DEBUG_FUNCTION_CHOOSER(...) GET_3RD_ARG(__VA_ARGS__,DEBUG_FUNCTION_NAME_PRINT_2,DEBUG_FUNCTION_NAME_PRINT_1)
#define DEBUG_PRINT_FUNCTION_NAME(...) 	DEBUG_FUNCTION_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

void debug_function_in_name_print(const uint8_t level,const char fname[]);
void debug_function_out_name_print(const char * const * const fname);

void debug_function_in_name_print_P(const uint8_t level,const char fname[]);
void debug_function_out_name_print_P(const char * const * const fname);


static inline void PORT_ConfigureInterrupt0( PORT_t * port,
        PORT_INT0LVL_t intLevel,
        uint8_t pinMask )
{
    port->INTCTRL = ( port->INTCTRL & ~PORT_INT0LVL_gm ) | intLevel;
    port->INT0MASK = pinMask;
    /* Enable the level interrupts in the PMIC. */
    PMIC.CTRL |= intLevel;
}


//#include "usb_protocol_cdc.h"

/*! \brief Opens the communication port
 * This is called by CDC interface when USB Host enable it.
 *
 * \retval true if cdc startup is successfully done
 */
bool cdc_enable(void);

/*! \brief Closes the communication port
 * This is called by CDC interface when USB Host disable it.
 */
void cdc_disable(void);

/*! \brief Attach or detach USB device
 * Called by UDC when Vbus line state changes
 */
void vbus_action(bool b_high);

/*! \brief Manages the leds behaviors
 * Called when a start of frame is received on USB line each 1ms.
 */
void main_sof_action(void);

/*! \brief Enters the application in low power mode
 * Callback called when USB host sets USB line in suspend state
 */
void main_suspend_action(void);

/*! \brief Turn on a led to notify active mode
 * Called when the USB line is resumed from the suspend state
 */
void main_resume_action(void);

/*! \brief Save new DTR state to change led behavior.
 * The DTR notify that the terminal have open or close the communication port.
 */
void cdc_set_dtr(bool b_enable);


void dump_rainstats_to_log(const uint8_t id);



#endif /* GLOBALS_H_ */