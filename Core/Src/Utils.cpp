/*
 * Utils.cpp	INTENDED TO BE PROJECT SPECIFIC
 *
 *  Created on: Feb 11, 2024
 *      Author: Jon Freeman  B. Eng. Hons
 *
 *	For menus, and functions executed through menus.
 *	Put such clutter here,
 *
 *	Keep 'Project.cpp' for the main logical flow of your project
 */
#include	<cstdio>			//	for sprintf
#include	<cstring>			//	for strlen

#include	"Project.hpp"
#include	"parameters.hpp"
#include	"Serial.hpp"
#include	"CmdLine.hpp"
#include	"settings.hpp"


//const	char	version_str[] = "Info About Project Here," __DATE__;
const	char	version_str[] = "BSMEE Loco Battery Management System, Jon Freeman, " __DATE__;

const 	char * 	get_version	()	{	//	Makes above available throughout code.
	return	(version_str);
}


//	Prototypes for functions included in 'settings_data' menu structure
bool	null_cmd	(parameters &);
bool	set_defaults_cmd	(parameters &);
bool	set_one_wrapper_cmd	(parameters &);

//enum	class	MenuType	{MENU, SETTINGS}	;	//	in 'parameters.hpp'

struct cli_menu_entry_set	const  settings_data[]
{    // Can not form pointers to member functions.
	{"?",     	"Lists all user settings, alters none", null_cmd, static_cast<int32_t>(MenuType::SETTINGS)}, //	Examples of use follow
	{"defaults","Reset settings to factory defaults", set_defaults_cmd},     //	restore factory defaults to all settings
	{"timea",	"Set timea WARNING1 seconds, spec is 600 (ten mins)", set_one_wrapper_cmd, 10, 900, 600},     //
	{"timeb",	"Set timeb seconds", set_one_wrapper_cmd, 10, 900, 60},     //
	{"timec",	"Set timec seconds", set_one_wrapper_cmd, 10, 900, 60},     //
	{"minv",	"Low Volts Cutoff V times 10", set_one_wrapper_cmd,	100, 650, 220, 0.1},    //
	{"vbounce",	"Low Volts recovery, V times 10", 	set_one_wrapper_cmd,	200, 600, 230, 0.1}, //
	{"chargedv","Bat volts required to start times 10", 	set_one_wrapper_cmd, 	200, 600, 260, 0.1},   //
	{nullptr},	//	June 2023 new end of list delimiter. No need to pass sizeof
}	;

//	Prototypes for functions included in 'pc_command_list' menu structure
bool	menucmd	(parameters &);
bool	rtc_cmd	(parameters &);
bool	adc_cmd	(parameters &);
bool	st_cmd	(parameters &);
bool	sd_cmd	(parameters &);
bool	edit_settings_cmd	(parameters &);
bool	beep_set_cmd	(parameters &);
bool	proj_rep_cmd	(parameters & a)	;
/**
struct  cli_menu_entry_set      const loco_command_list[] = {
List of commands accepted from external pc through non-opto isolated com port 115200, 8,n,1
*/
struct  cli_menu_entry_set	const pc_command_list[] = {
    {"?", "Lists available commands", 	menucmd, static_cast<int32_t>(MenuType::MENU)},
	{"rtc", "real time clock buggery", 	rtc_cmd},
	{"adc", "check adc dma working", 	adc_cmd},
	{"st", "real time clock Time", 		st_cmd},
	{"sd", "real time clock Date", 		sd_cmd},
	{"us", "user settings", 			edit_settings_cmd},
	{"bp", "beep pwm reg value set 0-??", 			beep_set_cmd},
	{"rep", "Report State of System", 			proj_rep_cmd},
    {"nu", "do nothing", null_cmd},
    {nullptr},	//	June 2023 new end of list delimiter. No need to pass sizeof
}   ;


//	************* Create Utilities *****************
extern	UART_HandleTypeDef	huart2;	//	uarts used in this project
extern	I2C_HandleTypeDef 	hi2c1;	//	I2C

Serial				pc(huart2);		//, * Com_ptrs[];
i2eeprom_settings	my_settings	(settings_data, hi2c1)	;	//	Create one i2eeprom_settings object named 'j_settings'
CommandLineHandler	command_line_handler	(pc_command_list, & pc);	//	Nice and clean

#define	COM_PORT	pc

/**
*   void    menucmd 		(struct parameters & a)
*	void	list_settings	(const menu_entry_set * list)
*   List available terminal commands to pc terminal. No sense in touch screen using this
*/
void	list_settings	(const cli_menu_entry_set * list)	{
	int i = 0;
	int len;
	int32_t	ival;
	float	fval;
	char	t[200];
	char	ins_tab[2] {0,0};
	len = sprintf	(t, "Listing %s Functions and Values :\r\n", list[0].min ? "SETTINGS" : "MENU");
	pc.write	(t, len);
	while	(list[i].cmd_word)	{
		(6 > strlen(list[i].cmd_word)) ? ins_tab[0] = '\t' : ins_tab[0] = 0;
		len = sprintf	(t, "[%s]\t%s%s"
			, list[i].cmd_word
			, ins_tab
			, list[i].description	);
		pc.write	(t, len);	//	This much common to MENU and SETTINGS
		if	(list[0].min)	{	//	is SETTINGS, not MENU
			if	(my_settings.read1(list[i].cmd_word, ival, fval))	{
				(6 > strlen(list[i].cmd_word)) ? ins_tab[0] = '\t' : ins_tab[0] = 0;
				len = sprintf	(t, "\tmin%ld, max%ld, def%ld\t[is %ld]\t(float mpr %.2f)"
					, list[i].min
					, list[i].max
					, list[i].de_fault
					, ival
					, fval	);
				pc.write	(t, len);
			}
			else	pc.write	("Settings Read Error\r\n", 21);
		}	//	Endof is SETTINGS, not MENU
		pc.write	("\r\n", 2);
		i++;
	}	//	Endof 	while	(list[i].cmd_word)
	pc.write("End of List of Commands\r\n", 25);
}


bool    menucmd (struct parameters & a)     {
	list_settings	(a.command_list)	;
    return	(true);
}


bool	beep_set_cmd	(parameters & a)	{
	TIM2->CCR4 = (uint32_t)a.flt[0];
    return	(true);
}


extern	void	projreport	()	;
bool	proj_rep_cmd	(parameters & a)	{
	projreport();
    return	(true);
}


bool	edit_settings_cmd (struct parameters & a)     {	//	Here from CLI having found "us "
	bool	rv =	(my_settings.edit	(a));
	list_settings	(settings_data)	;
	return	(rv)	;
}


bool    set_one_wrapper_cmd (struct parameters & a)     {	//	Called via edit, a.second_word found in edit
	return	(my_settings.set_one	(a));
}


bool    null_cmd (struct parameters & a)     {
	const char t[] = "null command - does nothing useful!\r\n";
	COM_PORT.write(t, strlen(t));
    return	(true);
}


bool    set_defaults_cmd (struct parameters & a)     {
	return	(my_settings.set_defaults());
}


void	check_commands	()	{	//	Called from ForeverLoop
/**
 * bool	Serial::test_for_message	()	{
 *
 * Called from ForeverLoop at repetition rate
 * Returns true when "\r\n" terminated command line has been assembled in lin_inbuff
 */
	char * buff_start = COM_PORT.test_for_message();
	if	(buff_start)
		command_line_handler.CommandExec(buff_start);
}



extern	uint32_t	can_errors;
extern	void	rtc_buggery	()	;
extern	void	adc_cnt_report	()	;

bool	adc_cmd	 (struct parameters & a)     {
#ifdef	USING_ANALOG
	adc_cnt_report();
#endif
	return	(true);
}


#ifdef	USING_RTC
extern	bool	set_time	(struct parameters & a)	;
extern	bool	set_date	(struct parameters & a)	;

bool	st_cmd	 (struct parameters & a)     {
	return	(set_time	(a));
}


bool	sd_cmd	 (struct parameters & a)     {
	return	(set_date	(a));
}

bool	rtc_cmd	 (struct parameters & a)     {
	rtc_buggery();
	return	(true);
}


#else
bool	st_cmd	 (struct parameters & a)     {
	return	(false);
}


bool	sd_cmd	 (struct parameters & a)     {
	return	(false);
}


bool	rtc_cmd	 (struct parameters & a)     {
	return	(false);
}


#endif






