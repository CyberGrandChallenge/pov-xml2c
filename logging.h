/*
 * Id:             $Id: logging.h 3725 2015-02-10 23:48:55Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-02-10 23:48:55 +0000 (Tue, 10 Feb 2015) $ 
 */

#ifndef __LOGGING_H
#define __LOGGING_H

void enableDebug();
void disableDebug();

void log_error(const char *prefix);
void log_ok(const char *msg, ...);
void log_fail(const char *msg, ...);
void log(const char *msg, ...);

#endif
