#ifndef BG2V_FALSOJNI_LOG_REDIRECT_H
#define BG2V_FALSOJNI_LOG_REDIRECT_H

#include "utils/logger.h"

/*
 * FalsoJNI and so_util normally write only to the debug console. Redirect
 * their existing logging calls through BG2V's dual console/file logger for
 * device iteration.
 */
#define sceClibPrintf bg2v_log_printf

#endif
