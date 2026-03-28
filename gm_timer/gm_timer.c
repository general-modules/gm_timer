/**
 * @file      gm_timer.c
 * @brief     定时器模块源文件
 * @author    huenrong (sgyhy1028@outlook.com)
 * @date      2026-02-16 16:28:22
 *
 * @copyright Copyright (c) 2026 huenrong
 *
 */

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "gm_timer.h"

// 定时器对象
struct _gm_timer_t
{
    timer_t timer_id;           // 定时器 ID
    pthread_mutex_t mutex;      // 互斥锁
    gm_timer_status_e status;   // 定时器状态
    gm_timer_cb timer_cb;       // 定时器回调函数
    uint32_t repeat_count;      // 定时器重复次数 (0: 已停止; 1: 执行一次; UINT32_MAX: 无限循环)
    uint32_t repeat_count_init; // 定时器初始重复次数 (用于重启定时器时，恢复初始值)
    uint32_t timeout_ms;        // 定时器超时时间 (单位: ms)
    const void *user_data;      // 用户数据
};

/**
 * @brief 判断定时器是否可以设置参数
 *
 * @param[in] gm_timer: 定时器对象
 *
 * @return true : 可以
 * @return false: 不可以
 */
static bool gm_timer_can_set_params(const gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return false;
    }

    if ((gm_timer->status == E_GM_TIMER_STATUS_CREATED) || (gm_timer->status == E_GM_TIMER_STATUS_RUNNING) ||
        (gm_timer->status == E_GM_TIMER_STATUS_PAUSED))
    {
        return true;
    }

    return false;
}

/**
 * @brief 将毫秒转换为 itimerspec 结构体
 *
 * @note 该函数只适用于单次定时器
 *
 * @param[in]  ms        : 毫秒数
 * @param[out] timer_spec: 转换后的 itimerspec 结构体
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
static int gm_timer_ms_to_timespec_one_shot(const uint32_t ms, struct itimerspec *timer_spec)
{
    if (timer_spec == NULL)
    {
        return -1;
    }

    // 因为使用 one-shot 模式，这里不设置 it_interval。在回调函数中，根据 repeat_count 决定是否再次启动
    timer_spec->it_interval.tv_sec = 0;
    timer_spec->it_interval.tv_nsec = 0;

    timer_spec->it_value.tv_sec = ms / 1000;
    timer_spec->it_value.tv_nsec = (ms % 1000) * 1000000;

    return 0;
}

/**
 * @brief 定时器回调线程
 *
 * @param[in] sigev_value: 用户自定义参数，在调用timer_create()时传入
 */
static void gm_timer_thread(union sigval sigev_value)
{
    gm_timer_t *gm_timer = (gm_timer_t *)sigev_value.sival_ptr;
    if (gm_timer == NULL)
    {
        return;
    }

    pthread_mutex_lock(&gm_timer->mutex);

    // 已请求销毁，立即销毁
    if (gm_timer->status == E_GM_TIMER_STATUS_DESTROYING)
    {
        timer_delete(gm_timer->timer_id);
        pthread_mutex_unlock(&gm_timer->mutex);
        pthread_mutex_destroy(&gm_timer->mutex);
        free(gm_timer);

        return;
    }

    // 已暂停/已停止的定时器，不执行回调函数
    if ((gm_timer->status == E_GM_TIMER_STATUS_PAUSED) || (gm_timer->status == E_GM_TIMER_STATUS_STOPPED))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return;
    }

    // 先把 repeat_count 减一，防止回调函数会根据该值判断是否是最后一次运行或者是否需要销毁定时器
    if ((gm_timer->repeat_count > 0) && (gm_timer->repeat_count < GM_TIMER_REPEAT_FOREVER))
    {
        gm_timer->repeat_count--;

        // 重复次数已用完，设置定时器状态为已停止
        if (gm_timer->repeat_count == 0)
        {
            gm_timer->status = E_GM_TIMER_STATUS_STOPPED;
        }
    }
    pthread_mutex_unlock(&gm_timer->mutex);

    // 用户回调不加锁，防止在回调函数设置参数等造成死锁
    if (gm_timer->timer_cb != NULL)
    {
        gm_timer->timer_cb(gm_timer);
    }

    // 在回调函数中请求了销毁定时器，立即销毁
    pthread_mutex_lock(&gm_timer->mutex);
    if ((gm_timer->status == E_GM_TIMER_STATUS_DESTROYING))
    {
        timer_delete(gm_timer->timer_id);
        pthread_mutex_unlock(&gm_timer->mutex);
        pthread_mutex_destroy(&gm_timer->mutex);
        free(gm_timer);

        return;
    }

    // 没有请求销毁定时器，则重新启动定时器
    if ((gm_timer->status == E_GM_TIMER_STATUS_RUNNING) && (gm_timer->repeat_count != 0))
    {
        struct itimerspec timer_spec = {0};
        if (gm_timer_ms_to_timespec_one_shot(gm_timer->timeout_ms, &timer_spec) != 0)
        {
            pthread_mutex_unlock(&gm_timer->mutex);

            return;
        }

        timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL);
    }

    pthread_mutex_unlock(&gm_timer->mutex);
}

gm_timer_t *gm_timer_create(void)
{
    gm_timer_t *gm_timer = (gm_timer_t *)malloc(sizeof(gm_timer_t));
    if (gm_timer == NULL)
    {
        return NULL;
    }

    gm_timer->status = E_GM_TIMER_STATUS_CREATED;
    gm_timer->timer_cb = NULL;
    gm_timer->repeat_count = GM_TIMER_REPEAT_FOREVER;
    gm_timer->repeat_count_init = GM_TIMER_REPEAT_FOREVER;
    gm_timer->timeout_ms = 0;
    gm_timer->user_data = NULL;
    pthread_mutex_init(&gm_timer->mutex, NULL);

    return gm_timer;
}

int gm_timer_init(gm_timer_t *gm_timer, const gm_timer_cb timer_cb, const uint32_t repeat_count,
                  const uint32_t timeout_ms, const void *user_data)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);

    // 定时器已初始化，关闭后再初始化
    if (gm_timer->status != E_GM_TIMER_STATUS_CREATED)
    {
        timer_delete(gm_timer->timer_id);
    }

    struct sigevent sig_event = {0};
    sig_event.sigev_notify = SIGEV_THREAD;
    sig_event.sigev_notify_function = gm_timer_thread;
    sig_event.sigev_value.sival_ptr = gm_timer;
    // CLOCK_MONOTONIC: 获取的时间为系统重启到现在的时间, 更改系统时间对其没有影响
    if (timer_create(CLOCK_MONOTONIC, &sig_event, &gm_timer->timer_id) == -1)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (gm_timer_ms_to_timespec_one_shot(timeout_ms, &timer_spec) != 0)
    {
        timer_delete(gm_timer->timer_id);
        pthread_mutex_unlock(&gm_timer->mutex);

        return -3;
    }

    if (timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        timer_delete(gm_timer->timer_id);
        pthread_mutex_unlock(&gm_timer->mutex);

        return -4;
    }

    gm_timer->status = E_GM_TIMER_STATUS_RUNNING;
    gm_timer->timer_cb = timer_cb;
    gm_timer->repeat_count = repeat_count;
    gm_timer->repeat_count_init = repeat_count;
    gm_timer->timeout_ms = timeout_ms;
    gm_timer->user_data = user_data;

    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_destroy(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);

    switch (gm_timer->status)
    {
    // 未初始化，立即销毁(无需销毁 timer_id，因为还未调用 gm_timer_init() 创建)
    case E_GM_TIMER_STATUS_CREATED:
    {
        pthread_mutex_unlock(&gm_timer->mutex);
        pthread_mutex_destroy(&gm_timer->mutex);
        free(gm_timer);

        return 0;
    }

    // 运行中，异步销毁(等待在回调函数中销毁)
    case E_GM_TIMER_STATUS_RUNNING:
    {
        gm_timer->status = E_GM_TIMER_STATUS_DESTROYING;
        pthread_mutex_unlock(&gm_timer->mutex);

        return 1;
    }

    // 已暂停/已停止，立即销毁
    case E_GM_TIMER_STATUS_PAUSED:
    case E_GM_TIMER_STATUS_STOPPED:
    {
        timer_delete(gm_timer->timer_id);
        pthread_mutex_unlock(&gm_timer->mutex);
        pthread_mutex_destroy(&gm_timer->mutex);
        free(gm_timer);

        return 0;
    }

    // 销毁中，忽略
    case E_GM_TIMER_STATUS_DESTROYING:
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return 1;
    }

    default:
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }
    }
}

int gm_timer_set_cb(gm_timer_t *gm_timer, const gm_timer_cb timer_cb)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if (!gm_timer_can_set_params(gm_timer))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    gm_timer->timer_cb = timer_cb;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_set_repeat_count(gm_timer_t *gm_timer, const uint32_t repeat_count)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if (!gm_timer_can_set_params(gm_timer))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    gm_timer->repeat_count = repeat_count;
    gm_timer->repeat_count_init = repeat_count;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_set_timeout(gm_timer_t *gm_timer, const uint32_t timeout_ms)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if (!gm_timer_can_set_params(gm_timer))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (gm_timer_ms_to_timespec_one_shot(timeout_ms, &timer_spec) != 0)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -3;
    }

    if (timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -4;
    }

    gm_timer->timeout_ms = timeout_ms;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_set_user_data(gm_timer_t *gm_timer, const void *user_data)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if (!gm_timer_can_set_params(gm_timer))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    gm_timer->user_data = user_data;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_get_remaining_repeat_count(gm_timer_t *gm_timer, uint32_t *repeat_count)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    if (repeat_count == NULL)
    {
        return -2;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    *repeat_count = gm_timer->repeat_count;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_get_init_repeat_count(gm_timer_t *gm_timer, uint32_t *repeat_count)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    if (repeat_count == NULL)
    {
        return -2;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    *repeat_count = gm_timer->repeat_count_init;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_get_timeout(gm_timer_t *gm_timer, uint32_t *timeout_ms)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    if (timeout_ms == NULL)
    {
        return -2;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    *timeout_ms = gm_timer->timeout_ms;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

const void *gm_timer_get_user_data(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    const void *user_data = gm_timer->user_data;
    pthread_mutex_unlock(&gm_timer->mutex);

    return user_data;
}

int gm_timer_ready(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if ((gm_timer->status != E_GM_TIMER_STATUS_RUNNING) && (gm_timer->status != E_GM_TIMER_STATUS_PAUSED))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    // 因为使用 one-shot 模式，这里不设置 it_interval。在回调函数中，根据 repeat_count 决定是否再次启动
    struct itimerspec timer_spec = {0};
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;
    timer_spec.it_value.tv_sec = 0;
    timer_spec.it_value.tv_nsec = 1;
    if (timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -3;
    }

    gm_timer->status = E_GM_TIMER_STATUS_RUNNING;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_pause(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    // 已暂停的定时器，无需再暂停
    if (gm_timer->status == E_GM_TIMER_STATUS_PAUSED)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return 0;
    }

    // 其它状态仅运行中的定时器可以暂停
    if (gm_timer->status != E_GM_TIMER_STATUS_RUNNING)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -3;
    }

    gm_timer->status = E_GM_TIMER_STATUS_PAUSED;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_resume(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if (gm_timer->status == E_GM_TIMER_STATUS_RUNNING)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return 0;
    }

    if (gm_timer->status != E_GM_TIMER_STATUS_PAUSED)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (gm_timer_ms_to_timespec_one_shot(gm_timer->timeout_ms, &timer_spec) != 0)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -3;
    }

    if (timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -4;
    }

    gm_timer->status = E_GM_TIMER_STATUS_RUNNING;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_stop(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if (gm_timer->status == E_GM_TIMER_STATUS_STOPPED)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return 0;
    }

    if ((gm_timer->status != E_GM_TIMER_STATUS_RUNNING) && (gm_timer->status != E_GM_TIMER_STATUS_PAUSED))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -3;
    }

    gm_timer->status = E_GM_TIMER_STATUS_STOPPED;
    gm_timer->repeat_count = 0;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

int gm_timer_restart(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    if ((gm_timer->status != E_GM_TIMER_STATUS_RUNNING) && (gm_timer->status != E_GM_TIMER_STATUS_PAUSED) &&
        (gm_timer->status != E_GM_TIMER_STATUS_STOPPED))
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (gm_timer_ms_to_timespec_one_shot(gm_timer->timeout_ms, &timer_spec) != 0)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -3;
    }

    if (timer_settime(gm_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&gm_timer->mutex);

        return -4;
    }

    gm_timer->status = E_GM_TIMER_STATUS_RUNNING;
    gm_timer->repeat_count = gm_timer->repeat_count_init;
    pthread_mutex_unlock(&gm_timer->mutex);

    return 0;
}

gm_timer_status_e gm_timer_get_status(gm_timer_t *gm_timer)
{
    if (gm_timer == NULL)
    {
        return E_GM_TIMER_STATUS_NONE;
    }

    pthread_mutex_lock(&gm_timer->mutex);
    gm_timer_status_e status = gm_timer->status;
    pthread_mutex_unlock(&gm_timer->mutex);

    return status;
}

bool gm_timer_is_created(gm_timer_t *gm_timer)
{
    return (gm_timer_get_status(gm_timer) == E_GM_TIMER_STATUS_CREATED);
}

bool gm_timer_is_running(gm_timer_t *gm_timer)
{
    return (gm_timer_get_status(gm_timer) == E_GM_TIMER_STATUS_RUNNING);
}

bool gm_timer_is_paused(gm_timer_t *gm_timer)
{
    return (gm_timer_get_status(gm_timer) == E_GM_TIMER_STATUS_PAUSED);
}

bool gm_timer_is_stopped(gm_timer_t *gm_timer)
{
    return (gm_timer_get_status(gm_timer) == E_GM_TIMER_STATUS_STOPPED);
}
