
#ifndef BUFFER_USART_H_
#define BUFFER_USART_H_

/* \brief  Receive buffer size: 2,4,8,16,32,64,128 or 256 bytes. */
#define USART_RX_BUFFER_SIZE 8
/* \brief Transmit buffer size: 2,4,8,16,32,64,128 or 256 bytes */
#define USART_TX_BUFFER_SIZE 2
/* \brief Receive buffer mask. */
#define USART_RX_BUFFER_MASK ( USART_RX_BUFFER_SIZE - 1 )
/* \brief Transmit buffer mask. */
#define USART_TX_BUFFER_MASK ( USART_TX_BUFFER_SIZE - 1 )


#if ( USART_RX_BUFFER_SIZE & USART_RX_BUFFER_MASK )
#error RX buffer size is not a power of 2
#endif
#if ( USART_TX_BUFFER_SIZE & USART_TX_BUFFER_MASK )
#error TX buffer size is not a power of 2
#endif

/* \brief USART transmit and receive ring buffer. */
//typedef struct USART_Buffer
//{
	///* \brief Receive buffer. */
	//volatile uint8_t RX[USART_RX_BUFFER_SIZE];
	///* \brief Transmit buffer. */
	//volatile uint8_t TX[USART_TX_BUFFER_SIZE];
	///* \brief Receive buffer head. */
	//volatile uint8_t RX_Head;
	///* \brief Receive buffer tail. */
	//volatile uint8_t RX_Tail;
	///* \brief Transmit buffer head. */
	//volatile uint8_t TX_Head;
	///* \brief Transmit buffer tail. */
	//volatile uint8_t TX_Tail;
//} USART_Buffer_t;
//

/*! \brief Struct used when interrupt driven driver is used.
*
*  Struct containing pointer to a usart, a buffer and a location to store Data
*  register interrupt level temporary.
*/
typedef struct Usart_and_buffer
{
	/* \brief Pointer to USART module to use. */
	USART_t * usart;
	/* \brief Data register empty interrupt level. */
	USART_DREINTLVL_t dreIntLevel;
	/* \brief Data buffer. */
	//USART_Buffer_t buffer;
	
	volatile uint8_t RX_Head;
	volatile uint8_t RX_Tail;
	
	volatile uint8_t TX_Head;
	volatile uint8_t TX_Tail;
	
	volatile uint8_t RX[USART_RX_BUFFER_SIZE];
	volatile uint8_t TX[USART_TX_BUFFER_SIZE];
	
} USART_data_t;



/* Functions for interrupt driven driver. */
void usart_interruptdriver_initialize(/*volatile*/ USART_data_t * const usart_data,USART_t * const usart,USART_DREINTLVL_t dreIntLevel );


bool USART_TXBuffer_FreeSpace(/*volatile*/ USART_data_t * const usart_data);
bool USART_TXBuffer_PutByte(/*volatile*/ USART_data_t * const usart_data, uint8_t data);
//bool USART_RXBufferData_Available(USART_data_t * usart_data);
uint8_t USART_RXBuffer_GetByte(/*volatile*/ USART_data_t * const usart_data);
bool USART_RXComplete(/*volatile*/ USART_data_t * const usart_data);
void USART_DataRegEmpty(/*volatile*/ USART_data_t * const usart_data);

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
static inline bool USART_RXBufferData_Available(const /*volatile*/ USART_data_t * const usart_data)
{
	/* Make copies to make sure that volatile access is specified. */
	const uint8_t tempHead = usart_data->RX_Head;
	const uint8_t tempTail = usart_data->RX_Tail;

	/* There are data left in the buffer unless Head and Tail are equal. */
	return (tempHead != tempTail);
}

static inline void usart_buffer_flush(/*volatile*/ USART_data_t * const usart_data)
{
	usart_data->RX_Tail = 0;
	usart_data->RX_Head = 0;
	usart_data->TX_Tail = 0;
	usart_data->TX_Head = 0;
}


#endif /* BUFFER_USART_H_ */