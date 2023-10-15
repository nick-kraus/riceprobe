#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(swo, LOG_LEVEL_INF);

static struct k_timer swo_count_timer;
void swo_count_fn(struct k_timer *timer) {
	static uint32_t count = 0;

    LOG_WRN("count: %u", count);
    count++;
}

void swo_count_init(void) {
    /* unsure why but setting log filter level doesn't seem to work without a delay here */
    k_sleep(K_MSEC(100));

    const struct log_backend *swo_log_backend = log_backend_get_by_name("log_backend_swo");
    const struct log_backend *rtt_log_backend = log_backend_get_by_name("shell_rtt_backend");

    /* swo log backend will only process messages for this log module, rtt module will not process this module */
    for (uint32_t src = 0; src < log_src_cnt_get(0); src++) {
        const char *source_name = log_source_name_get(0, src);
        if (source_name != NULL && strncmp(source_name, "swo", strlen("swo")) == 0) {
            log_filter_set(swo_log_backend, 0, src, LOG_LEVEL_INF);
            log_filter_set(rtt_log_backend, 0, src, LOG_LEVEL_NONE);
        } else {
            log_filter_set(swo_log_backend, 0, src, LOG_LEVEL_NONE);
        }
    }

    k_timer_init(&swo_count_timer, swo_count_fn, NULL);
    k_timer_start(&swo_count_timer, K_SECONDS(1), K_SECONDS(1));
}
