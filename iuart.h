/*
** @file iuart.h
** @author Anderson Felippe <anderson@sagainfo.com.br>
** @date Mon Aug 20 	2012
*/

/*	This code implements an interrupt driven UART (both TX and RX)
	on the ATmega Serial port. */


#ifndef _IUART_H_
#define _IUART_H_


#define USART_BAUDRATE	 	9600
#define BAUD_PRESCALE 		(((F_CPU / (USART_BAUDRATE * 16UL))) - 1)
#define UART_BUFFER_SIZE	256	
#define RX_BUFSIZE 			80


char UartBuffer[UART_BUFFER_SIZE]; 		// this is a wrap-around buffer
int  UartBufferNextByteToSend; 			// position of next byte to be sent
int  UartBufferNextFree;				// position of next free byte of buffer
int  UartSendingInProgress; 			// 1 = sending is in progress

//Initialise UART and set all the parameters
void init_iuart(void);

//Putchar function to attend the printing function call
int UART0_Putchar(char c, FILE *stream);

/* Receive one character from the UART.  The actual reception is
 * line-buffered, and one character is returned from the buffer at
 * each invokation. */
int uart_getchar(FILE *stream);

#endif 	    /* !_IUART_H_ */
