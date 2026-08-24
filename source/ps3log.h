#ifndef PS3LOG_H
#define PS3LOG_H

/* Gercek PS3'te teshis kaydi.
 *
 * Emulatorde printf ciktisi TTY'den okunabiliyor, ancak konsolda boyle bir
 * imkan yok. Bu yuzden onemli adimlar dosyaya yazilir; siyah ekran durumunda
 * bile dosya FTP ile alinip nereye kadar gidildigi gorulebilir.
 *
 * Dosya: /dev_hdd0/tmp/basicwater.log */

void ps3log_open(void);
void ps3log(const char *fmt, ...);
void ps3log_close(void);

#endif
