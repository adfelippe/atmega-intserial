/*
** @file iuart.c
** @author Anderson Felippe <afelippe@techworks.ie>
** @date Mon Aug 20 	2012
*/

/*	This code implements an interrupt driven UART (both TX and RX)
	on the ATmega Serial port. */
	
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
	
#include "iuart.h"

char ReceivedByte;

void init_iuart(void)
{
	// init buffer data structures
	UartBuffer[0] = '\0';						// clear the first byte of buffer
   	UartBufferNextByteToSend = 0; 				// set "next byte to send" to beginning
   	UartBufferNextFree = 0; 					// next free byte is also beginning of buffer
   	UartSendingInProgress = 0; 					// clear "sending in progress" flag 

	UCSR0B |= _BV(TXEN0) | _BV(RXEN0); 			// Turn on the transmission and reception circuitry
   	UCSR0C |= _BV(UCSZ00) | _BV(UCSZ01);	 	// Use 8-bit character sizes
	UBRR0H = (BAUD_PRESCALE >> 8); 				// Load upper 8-bits of the baud rate value into the high byte of the UBRR register
	UBRR0L = BAUD_PRESCALE;						// Load lower 8-bits of the baud rate value into the low byte of the UBRR register
	UCSR0B |= _BV(RXCIE0) | _BV(TXCIE0); 		// Enable the USART Receive and Transmit Complete interrupt (USART_RXC)

}

// If transmit is in progress, adds a character to the UART output buffer.
// If transmit is not in progress, kicks off a transmit.
//
// The send buffer is a wrap-around buffer.
//
// If the buffer is full, then this routine returns EOF.
// A successful completion returns 0.
//
// This routine disables the UART Tx interrupt temporarily, because
// things would get funky if the interrupt signal routine were called during
// execution of this routione.
//
// If the buffer was empty to start with, then this routine "primes the pump"
// sending the character directly to the UART.
//
// This routine also adds carriage returns to newlines.
//
int UART0_Putchar(char c, FILE *stream)
{
   register int ReturnStatus = 0; 			// return 0 for success
   register int UartBufferNextFree_last; 	// space to save last UartBufferNextFree

   // if character is a "newline" then add a "carriage return" before it.
   if (c == '\n')
      UART0_Putchar('\r', stream);


   // if no send is already in progress, then "prime the pump" by sending directly to UART
   if (UartSendingInProgress == 0)
   {
      // set "sending in progress" flag
      UartSendingInProgress = 1;
      // send the first byte!
      UDR0 = c;
   }
   else
   {
      // disable the Tx Complete interrupt
      UCSR0B &= ~_BV(TXCIE0);

      UartBuffer[UartBufferNextFree] = c;

      // increment the next free byte index, while saving last value
      UartBufferNextFree_last = UartBufferNextFree++;

       // check for wrap-around
      if (UartBufferNextFree == UART_BUFFER_SIZE) // if we reached the end of the buffer -
           UartBufferNextFree = 0; // start back at the beginning

      if (UartBufferNextFree == UartBufferNextByteToSend) // if buffer is full -
      {
         // bump back the index so transmit routine doesn't think buffer's empty
         UartBufferNextFree = UartBufferNextFree_last;
         // return with error code   
         ReturnStatus = EOF;
      }
      
      // enable the Tx Complete interrupt
      UCSR0B |= _BV(TXCIE0);
   }


   // return with status code
   return ReturnStatus;
} 

/*
 * Receive a character from the UART Rx.
 *
 * This features a simple line-editor that allows to delete and
 * re-edit the characters entered, until either CR or NL is entered.
 * Printable characters entered will be echoed using UART0_Putchar().
 *
 * Editing characters:
 *
 * . \b (BS) or \177 (DEL) delete the previous character
 * . ^u kills the entire input buffer
 * . ^w deletes the previous word
 * . ^r sends a CR, and then reprints the buffer
 * . \t will be replaced by a single space
 *
 * All other control characters will be ignored.
 *
 * The internal line buffer is RX_BUFSIZE (80) characters long, which
 * includes the terminating \n (but no terminating \0).  If the buffer
 * is full (i. e., at RX_BUFSIZE-1 characters in order to keep space for
 * the trailing \n), any further input attempts will send a \a to
 * UART0_Putchar() (BEL character), although line editing is still
 * allowed.
 *
 * Input errors while talking to the UART will cause an immediate
 * return of -1 (error indication).  Notably, this will be caused by a
 * framing error (e. g. serial line "break" condition), by an input
 * overrun, and by a parity error (if parity was enabled and automatic
 * parity recognition is supported by hardware).
 *
 * Successive calls to uart_getchar() will be satisfied from the
 * internal buffer until that buffer is emptied again.
 */
int uart_getchar(FILE *stream)
{
  uint8_t c;
  char *cp, *cp2;
  static char b[RX_BUFSIZE];
  static char *rxp;

  if (rxp == 0)
    for (cp = b;;)
      {
	loop_until_bit_is_set(UCSR0A, RXC0);
	if (UCSR0A & _BV(FE0))
	  return _FDEV_EOF;
	if (UCSR0A & _BV(DOR0))
	  return _FDEV_ERR;
	c = ReceivedByte;
	/* behaviour similar to Unix stty ICRNL */
	if (c == '\r')
	  c = '\n';
	if (c == '\n')
	  {
	    *cp = c;
	    UART0_Putchar(c, stream);
	    rxp = b;
	    break;
	  }
	else if (c == '\t')
	  c = ' ';

	if ((c >= (uint8_t)' ' && c <= (uint8_t)'\x7e') ||
	    c >= (uint8_t)'\xa0')
	  {
	    if (cp == b + RX_BUFSIZE - 1)
	      UART0_Putchar('\a', stream);
	    else
	      {
		*cp++ = c;
		UART0_Putchar(c, stream);
	      }
	    continue;
	  }

	switch (c)
	  {
	  case 'c' & 0x1f:
	    return -1;

	  case '\b':
	  case '\x7f':
	    if (cp > b)
	      {
		UART0_Putchar('\b', stream);
		UART0_Putchar(' ', stream);
		UART0_Putchar('\b', stream);
		cp--;
	      }
	    break;

	  case 'r' & 0x1f:
	    UART0_Putchar('\r', stream);
	    for (cp2 = b; cp2 < cp; cp2++)
	      UART0_Putchar(*cp2, stream);
	    break;

	  case 'u' & 0x1f:
	    while (cp > b)
	      {
		UART0_Putchar('\b', stream);
		UART0_Putchar(' ', stream);
		UART0_Putchar('\b', stream);
		cp--;
	      }
	    break;

	  case 'w' & 0x1f:
	    while (cp > b && cp[-1] != ' ')
	      {
		UART0_Putchar('\b', stream);
		UART0_Putchar(' ', stream);
		UART0_Putchar('\b', stream);
		cp--;
	      }
	    break;
	  }
      }

  c = *rxp++;
  if (c == '\n')
    rxp = 0;

  return c;
}

//********************************
//       INTERRUPT HANDLERS
//********************************

#if defined (__AVR_ATmega325__)
ISR(USART0_RX_vect)
{
#elif defined (__AVR_ATmega328P__)
ISR(USART_RX_vect)
{
#endif
	
	ReceivedByte = UDR0; // Fetch the received byte value into the variable "ReceivedByte" 
}

// This interrupt service routine is called when a byte has been sent through the
// UART0 port, and it's ready to receive the next byte for transmission.
//
// If there are more bytes to send, then send the next one and increment the index.
// If the index reached the end of the buffer, then wrap around to the beginning.
//
// If there is not another byte to write, then clear the "UartSendingInProgress"
// flag, otherwise set it if a byte has just been sent.
//
#if defined (__AVR_ATmega325__)
ISR(USART0_TX_vect)
{
#elif defined (__AVR_ATmega328P__)
ISR(USART_TX_vect)
{
#endif

	if (UartBufferNextByteToSend == UartBufferNextFree) {  // if nothing to send
	
		UartSendingInProgress = 0;  // clear "sending in progress" flag
		return; 					// then we have nothing to do, so return
	}

	// set "sending in progress" flag
	UartSendingInProgress = 1;

	// send the next byte on UART0 port
	UDR0 = UartBuffer[UartBufferNextByteToSend];

	// increment index and check for wrap-around
	UartBufferNextByteToSend++;
	if (UartBufferNextByteToSend == UART_BUFFER_SIZE)	// if we reached the end of the buffer -
	UartBufferNextByteToSend = 0; 				 		// then start back at the beginning
}
