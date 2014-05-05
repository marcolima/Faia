/*
 * cbuffer_usart.h
 *
 * Created: 08/11/2012 10:13:43
 *  Author: fabio
 */ 


#ifndef CBUFFER_USART_H_
#define CBUFFER_USART_H_


#include "drivers/usart_interrupt/buffer_usart.h"


//uint8_t USART_RX_CBuffer_GetByte(volatile USART_data_t * const usart_data);
bool USART_RX_CBuffer_Complete(/*volatile*/ USART_data_t * const usart_data);



/*! \brief Test if there is data in the receive software buffer.
 *
 *  This function can be used to test if there is data in the receive software
 *  buffer.
 *
 *  \param usart_data         The USART_data_t struct instance
 *
 *  \retval true      There is data in the receive buffer.
 *  \retval false     The receive buffer is empty.
 */
static inline bool USART_RX_CBuffer_Data_Available(/*volatile*/ USART_data_t * const usart_data)
{
	/*volatile*/ USART_data_t * const ad = usart_data;

	/* Make copies to make sure that volatile access is specified. */
	uint8_t tempHead = ad->RX_Head;
	uint8_t tempTail = ad->RX_Tail;

	/* There are data left in the buffer unless Head and Tail are equal. */
	return (tempHead != tempTail);
}

/*! \brief Get received data (5-8 bit character).
 *
 *  The function USART_RX_CBufferData_Available should be used before this
 *  function is used to ensure that data is available.
 *
 *  Returns data from RX software buffer.
 *
 *  \param usart_data       The USART_data_t struct instance.
 *
 *  \return         Received data.
 */
static inline uint8_t USART_RX_CBuffer_GetByte(/*volatile*/ USART_data_t * const ud)
{
	//USART_Buffer_t * const bufPtr = &usart_data->buffer;
	const uint8_t ans = (ud->RX[ud->RX_Tail]);

	/* Advance buffer tail. */
	ud->RX_Tail = (ud->RX_Tail + 1) & USART_RX_BUFFER_MASK;

	return ans;
}


#endif /* CBUFFER_USART_H_ */