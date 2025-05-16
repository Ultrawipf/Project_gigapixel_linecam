/*
 *  wiringPi.cpp
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


#include <time.h>

#ifdef __unix__                    /* __unix__ is usually defined by compilers targeting Unix systems */
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)     /* _Win32 is usually defined by compilers targeting 32 or   64 bit Windows systems */
#include <windows.h>
#endif
#include "wiringPi.h"

/* private storage */

struct timespec wiring_pi_setup_time;

/* implementation */

int wiringPiSetup(void) {

	// record what time we were initialized. Not quite the same as when
	// the program was started, but good enough.
    //clock_gettime(CLOCK_REALTIME, &wiring_pi_setup_time);
	return 0;
}
;

int wiringPiSetupGpio(void) {
	return 0;
}
;

int wiringPiSetupPhys(void) {
	return 0;
}
;

int wiringPiSetupSys(void) {
	return 0;
}
;

int wiringPiISR (int pin, int edgeType, void (*function)(void))
{
    return 0;
}
;

void pinMode(int pin, int mode) {
}
;

void pullUpDnControl(int pin, int pud) {
}
;

void digitalWrite(int pin, int value) {
}
;

void pwmWrite(int pin, int value) {
}
;

int digitalRead(int pin) {
	return 1;
}
;

int analogRead(int pin) {
	return 0;
}
;

void analogWrite(int pin, int value) {
}
;

int waitForInterrupt (int pin, int timeOut, int mode){
    delay(timeOut/4);
    return 0;
}
;

/* timing functions */

/* function to calculate the difference between two timespecs */

void wiringpi_helper_timespec_diff(struct timespec *start,
		struct timespec *stop, struct timespec *result) {
	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}

	return;
}

unsigned int millis(void) {
/*
	struct timespec nowtime;
	clock_gettime(CLOCK_REALTIME, &nowtime);

	struct timespec result;
	wiringpi_helper_timespec_diff(&nowtime, &wiring_pi_setup_time, &result);

	return (result.tv_sec * 1000) + (result.tv_nsec / 1000000);

*/
    return 0;
}

unsigned int micros(void) {
/*
	struct timespec nowtime;
	clock_gettime(CLOCK_REALTIME, &nowtime);

	struct timespec result;
	wiringpi_helper_timespec_diff(&nowtime, &wiring_pi_setup_time, &result);

	return (result.tv_sec * 1000) + (result.tv_nsec / 1000);
*/
    return 0;
}

void delay(unsigned int howLongMillis) {

    Sleep (howLongMillis);
}


void delayMicroseconds (unsigned int howLongMicros) {

    Sleep (howLongMicros / 1000);
}


/* version */
void wiringPiVersion( int *major, int *minor){
    *major = 0;
    *minor = 0;
};
