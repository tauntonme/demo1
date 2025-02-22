/*
 * ForeverLoop.cpp
 *
 *  Created on: Jun 6, 2023
 *      Author: Jon Freeman B Eng Hons
 */
#include 	"main.h"

#include	<cstdio>


#include	"Project.hpp"
#include	"Serial.hpp"
#include	"CmdLine.hpp"
extern	Serial	pc;

extern	void	check_commands	()	;	//	Looks after all command line input

uint32_t	millisecs 			= 0L;
int32_t		forever_loop_timer 	= 0L;
int32_t		slow_loop_timer 	= 0L;
volatile	bool	timer_1ms_flag;

bool	eeprom_valid_flag = false;
extern	Serial	COM_PORT	;//, *Com_ptrs[];


#define	FOREVER_LOOP_REPEAT_MS	20
#define	SLOW_LOOP_REPEAT_MS	500
//extern	void	send_queued_uart_messages	()	;
extern	bool	adc_updates	()	;

int32_t	sigs_time;

uint32_t	get_millisecs	()	{	return	(millisecs);	}


void	do_even_halfsec_stuff	()	{
#ifdef	USING_CAN
	can_try();
#endif
}

void	do_odd_halfsec_stuff	()	{
//char	t[100];
//	int len = sprintf	(t, "Sigs_time us = %d\r\n", sigs_time);
//	pc.write	(t, len);
}

void	do_fastest_stuff	()	{
	check_commands	()	;			//	Read and execute any commands rec'd from pc
#ifdef	USING_ANALOG
	if	(adc_updates	())	{	//	Checks flags, updates averaging buffers when data available
		CHARGE_PUMP;			//	Macro in Project.hpp
	}
#endif
}


extern	void	ProjectCore	()	;
extern	TIM_HandleTypeDef htim6;

void	do_forever_loop_rate_stuff	()	{
	ProjectCore	()	;
/*	if	(can_flag)	{
		can_flag = false;
//		ce_show	();
	}*/
	int32_t	old_sigs_time =	__HAL_TIM_GET_COUNTER(&htim6);
//	int	pld_sigs_time =	(&htim6)->Instance->CNT;	//	same as '__HAL_TIM_GET_COUNTER(&htim6)' above


//		signals_engine	();


	sigs_time =	__HAL_TIM_GET_COUNTER(&htim6) - old_sigs_time;	//	time consumed by signals_engine in us
	if	(sigs_time < 0)
		sigs_time += 1000;
}

void	one_ms_stuff	()	{
	pc.tx_any_buffered();
//	send_queued_uart_messages	();
//	if	(spi_inputs_updated())	{	//	Returns true after update, false when updates not yet available
//		do_forever_loop_rate_stuff	()	;	//	As soon as latest inputs have been updated
//	}
}


void	do_slow_loop_rate_stuff	()	{
	static	bool	oddeven;
//	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);	//	as good a place as any to toggle green Nucleo led
	oddeven = !oddeven;
	if	(oddeven)
		do_even_halfsec_stuff	();
	else
		do_odd_halfsec_stuff	();
}


extern	bool	System_Setup	()	;	//	see 'Project.cpp', possibly the best place for this

extern "C" void	ForeverLoop	()	{	// Jumps to here from 'main.c'
	char	t[50];
	int		len;
	bool	sys_health = System_Setup	(); // Good eeprom read and few other checks for true
	len = sprintf	(t, "System Startup, health %s\r\n", sys_health ? "Good" : "Bad");
	pc.write	(t, len);

	while	(true)	{				//	Always forever loop top
		do_fastest_stuff	();		//	While waiting for timer to sync slower stuff
		if	(timer_1ms_flag)	{
			timer_1ms_flag = false;
			one_ms_stuff	();		//	Do all the once per millisec stuff
			if	(forever_loop_timer >= FOREVER_LOOP_REPEAT_MS)	{	//	probably still 20 ms
				forever_loop_timer -= FOREVER_LOOP_REPEAT_MS;
				do_forever_loop_rate_stuff	();
				//	start spi In/Out update sequence all under DMA control.
//				start_spi_TxRx	();		//	SPI comms all happens under DMA control with no code interventions
				//	When spi shift reg read/write ends, DMA raises 'complete' interrupt
				//	It is this interrupt that initiates 'do_forever_loop_rate_stuff	()'
				//	By so doing, io data is used at its freshest possible.
				if	(slow_loop_timer > SLOW_LOOP_REPEAT_MS)	{	//	probably still 500 ms
					slow_loop_timer -= SLOW_LOOP_REPEAT_MS;
					do_slow_loop_rate_stuff	();					//	once per half second
				}	//	endof if	(slow_loop_timer > SLOW_LOOP_REPEAT_MS)
			}		//	endof if	(forever_loop_timer >= FOREVER_LOOP_REPEAT_MS)
		}			//	endof if	(timer_1ms_flag)
	}				//	endof while	(true)
}					//	endof void	ForeverLoop(). Never gets to here, function never returns.


//	Callback: timer has reset
void	HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{	//	check for timer 6
	if	(htim == &htim6)	{
		timer_1ms_flag = true;
		millisecs++;		//	Global count of milli seconds since power-up
		forever_loop_timer++;
		slow_loop_timer++;
	}
}


