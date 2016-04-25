#include "config_env.h"

#define LOG_TAG "config_env"

char* configvar_raw(char* varname)
{
    char* val = getenv(varname);
    if (val == NULL)
    {
        LOG(G_LOG_LEVEL_ERROR, "No envvar %s", varname);
        exit(1);
    }
    if (strlen(val) == 0)
    {
        LOG(G_LOG_LEVEL_ERROR, "Envvar %s is empty", varname);
        exit(1);
    }
    return val;
}

char* configvar_string(char* varname)
{
    char* val = configvar_raw(varname);
    LOG(G_LOG_LEVEL_DEBUG, "%s: %s", varname, val);
    return val;
}

int configvar_int(char* varname)
{
    int ret;
    char* val = configvar_raw(varname);
    ret = atoi(val);
    LOG(G_LOG_LEVEL_DEBUG, "%s: %d", varname, ret);
    return ret;
}

int configvar_bool(char* varname)
{
    int ret = 0;
    char* val = configvar_raw(varname);
    char yn = val[0];
    if (yn == 'y' || yn == 'Y' || yn == '1')
    {
        ret = 1;
    }
    else if (yn != 'n' && yn != 'N' && yn != '0')
    {
        LOG(G_LOG_LEVEL_ERROR, "%s: value must start with (y|n|0|1), was %s", varname, val);
        exit(1);
    }
    LOG(G_LOG_LEVEL_DEBUG, "%s: %d", varname, ret);
    return ret;
}
