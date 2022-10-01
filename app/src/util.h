#ifndef __UTIL_H__
#define __UTIL_H__

/* tests a condition, and triggers a kernel oops with a log message on failure */
#define FATAL_CHECK(_cond, _msg)    \
    do {                            \
        if ((_cond) == false) {     \
            LOG_ERR(_msg);          \
            k_oops();               \
        }                           \
    } while (0)

#endif /* __UTIL_H__ */
