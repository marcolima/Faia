#include <asf.h>
#include "cbuffer_usart.h"





/*! \brief RX Complete Interrupt Service Routine.
 *
 *  RX Complete Interrupt Service Routine.
 *  Stores received data in RX software buffer.
 *
 *  \param usart_data      The USART_data_t struct instance.
 */
bool USART_RX_CBuffer_Complete(/*volatile*/ USART_data_t * usart_data)
{
	/*volatile*/ USART_data_t * const ad = usart_data;

	//USART_Buffer_t * const bufPtr = &usart_data->buffer;
	/* Advance buffer head. */
	const uint8_t idx = ad->RX_Head;
	const uint8_t tempRX_Head = (idx + 1) & USART_RX_BUFFER_MASK;

	/* Check for overflow. */

	if (tempRX_Head == ad->RX_Tail) {
		ad->RX_Tail = (tempRX_Head + 1) & USART_RX_BUFFER_MASK;
	}
	
	const uint8_t data = ad->usart->DATA;
	ad->RX[idx] = data;
	ad->RX_Head = tempRX_Head;

	if(g_log_verbosity>=VERY_VERBOSE)  usart_putchar(USART_DEBUG, data);
	//usart_putchar(USART_DEBUG, data);


	return true;
}

