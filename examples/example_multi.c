/**
 * @file      example_multi.c
 * @brief     多定时器示例
 * @author    huenrong (sgyhy1028@outlook.com)
 * @date      2026-03-28 13:41:43
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
static int gm_time_get_current_timestamp_ms(uint64_t *timestamp_ms)
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

/**
 * @brief 定时器回调函数
 *
 * @param[in,out] gm_timer: 定时器对象
 */
static void on_test_timer1(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return;
    }
    uint8_t user_data = *(uint8_t *)gm_timer_get_user_data(gm_timer);

    uint64_t current_timestamp = 0;
    gm_time_get_current_timestamp_ms(&current_timestamp);
    printf("[%ld] this is %s, user_data: %d\n", current_timestamp, __FUNCTION__, user_data);
}

/**
 * @brief 定时器回调函数
 *
 * @param[in,out] gm_timer: 定时器对象
 */
static void on_test_timer2(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return;
    }
    uint8_t user_data = *(uint8_t *)gm_timer_get_user_data(gm_timer);

    uint64_t current_timestamp = 0;
    gm_time_get_current_timestamp_ms(&current_timestamp);
    printf("[%ld] this is %s, user_data: %d\n", current_timestamp, __FUNCTION__, user_data);
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
    gm_timer_t *test_timer1 = gm_timer_create();
    if (test_timer1 == NULL)
    {
        printf("create test timer1 failed\n");

        return -1;
    }

    gm_timer_t *test_timer2 = gm_timer_create();
    if (test_timer2 == NULL)
    {
        printf("create test timer2 failed\n");

        return -1;
    }

    uint8_t user_data1 = 10;
    int ret = gm_timer_init(test_timer1, on_test_timer1, GM_TIMER_REPEAT_FOREVER, 1000, &user_data1);
    if (ret != 0)
    {
        printf("init test timer1 failed. ret: %d\n", ret);

        return -1;
    }

    uint8_t user_data2 = 20;
    ret = gm_timer_init(test_timer2, on_test_timer2, GM_TIMER_REPEAT_FOREVER, 1500, &user_data2);
    if (ret != 0)
    {
        printf("init test timer2 failed. ret: %d\n", ret);

        return -1;
    }

    printf("all timer is running, press enter to exit\n");
    getchar();

    printf("destroy timer\n");
    gm_timer_destroy(test_timer1);

    return 0;
}
