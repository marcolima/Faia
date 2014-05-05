
#ifndef LOWPOWER_MACROS_H
#define LOWPOWER_MACROS_H


/*============================ MACROS ========================================*/
//! Convenience macro for disabling the JTAG interface.
#define DISABLE_JTAG( ) { \
	ENTER_CRITICAL_REGION(); \
	CCP = 0xD8; \
	MCU_MCUCR = MCU_JTAGD_bm; \
	LEAVE_CRITICAL_REGION(); \
}

//! Convenience macro for enabling pull-ups on specified pins on any port.
#define __PORT_PULLUP(port, mask) { \
	PORTCFG.MPCMASK = mask ; \
	port.PIN0CTRL = PORT_OPC_PULLUP_gc; \
}



#define DISABLE_GEN( ) { \
	PR.PRGEN |= PR_AES_bm | PR_DMA_bm | PR_EVSYS_bm | PR_RTC_bm; \
}

#define DISABLE_TC( ) { \
	PR.PRPC |= PR_HIRES_bm | PR_TC0_bm | PR_TC1_bm; \
	PR.PRPD |= PR_HIRES_bm | PR_TC0_bm | PR_TC1_bm; \
	PR.PRPE |= PR_HIRES_bm | PR_TC0_bm | PR_TC1_bm; \
	PR.PRPF |= PR_HIRES_bm | PR_TC0_bm; \
}

#define DISABLE_COM( ) { \
	PR.PRPC |= PR_SPI_bm | PR_TWI_bm | PR_USART0_bm | PR_USART1_bm; \
	PR.PRPD |= PR_SPI_bm | PR_USART0_bm | PR_USART1_bm; \
	PR.PRPE |= PR_TWI_bm | PR_USART0_bm; \
	PR.PRPF |= PR_USART0_bm; \
}

#define DISABLE_ANLG( ) { \
	PR.PRPA |= PR_AC_bm | PR_ADC_bm; \
	PR.PRPB |= PR_AC_bm | PR_ADC_bm | PR_DAC_bm; \
}

#define ENABLE_PULLUP( ) { \
	__PORT_PULLUP(PORTA, 0xFF); \
	__PORT_PULLUP(PORTB, 0xFF); \
	__PORT_PULLUP(PORTC, 0xFF); \
	__PORT_PULLUP(PORTD, 0xFF); \
	__PORT_PULLUP(PORTE, 0xFF); \
	__PORT_PULLUP(PORTF, 0xDF); \
	__PORT_PULLUP(PORTR, 0x03); \
}



#endif // LOWPOWER_MACROS_H
