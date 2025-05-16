/*
 *  wiringPi.h
 *
 *  Created on: Jan 7, 2018
 *      Author: shahada
 *  Modified and enhanced on: Feb 13, 2021
 *
 *  Copyright (c) 2018 Shahada Abubakar.
 *  Copyright (c) 2021 Jan-Hendrik Ewers.
 *
 *  This file is part of wiringPi-mock.
 *
 *  This program is free software: you can redistribute it and/or
 *  modify it under the terms of the Lesser GNU General Public
 *  License as  published by the Free Software Foundation, either
 *  version 3 of the  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  See the GNU General Public License for more details.
 *
 */


#ifndef WIRINGPI_H_
#define WIRINGPI_H_

#ifdef __cplusplus
extern "C" {
#endif

//#include "version.h"

/* constants and enums ... copied from original wiringPi.h */

#ifndef TRUE
/// Logical TRUE
#  define       TRUE    (1==1)
/// Logical FALSE
#  define       FALSE   (!TRUE)
#endif

// Pin type

/// Pin type: input
#define INPUT                    0 
/// Pin type: output
#define OUTPUT                   1 
/// Pin type: PWM output
#define PWM_OUTPUT               2 
/// Pin type: GPIO clock
#define GPIO_CLOCK               3 
/// Pin type: soft PWM output
#define SOFT_PWM_OUTPUT          4 
/// Pin type: soft tone ouput
#define SOFT_TONE_OUTPUT         5 
/// Pin type: PWM tone output
#define PWM_TONE_OUTPUT          6 

// Define low and high

/// Digital LOW
#define LOW                      0
/// Digital HIGH
#define HIGH                     1

// Pull up/down/none

/// Pull Up/Down resistor: off
#define PUD_OFF                  0
/// Pull Up/Down resistor: down 
#define PUD_DOWN                 1
/// Pull Up/Down resistor: up
#define PUD_UP                   2

// PWM

/// PWM milliseconds mode
#define PWM_MODE_MS             0
/// PWM balanced mode
#define PWM_MODE_BAL            1

// Interrupt levels

/**
 * If ::INT_EDGE_SETUP is used for ::wiringPiISR, it is assumed that the pin is
 * assumed that you have already setup the pin elsewhere (e.g. with the gpio program)
 */
#define INT_EDGE_SETUP          0
/// Interrupt triggered on the falling edge
#define INT_EDGE_FALLING        1
/// Interrupt triggered on the rising edge
#define INT_EDGE_RISING         2
/// Interrupt triggered on BOTH edges
#define INT_EDGE_BOTH           3

/**********************************/
/**** core function prototypes ****/
/**********************************/

/**
 * @brief Must be called once at the start of your program execution.
 * 
 * Default setup: Initialises the system into wiringPi Pin mode and uses the memory mapped hardware directly.
 * @todo Actually enforce this.
 * 
 * @return int Exit code.
 */
int wiringPiSetup (void);

/**
 * @brief Must be called once at the start of your program execution.
 * 
 * GPIO setup: Initialises the system into GPIO Pin mode and uses the memory mapped hardware directly. 
 * @todo Actually enforce this.
 * 
 * @return int Exit code.
 */
int wiringPiSetupGpio (void);

/**
 * @brief Must be called once at the start of your program execution.
 * 
 * Phys setup: Initialises the system into Physical Pin mode and uses the memory mapped hardware directly. 
 * @todo Actually enforce this.
 * 
 * @return int Exit code.
 */
int wiringPiSetupPhys (void);

/**
 * @brief Must be called once at the start of your program execution.
 * 
 * Initialisation (again), however this time we are using the /sys/class/gpio 
 * interface to the GPIO systems - slightly slower, but always usable as a non-root user, 
 * assuming the devices are already exported and setup correctly. 
 * @todo Actually enforce this.
 * 
 * @return int Exit code.
 */
int wiringPiSetupSys (void);

/**
 * @brief Register a Interrupt Service Routine (ISR) - Pi Specific 
 * 
 * @param pin Pin number
 * @param edgeType ::INT_EDGE_SETUP, ::INT_EDGE_FALLING, ::INT_EDGE_RISING, ::INT_EDGE_BOTH           
 * @param function The ISR callback function
 * @return int Exit code.
 */
int wiringPiISR (int pin, int edgeType, void (*function)(void));

/**
 * @brief Sets the mode of a pin to be input, output or PWM output 
 * 
 * @param pin Pin number
 * @param mode Pin type (::INPUT, ::OUTPUT, ::PWM_OUTPUT, etc.)
 */
void pinMode (int pin, int mode);

/**
 * @brief Control the internal pull-up/down resistors on a GPIO pin. 
 * 
 * @param pin Pin number
 * @param mode ::PUD_OFF, ::PUD_DOWN, or ::PUD_UP
 */
void pullUpDnControl (int pin, int pud);

/**
 * @brief Set an output bit. 
 * 
 * @param pin Pin number
 * @param value PWM value
 */
void digitalWrite (int pin, int value);

/**
 * @brief Set an output PWM value. 
 * 
 * @param pin Pin number
 * @param value ::LOW or ::HIGH
 */
void pwmWrite (int pin, int value);

/**
 * @brief Read the value of a given Pin. 
 * 
 * @param pin Pin number
 * @returns ::LOW or ::HIGH
 */
int digitalRead (int pin);

/**
 * @brief Read the analogue value of a given Pin. 
 * 
 * There is no on-board Pi analog hardware, so this needs to go to a new node.
 * 
 * @param pin Pin number
 * @returns ::LOW or ::HIGH
 */
int analogRead (int pin);

/**
 * @brief Write the analog value to the given Pin.
 * 
 * There is no on-board Pi analog hardware, so this needs to go to a new node.
 * 
 * @param pin Pin number
 * @param value Value
 */
void analogWrite (int pin, int value);

/**
 * @brief When called, it will wait for an interrupt event to happen on that pin and your program will be stalled..
 * 
 * The timeOut parameter is given in milliseconds, or can be -1 which means to wait forever.
 *
 * 
 * @param pin Pin number
 * @param timeout Timeout (milliseconds)
 * @param mode ::INT_EDGE_SETUP, ::INT_EDGE_FALLING, ::INT_EDGE_RISING, ::INT_EDGE_BOTH
 *
 * @return The return value is -1 if an error occurred (and errno will be set appropriately), 0 if it timed out, or 1 on a successful interrupt event.
 */
int waitForInterrupt (int pin, int timeOut, int mode);

/************************************/
/**** timing function prototypes ****/
/************************************/

/**
 * @brief Get time in milliseconds.
 * 
 * Wraps after 49 days.
 * 
 * @returns Milliseconds
 */
unsigned int millis (void);

/**
 * @brief Get time in microseconds. 
 * 
 * Wraps after 71 minutes.
 * 
 * @returns Microseconds
 */
unsigned int micros (void);

/**
 * @brief Wait for an amount of milliseconds.
 * 
 * @param howLong Milliseconds to wait
 */
void delay (unsigned int howLong);

/**
 * @brief Wait for an amount of microseconds.
 * 
 * @param howLong Microseconds to wait
 */
void delayMicroseconds (unsigned int howLong);

/*****************/
/**** version ****/
/*****************/

/**
 * @brief Get the current major and minor version number.
 * 
 * @param[out] major Major version number
 * @param[out] minor Minor version number
 */
void wiringPiVersion( int *major, int *minor);


#ifdef __cplusplus
}
#endif


#endif /* WIRINGPI_H_ */

