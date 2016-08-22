/*
 Copyright 2012 Fival Co.,Ltd. All Rights Reserved.

 $Author: zhou.k $
 $Id: fica_opt.h 18 2012-10-15 15:41:34Z zhou.k $
 $Date: 2012-10-15 23:41:34 +0800 (Mon, 15 Oct 2012) $
 $Revision: 18 $
 */
#ifndef __FICA_OPT_H__
#define __FICA_OPT_H__

#include <ctype.h>
#include <string.h>

static char* fica_trim(char* string)
{
    char* trimmed = string;

    // trim right
    while (strlen(trimmed) > 0 && !isgraph(trimmed[strlen(trimmed) - 1])) {
        trimmed[strlen(trimmed) - 1] = '\0';
    }
    // trim left
    while (strlen(trimmed) > 0 && !isgraph(trimmed[0])) {
        trimmed++;
    }

    return (trimmed);
}

#define DELIMITOR   ' '
#define EQUAL       '='
static char* fica_shift_option(char *args, char** key, char** value)
{
    char* start = args;
    char* next = NULL;
    char* ptr_eq = NULL;
    char* ptr_sp = NULL;

    *key = NULL;
    *value = NULL;

    start = fica_trim(start);
    if (strlen(start) <= 0)
        return (NULL);

    ptr_sp = strchr(start, DELIMITOR);
    if (ptr_sp) {
        *(ptr_sp) = '\0';
        next = ptr_sp + 1;
    } else {
        next = start + strlen(start);
    }

    *key = fica_trim(start);
    ptr_eq = strchr(start, '=');
    if (ptr_eq) {
        *ptr_eq = '\0';
        *value = fica_trim(ptr_eq + 1);
    } else {
        *value = "0";
    }

    return (next);
}

static char* fica_shift_option2(char ***arglist, const char* option)
{
    char** args = *arglist;
    char* arg = args[0], **rest = &args[1];
    int optlen = strlen(option);
    char* val = arg + optlen;
    if (option[optlen - 1] != '=') {
        if (strcmp(arg, option))
            return NULL;
    } else {
        if (strncmp(arg, option, optlen - 1))
            return NULL;
        if (arg[optlen - 1] == '\0')
            val = *rest++;
        else if (arg[optlen - 1] != '=')
            return NULL;
    }
    *arglist = rest;
    return val;
}

#endif // __FICA_OPT_H__
