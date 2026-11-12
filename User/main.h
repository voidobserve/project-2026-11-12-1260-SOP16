#ifndef __MAIN_H__
#define __MAIN_H__

enum
{
    LED_NULL = 0,
    LED_PWR_ON, //上电模式,禁止调光
    LED_ADJ_P,  //调光模式+
    LED_ADJ_S,  //调光模式-
};

typedef u8 led_adjust_sta;

extern led_adjust_sta led_state;

#endif