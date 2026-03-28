/**
 * @file      gm_timer.h
 * @brief     定时器模块头文件
 * @author    huenrong (sgyhy1028@outlook.com)
 * @date      2026-02-16 16:28:29
 *
 * @copyright Copyright (c) 2026 huenrong
 *
 */

#ifndef __GM_TIMER_H
#define __GM_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define GM_TIMER_VERSION_MAJOR 1
#define GM_TIMER_VERSION_MINOR 1
#define GM_TIMER_VERSION_PATCH 0

#define GM_TIMER_REPEAT_ONCE    (1U)         // 定时器执行一次
#define GM_TIMER_REPEAT_FOREVER (UINT32_MAX) // 定时器无限循环

// 定时器状态
typedef enum _gm_timer_status_e
{
    E_GM_TIMER_STATUS_NONE,       // 未创建
    E_GM_TIMER_STATUS_CREATED,    // 已创建但未启动
    E_GM_TIMER_STATUS_RUNNING,    // 运行中
    E_GM_TIMER_STATUS_PAUSED,     // 已暂停
    E_GM_TIMER_STATUS_STOPPED,    // 已停止（重复次数用完后进入该状态)
    E_GM_TIMER_STATUS_DESTROYING, // 销毁中
} gm_timer_status_e;

// 定时器对象
typedef struct _gm_timer_t gm_timer_t;

/**
 * @brief 定时器回调函数
 *
 * @param[in,out] gm_timer: 定时器对象
 */
typedef void (*gm_timer_cb)(gm_timer_t *gm_timer);

/**
 * @brief 创建定时器对象
 *
 * @return 成功: 定时器对象
 * @return 失败: NULL
 */
gm_timer_t *gm_timer_create(void);

/**
 * @brief 初始化定时器对象
 *
 * @note 1. 初始化后，会直接启动定时器，初始化后进入 RUNNING 状态
 *       2. 该函数支持重复调用，重复调用时会重新关闭并重新启动定时器
 *       3. 调用该函数前必须确保没有其它线程正在使用该定时器对象，否则可能导致未定义行为
 *
 * @param[in,out] gm_timer    : 定时器对象
 * @param[in]     timer_cb    : 定时器回调函数
 * @param[in]     repeat_count: 定时器重复次数
 *                              GM_TIMER_REPEAT_ONCE: 执行一次
 *                              GM_TIMER_REPEAT_FOREVER: 无限循环
 * @param[in]     timeout     : 定时器超时时间 (单位: ms)
 * @param[in]     user_data   : 用户数据
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_init(gm_timer_t *gm_timer, const gm_timer_cb timer_cb, const uint32_t repeat_count, const uint32_t timeout,
                  const void *user_data);

/**
 * @brief 销毁定时器对象
 *
 * @note 1. RUNNING 状态的定时器不会立即销毁，而是标记为 DESTROYING，在回调线程中安全销毁，其余状态下的定时器会立即销毁
 *       2. 对已处于 DESTROYING 状态的定时器再次调用不会产生副作用，但不建议这样做
 *       3. 销毁后，定时器对象将不再可用
 *       4. 建议仅在完全不会再使用该定时器对象时才调用该函数，
 *          若仅是暂停或停止定时器，应使用 gm_timer_pause() 或 gm_timer_stop() 函数
 *
 * @param[in,out] gm_timer: 定时器对象
 *
 * @return 1 : 等待异步销毁
 * @return 0 : 已销毁
 * @return <0: 失败
 */
int gm_timer_destroy(gm_timer_t *gm_timer);

/**
 * @brief 设置定时器回调函数
 *
 * @param[in,out] gm_timer: 定时器对象
 * @param[in]     timer_cb: 定时器回调函数
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_set_cb(gm_timer_t *gm_timer, const gm_timer_cb timer_cb);

/**
 * @brief 设置定时器重复次数
 *
 * @param[in,out] gm_timer    : 定时器对象
 * @param[in]     repeat_count: 定时器重复次数
 *                              GM_TIMER_REPEAT_ONCE: 执行一次
 *                              GM_TIMER_REPEAT_FOREVER: 无限循环
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_set_repeat_count(gm_timer_t *gm_timer, const uint32_t repeat_count);

/**
 * @brief 设置定时器超时时间
 *
 * @param[in,out] gm_timer  : 定时器对象
 * @param[in]     timeout_ms: 定时器超时时间 (单位: ms)
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_set_timeout(gm_timer_t *gm_timer, const uint32_t timeout_ms);

/**
 * @brief 设置定时器用户数据
 *
 * @param[in,out] gm_timer : 定时器对象
 * @param[in]     user_data: 用户数据
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_set_user_data(gm_timer_t *gm_timer, const void *user_data);

/**
 * @brief 获取定时器剩余重复次数
 *
 * @param[in,out] gm_timer    : 定时器对象
 * @param[out]    repeat_count: 定时器剩余重复次数
 *                              GM_TIMER_REPEAT_ONCE: 执行一次
 *                              GM_TIMER_REPEAT_FOREVER: 无限循环
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_get_remaining_repeat_count(gm_timer_t *gm_timer, uint32_t *repeat_count);

/**
 * @brief 获取定时器初始重复次数
 *
 * @param[in,out] gm_timer    : 定时器对象
 * @param[out]    repeat_count: 定时器初始重复次数
 *                              GM_TIMER_REPEAT_ONCE: 执行一次
 *                              GM_TIMER_REPEAT_FOREVER: 无限循环
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_get_init_repeat_count(gm_timer_t *gm_timer, uint32_t *repeat_count);

/**
 * @brief 获取定时器超时时间
 *
 * @param[in,out] gm_timer  : 定时器对象
 * @param[out]    timeout_ms: 定时器超时时间 (单位: ms)
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_get_timeout(gm_timer_t *gm_timer, uint32_t *timeout_ms);

/**
 * @brief 获取定时器用户数据
 *
 * @param[in,out] gm_timer: 定时器对象
 *
 * @return 用户数据
 */
const void *gm_timer_get_user_data(gm_timer_t *gm_timer);

/**
 * @brief 设置定时器就绪
 *
 * @note 1. 仅 RUNNING 和 PAUSED 状态的定时器可以就绪
 *       2. 本次立即执行，后续根据定时器超时时间执行
 *       3. 若定时器为有限次数模式，本次触发会消耗一次重复次数
 *
 * @param[in,out] gm_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_ready(gm_timer_t *gm_timer);

/**
 * @brief 暂停定时器
 *
 * @note 1. 仅 RUNNING 状态可暂停，暂停后进入 PAUSED 状态
 *       2. 对已处于 PAUSED 状态的定时器再次调用不会产生副作用
 *       3. 暂停期间不会触发回调
 *       4. 暂停后的定时器可通过 gm_timer_resume() 恢复运行
 *
 * @param[in,out] gm_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_pause(gm_timer_t *gm_timer);

/**
 * @brief 恢复定时器
 *
 * @note 1. 仅 PAUSED 状态的定时器可以恢复，恢复后进入 RUNNING 状态
 *       2. 对已处于 RUNNING 状态的定时器再次调用不会产生副作用
 *
 * @param[in,out] gm_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_resume(gm_timer_t *gm_timer);

/**
 * @brief 停止定时器
 *
 * @note 1. 仅 RUNNING 和 PAUSED 状态的定时器可以停止，停止后进入 STOPPED 状态
 *       2. 对已处于 STOPPED 状态的定时器再次调用不会产生副作用
 *       3. 停止操作不会释放定时器资源
 *       4. 停止后定时器不再触发回调，直到重新启动
 *       5. 停止后的定时器可通过 gm_timer_init() 或者 gm_timer_restart() 重新启动
 *
 * @param[in,out] gm_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_stop(gm_timer_t *gm_timer);

/**
 * @brief 重启定时器
 *
 * @note 1. 重置定时器的重复次数为初始值，并重新启动定时器
 *       2. RUNNING、 PAUSED、STOPPED 状态的定时器均可重启，重启后进入 RUNNING 状态
 *
 * @param[in,out] gm_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int gm_timer_restart(gm_timer_t *gm_timer);

/**
 * @brief 获取定时器状态
 *
 * @param[in] gm_timer: 定时器对象
 *
 * @return 定时器状态
 */
gm_timer_status_e gm_timer_get_status(gm_timer_t *gm_timer);

/**
 * @brief 定时器是否处于已创建状态
 *
 * @param[in] gm_timer: 定时器对象
 *
 * @return true : 已创建
 * @return false: 定时器未初始化，发生错误
 */
bool gm_timer_is_created(gm_timer_t *gm_timer);

/**
 * @brief 定时器是否运行中
 *
 * @param[in] gm_timer: 定时器对象
 *
 * @return true : 运行中
 * @return false: 定时器未初始化，发生错误，未运行
 */
bool gm_timer_is_running(gm_timer_t *gm_timer);

/**
 * @brief 定时器是否已暂停
 *
 * @param[in] gm_timer: 定时器对象
 *
 * @return true : 已暂停
 * @return false: 定时器未初始化，发生错误，未暂停
 */
bool gm_timer_is_paused(gm_timer_t *gm_timer);

/**
 * @brief 定时器是否已停止
 *
 * @param[in] gm_timer: 定时器对象
 *
 * @return true : 已停止
 * @return false: 定时器未初始化，发生错误，未停止
 */
bool gm_timer_is_stopped(gm_timer_t *gm_timer);

#ifdef __cplusplus
}
#endif

#endif // __GM_TIMER_H
