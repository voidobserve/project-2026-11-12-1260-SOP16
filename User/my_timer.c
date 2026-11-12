#include "include.h"

// 1ms
void timer0_init()
{
    __EnableIRQ(TMR0_IRQn); // 使能timer0中断
    IE_EA = 1;              // 使能总中断

#define PEROID_VAL (SYSCLK / 128 / 1000 - 1) // 周期值=系统时钟/分频/频率 - 1
    // 设置timer0的计数功能，配置一个频率为1kHz的中断
    TMR_ALLCON = TMR0_CNT_CLR(0x1);                        // 清除计数值
    TMR0_PRH = TMR_PERIOD_VAL_H((PEROID_VAL >> 8) & 0xFF); // 周期值
    TMR0_PRL = TMR_PERIOD_VAL_L((PEROID_VAL >> 0) & 0xFF);
    TMR0_CONH =
        TMR_PRD_PND(0x1) | TMR_PRD_IRQ_EN(0x1); // 计数等于周期时允许发生中断
    TMR0_CONL = TMR_SOURCE_SEL(0x7) | TMR_PRESCALE_SEL(0x7) |
                TMR_MODE_SEL(0x1); // 选择系统时钟，128分频，计数模式
}

extern void cap_timer(void);
extern void l_pwr_timer_handler(void);
extern void t_ctrl_timer_handler(void);
extern void dly_pwr_on_handler(void);
extern void ex_temp_adjust_timer();
extern void adc_val();

void TIMR0_IRQHandler(void) interrupt TMR0_IRQn
{
    static volatile u16 ms_cnt = 0;
    // 进入中断设置IP，不可删除
    __IRQnIPnPush(TMR0_IRQn);

    // ---------------- 用户函数处理 -------------------

    // 周期中断
    if (TMR0_CONH & TMR_PRD_PND(0x1)) {
        TMR0_CONH |= TMR_PRD_PND(0x1); // 清除pending
        // 目前 1ms 进入一次

        cap_timer();
        adc_val();
        ms_cnt++;
        if (ms_cnt >= 1000) {
            ms_cnt = 0;
            ex_temp_adjust_timer(); //过温降功率 max值由过温的决定
            t_ctrl_timer_handler();
            l_pwr_timer_handler();
            dly_pwr_on_handler();
        }
    }

    // 退出中断设置IP，不可删除
    __IRQnIPnPop(TMR0_IRQn);
}

void pwm_init(void)
{

#define STMR0_PEROID_VAL (SYSCLK / 8000 - 1)
    STMR0_PSC = STMR_PRESCALE_VAL(0x07);
    STMR0_PRH = STMR_PRD_VAL_H((STMR0_PEROID_VAL >> 8) & 0xFF);
    STMR0_PRL = STMR_PRD_VAL_L((STMR0_PEROID_VAL >> 0) & 0xFF);
    STMR0_CMPAH = STMR_CMPA_VAL_H(((0) >> 8) & 0xFF); //比较值
    STMR0_CMPAL = STMR_CMPA_VAL_L(((0) >> 0) & 0xFF); //比较值
    //STMR_PWMVALA &= ~STMR_0_PWMVALA(0x1);
    STMR_PWMVALA |= STMR_0_PWMVALA(0x1);

    STMR_CNTMD |= STMR_0_CNT_MODE(0x1); //连续计数模式
    STMR_LOADEN |= STMR_0_LOAD_EN(0x1); //自动装载使能
    STMR_CNTCLR |= STMR_0_CNT_CLR(0x1); //
    STMR_CNTEN |= STMR_0_CNT_EN(0x1);   //使能
    STMR_PWMEN |= STMR_0_PWM_EN(0x1);   // PWM输出使能
    P1_MD1 &= ~GPIO_P16_MODE_SEL(0x03);
    P1_MD1 |= GPIO_P16_MODE_SEL(0x01);
    FOUT_S16 = GPIO_FOUT_STMR0_PWMOUT;
}
