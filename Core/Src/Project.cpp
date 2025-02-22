/*
 * Project.cpp
 *
 * Project Non-Library Functions
 *
 *  Created on: Jan 26, 2024
 *      Author: Jon Freeman  B. Eng. Hons
 *
 *	Keep this file, 'Project.cpp', for the main logical flow of your project
 */
#include	<cstdio>			//	for sprintf
#include	<cstring>			//	for strlen

#include	"Project.hpp"
#include	"parameters.hpp"
#include	"Serial.hpp"
#include	"CmdLine.hpp"
#include	"settings.hpp"

extern	Serial	pc;
extern	i2eeprom_settings	my_settings;
extern	CommandLineHandler	command_line_handler;	//	(pc_command_list);	//	Nice and clean


extern	bool	start_ADC();		//	Using 5 chans of Analogue to Digital converter
extern	bool	start_DAC();		//	Using 1 chan of Digital to Analogue converter
//extern	float	get_adc_ave_normalised	(ADC_Chans x)	;	//	In Analog.cpp
extern	float	get_ADC_filtered	(ADC_Chans x)	;				//
extern	uint32_t	get_millisecs()	;	//()	{	return	(millisecs);	}
extern	bool	DAC_write	(float outvalnorm, uint32_t Chan)	;	// to forward control volts to motorkit
extern	const char * get_version();

extern	TIM_HandleTypeDef htim2;
extern	char * get_time	(char * dest)	;	//	get e.g. "16:50:11"
extern	char * get_date	(char * dest)	;	//	get e.g. "16:50:11"

//	Uses idea of voltage range ramp. ramp_minv to be min suggested running voltage, maybe 22.6 ?
//	ramp_range might be a couple of volts
//	chargedv should be set to useful full charge voltage, suggest maybe 26.4.
//	These are User Settable by connecting to laptop over USB and using e.g. 'PuTTY' prog for comms.
float	ramp_minv = 0.0;	//
//float	ramp_range = 0.0;	//	Values get set once EEPROM read completes
float	chargedv = 0.0;
float	vbounce = 0.0;
int32_t	warn_secs	;
int32_t	timea_secs	;
int32_t	timeb_secs	;
int32_t	timec_secs	;




bool	System_Setup	()	{	//	Here at start of programme.
	char	t[100];
	int	len;
	bool	rv = true;
	const char * vs = get_version()	;
	pc.start_rx();
	pc.write("\r\n\n\n\n", 5);
	pc.write(vs, strlen(vs));
	pc.write("\r\n\n", 3);

#ifdef	USING_ANALOG
	if	(!(start_ADC()))	{
		pc.write	("start_ADC Failed\r\n", 18);
		rv = false;
	}
	if	(!(start_DAC()))	{
		pc.write	("start_DAC Failed\r\n", 18);
		rv = false;
	}
#endif
	if	(!(my_settings.load()))	{
		pc.write	("EEPROM Read Failed\r\n", 20);
		rv = false;
	}
	else	{	//	Reading settings from eeprom good
		int32_t	seti = 0;
		float	mpy = 0.0;
		if	(my_settings.read1	("minv", seti, mpy))	{
			ramp_minv = mpy;
			len = sprintf	(t, "Got 1 setting, %ld, %2.2f\r\n", seti, mpy);
//			if	(!(my_settings.read1	("range", seti, mpy)))
//				rv = false;
//			ramp_range = mpy;
			if	(!(my_settings.read1	("chargedv", seti, mpy)))
				rv = false;
			chargedv = mpy;
			if	(!(my_settings.read1	("vbounce", seti, mpy)))
				rv = false;
			vbounce = mpy;
			if	(!(my_settings.read1	("timea", timea_secs, mpy)))
				rv = false;
			if	(!(my_settings.read1	("timeb", timeb_secs, mpy)))
				rv = false;
			if	(!(my_settings.read1	("timec", timec_secs, mpy)))
				rv = false;
		}
		else	{		//	read_one_setting failed
			rv = false;
		}
		if	(!rv)
			len = sprintf	(t, "Failed to read 1 setting\r\n");
		pc.write	(t, len);
		len = sprintf	(t, "Voltage values min %2.2f, vbounce recoverye %2.2f\r\n", ramp_minv, vbounce);
		pc.write	(t, len);
		len = sprintf	(t, "timea_secs %ld, timeb_secs %ld, timec %ld\r\n", timea_secs, timeb_secs, timec_secs);
		pc.write	(t, len);
		DAC_write	(0.7, DAC_CHANNEL_2);	//	Set Vref to MCP1630
	}
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
	TIM2->CCR4 = 163;	//	Intensity in CANLamp
	return	(rv);
}					//	End of bool	System_Setup	()	;


//float	get_adc_float	(ADC_Chans x)	{
//	ADC sequence of 5 is 0		1		2		3		4
//	ADC channel			11		12		15		6		5
//	Pinout				PA6		PA7		PB0		PA1		PA0
//	Use					nc		v_link	vfield	pot		ammeter

//enum	ADC_Chans	{NOTHING, VLINK, VFIELD, DRIVER_POT, AMMETER}	; In Project.hpp

const	float	ADC_chan_factors[] = {	//	A to D reading gain fiddle multipliers
		0.0, 	//	NOTHING
		41.0, 	//	VLINK
		41.0, 	//	VFIELD
		1.536,	//	DRIVER_POT
		45.0,	//	AMMETER
		}	;

const	float	ADC_chan_offsets[] = {	//	A to D reading offset addition fiddle factors
		0.0, 	//	NOTHING
		0.0, 	//	VLINK
		0.0, 	//	VFIELD
		0.0,	//	DRIVER_POT
		-21.7,	//	AMMETER
		}	;

const	float	Filter_Coefficients[] = {	//	A to D 0.0 <= filter coefficient <= 1.0 (smaller value longer time constant)
		1.0, 	//	NOTHING
		1.0, 	//	VLINK
		0.1, 	//	VFIELD
		0.05,	//	DRIVER_POT (heavily slugged)	changed from 0.005 Feb 24
		1.0,	//	AMMETER
		}	;


#define	ON	true
#define	OFF	!ON

void	red		(bool x)	{	//	Turn red indicator ON or OFF
	  HAL_GPIO_WritePin	(GPIOB, GPIO_PIN_4, x ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


void	green	(bool x)	{	//	Turn green indicator ON or OFF
	  HAL_GPIO_WritePin	(GPIOB, GPIO_PIN_5, x ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


void	beep	(bool x)	{	//	Turn beep sounder ON or OFF - Now using pwm output via other stuff
	uint32_t	val;
	x ? val = 1017 : val = 0;
	TIM2->CCR4 = val;
//	HAL_GPIO_WritePin	(GPIOA, GPIO_PIN_8, x ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


#if 0
void	beep_toggle	()	{
	  HAL_GPIO_TogglePin	(GPIOA, GPIO_PIN_8);
}
#endif


float	get_filter_coeff	(ADC_Chans x)	{
	return	(Filter_Coefficients [x])	;
}


float	get_adj_filt_adc_reading	(ADC_Chans x)	{
	return	((get_ADC_filtered	(x) * ADC_chan_factors[x]) + ADC_chan_offsets[x]);
}


enum	Project_States	{STARTUP, VOLTCHECK, V_GOOD, WARNING1, WARNING2, DEATH, LOCKOUT, ST_LAST = LOCKOUT}	;
const char * state_names[] {"Startup", "Voltcheck", "V_Good", "Warning1", "Warning2", "Death", "Lockout", "Invalid"}	;

	Project_States state	= STARTUP;	//	Global
	int	gp_timer = 0;		//	Global
	float	V_Batt { 0.0 };	//	Global

#define	ramp_maxv	(ramp_minv + ramp_range)

/**
 * void	projreport	()	{
 * 	Here when user types 'rep<Enter.' from command line.
 * 	Reports state of system
 */
void	projreport	()	{
	char	t[200];
	volatile char	time[30] = {0};
	int		len;
	len = sprintf	(t, "Project Report - State [%s], gp_timer %d, ", state_names[state], gp_timer);
	pc.write	(t, len);
	get_time	((char*)time) ;
	len = sprintf	(t, "V_Batt %2.1f, Time %s\r\n", V_Batt, time);
	pc.write	(t, len);
	get_date	((char*)time) ;	//	Needed to un-freeze time registers - Problem Solved !!
	len = sprintf	(t, "\tMin Bat volts 'chargedv' required to start = %.1f\r\n", chargedv);
	pc.write	(t, len);
	len = sprintf	(t, "\tEnter WARNING1 when Bat volts 'minv' falls below %.1f\r\n", ramp_minv);
	pc.write	(t, len);
	len = sprintf	(t, "\tWARNING1 timer 'timea' set to %ld seconds\r\n", timea_secs);
	pc.write	(t, len);
	len = sprintf	(t, "\tMay escape WARNING1 should Bat volts recover to 'vbounce' %.1f during 'timea'\r\n", (vbounce));
	pc.write	(t, len);
	len = sprintf	(t, "\tTimer 'timeb' set to %ld seconds, Loco power HALF, power to 0 in final 15 seconds\r\n", timeb_secs);
	pc.write	(t, len);
	len = sprintf	(t, "\tTimer 'timec' set to %ld seconds, power restored to HALF for this time, then DEATH\r\n", timec_secs);
	pc.write	(t, len);
//	len = sprintf	(t, "\tTimer not used maybe, 'lvsecs' set to %ld seconds\r\n", warn_secs);
//	pc.write	(t, len);
}

bool	may_restore = false;

void	ProjectCore	()	{	//	Here at forever loop repeat rate (30Hz?, 50Hz? around there)

	static	float	ceiling = 0.0;	//	Value between 0.0 and 1.0 used to limit power, e.g. 0.5 half power
	static 	uint32_t	ms_timer = 0L;
	static 	int	goodcount = 0;
	static 	int	badcount = 0;
	static 	int	tenth_secs = 0;	//	Used as count of tenth seconds elapsed
 	char	t[100];		//	Just some string space for assembling text messages
	int		len;		//	Typically used to store length of any text message
/*
 * 	Main Battery Management System State Machine
 *
 * 	Executed twice per second.
 * 	Fast enough to manage 'beep'
 */
	if	(ms_timer < get_millisecs())	{	//	else nothing to do
		ms_timer += 100;					//	sets next timeout in 0.1 sec
		tenth_secs++;		//	Originally used half secs, upgraged Feb 2024 to tenth secs.
		V_Batt = get_adj_filt_adc_reading	(ADC_Chans::VFIELD)	;	//	Get latest filtered battery voltage reading
		switch	(state)	{	//	Happens not twice now but ten times per second

		//	Only visit to state STARTUP is at startup. No return route.
		case	Project_States::STARTUP:	//	At power up, turn on beep to acknowledge power up
			if	(tenth_secs < 2)	{	//	First pass only
				DAC_write	(0, DAC_CHANNEL_1);	//	Control Voltage to motor controller = 0.0
				//NO says Andy				beep	(ON);	//	Sound continuous beep at switch-on until voltage good
				beep	(OFF);	//	Andy says no beep on startup
				red		(ON);	//	Some acknowledment of switch-on
				green	(OFF);	//	No lights
				len = sprintf	(t, "Starting Battery Management System State Machine, V=%2.1f, warn_secs %ld\r\n", V_Batt, warn_secs);
				pc.write	(t, len);	//	Will be seen on laptop 'PuTTY' screen
			}
			if	(tenth_secs > 5)	{	//	stick here for 0.5 secs to allow ADC to read bat volts REDUCED Feb24
				len = sprintf	(t, "Proceeding to state VOLTCHECK with initial voltage V=%2.1f\r\n", V_Batt);
				pc.write	(t, len);	//	Delay about half sec after switch-on for all to stabilise
				state = Project_States::VOLTCHECK;	//	Nothing more to do here, wait for good battery voltage reading (filtered)
			}
			break;

		//	Only visit to state VOLTCHECK is to follow startup. No return route.
		case	Project_States::VOLTCHECK:		//	Battery voltmeter has 'slow' movement. Wait for good voltage reading
//No says Andy			beep_toggle	();		//	ON, OFF, ON, OFF etc
			if	(tenth_secs > 15)	{	//	Have been in VOLTCHECK for sufficient time. Proceed
				len = sprintf	(t, "state VOLTCHECK, Bat Voltage found GOOD %d out of %d times\r\n", goodcount, tenth_secs - 5);
				pc.write	(t, len);
				if	(goodcount > 6)	{	//	Must be found good how many out of 10 times?
					goodcount = 0;
					red		(OFF);
					green	(ON);
					len = sprintf	(t, "Voltage GOOD, enjoy your drive !\r\n");
					ceiling = 1.0;		//	allow full driver pot range through
					state = Project_States::V_GOOD	;
				}
				else	{	//	Battery state was not good enough to allow running. Sorry!
					len = sprintf	(t, "Voltage Too Low, no drive today !\r\n");
					state = Project_States::DEATH;
				}
				pc.write	(t, len);	//	Exit VOLTCHECK state, voltage report
			}
			else	{					//	Remain in state VOLTCHECK
				if	(V_Batt > chargedv)	{	//	Voltage satisfactory
					goodcount++;
					red		(OFF);
					green	(ON);
				}
				else	{					//	Voltage low
					red		(ON);
					green	(OFF);
				}
			}
			break;

		case	Project_States::V_GOOD:	//	Can remain in V_GOOD until voltage falls below ramp_minv for some seconds
			if	(V_Batt > ramp_minv)	{	//	if Battery Voltage above min threshold
				badcount = 0;			//	all is well
			}
			else	{
				badcount++;

				if	(badcount > 20)	{	//	consistently below threshold for some seconds
					len = sprintf	(t, "In state V_GOOD, voltage fallen to %.1f, next state WARNING1\r\n", V_Batt);
					pc.write	(t, len);
					green	(OFF);
					red		(ON);
					badcount = 0;	//	Reuse this variable in next state
					gp_timer = 0;
					state = Project_States::WARNING1;	//	Voltage has been below min for at least 10 seconds
				}
			}
#if 0
			if	((tenth_secs % 10) == 0)	{	//	once per sec or thereabouts
				len = sprintf	(t, ", V=%2.1f", V_Batt);
				pc.write	(t, len);
			}
#endif
			break;

		case	Project_States::WARNING1:	//	green_off, red_flash, 5s beep, full power, stay for 10 mins
//			tmpf = get_adj_filt_adc_reading	(VLINK)	;			//	Make return to V_GOOD possible
//			if	(V_Batt > (ramp_minv + (ramp_range / 2.0)))	{	//	if volts half up ramp or more
			if	(V_Batt > vbounce)	{	//
				goodcount++;	//	0, 1, 2 etc
				if	(goodcount > 100)	{	//	Bat voltage has been reasonable if not great for 10 secs
					red		(OFF);
					beep	(OFF);
					green	(ON);
					len = sprintf	(t, "Voltage recovery in WARNING1, %.1f, next state V_GOOD\r\n", V_Batt);
					pc.write	(t, len);
					goodcount = 0;
					state = Project_States::V_GOOD;
				}
			}
			else	{	//	Bat voltage deemed unreasonably low, remain 'til timeout
				goodcount = 0;
			}
			switch	(tenth_secs % 50)	{	//
			case	0:	case	10:	case	20:	case	30:	case	40:
				red		(OFF);	//	flash rate not specified, use once per sec
				break;
			case	5:	case	15:	case	25:	case	35:	case	45:
				red		(ON);
				break;
			case	1:
				beep	(ON);	//	beep rate specified as 5 sec
				break;
			case	41:
				beep	(OFF);
				break;
			case	12:				//	red will be off. Here once per 5 seconds
				len = sprintf	(t, "In state WARNING1, voltage fallen to %.1f, time secs %d\r\n", V_Batt, gp_timer * 5);
				pc.write	(t, len);
				gp_timer++;		//	incremented at 5 sec intervals
				if	(gp_timer > (timea_secs / 5))	{	//	120*5 seconds = 10 minutes elapsed
					red		(ON);
					ceiling = 0.5;			//	half power
					len = sprintf	(t, "state WARNING1 time out after %d mins.\r\nWARNING2 - Half power\r\n", gp_timer / 12);
					pc.write	(t, len);
					gp_timer = 0;	//	samplecount set to 0 above
					state = Project_States::WARNING2;
				}
				break;
			default:
				break;
			}	//	End of switch (samplecount)
			break;	//	End of case WARNING1

		case	Project_States::WARNING2:	//	red_on, 1s beep, half power, stay for 7 mins
			switch	(tenth_secs % 10)	{	//	cycles once per second -> 'beep' rate once per sec
			case	0:
				gp_timer++;
				if	(gp_timer == (timeb_secs - 15))	{
					len = sprintf	(t, "Starting WARNING2 15 sec power blackout\r\n");
					pc.write	(t, len);
					ceiling = 0.0;	//	cut power
				}
				if	(gp_timer == timeb_secs)	{
					len = sprintf	(t, "state WARNING2 at %ld mins, restoring half power\r\nNEARDEATH - You MUST go home\r\n", timeb_secs / 60);
					pc.write	(t, len);
					may_restore = true;
					ceiling = 0.5;	//	restore half power
				}
				if	(gp_timer > (timeb_secs + timec_secs))	{
					len = sprintf	(t, "state WARNING2 at %ld mins, You've been warned.\r\nNEARDEATH - Now You MUST push it home!\r\n", timeb_secs / 60);
					pc.write	(t, len);
					gp_timer = 0;
					ceiling = 0.0;	//	cut power
					state = Project_States::DEATH;
				}
				break;
			case	1:
				beep	(ON);	//	for 0.7 sec
				break;
			case	8:
				beep	(OFF);	//	for 0.3 sec
				break;
			default:
				break;
			}

			if	((tenth_secs % 130) == 0)	{	//	Report to 'PuTTY' once every 13 seconds. Why 13? Why not?
				len = sprintf	(t, "In state WARNING2, V= %.1f, time secs %d\r\n", V_Batt, gp_timer);
				pc.write	(t, len);
			}
			break;

		case	Project_States::DEATH:				//
			ceiling = 0.0;			//	Cut power to zero
			beep	(ON);			//	Continuous beeeep to annoy driver into using the power switch!
			red		(OFF);
			green	(OFF);
			len = sprintf	(t, "In state DEATH, proceeding to LOCKOUT. That's it. Had enough. More than enough! Bye !\r\n");
			pc.write	(t, len);
			state = Project_States::LOCKOUT;
			break;

		case	Project_States::LOCKOUT:	//	no action and no escape.
			switch	(tenth_secs % 12)	{
			case	0:	case	2:	case	4:
				beep	(ON);
				break;
			case	1:	case	3:	case	5:
				beep	(OFF);
				break;
			default:
				break;
			}
			break;

		default:
			break;
		}			//	End of State Machine switch state
	}				//	End of if (ms_timer < get_millisecs())

					//	Executes code below at forever loop repeat rate (maybe 50 times per sec)
	float Pot = (get_ADC_filtered(ADC_Chans::DRIVER_POT) * ADC_chan_factors[ADC_Chans::DRIVER_POT]);	//	get driver pot
	if	(may_restore && (Pot < 0.1))	{
		may_restore = false;
		ceiling = 0.5;
	}
	DAC_write	(Pot > ceiling ? ceiling : Pot, DAC_CHANNEL_1);			//	modify and forward
#if 0
	count++;
	if	(count > 50)	{
		count = 0;
//		len = sprintf	(t, ", %.3fV ", 45.0 * get_ADC_filtered(DRIVER_POT));
//		tmpf = get_adj_filt_adc_reading	(ADC_Chans::VFIELD)	;	//	Get latest filtered battery voltage reading
//		len = sprintf	(t, ", %.3fV ", 45.0 * get_ADC_filtered(ADC_Chans::VFIELD));
		len = sprintf	(t, ", %.3fV ", get_adj_filt_adc_reading	(ADC_Chans::VFIELD));
		pc.write	(t, len);
		len = sprintf	(t, ", %.3fV ", get_adj_filt_adc_reading	(ADC_Chans::VLINK));
		pc.write	(t, len);
	}
#endif
}		//	End of function ProjectCore()


