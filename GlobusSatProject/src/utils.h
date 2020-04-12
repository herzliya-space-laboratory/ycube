/*
 * utils.h
 *
 *  Created on: Sep 19, 2019
 *      Author: pc
 */

#ifndef UTILS_H_
#define UTILS_H_

#include "GlobalStandards.h"
#include <hal/Timing/Time.h>


#define E_CANT_TRANSMIT    		-200
#define E_TOO_EARLY_4_BEACON    -201
#define E_INVALID_PARAMETERS    -204
#define TRXVU_MUTE_TOO_LOMG    -202
#define TRXVU_IDLE_TOO_LOMG    -203


#define MAX_LOG_STR				40

typedef struct data
{
	int error;
	char msg[MAX_LOG_STR];
} logData_t;


/*
 * convert unix time to Time struct
 */
void timeU2time(time_unix utime, Time *time);
/*
 * log error message
 */
int logError(int error);

/*
 * log info message
 */
int logInfo(char *info);

#endif /* UTILS_H_ */

