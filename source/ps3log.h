#ifndef PS3LOG_H
#define PS3LOG_H

/* Diagnostic log on a real PS3.
 *
 * In the emulator printf output can be read from the TTY, but the console
 * has no such thing. So the important steps are written to a file; even
 * on a black screen the file can be fetched over FTP to see how far it got.
 *
 * File: /dev_hdd0/tmp/basicwater.log */

void ps3log_open(void);
void ps3log(const char *fmt, ...);
void ps3log_close(void);

#endif
