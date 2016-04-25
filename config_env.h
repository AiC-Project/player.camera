#ifndef __CONFIG_ENV_H_
#define __CONFIG_ENV_H_

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

char* configvar_raw(char* varname);
char* configvar_string(char* varname);
int configvar_int(char* varname);
int configvar_bool(char* varname);

#endif
