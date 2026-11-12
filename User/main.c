

#include "include.h"
#include <stdio.h>
#include "main.h"
#include <math.h>
/** @addtogroup Template_Project
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

led_adjust_sta led_state;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*********************************
|--------------------------------|
| 使用%d打印的数值不对时，请注意 |
|--------------------------------|
| 格式 |     含义     | 针对类型 |
|------|--------------|----------|
| %bd  | 单个字节变量 | char     |
| % d  | 两个字节变量 | int      |
| %ld  | 四个字节变量 | long int |
|--------------------------------|
**********************************/
// 重写puchar()函数,用于标准printf()函数 (注意:使用printf()函数占用1k左右的code空间)
char putchar(char c)
{
    debug_putchar(c);

    return c;
}

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
extern void timer4_cap_init(void);
extern void timer0_init();
extern void get_duty(void);
extern void pwm_init(void);
extern void option_fun(void);
// extern void init_lvd_4V(void);
extern void init_lvd_3v(void);

extern void lvd_handler(void);
extern void adc_init();
extern void adc_val();
extern u16 ntc_val;
extern u16 adjust_val;
extern u16 limited_pwm_duty_val_by_knob;
extern u16 adc0_val;
extern u16 adc1_val;
extern void adjust_pwm();
extern u16 max_duty;
extern bit dly_pwr_on;
extern u8 simple_over;
extern u16 max_duty;
void main(void)
{

    system_init();

    // 看门狗默认打开, 复位时间2s
    // WDT_KEY = WDT_KEY_VAL(0xDD);               //  关闭看门狗 (如需配置看门狗请查看“WDT\WDT_Reset”示例)
    led_state = LED_PWR_ON;

    delay_ms(10);
    // 初始化打印
    debug_init();
    user_printf("TX8C126x_SDK main start\n");
    timer4_cap_init();

    __SetIRQnIP(TMR4_IRQn, TMR4_IQn_CFG);
    __SetIRQnIP(STMR0_IRQn, STMR0_IQn_CFG);

    timer0_init();
    pwm_init();
    adc_init();
    adc_val(); //初始化时运行一次，用于标志位判断
    option_fun();
    printf("adc0_val = %d  adc1_val = %d \r\n ", adc0_val, adc1_val);
    init_lvd_3v();
    while (1) {
        WDT_KEY = WDT_KEY_VAL(0xAA); // 喂狗并清除 wdt_pending
        get_duty();
        lvd_handler();
        //adc_val();//adc值
        adjust_pwm(); //电位器值
    }
}

/**
  * @}
  */

/*************************** (C) COPYRIGHT 2021 HUGE-IC ***** END OF FILE *****/
