/*
 * Project.hpp
 *
 *  Created on: Jan 25, 2024
 *      Author: Jon Freeman  B. Eng. Hons
 */
#ifndef INC_PROJECT_COMPONENTS_HPP_
#define INC_PROJECT_COMPONENTS_HPP_

#define	USING_ANALOG
#define	USING_ADC
#define	USING_DAC
#define	USING_RTC		//	Real Time Clock
#define	USING_USART1
//#define	USING_LM75		//	Temperature sensor
//#define	USING_CAN		//	CAN bus

#define	CHARGE_PUMP	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1)
enum	ADC_Chans	{NOTHING, VLINK, VFIELD, DRIVER_POT, AMMETER, ADC_LAST = AMMETER}	;

#endif


