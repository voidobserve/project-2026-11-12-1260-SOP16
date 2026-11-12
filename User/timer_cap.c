#include "include.h"

u16 cap0_val = 0;
u16 cap1_val = 0; //  捕获源的占空比=(cap1_val+1)/(cap0_val+1)*100%

void timer4_cap_init(void)
{
    // 配置P14输入模式，CAP0_PIN--P14
    P1_MD1 &= ~GPIO_P14_MODE_SEL(0x03);
    FIN_S4 = GPIO_FIN_SEL_P14; // tmr4_cap0_pin 输入功能pin脚选择P00

    // TIMER4配置捕获功能
    __EnableIRQ(TMR4_IRQn);              // 打开TIMER4模块中断
    TMR_ALLCON = TMR4_CNT_CLR(0x1);      // Timer4 计数清零
    TMR4_CAP10 = TMR4_PRD_VAL_L(0xFF);   // 周期低八位寄存器
    TMR4_CAP11 = TMR4_PRD_VAL_H(0xFF);   // 周期高八位寄存器
    TMR4_CON0 = TMR4_PRESCALE_SEL(0x7) | // 128分频
                TMR4_SOURCE_SEL(0x7) |   // 计数源选择系统时钟
                TMR4_MODE_SEL(0x2);      // 捕获模式
    TMR4_CON1 = TMR4_CAP_SRC_SEL(0x0) |  // 捕获信号源选择GPIO,CAP = CAP0_PIN
                TMR4_SYNC_OUT_SEL(0x0);  // 同步输出信号选择计数等于周期
    TMR4_CON2 = TMR4_CAP0_POL(0x0) |     // 1级捕获上升沿
                TMR4_CAP1_POL(0x1) |     // 2级捕获下升沿
                TMR4_CTRRST0_EN(0x1);    // 1级捕获信号有效时,计数复位
    TMR4_CON3 = TMR4_CAP_CLASS(0x1);     // 捕获有效级数2级
    TMR4_IE0 = TMR4_CAP0_IRQ_EN(0x1) |   // 使能1级捕获信号有效中断
               TMR4_CAP1_IRQ_EN(0x1);    // 使能2级捕获信号有效中断
    TMR4_EN = TMR4_EN(0x1);              // 使能定时器
    IE_EA = 1;                           // 使能总中断
}

void TMR4_IRQHandler(void) interrupt TMR4_IRQn
{
    // 进入中断设置IP，不可删除
    __IRQnIPnPush(TMR4_IRQn);

    // ---------------- 用户函数处理 -------------------
    // 2级捕获中断
    if (TMR4_FLAG0 & TMR4_CAP1_FLAG(0x1)) {
        TMR4_CLR0 |= TMR4_CAP1_FLAG(0x1);

        cap1_val = (TMR4_CAP21 << 8) | TMR4_CAP20;
    }
    // 1级捕获中断
    if (TMR4_FLAG0 & TMR4_CAP0_FLAG(0x1)) {
        TMR4_CLR0 |= TMR4_CAP0_FLAG(0x1);

        cap0_val = (TMR4_CAP11 << 8) | TMR4_CAP10;
    }

    // 退出中断设置IP，不可删除
    __IRQnIPnPop(TMR4_IRQn);
}
