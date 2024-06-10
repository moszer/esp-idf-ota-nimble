#include "pwmCON.h"

#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_BIT     LEDC_TIMER_10_BIT
#define LEDC_TIMER_FREQ    5000

void intit_pin_pwm(int setPIN){
  // Configure LEDC peripheral
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_BIT,
        .freq_hz = LEDC_TIMER_FREQ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = setPIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);
}

void pwm_con(int duty){
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}