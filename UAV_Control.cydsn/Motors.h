/*
 *     4         3
 *      \       /
 *       \     /
 *        \---/
 *        |   |
 *        /---\
 *       /     \
 *      / Front \
 *     1         2
 */

#ifndef MOTORS_H_
#define MOTORS_H_

#include <project.h>

//Speed should be between 0 and 1000
void Set_Motor_1_Speed(int speed) {
    if(speed < 0 || speed > 1000)
        return;
    PWM_1_WriteCompare1(999 + speed);
}

void Set_Motor_2_Speed(int speed) {
    if(speed < 0 || speed > 1000)
        return;
    PWM_1_WriteCompare2(999 + speed);
}

void Set_Motor_3_Speed(int speed) {
    if(speed < 0 || speed > 1000)
        return;
    PWM_2_WriteCompare1(999 + speed);
}

void Set_Motor_4_Speed(int speed) {
    if(speed < 0 || speed > 1000)
        return;
    PWM_2_WriteCompare2(999 + speed);
}

void Set_All_Motors_Speed(int p1, int p2, int p3, int p4) {
    Set_Motor_1_Speed(p1);
    Set_Motor_2_Speed(p2);
    Set_Motor_3_Speed(p3);
    Set_Motor_4_Speed(p4);
}

void Initialize_All_PWM() {
    PWM_1_Start();
    PWM_2_Start();
    PWM_1_WriteCompare1(0);
    PWM_1_WriteCompare2(0);
    PWM_2_WriteCompare1(0);
    PWM_2_WriteCompare2(0);
}

void Motor_Calibration() {
    Set_Motor_1_Speed(1000);
    Set_Motor_2_Speed(1000);
    Set_Motor_3_Speed(1000);
    Set_Motor_4_Speed(1000);
    CyDelay(2000);
    Set_Motor_1_Speed(0);
    Set_Motor_2_Speed(0);
    Set_Motor_3_Speed(0);
    Set_Motor_4_Speed(0);
    CyDelay(1000);
}

#endif
