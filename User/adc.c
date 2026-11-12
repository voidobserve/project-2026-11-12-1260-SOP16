#include "include.h"
#include "main.h"
u16 ntc_val;
u16 adjust_val;
u16 ntc_val_all;
u16 adjust_val_all;
u8 adc_cnt = 0;
u8 loop_cnt = 0;
u16 adc0_val;
u16 adc1_val;
void adc_init()
{
    // 配置P27 p30为模拟输入模式 并且上拉
    P2_MD0 |= GPIO_P27_MODE_SEL(0x3); // P27设为模拟模式  PIN9
    P3_MD0 |= GPIO_P30_MODE_SEL(0x3); // P30设为模拟模式  PIN8
    P2_PU |= (GPIO_P27_PULL_UP(
        0x1)); // P27使能上拉      PIN9  用于初始化判断是否接入ntc
    P2_PD |= (GPIO_P27_PULL_PD(0x0)); // P27关闭下拉
    P3_PU |= (GPIO_P30_PULL_UP(
        0x1)); // P30使能上拉      PIN8  用于初始化判断是否接入电位器
    P3_PD |= (GPIO_P30_PULL_PD(0x0)); // P30关闭下拉
    //ADC配置
    ADC_ACON1 &= ~(ADC_VREF_SEL(0x7) | ADC_EXREF_SEL(0x1)); // 关闭外部参考电压
    ADC_ACON1 |= ADC_VREF_SEL(0x6) | // 选择内部参考电压VCCA  5v
                 ADC_TEN_SEL(0x3);
    ADC_ACON0 = ADC_CMP_EN(0x1) |  // 打开ADC中的CMP使能信号
                ADC_BIAS_EN(0x1) | // 打开ADC偏置电流能使信号
                ADC_BIAS_SEL(0x1);

    ADC_CHS0 = ADC_ANALOG_CHAN(0x17) | // P27通路
               ADC_EXT_SEL(0x0);       // 选择外部通路
    ADC_CFG0 |= ADC_CHAN0_EN(0x1) |    // 使能通道0转换
                ADC_EN(0x1);           // 使能A/D转换

    ADC_CHS1 = ADC_ANALOG_CHAN(0x18) | // P30通路
               ADC_EXT_SEL(0x0);       // 选择外部通路
    ADC_CFG0 |= ADC_CHAN1_EN(0x1) |    // 使能通道1转换
                ADC_EN(0x1);           // 使能A/D转换

    delay_ms(1); // 等待ADC模块配置稳定，需要等待20us以上
}

void adc_val()
{
    ADC_CFG0 |= ADC_CHAN0_TRG(0x1); // 触发ADC0转换
    while (!(ADC_STA & ADC_CHAN0_DONE(0x1)))
        ;                                             // 等待转换完成
    ADC_STA = ADC_CHAN0_DONE(0x1);                    // 清除ADC0转换完成标志位
    adc0_val = (ADC_DATAH0 << 4) | (ADC_DATAL0 >> 4); // 读取channel0的值

    ADC_CFG0 |= ADC_CHAN1_TRG(0x1); // 触发ADC0转换
    while (!(ADC_STA & ADC_CHAN1_DONE(0x1)))
        ;                                             // 等待转换完成
    ADC_STA = ADC_CHAN1_DONE(0x1);                    // 清除ADC0转换完成标志位
    adc1_val = (ADC_DATAH1 << 4) | (ADC_DATAL1 >> 4); // 读取channel1的值

    ntc_val_all += adc0_val;
    adjust_val_all += adc1_val;

    adc_cnt++;
    if (adc_cnt == 10) //均值滤波
    {
        ntc_val = ntc_val_all / 10;
        adjust_val = adjust_val_all / 10;
        adc_cnt = 0;
        ntc_val_all = 0;
        adjust_val_all = 0;
    }
}
