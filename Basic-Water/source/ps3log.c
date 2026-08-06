#include <stdio.h>
#include <stdarg.h>

#include "ps3log.h"

#define LOG_PATH "/dev_hdd0/tmp/basicwater.log"

static FILE *lf = NULL;

void ps3log_open(void)
{
    lf = fopen(LOG_PATH, "w");
}

void ps3log(const char *fmt, ...)
{
    va_list ap;

    /* Emulatorde TTY'ye, konsolda dosyaya (ikisi de zararsiz) */
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    if (lf == NULL)
        return;

    va_start(ap, fmt);
    vfprintf(lf, fmt, ap);
    va_end(ap);
    fprintf(lf, "\n");
    fflush(lf);
}

void ps3log_close(void)
{
    if (lf != NULL) {
        fclose(lf);
        lf = NULL;
    }
}
