#include "include.h"
#include "main.h"
#include <stdio.h>

#include "user_config.h"

extern void pwm_in_handler(void);
extern u16 ntc_val;
extern u16 adc0_val;
extern u16 adc1_val;

// 输出占空比100%时，对应的占空比参数
#define MAX_DUTY (6000)
// 开机缓启动时长：0-100% 固定 3s（需求指定）
#define PWR_ON_RAMP_MS 3000
#define PER_DUTY (10) //
#define _75_DUTY 4500 // 75%亮度
#define _70_DUTY 4200
#define _60_DUTY 3600
#define _50_DUTY 3000
#define _30_DUTY 1800

#define _75_TEMP     2420          // 75摄氏度对应的ADC值
#define _3_1V        3000          // 3.4V对应的ADC值
#define L_PWR_T      (1 * 60 * 60) // 降功率
#define L_TEMP_PWR_T (1 * 30 * 60) // 过温半小时降功率
// #define LEVLE_PER 184              // 每个档位对应的AD值 （20级挡位调节）
// #define LEVLE_PER 92 // 每个档位对应的AD值 (40级挡位调节)
#define LEVLE_PER   23  // 每个档位对应的AD值 (160级挡位调节)
#define ADJUST_STEP 160 // 调节级数，例如 20级、40级、160级
#define DEAD_ZONE   5   // adc死区范围± 5

bit ex_temp_en; // 过温降功率标志位

typedef struct
{
    u32 t;    // 计时器
    u8 level; // 时空列表执行的哪一个时间段
    u8 list;  // 时控列表，5个列表
    u8 en;    // 使能
} t_ctrl_t;   // 时控结构体

typedef struct
{
    u8 en;
    u8 act; // 动作，执行降功率动作：1
    u32 t;  // 秒

} l_pwr_t; // 降功率结构体

typedef struct
{
    u8 en;
} _3_1_t; // 三合一结构体

u8 cap_10ms_cnt; // 10ms获取一次捕获占空比
u8 pwr_on_cnt;
static u32 pwr_on_ramp_acc; // 开机缓启动计时(ms)，配合 pwr_on_cnt 累加
u16 c_duty = 0;      // 当前设置的占空比
u32 pwm_in_duty = 0; // 三合一PWM输入的占空比
u32 pwm_in_duty_xuwei = 0;
u32 ex_pwm_in_duty = 0;
u16 ex_max_duty = 0;
u16 max_duty = 6000; // 可调的最大占空比
t_ctrl_t t_ctrl;
l_pwr_t l_pwr;
_3_1_t _3_1;
bit dly_pwr_on = 0;
u8 dly_pwr_on_cnt = 0;

/*
    标志位
    8脚接了 10K上拉电阻，0 ~ 100K可调下拉电阻，
    表示 60% - 100%可调占空比使能，
    8脚没有接 10K上拉电阻，
    表示 不使能 60% - 100%可调占空比，默认使用 100 %占空比
*/
bit adjust_en;

// 时控模式0：100%6H 50%6H 0%
// 时控模式1：100%4H 50%8H 0%
// 时控模式2：100%5H 70%3H 50%4H 0%
// 时控模式3：100%4H 70%4H 50%4H 0%
// 时控模式4：100%4H 60%2H 30%3H 60%2H 100%1H 0%
void flag_init();
typedef struct
{
    u16 b; // 当前亮度
    u8 t;  // 当前亮度保持时间,255时控结束
} t_ctrl_list_t;

t_ctrl_list_t t_ctrl_lis_0[3] = {
    {MAX_DUTY, 6}, {_50_DUTY, 6}, {0, 255} // 6 6
};

t_ctrl_list_t code t_ctrl_lis_1[3] = {{MAX_DUTY, 4}, {_50_DUTY, 8}, {0, 255}};

t_ctrl_list_t code t_ctrl_lis_2[4] = {
    {MAX_DUTY, 5}, {_70_DUTY, 3}, {_50_DUTY, 4}, {0, 255} // 534
};

t_ctrl_list_t code t_ctrl_lis_3[4] = {
    {MAX_DUTY, 4}, {_70_DUTY, 4}, {_50_DUTY, 4}, {0, 255} // 444
};

t_ctrl_list_t code t_ctrl_lis_4[6] = {
    {MAX_DUTY, 4}, {_60_DUTY, 2}, {_30_DUTY, 3},
    {_60_DUTY, 2}, {MAX_DUTY, 1}, {0, 255} // 42321
};

t_ctrl_list_t *p_list;

extern u16 cap0_val;
extern u16 cap1_val;

extern void led_pwr_on(void);
extern void cmp_duty(void);
extern void led_plus(void);
extern void led_sub(void);
extern void cal_pwm_in_duty(void);

extern u16 adjust_val; // 电位器的ADC值 0~3686---->60%~100%---->3600~6000
u16 limited_pwm_duty_val_by_knob;
// 20档电位器调节pwm   60%-62%-64%-66%- 68%-70%-72%-74%-76%6-7896-80%--82. 5%-85%-87.5%-90%-92%-94%-96%-98%-100%
// 修改为40档电位器调节 pwm
void adjust_pwm()
{
    static u16 last_level = 0xFF;           // 记录之前的挡位
    u16 cur_level = adjust_val / LEVLE_PER; // 计算当前挡位

    if (0 == adjust_en) {
        // 电位器调节没有使能，直接返回
        return;
    }

    if (adjust_val > 3680) // 超出了4.5V对应的AD值
    {
        limited_pwm_duty_val_by_knob = 6000;
        last_level = cur_level;
        return;
    }

    if (0 == cur_level) // 最小挡位
    {
        limited_pwm_duty_val_by_knob = 3600;
        last_level = cur_level;
        return;
    }

    if (cur_level >= ADJUST_STEP) // 最大挡位
    {
        limited_pwm_duty_val_by_knob = 6000;
        last_level = cur_level;
        return;
    }

    if (last_level == cur_level) {
        // 之前的挡位和当前计算得出的挡位一样，不调节
        return;
    }

    limited_pwm_duty_val_by_knob =
        3600 + cur_level * ((6000 - 3600) / ADJUST_STEP);
    last_level = cur_level;
}

//
u16 ex_temp_t = 0;
u8 ex_temp_cnt = 0; // 过温降功率处理次数，第一次降到90% 第二次80%-----
u8 ex_temp_state = 1;

u16 pwm_save[10] = 0;
u16 ex_duty = 0;
bit extemp_flag = 0;
bit extemp_tctrl_flag = 0;
void ex_temp_adjust_timer() // 过温降功率 定时判断 每秒执行一次
{
    if (ex_temp_en == 1 && t_ctrl.en == 0) {
        if (extemp_tctrl_flag == 1) {
            pwm_save[0] = max_duty;
            ex_temp_cnt = 0;
            extemp_tctrl_flag = 0;
        }

        if (ntc_val < _3_1V && ex_temp_state == 1) {
            // 半小时处理一次 AD值越小 温度越高
            pwm_save[ex_temp_cnt] = pwm_in_duty; // 用数组存住第一个值

            // 第一次降到90%，之后每次将10%
            ex_duty = pwm_save[0] - (pwm_save[0] / 10 * (ex_temp_cnt + 1));
            ex_temp_state = 0; // 处理完成 半小时后再开启
            ex_temp_t = 0;

            max_duty = ex_duty;
            ex_temp_cnt++;
            if (ex_temp_cnt > 9) {
                max_duty = 0;
            }
            extemp_flag = 1;
        }

        // 温度降下来后 不干预最大值 并且计数置0 下次过温再从90%开始
        if (ntc_val > _3_1V) {
            ex_temp_cnt = 0;
            if (extemp_flag == 1 && ex_temp_state == 1) {
                if (extemp_tctrl_flag == 0) {
                    if (adjust_en == 1) {
                        if (_3_1.en == 1) {
                            max_duty = pwm_in_duty_xuwei;
                        } else {
                            max_duty = limited_pwm_duty_val_by_knob;
                        }
                    }
                    if (adjust_en == 0) {
                        if (_3_1.en == 1) {
                            max_duty = ex_pwm_in_duty;
                        } else {
                            max_duty = pwm_save[0];
                        }
                    }
                    if (P00 == 0) {
                        if (max_duty > ex_max_duty) {
                            max_duty = ex_max_duty;
                        }
                    }
                }
                if (extemp_tctrl_flag == 1) {
                    extemp_tctrl_flag = 0;
                    extemp_flag = 0;
                    ex_temp_cnt = 0;
                    return;
                }
            }
        }
        ex_temp_t++;
        if (ex_temp_t == L_TEMP_PWR_T) {
            ex_temp_t = 0; // 半小时计数清零
            ex_temp_state = 1;
            // extemp_flag=0;
        }
    }
}

void set_pwm_duty(void)
{
    u16 duty_tmp;
    duty_tmp = MAX_DUTY - c_duty;
    STMR0_CMPAH = STMR_CMPA_VAL_H(((c_duty) >> 8) & 0xFF); // 比较值
    STMR0_CMPAL = STMR_CMPA_VAL_L(((c_duty) >> 0) & 0xFF); // 比较值
    STMR_LOADEN |= STMR_0_LOAD_EN(0x1);
}

u8 adjust_ms = 0;
void cap_timer(void)
{
    if (cap_10ms_cnt < 10) {
        cap_10ms_cnt++;
    }
    pwr_on_cnt++;
    adjust_ms++;
}

// 放在while,没10ms获取一次PWM占空比
// 连续获取5次，占空比稳定才输出
u16 duty_buff[5];
u8 duty_cnt = 0;
// 上电采样5次完成标志位
u8 simple_over = 0;
// 放主循环
void get_duty(void)
{
    // 10ms获取一次
    if (cap_10ms_cnt >= 10) {
        cap_10ms_cnt = 0;
        if (_3_1.en == 1) // 三合一功能开启才调光
        {
            // 非三合一不会更新pwm_in_duty
            cal_pwm_in_duty();
        }
        // 处理降功率，时控激活，限制可调的占空比范围
        // 延时开机模式，强制pwm_in_duty=0

        pwm_in_handler();
        cmp_duty();
    }

    if (adjust_ms == 3) {
        adjust_ms = 0;
        led_plus();
        led_sub();
    }

    led_pwr_on();
}

// 计算三合一的占空比,储存pwm_in_duty
void cal_pwm_in_duty(void)
{
    u8 i;
    if (cap1_val == 0 && cap0_val == 0) {
        if (P14 == 0) {
            // 占空比 0
            pwm_in_duty = 0;
        }
        if (P14 == 1) {
            pwm_in_duty = 6000;
        }
    } else {
        pwm_in_duty = (u32)MAX_DUTY * (cap1_val + 1);
        pwm_in_duty = pwm_in_duty / (cap0_val + 1);
    }

    cap1_val = 0;
    cap0_val = 0;
    duty_buff[duty_cnt] = pwm_in_duty;
    duty_cnt++;
    if (duty_cnt == 5) {
        simple_over = 1;
        duty_cnt = 0;
    }
    if (simple_over == 1) {
        pwm_in_duty = 0;
        for (i = 0; i < 5; i++) {
            pwm_in_duty += duty_buff[i];
        }
        // 求平均
        pwm_in_duty = pwm_in_duty / 5;
    }

    if (adjust_en == 1) {
        pwm_in_duty_xuwei = pwm_in_duty * limited_pwm_duty_val_by_knob / 6000;
        pwm_in_duty = pwm_in_duty_xuwei;
    }
    ex_pwm_in_duty = pwm_in_duty;
}

// 对比当前的duty是否发生变化
void cmp_duty(void)
{
    if (led_state == LED_PWR_ON) {
        return;
    }

    if (pwm_in_duty > 10) {
        if ((pwm_in_duty - 10) > c_duty) {
            led_state = LED_ADJ_P;
        }
        if ((pwm_in_duty + 10) < c_duty) {
            led_state = LED_ADJ_S;
        }
    } else {
        if (pwm_in_duty > c_duty) {
            led_state = LED_ADJ_P;
        }
        if (pwm_in_duty < c_duty) {
            led_state = LED_ADJ_S;
        }
    }
}

void debug_putchar(u8 uart_data);

// 开机缓启动：1ms 调用一次，0 -> 目标占空比在 PWR_ON_RAMP_MS 内线性均匀上升
void led_pwr_on(void)
{
    if (0 == (led_state == LED_PWR_ON && simple_over == 1)) {
        pwr_on_cnt = 0;
        pwr_on_ramp_acc = 0; // 非缓启动阶段清零，保证下次上电从 0 开始
        return;
    }

    // 没有电位器时
    // 没有三合一功能，上电默认最大占空比
    // 有三合一功能，按采用的占空比调光
    if (adjust_en == 0) {
        if (_3_1.en == 0) {
            pwm_in_duty = max_duty;
        }
    } else if (adjust_en == 1) {
        // 有没有三合一功能，上电都默认电位器的最大占空比
        if (_3_1.en == 0) {
            pwm_in_duty = limited_pwm_duty_val_by_knob;
        }

        if (_3_1.en == 1) {
            pwm_in_duty = pwm_in_duty_xuwei;
        }
    }

    pwr_on_ramp_acc += pwr_on_cnt; // 1ms 节拍累计缓启动时间
    pwr_on_cnt = 0;
    if (pwr_on_ramp_acc >= PWR_ON_RAMP_MS) {
        c_duty = pwm_in_duty;
        led_state = LED_NULL;
    } else {
        // 线性均匀上升：固定 PWR_ON_RAMP_MS 内从 0 到目标占空比
        c_duty = (u16)((u32)pwm_in_duty * pwr_on_ramp_acc / PWR_ON_RAMP_MS);
    }

    set_pwm_duty();
}

void led_plus(void)
{
    if (led_state != LED_ADJ_P) {
        return;
    }

    c_duty = pwm_in_duty; // 屏蔽缓升，直接输出目标占空比
    led_state = LED_NULL;
    set_pwm_duty();
}

void led_sub(void)
{
    if (led_state != LED_ADJ_S)
        return;

    c_duty = pwm_in_duty; // 屏蔽缓降，直接输出目标占空比
    led_state = LED_NULL;
    set_pwm_duty();
}

// 有没有降功率
#define LOW_PWR_R7 P00
// 有没有三合一
#define _3_1_R14 P02
// 有没有时空
#define T_C_R11 P03

#define T_C_R8  P13
#define T_C_R9  P12
#define T_C_R10 P11

// 降功率、时空、三合一功能
void option_fun(void)
{
    flag_init(); // 初始化变量
    // p13输入上拉，时空组合R8
    P1_MD0 &= ~(GPIO_P13_MODE_SEL(0x3));
    P1_PU |= (GPIO_P13_PULL_UP(0x1));
    P1_PD |= (GPIO_P13_PULL_PD(1));
    //   时空组合R9
    P1_MD0 &= ~(GPIO_P12_MODE_SEL(0x3));
    P1_PU |= (GPIO_P12_PULL_UP(0x1));
    // 时空组合R10
    P1_MD0 &= ~(GPIO_P11_MODE_SEL(0x3));
    P1_PU |= (GPIO_P11_PULL_UP(0x1));
    // 降功率有/无 R7
    P0_MD0 &= ~(GPIO_P00_MODE_SEL(0x3));
    P0_PU |= (GPIO_P00_PULL_UP(0x1));
    // 三合一有/无 R14
    P0_MD0 &= ~(GPIO_P02_MODE_SEL(0x3));
    P0_PU |= (GPIO_P02_PULL_UP(0x1));
    // 时空有/无 R11
    P0_MD0 &= ~(GPIO_P03_MODE_SEL(0x3));
    P0_PU |= (GPIO_P03_PULL_UP(0x1));

    if (LOW_PWR_R7 == 0) // 低电平有降功率
    {
        l_pwr.en = 1;
        // printf("l_pwr.en = 1\r\n");
    } else {
        l_pwr.en = 0;
        // printf("l_pwr.en = 0\r\n");
    }
    if (adc0_val > 4000) // LIN 改 加入了ntc和电位器AD值的判断 ntc和电位器都上拉
    {
        ex_temp_en = 0;
        // printf("ex_temp_en = 0\r\n");
    } else {
        ex_temp_en = 1;
        // printf("ex_temp_en = 1\r\n");
    }

    /*
        8脚接了 10K上拉电阻，0 ~ 100K可调下拉电阻，
        表示 60% - 100%可调占空比使能，
        8脚没有接 10K上拉电阻，
        表示 不使能 60% - 100%可调占空比，默认使用 100 %占空比
    */
    if (adc1_val > 4000) {
        adjust_en = 0;
        // printf("adjust_en = 0\r\n");
    } else {
        adjust_en = 1;
        // printf("adjust_en = 1\r\n");
    }

    /*
        引脚配置为上拉输入，检测到高电平，则表示有3合一功能
        检测到低电平，说明外部接了 0 R 的下拉电阻，表示没有3合一功能
    */
    if (_3_1_R14 == 1) {
        _3_1.en = 1;
        // printf("_3_1.en  = 1\r\n");
    } else {
        _3_1.en = 0;
        simple_over = 1;
        // printf("_3_1.en  = 0\r\n");
    }

    // 低电平时空
    if (T_C_R11 == 0) {
        t_ctrl.en = 1;
        // printf("t_ctrl.en = 1\r\n");
    } else {
        t_ctrl.en = 0;
        // printf("t_ctrl.en = 0\r\n");
    }

    // 时控模式0：100%6H 50%6H 0%
    // 时控模式1：100%4H 50%8H 0%
    // 时控模式2：100%5H 70%3H 50%4H 0%
    // 时控模式3：100%4H 70%4H 50%4H 0%
    // 时控模式4：100%4H 60%2H 30%3H 60%2H 100%1H 0%

    if (T_C_R8 == 1 && T_C_R9 == 1 && T_C_R10 == 1) {
        t_ctrl.list = 0;
        p_list = &t_ctrl_lis_0;
        // printf("t_ctrl.list = 0 \r\n");
    } else if (T_C_R8 == 0 && T_C_R9 == 1 && T_C_R10 == 1) {
        t_ctrl.list = 1;
        p_list = &t_ctrl_lis_1;
        // printf("t_ctrl.list = 1\r\n ");
    } else if (T_C_R8 == 1 && T_C_R9 == 0 && T_C_R10 == 1) {
        t_ctrl.list = 2;
        p_list = &t_ctrl_lis_2;
        // printf("t_ctrl.list = 2\r\n ");
    } else if (T_C_R8 == 1 && T_C_R9 == 1 && T_C_R10 == 0) {
        t_ctrl.list = 3;
        p_list = &t_ctrl_lis_3;
        // printf("t_ctrl.list = 3\r\n ");
    } else if (T_C_R8 == 0 && T_C_R9 == 0 && T_C_R10 == 1) {
        t_ctrl.list = 4;
        p_list = &t_ctrl_lis_4;
        // printf("t_ctrl.list = 4\r\n");
    }
}

// 降功率计时处理
void l_pwr_timer_handler(void)
{
    if (l_pwr.en == 1) {
        if (l_pwr.t < L_PWR_T) // 小时
        {
            l_pwr.t++;
        } else {
            extemp_tctrl_flag = 1;
            if (adjust_en == 1) // 电位器使能
            {
                if (_3_1.en == 1) // 有三合一功能和电位器
                {
                    // 判断PWM_in占空比大于74
                    if (pwm_in_duty_xuwei >
                        limited_pwm_duty_val_by_knob / 100 *
                            75) // LIN 改  75%改成电位器的75%
                    {
                        max_duty = limited_pwm_duty_val_by_knob / 100 * 75;
                        l_pwr.act = 1; // 降功率状态
                        l_pwr.en = 0;  // 处理完成，使能降功率
                    }
                } else {
                    max_duty = limited_pwm_duty_val_by_knob / 100 * 75;
                    l_pwr.act = 1; // 降功率状态
                    l_pwr.en = 0;  // 处理完成，使能降功率
                }
            }

            if (adjust_en == 0) // 无电位器
            {
                if (_3_1.en) // 有三合一功能
                {
                    // 判断PWM_in占空比大于74
                    if (pwm_in_duty > _75_DUTY) {
                        max_duty = _75_DUTY;
                        l_pwr.act = 1; // 降功率状态
                        l_pwr.en = 0;  // 处理完成，使能降功率
                    }
                } else {
                    max_duty = _75_DUTY;
                    l_pwr.act = 1; // 降功率状态
                    l_pwr.en = 0;  // 处理完成，使能降功率
                }
            }
            l_pwr.en = 0;
            ex_max_duty = max_duty;
        }
    }
}

// 处理降功率，时控时，可调的占空比范围
// 延时开机模式，强制pwm_in_duty=0
void pwm_in_handler(void)
{
    if (t_ctrl.en == 0 && adjust_en == 1) {
        if (_3_1.en == 1) {
            pwm_in_duty = pwm_in_duty_xuwei;
        } else {
            pwm_in_duty = limited_pwm_duty_val_by_knob;
        }
    }
    if (l_pwr.act == 1 || t_ctrl.en == 1) {
        // 进入降功率模式

        if (adjust_en == 1 && _3_1.en == 1) {

            if (pwm_in_duty_xuwei > max_duty) {
                pwm_in_duty_xuwei = max_duty;
            }
            pwm_in_duty = pwm_in_duty_xuwei;
        } else {
            if (pwm_in_duty > max_duty) {
                pwm_in_duty = max_duty;
            }
        }
    }
    if (ex_temp_en == 1 && t_ctrl.en == 0) {
        if (adjust_en == 0 && _3_1.en == 0) {
            pwm_in_duty = max_duty;
        }
        if (pwm_in_duty > max_duty) {
            pwm_in_duty = max_duty;
        }
        if (max_duty == 0) {
            pwm_in_duty = 0;
        }
    }
}

// 1s调用一次
void t_ctrl_timer_handler(void)
{
    if (t_ctrl.en == 1) {
        // 当前列表时间非255，如果是255则说明时空结束
        if (p_list[t_ctrl.level].t != 255) {
            t_ctrl.t++;
            if (t_ctrl.t >= (p_list[t_ctrl.level].t * 60 * 60)) // 改成10s
            {
                t_ctrl.level++;
                t_ctrl.t = 0;
                max_duty = p_list[t_ctrl.level].b;
                extemp_tctrl_flag = 1;

                if (adjust_en == 1) {
                    switch (max_duty) {
                    case 6000:
                        max_duty = limited_pwm_duty_val_by_knob;
                        break;
                    case 4200:
                        max_duty = limited_pwm_duty_val_by_knob / 10 * 7;
                        break;
                    case 3600:
                        max_duty = limited_pwm_duty_val_by_knob / 10 * 6;
                        break;
                    case 3000:
                        max_duty = limited_pwm_duty_val_by_knob / 2;
                        break;
                    case 1800:
                        max_duty = limited_pwm_duty_val_by_knob / 10 * 3;
                        break;
                    default:
                        break;
                    }
                }

                if (_3_1.en == 1 && ex_temp_en == 1 && t_ctrl.en == 0) {
                    if (ex_pwm_in_duty < max_duty) {
                        max_duty = ex_pwm_in_duty;
                    }
                }

                // 修复t_ctrl_lis_4，功率降到30%升不了60%
                pwm_in_duty = max_duty;
                if (adjust_en == 1 && _3_1.en == 1) {
                    pwm_in_duty_xuwei = max_duty;
                }
            }
        }
    }
}

// 1秒调用一次
extern u8 lvd_mode;
extern volatile u8 lvd_flag;
void dly_pwr_on_handler(void)
{
    if (dly_pwr_on == 1) {
        // 延时启动
        // user_printf("lvd pwr low \n");
        dly_pwr_on_cnt++;
        if (dly_pwr_on_cnt >= 5) {
            lvd_flag = 0;
            dly_pwr_on_cnt = 0;

            dly_pwr_on = 0;

            c_duty = 0;
            max_duty = MAX_DUTY;

            t_ctrl.list = 0;
            t_ctrl.t = 0;
            t_ctrl.en = 0;
            t_ctrl.level = 0;

            l_pwr.en = 0;
            l_pwr.act = 0;
            l_pwr.t = 0;

            _3_1.en = 0;
            pwm_in_duty = 0;
            ex_temp_en = 0;
            adjust_en = 0;
            option_fun();

            led_state = LED_PWR_ON;
#if USER_DEBUG_ENABLE
// printf("LED_PWR_ON\n");
#endif
        }
    }
}
void flag_init()
{
    lvd_flag = 0;
    dly_pwr_on_cnt = 0;

    dly_pwr_on = 0;

    c_duty = 0;
    max_duty = MAX_DUTY;

    t_ctrl.list = 0;
    t_ctrl.t = 0;
    t_ctrl.en = 0;
    t_ctrl.level = 0;

    l_pwr.en = 0;
    l_pwr.act = 0;
    l_pwr.t = 0;

    _3_1.en = 0;
    pwm_in_duty = 0;
    ex_temp_en = 0;
    adjust_en = 0;
}