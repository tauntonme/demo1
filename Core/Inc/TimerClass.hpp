/*
 * TimerClass.hpp
 *
 *  Created on: Jan 20, 2024
 *      Author: jon34
 */

#include	"main.h"
#ifndef INC_TIMCLAS_HPP_
#define INC_TIMCLAS_HPP_

extern	uint32_t	millisecs;

/**	class	TimerClass	{
 *
 * Jan 2024, thinking these will be useful.
 *
 *
 */
class	TimerClass	{	//	Set timeout for some millisecs hence using e.g. MyTimer.set(1000);	. Test using e.g. if(MyTimer.timeout())
	uint32_t	timer = 0L;
	uint32_t	period = 0L;
	bool		timer_active = false;
	const char	noid[2] = {'?', 0};
	const char * id = noid;
	bool (*start_func)	() = nullptr;   //  points to function executed at start of timer period
	bool (*end_func)	() = nullptr;   //  points to function executed at end of timer period (timeout)
	const char * txt	()	const	{	return	(id);	}
  public:
	TimerClass	(const char * timer_name, bool(*start)(), bool(*end)())
	{
		if	(timer_name)	id 	= timer_name;
		if	(start)	start_func 	= start;
		if	(end)	end_func 	= end;
	}	//	Constructor, nothing else to do

	void	start_timer	()	{	//	Use to load timer with desired timeout delay, in ms
		timer = period + millisecs;
		if	(start_func != nullptr)	start_func	();
		timer_active = (true);
	}

	void	start_timer	(uint32_t timer_ms)	{	//	Use to load timer with desired timeout delay, in ms
		period = timer_ms;
		start_timer	();
	}

	void	stop_timer	()	{	timer_active = false;	}

	void	set_period_ms	(uint32_t p)	{	period = p;	}

	bool	is_active	()	{	return	(timer_active);	}

	void	read_update	()	{
		if	(timer_active && (timer <= millisecs))	{	//	Active timer period has just ended
			if	(end_func != nullptr)	end_func();
			timer_active = false;
		}
	}
}	;	//	End of class	TimerClass	{



#endif


