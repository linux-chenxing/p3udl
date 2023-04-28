//SPDX-License-Identifier: GPL-3.0-or-later
/*
 */

#ifndef SRC_LOG_H_
#define SRC_LOG_H_

int log_printf(int level, const char *tag, const char *format,...) __attribute__ ((format (printf, 3, 4)));

#endif /* SRC_LOG_H_ */
