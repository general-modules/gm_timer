/**
 * @file      example_timer.c
 * @brief     定时器模块示例代码源文件
 * @author    huenrong (sgyhy1028@outlook.com)
 * @date      2026-02-16 16:28:39
 *
 * @copyright Copyright (c) 2026 huenrong
 *
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "gm_timer.h"

/**
 * @brief 获取当前 UTC 时间戳(毫秒)
 *
 * @param[out] timestamp_ms: 自 1970-01-01 00:00:00 UTC 起的毫秒数
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_time_get_current_timestamp_ms(uint64_t *timestamp_ms)
{
    if (timestamp_ms == NULL)
    {
        return -1;
    }

    struct timespec time_spec = {0};
    if (clock_gettime(CLOCK_REALTIME, &time_spec) == -1)
    {
        return -2;
    }

    *timestamp_ms = time_spec.tv_sec * 1000 + time_spec.tv_nsec / 1000000;

    return 0;
}

static void test_timer1_cb(gm_timer_t *gm_timer)
{
    uint64_t current_timestamp = 0;
    gm_time_get_current_timestamp_ms(&current_timestamp);

    printf("[%ld] this is %s, user_data: %d\n", current_timestamp, __FUNCTION__,
           *(uint8_t *)gm_timer_get_user_data(gm_timer));
}

static void test_timer2_cb(gm_timer_t *gm_timer)
{
    uint64_t current_timestamp = 0;
    gm_time_get_current_timestamp_ms(&current_timestamp);

    printf("[%ld] this is %s\n", current_timestamp, __FUNCTION__);

    static uint8_t count = 0;
    if (count++ == 5)
    {
        printf("deinit and destroy timer2\n");
        gm_timer_destroy(gm_timer);
    }
}

/**
 * @brief 程序入口
 *
 * @param[in] argc: 参数个数
 * @param[in] argv: 参数列表
 *
 * @return 成功: 0
 * @return 失败: 其它
 */
int main(int argc, char *argv[])
{
    uint8_t user_data = 10;
    uint64_t current_timestamp = 0;

    printf("create timer1\n");
    gm_timer_t *test_timer1 = gm_timer_create();
    if (test_timer1 == NULL)
    {
        printf("create timer1 failed\n");

        return -1;
    }
    printf("create timer1 success. timer: %p\n", test_timer1);

    int ret = gm_timer_init(test_timer1, test_timer1_cb, GM_TIMER_REPEAT_FOREVER, 1000, &user_data);
    if (ret != 0)
    {
        printf("init timer1 failed. ret: %d\n", ret);

        return -1;
    }

    gm_time_get_current_timestamp_ms(&current_timestamp);
    printf("[%ld] init timer1 success. set timer1 ready\n", current_timestamp);
    ret = gm_timer_ready(test_timer1);
    if (ret != 0)
    {
        printf("set timer1 ready failed. ret: %d\n", ret);

        return -1;
    }

    int32_t repeat_count = 5;
    printf("set timer1 repeat count. count: %d\n", repeat_count);
    ret = gm_timer_set_repeat_count(test_timer1, repeat_count);
    if (ret != 0)
    {
        printf("set timer1 repeat count failed. ret: %d\n", ret);

        return -1;
    }

    printf("create timer2\n");
    gm_timer_t *test_timer2 = gm_timer_create();
    if (test_timer2 == NULL)
    {
        printf("create timer2 failed\n");

        return -1;
    }
    printf("create timer2 success. timer: %p\n", test_timer2);

    ret = gm_timer_init(test_timer2, test_timer2_cb, GM_TIMER_REPEAT_FOREVER, 1000, NULL);
    if (ret != 0)
    {
        printf("init timer2 failed. ret: %d\n", ret);

        return -6;
    }

    sleep(3);
    // gm_time_get_current_timestamp_ms(&current_timestamp);
    // printf("[%ld] pause timer2\n", current_timestamp);
    // gm_timer_pause(test_timer2);

    sleep(3);
    gm_time_get_current_timestamp_ms(&current_timestamp);
    printf("[%ld] destroy timer2\n", current_timestamp);
    gm_timer_destroy(test_timer2);
    // gm_time_get_current_timestamp_ms(&current_timestamp);
    // printf("[%ld] resume timer2\n", current_timestamp);
    // gm_timer_resume(test_timer2);

    while (true)
    {
        sleep(1);
    }

    return 0;
}
