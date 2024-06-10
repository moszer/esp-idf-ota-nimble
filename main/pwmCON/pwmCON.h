#ifndef PWM_CON_H
#define PWM_CON_H

#include "driver/ledc.h"

void intit_pin_pwm(int setPIN);
void pwm_con(int duty);

#endif

