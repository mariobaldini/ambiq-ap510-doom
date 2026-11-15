#ifndef ZEPHYR_COMPAT_LOG_H
#define ZEPHYR_COMPAT_LOG_H

#define LOG_MODULE_REGISTER(...) extern int zephyr_compat_log_unused
#define LOG_ERR(...) ((void)0)

#endif
