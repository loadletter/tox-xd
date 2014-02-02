#ifndef MISC_H
#define MISC_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h> /* for memcpy() */

/*********************Debugging Macros********************
 * wiki.tox.im/index.php/Internal_functions_and_data_structures#Debugging
 *********************************************************/
#ifdef DEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_PRINT(str, ...) do { \
        char msg[1000]; \
        sprintf(msg, "%s(): line %d (file %s): %s%%c\n", __FUNCTION__, __LINE__, __FILE__, str); \
        fprintf(stderr, msg, __VA_ARGS__); \
    } while (0)

#define WARNING(...) do { \
        fprintf(stderr, "warning in "); \
        DEBUG_PRINT(__VA_ARGS__, ' '); \
    } while (0)

#define INFO(...) do { \
        DEBUG_PRINT(__VA_ARGS__, ' '); \
    } while (0)

#undef ERROR
#define ERROR(exit_status, ...) do { \
        fprintf(stderr, "error in "); \
        DEBUG_PRINT(__VA_ARGS__, ' '); \
        exit(exit_status); \
    } while (0)
#else
#define WARNING(...)
#define INFO(...)
#undef ERROR
#define ERROR(...)
#endif // DEBUG

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

unsigned char *hex_string_to_bin(char hex_string[]);
#endif
