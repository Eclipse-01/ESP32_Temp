#include "Pages.h" // 假设你的页面管理头文件在这里
#include <Arduino.h>
/**
 * @file NewUserPage1_Hello.c
 * @brief LVGL中使用独立的屏幕并通过过渡动画进行切换的演示。
 *
 * 这段代码演示了如何创建两个独立的屏幕，并使用平滑的滑动动画从一个切换到另一个。
 * 1. 程序启动，加载屏幕1，显示 "Hello!"。
 * 2. 1.5秒后，自动以动画形式加载屏幕2。
 * 3. 屏幕2显示长文本，并在3秒后自动调用函数切换到下一个主页面。
 */

// --- 全局静态变量 ---
static lv_obj_t *screen1; // 指向第一个屏幕的指针
static lv_obj_t *screen2; // 指向第二个屏幕的指针
static lv_obj_t *screen2_label_continue = NULL; // 新增：用于保存“Press Button to Continue”标签指针
static bool button_pressed = false; // 新增：按钮状态变量

// --- 函数前向声明 ---
static void create_screen1(void);
static void create_screen2(void);
static void switch_to_screen2_cb(lv_timer_t *timer);
static void load_next_page_cb(lv_timer_t *timer);
static void fadein_continue_label_cb(lv_timer_t *timer);
static void check_button_cb(lv_timer_t *timer);

/**
 * @brief 创建并启动用户介绍动画序列。
 * 这是此模块的入口函数。
 */
void NewUserPage1_Hello(void)
{
    // 1. 创建两个屏幕
    create_screen1();
    create_screen2();

    // 2. 首先加载屏幕1
    lv_scr_load(screen1);

    // 3. 创建一个一次性定时器，在1500毫秒后触发屏幕切换
    lv_timer_create(switch_to_screen2_cb, 1500, NULL);
}

/**
 * @brief 创建第一个屏幕 ("Hello!")。
 */
static void create_screen1(void)
{
    screen1 = lv_obj_create(NULL); // 创建一个基础对象作为屏幕
    // 为屏幕设置一个干净的白色背景
    lv_obj_set_style_bg_color(screen1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(screen1, LV_OPA_COVER, 0);

    // 创建 "Hello!" 标签
    lv_obj_t *label_hello = lv_label_create(screen1);
    lv_label_set_text(label_hello, "Hello!");
    lv_obj_set_style_text_color(label_hello, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(label_hello, &lv_font_montserrat_48, 0);
    lv_obj_center(label_hello); // 将标签居中于其父对象(screen1)

    // 设置初始透明度为0，实现淡入动画
    lv_obj_set_style_opa(label_hello, LV_OPA_TRANSP, 0);

    // 创建淡入动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_hello);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 800); // 动画时长800ms
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); // 使用平滑的非线性动画
    lv_anim_set_exec_cb(&a, [](void * obj, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), v, 0);
    });
    lv_anim_start(&a);
}

/**
 * @brief 创建第二个屏幕 ("Let's thrill...")。
 */
static void create_screen2(void)
{
    screen2 = lv_obj_create(NULL); // 创建一个基础对象作为屏幕
    // 为屏幕设置一个干净的白色背景
    lv_obj_set_style_bg_color(screen2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(screen2, LV_OPA_COVER, 0);

    // 创建长文本标签
    lv_obj_t *label_magic = lv_label_create(screen2);
    lv_label_set_long_mode(label_magic, LV_LABEL_LONG_WRAP); // 启用自动换行
    lv_obj_set_width(label_magic, lv_disp_get_hor_res(NULL) * 0.9); // 设置宽度
    lv_label_set_text(label_magic, "Let's thrill your life with a little bit magic");
    lv_obj_set_style_text_color(label_magic, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(label_magic, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_align(label_magic, LV_TEXT_ALIGN_CENTER, 0); // 文本居中对齐
    lv_obj_center(label_magic); // 将标签居中于其父对象(screen2)

    // 新增：创建“Press Button to Continue”标签，初始透明度为0，放在屏幕下方
    screen2_label_continue = lv_label_create(screen2);
    lv_label_set_text(screen2_label_continue, "Press Button to Continue");
    lv_obj_set_style_text_color(screen2_label_continue, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(screen2_label_continue, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(screen2_label_continue, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_opa(screen2_label_continue, LV_OPA_TRANSP, 0);
    // 居中于更靠下方（y偏移-8，更小字体）
    lv_obj_align(screen2_label_continue, LV_ALIGN_BOTTOM_MID, 0, -8);
}

// 新增：定时器回调，淡入“Press Button to Continue”标签
static void fadein_continue_label_cb(lv_timer_t *timer)
{
    if(screen2_label_continue) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, screen2_label_continue);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a, 800); // 动画时长800ms
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&a, [](void * obj, int32_t v) {
            lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), v, 0);
        });
        lv_anim_start(&a);
    }
    lv_timer_del(timer);
}

/**
 * @brief 定时器回调：用于启动屏幕切换动画。
 * @param timer 指向触发此回调的定时器的指针。
 */
static void switch_to_screen2_cb(lv_timer_t *timer)
{
    // 使用预设的动画（从右向左滑入）加载屏幕2。
    // 动画时长为500毫秒，没有延迟。
    // 最后一个参数 `true` 表示动画结束后自动删除旧的屏幕(screen1)，释放内存。
    lv_scr_load_anim(screen2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, true);

    // 新增：0.5s后淡入“Press Button to Continue”标签
    lv_timer_create(fadein_continue_label_cb, 500, NULL);

    // 再次创建一个一次性定时器，在3000毫秒后加载下一个主页面。
    // 这个时间足够让切换动画播放完毕，并让用户阅读屏幕上的文字。
    lv_timer_create(load_next_page_cb, 3000, NULL);
    
    // 新增：启动按钮监听定时器（50ms轮询）
    lv_timer_create(check_button_cb, 50, NULL);

    // 删除当前定时器，因为它已经完成了任务
    lv_timer_del(timer);
}

/**
 * @brief 定时器回调：用于加载下一个主页面。
 * @param timer 指向触发此回调的定时器的指针。
 */
static void load_next_page_cb(lv_timer_t *timer)
{
    // 调用你的函数来加载下一个页面。
    // 注意：这只是一个示例函数名，请替换为你项目中实际的函数名。
    WLAN_Setup_Page();
    
    // 删除当前定时器
    lv_timer_del(timer);
}

// 新增：定时器回调，轮询按钮状态
static void check_button_cb(lv_timer_t *timer)
{
    if (!button_pressed && digitalRead(BUTTON_PIN) == LOW) {
        button_pressed = true;
        // 切换到 WLAN 配置页，使用淡入动画前，主动释放screen1/screen2，防止内存泄漏
        if (screen1) {
            lv_obj_del(screen1);
            screen1 = NULL;
        }
        if (screen2) {
            lv_obj_del(screen2);
            screen2 = NULL;
        }
        WLAN_Setup_Page();
    }
}
