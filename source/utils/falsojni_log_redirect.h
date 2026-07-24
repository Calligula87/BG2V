#ifndef BG2V_FALSOJNI_LOG_REDIRECT_H
#define BG2V_FALSOJNI_LOG_REDIRECT_H

#include "utils/logger.h"

/*
 * FalsoJNI normally writes only to the debug console. Redirect its existing
 * logging calls through BG2V's dual console/file logger for device iteration.
 */
#define sceClibPrintf bg2v_log_printf

#endif
