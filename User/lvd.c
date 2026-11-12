#include "include.h"

volatile u8 lvd_flag;
volatile u8 lvd_mode = 0;
extern bit dly_pwr_on;
void LVD_IRQHandler(void) interrupt LVD_IRQn
{
    // 进入中断设置IP，不可删除
    __IRQnIPnPush(LVD_IRQn);

    // ---------------- 用户函数处理 -------------------

    if (LVD_CON1 & LVD_VCC_FLAG(0x1)) { // VCC低电
        LVD_CON1 |= LVD_VCC_FLAG(0x1);  // 清除LVD VCC低电标志位
        lvd_flag = 1;
        // user_printf("lvd pwr 3V \n");
        debug_putchar(lvd_mode);
    }
    if (lvd_mode == 3) {
        lvd_flag = 3;
        dly_pwr_on = 1;
        // user_printf("lvd3\n");
    }
    if (lvd_mode == 4) {
        lvd_flag = 4;
        // user_printf("lvd4\n");
    }

    // 退出中断设置IP，不可删除
    __IRQnIPnPop(LVD_IRQn);
}

void init_lvd_3v(void)
{
    __EnableIRQ(LVD_IRQn); // 使能LVD中断
    IE_EA = 1;             // 使能总中断

    LVD_CON0 &= ~(LVD_OUTSYS_EN(0x1) | LVD_VCC_RST_EN(0x1) |
                  LVD_VCC_VPT_SET(0x7) | LVD_VCC_DETE_EN(0x1));
    LVD_CON0 |= (LVD_OUTSYS_EN(0x1) |   // LVD中断和复位功能输出到系统使能
                 LVD_VCC_RST_EN(0x0) |  // VCC低电压中断使能
                 LVD_VCC_VPT_SET(0x5) | // VCCA电源电压低电检测阈值3.7v
                 LVD_VCC_DETE_EN(0x1)); // VCC 电源VCC电压低电检测功能使能
    LVD_CON1 &= ~LVD_VCC_SYN_DIS(0x0);  // LVD VCC低电检测同步器打开
    lvd_mode = 3;
}

#if 0
//上电配置4V,上电后再配置为3V，用于检查电压跌落
void init_lvd_4V(void)
{
    __EnableIRQ(LVD_IRQn); // 使能LVD中断
    IE_EA = 1;             // 使能总中断

    LVD_CON0 &= ~(LVD_OUTSYS_EN(0x1) | LVD_VCC_RST_EN(0x1) |
                  LVD_VCC_VPT_SET(0x7) | LVD_VCC_DETE_EN(0x1));
    LVD_CON0 |= (LVD_OUTSYS_EN(0x1) |   // LVD中断和复位功能输出到系统使能
                 LVD_VCC_RST_EN(0x0) |  // VCC低电压中断使能
                 LVD_VCC_VPT_SET(0x7) | // VCCA电源电压低电检测阈值4.3v
                 LVD_VCC_DETE_EN(0x1)); // VCC 电源VCC电压低电检测功能使能
    LVD_CON1 &= ~LVD_VCC_SYN_DIS(0x0);  // LVD VCC低电检测同步器打开
    lvd_mode = 4;
}
#endif

extern u8 dly_pwr_on_cnt;
void lvd_handler(void)
{
    if (lvd_flag == 4) {
        lvd_flag = 0;
        delay_ms(10); //消抖动，上电过程会触发多次
        if (lvd_flag == 0) {
            // user_printf("lvd pwr on \n");
            // 初始化所有参数
            init_lvd_3v();
        }
    }

    // 如果lvd_flag == 3，进入延时启动5秒
    if (lvd_flag == 3) {

        // user_printf("lvd pwr 3V \n");
        dly_pwr_on = 1;
    }
}
