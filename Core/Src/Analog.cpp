/*
 * Analog.cpp
 *
 *  Created on: Jan 24, 2024
 *      Author: Jon Freeman  B. Eng. Hons
 */
#include	"main.h"

#include	<cstdio>

#include	"Project.hpp"
#include	"Serial.hpp"

#ifdef	USING_ANALOG

extern	Serial	pc;

extern	ADC_HandleTypeDef hadc1;
//extern	DMA_HandleTypeDef hdma_adc1;

#define	ADC_buffsize_per_chan	32	//	Average sum of half this numof samples
//#define	NUMOF_ADC_CHANS			5	//	chans 1, 12, 15, 6, 5

uint16_t			ADC_buff_L	[ADC_buffsize_per_chan * (ADC_Chans::ADC_LAST + 1)] {0};	//	Ping pong buffer
uint16_t  * const 	ADC_buff_H	= ADC_buff_L + (sizeof(ADC_buff_L) / (sizeof(uint16_t) * 2));
uint32_t	adc_sums	[ADC_Chans::ADC_LAST + 1] {0L};
float		adc_filters	[ADC_Chans::ADC_LAST + 1] {0.0};
bool		adc_buff_half_full_flag	= false	;
bool		adc_buff_full_flag		= false	;


float	get_ADC_filtered	(ADC_Chans x)	{
	return	(adc_filters[x])	;
}


extern	float	get_filter_coeff	(ADC_Chans x)	;
//extern const	float	Filter_Coefficients []	;
//float 	Filter_Coefficients[6] ;	//	A to D 0.0 <= filter coefficient <= 1.0 (smaller value longer time constant)


void	adc_summer	(uint16_t * src)	{
	uint32_t	tmp	[ADC_Chans::ADC_LAST + 1] = {0L};
	float	tmpf;
	for	(uint32_t j = 0; j <= ADC_Chans::ADC_LAST; j++)
		adc_sums[j] = 0L;
	for	(uint32_t i = 0; i < (ADC_buffsize_per_chan >> 1); i++)
		for	(uint32_t j = 0; j <= ADC_Chans::ADC_LAST; j++)
			tmp[j] +=  *src++;
	for	(uint32_t j = 0; j <= ADC_Chans::ADC_LAST; j++)	{
		adc_sums[j] = tmp[j];
//		tmpf = get_adc_ave_normalised	((ADC_Chans)j)	;
		tmpf = ((float)adc_sums[j]) / (ADC_buffsize_per_chan << 11)	;
		adc_filters[j] =	(adc_filters[j]	 * (1.0 - get_filter_coeff((ADC_Chans)j))) + (tmpf * get_filter_coeff((ADC_Chans)j))	;
	}
}


bool	adc_updates	()	{	//	Call this often
	bool	rv = false;		//	Returns true if update was due and performed, false when no update available
	if	(adc_buff_half_full_flag)	{
		rv = true;
		adc_buff_half_full_flag = false;
		adc_summer	(ADC_buff_L)	;
	}
	if	(adc_buff_full_flag)	{
		rv = true;
		adc_buff_full_flag = false;
		adc_summer	(ADC_buff_H)	;
	}
	return	(rv);
}


bool	start_ADC	()	{	//	Inputs
	return	(HAL_OK == HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_buff_L, ADC_buffsize_per_chan * (ADC_Chans::ADC_LAST + 1)));
}


//uint32_t	halfcnt = 0L;
//uint32_t	fullcnt = 0L;

void	HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef * hadc1) 	{	//	Interrupt handler. Get out quick, do moves from Forever Loop
	adc_buff_half_full_flag	= true;
//	halfcnt++;
}


void	HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef * hadc1) 	{	//	Interrupt handler. Get out quick, do moves from Forever Loop
	adc_buff_full_flag	= true;
//	fullcnt++;
	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);	//	as good a place as any to toggle green Nucleo led
}


extern	char * get_date	(char * dest)	;	//	get e.g. "16:50:11"
extern	char * get_time	(char * dest)	;	//	get e.g. "16:50:11"
extern	int32_t	get_date	()	;
extern	int32_t	get_time	()	;
extern	void	rtc_buggery	()	;

#ifdef	USING_RTC
void	adc_cnt_report	()	{
	char	t[60] ;
	char	hms[20] ;
	int	len;
	int32_t	tim = get_date	();
//	len = sprintf	(t, "ADC Half cnt=%ld\r\n", halfcnt);
//	pc.write	(t, len);
//	len = sprintf	(t, "ADC Full cnt=%ld, ", fullcnt);
//	pc.write	(t, len);
	rtc_buggery	();
	get_time	(hms);
	tim = get_time();
	len = sprintf	(t, "%s, 0x%05lx\r\n", hms, tim);
	pc.write	(t, len);
	len = sprintf	(t, "A-D reading 0x%05lx, 0x%05lx, 0x%05lx 0x%05lx 0x%05lx\r\n", adc_sums[0], adc_sums[1], adc_sums[2], adc_sums[3], adc_sums[4]);
	pc.write	(t, len);
}
#else
void	adc_cnt_report	()	{
	char	t[60] = {0};
	char	hms[20] = {0};
	int	len;
	len = sprintf	(t, "ADC Half cnt=%ld\r\n", halfcnt);
	pc.write	(t, len);
	len = sprintf	(t, "ADC Full cnt=%ld\r\n", fullcnt);
	pc.write	(t, len);
}


#endif
	//	End of Inputs

	//	Start of Outputs
extern	DAC_HandleTypeDef hdac1;


bool	start_DAC	()	{	//	Outputs
	bool	rv = true;
	if	(HAL_OK != HAL_DAC_Start	(&hdac1, DAC_CHANNEL_1))
			rv = false;
	if	(HAL_OK != HAL_DAC_Start	(&hdac1, DAC_CHANNEL_2))
			rv = false;
	return	(rv);
}


bool	DAC_write	(uint32_t outval, uint32_t Chan)	{	//	DAC output 1 or 2
	return	(HAL_OK == HAL_DAC_SetValue	(&hdac1, Chan, DAC_ALIGN_12B_R, outval));
}


bool	DAC_write	(float outvalnorm, uint32_t DAC_Chan)	{
	uint32_t	outval = (uint32_t)(outvalnorm * (1 << 12));
	if	(outval < 0L)
		outval = 0L;
	if	(outval >= (1 << 12))
		outval = (1 << 12) - 1;
	return	(DAC_write	(outval, DAC_Chan));
}

#endif
