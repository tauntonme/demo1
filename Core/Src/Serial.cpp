/*
 * Serial.cpp
 *
 *  Created on: Jun 13, 2023
 *      Author: Jon Freeman
 */
#include 	"main.h"
#include	<cstdio>
#include	<cstdbool>
#include	<cstring>
//#include	<utility>	//	for C++23

#include	"Serial.hpp"

struct	SerialPortPointers	{	//	Used to bind uartnum to port
	class Serial * port		{};
	UART_HandleTypeDef* uart	{};
};


SerialPortPointers	MySerialPorts	[6];	//	Constructor builds table of ports

/*
void	myportsreport	()	{
	int	len;
	char	t[100];
	for	(int i = 0; i < 6; i++)	{
		len = sprintf	(t, "Port %d, this-> %ld, huartn %ld\r\n", i, (uint32_t)MySerialPorts[i].port,
				(uint32_t)MySerialPorts[i].uart);
//		pc.write(t, len);
		MySerialPorts[0].port->write(t, len);
	}
}
*/

Serial::Serial	(UART_HandleTypeDef &wot_port)	//	Constructor
	:	m_huartn {&wot_port}
{	//	Constructor
//	if	(HAL_OK != HAL_UART_Receive_DMA	(m_huartn, rxbuff, 1))	//	huartn and rxbuff 'private'
//		set_error	(SerialErrors::HAL_UART);
	int i = 0;
	while	(i < 6)	{
		if	(!(MySerialPorts[i].uart))	{	//got empty slot
			MySerialPorts[i].uart = m_huartn;
			MySerialPorts[i].port = this;
			i = 99;
		}
		i++;
	}
}



void	Serial::set_tx_busy	(bool true_or_false)	{
	tx_busy = true_or_false;
}


void	Serial::start_rx	()	{	//	Call from startup. Call from constructor is too early
	if	(HAL_OK != HAL_UART_Receive_DMA	(m_huartn, rxbuff, 1))	//	huartn and rxbuff 'private'
		set_error	(SerialErrors::HAL_UART);
}


bool	Serial::m_read1	(char * ch)	{
	if	(rx_buff_empty)
		return	(false);
	*ch = (char)rxbuff[rx_offptr++];
	if	(rx_offptr >= RXBUFF_SIZE)
		rx_offptr = 0;
	if	(rx_onptr == rx_offptr)
		rx_buff_empty	= true;
	return	(true);
}


/**
 * char *	Serial::test_for_message	()	{
 *
 * Called from ForeverLoop at repetition rate
 * Returns pointer to lin_inbuff when "\r\n" terminated command line has been assembled in lin_inbuff, NULL otherwise
 */
char *	Serial::test_for_message	()	{	//	Read in any received chars into linear command line buffer
	while	(m_read1(ch))	{	//	Get next received characters, if any have been received.
		if	(lin_inbuff_onptr >= LIN_INBUFF_SIZE)	{
			ch[0] = '\r';		//	Prevent command line buffer overrun
			set_error (SerialErrors::INPUT_OVERRUN);	//	set error flag
		}
		if(ch[0] != '\n')	{	//	Ignore newlines
			lin_inbuff[lin_inbuff_onptr++] = ch[0];
			lin_inbuff[lin_inbuff_onptr] = '\0';
			if(ch[0] == '\r')	{
				lin_inbuff[lin_inbuff_onptr++] = '\n';
				lin_inbuff[lin_inbuff_onptr] = '\0';
				write	(lin_inbuff, lin_inbuff_onptr);	//	echo received command string to originator
				lin_inbuff_onptr = 0;	//	Could return the length here, might be useful
				if	(test_error(SerialErrors::INPUT_OVERRUN))	{
					write	("INPUT_OVERRUN_ERROR\r\n", 21);
					clear_error	(SerialErrors::INPUT_OVERRUN);
				}
				return	(lin_inbuff);			//	Got '\r' command terminator
			}
		}
	}
	return	(nullptr);	//	Have not yet found any complete terminated command string to process
}


//bool	Serial::test_error	(int mask)	const	{	//	Return true for error, false for no error
bool	Serial::test_error	(SerialErrors mask)	const	{	//	Return true for error, false for no error
	return	(serial_error & static_cast<int>(mask));	//	true for NZ result (error flag set), false for 0 or no error result
}


void	Serial::set_error	(SerialErrors bit)	{
	serial_error |=	(1 << static_cast<int>(bit));
}


int		Serial::clear_error	(SerialErrors bit)	{	//
	serial_error &= ~(1 << static_cast<int>(bit));
//	serial_error &= ~(1 << std::to_underlying(bit));	//	C++23
	return	(serial_error);
}


/**
 * void	Serial::write	(const uint8_t * t, int len)	{
 *
 *
 *
 * Always copy send data into lin buff so that call can return and let code overwrite mem used for message.
 * */
bool	Serial::write	(const uint8_t * t, int len)	{	//	Only puts chars on buffer.
	if	(len < 1)
		return	(false);			//	Can not send zero or fewer chars !
	int	buff_space = ring_outbuff_offptr - ring_outbuff_onptr;
	if	(buff_space <= 0)
		buff_space += RING_OUTBUFF_SIZE;
	if	(buff_space < len)	{
		set_error (SerialErrors::OUTPUT_OVERRUN);
		return	(false);
	}
	int	space_to_bufftop = RING_OUTBUFF_SIZE - ring_outbuff_onptr;
	char *	dest1 = ring_outbuff + ring_outbuff_onptr;
	if	(len > space_to_bufftop)	{
		memmove	(dest1, t, space_to_bufftop);
		memmove	(ring_outbuff, t + space_to_bufftop, len - space_to_bufftop);
		ring_outbuff_onptr += len;	//	which takes us beyond end of buffer
		ring_outbuff_onptr -= RING_OUTBUFF_SIZE;
	}
	else	{
		memmove	(dest1, t, len);
		ring_outbuff_onptr += len;
	}
	tx_buff_empty = false;
	return	(true);
}


bool	Serial::write	(const char * t, int len)	{	//	Remembering to keep type-casting is such a bore
	return	(write	((uint8_t*)t, len));					//	Overloaded functions take char or uint8_t
}


bool	Serial::tx_any_buffered	()	{
	HAL_StatusTypeDef	ret;	//	Used to test return results of HAL functions
	if	(tx_buff_empty || tx_busy)
		return	(false);
	//	To be here, tx_buff has stuff to send, and uart tx chan is not busy

	int	len = 0;
	while	(!tx_buff_empty && (len < LIVE_TXBUFF_SIZE))	{
		//
		live_tx_buff[len++] = ring_outbuff[ring_outbuff_offptr++];
//		tx_buff_full = false;
		if	(ring_outbuff_offptr >= RING_OUTBUFF_SIZE)
			ring_outbuff_offptr = 0;
		if(ring_outbuff_onptr == ring_outbuff_offptr)
			tx_buff_empty = true;
	}
	if	(len > 0)	{
		tx_busy = true;
//		ret = HAL_UART_Transmit_IT	(huartn, (uint8_t *)live_tx_buff, len);
		ret = HAL_UART_Transmit_DMA	(m_huartn, (uint8_t *)live_tx_buff, len);
		if	(ret == HAL_OK)
			return	(true);
	}
return	(false);
}

//	USART Interrupt Handlers

void	Serial::rx_intrpt_handler_core	()	{	//	Interrupt has placed char on circular buffer.
	rx_buff_empty	= false;
	rx_onptr++;		//	update onptr for next char
	if	(rx_onptr >= RXBUFF_SIZE)
		rx_onptr = 0;
//		if	(com1_rx_onptr == com1_rx_offptr)
//			com1_rx_full	= true;			//	onptr now ready for HAL_UART_Receive_DMA()
	HAL_UART_Receive_DMA(m_huartn, rxbuff + rx_onptr, 1);	//	Enable following read
}


//void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)	{
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)	{
	int i = 0;
	while	(MySerialPorts[i].port)	{
		if	(huart == MySerialPorts[i].uart)
			MySerialPorts[i].port->rx_intrpt_handler_core();
		i++;
	}
}

//void HAL_UART_DMATxCpltCallback(UART_HandleTypeDef *huart)	//	This called as well as HalfCplt
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)	//	This called as well as HalfCplt
{
	int i = 0;
	while	(MySerialPorts[i].port)	{
		if	(huart == MySerialPorts[i].uart)
			MySerialPorts[i].port->set_tx_busy(false);
		i++;
	}
}


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	int i = 0;
	while	(MySerialPorts[i].port)	{
		if	(huart == MySerialPorts[i].uart)
			MySerialPorts[i].port->set_error	(SerialErrors::HAL_UART);
		i++;
	}
}




