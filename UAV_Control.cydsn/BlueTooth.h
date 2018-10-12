#ifndef BLUETOOTH_H_
#define BLUETOOTh_H_
    
#include <project.h>
#include "Motors.h"
#include <stdio.h>
#include <string.h>
    
volatile int BT_Throttle = 0, BT_Yaw = 0, BT_Pitch = 0, BT_Roll = 0;
volatile float roll_angle_offset, pitch_angle_offset;
volatile float Desire_angle_pitch, Desire_angle_roll, Desire_w_yaw;
    
void Process_Bluetooth_Message(uint8 buf[], uint8 buflen) {
    int sum = 0;
    int j;
    for(j = 0; j < 13; j++) //利用校验位进行校验
        sum += buf[j];
    if((sum & 0x00ff) != buf[14]) //如果校验失败则直接退出
        return;
    
    //通过蓝牙获得的遥控器控制量
    BT_Throttle = (int)buf[2] + ((int)buf[3] << 8);       //油门大小，0-1024
    BT_Yaw = (int)buf[4] + ((int)buf[5] << 8);            //目标偏航角，0-1024
    BT_Roll = 1024 - ((int)buf[6] + ((int)buf[7] << 8));  //目标滚转角，0-1024
    BT_Pitch = 1024 - ((int)buf[8] + ((int)buf[9] << 8)); //目标俯仰角，0-1024
    roll_angle_offset = (buf[12] - 120) / 15.01;          //滚转角偏置
    pitch_angle_offset = -(buf[11] - 120) / 15.01;        //俯仰角偏置
    
    /*
    //设定油门大小
    set_speed_1 = BT_Throttle * 900 / 1024;
    set_speed_2 = BT_Throttle * 900 / 1024;
    set_speed_3 = BT_Throttle * 900 / 1024;
    set_speed_4 = BT_Throttle * 900 / 1024;
    */
    
    //遥控器控制的偏转（设定期望角度数据），上限角度30度，角速度40rad/s
    Desire_angle_pitch = -((float)BT_Pitch - 512.0) * 30.0 / 512.0;
    Desire_angle_roll = ((float)BT_Roll - 512.0) * 30.0 / 512.0;
    Desire_w_yaw = ((float)BT_Yaw - 512.0) * 40.0 / 512.0;
    
/*  float p_roll = 2, p_pitch = 2;
    set_speed_1 += Desire_angle_roll*p_roll -Desire_angle_pitch*p_pitch;
    set_speed_2 += Desire_angle_roll*p_roll +Desire_angle_pitch*p_pitch;
    set_speed_3 +=-Desire_angle_roll*p_roll +Desire_angle_pitch*p_pitch;
    set_speed_4 +=-Desire_angle_roll*p_roll -Desire_angle_pitch*p_pitch;
    
    //因为陀螺仪解算姿态精度较差，考虑PID稳定三轴角速度和加速度
    float gyo_control[3], acc_control[3];
    //从陀螺仪处获取测量得到的角速度和加速度
    int i;
    //get_raw_gyo(gyo);
    //get_raw_acc(acc);
    for(i = 0; i < 3; i++) {
        gyo_int[i] += gyo[i];
        acc_int[i] += acc[i];
        gyo_int[i] = (gyo_int[i] > GYO_INT_THRESHOLD) ? (GYO_INT_THRESHOLD) : ((gyo_int[i] < -GYO_INT_THRESHOLD) ? (-GYO_INT_THRESHOLD) : gyo_int[i]);
        acc_int[i] = (acc_int[i] > ACC_INT_THRESHOLD) ? (ACC_INT_THRESHOLD) : ((acc_int[i] < -ACC_INT_THRESHOLD) ? (-ACC_INT_THRESHOLD) : acc_int[i]);
    }
    for(i = 0; i < 3; i++) {
        gyo_control[i] = gyo[i] * GYO_CTRL_P + (gyo[i] - gyo_pre[i]) * GYO_CTRL_D + gyo_int[i] * GYO_CTRL_I;
        acc_control[i] = acc[i] * ACC_CTRL_P + (acc[i] - acc_pre[i]) * ACC_CTRL_D + acc_int[i] * ACC_CTRL_I;
    }
    for(i = 0; i < 3; i++) {
        gyo_pre[i] = gyo[i];
        acc_pre[i] = acc[i];
    }
    //以下表达式中的正负号需要根据实际坐标设定调整
    set_speed_1 +=  acc_control[0] +acc_control[1] -acc_control[2];
    set_speed_2 +=  acc_control[0] -acc_control[1] -acc_control[2];
    set_speed_3 += -acc_control[0] -acc_control[1] -acc_control[2];
    set_speed_4 += -acc_control[0] +acc_control[1] -acc_control[2];
    set_speed_1 +=  gyo_control[0] -gyo_control[1] +gyo_control[2];
    set_speed_2 += -gyo_control[0] -gyo_control[1] -gyo_control[2];
    set_speed_3 += -gyo_control[0] +gyo_control[1] +gyo_control[2];
    set_speed_4 +=  gyo_control[0] +gyo_control[1] -gyo_control[2];
    
    //限制电机转速设定量的值
    set_speed_1 = (set_speed_1 > 1000) ? 1000 : set_speed_1;
    set_speed_1 = (set_speed_1 < 0) ? 0 : set_speed_1;
    set_speed_2 = (set_speed_2 > 1000) ? 1000 : set_speed_2;
    set_speed_2 = (set_speed_2 < 0) ? 0 : set_speed_2;
    set_speed_3 = (set_speed_3 > 1000) ? 1000 : set_speed_3;
    set_speed_3 = (set_speed_3 < 0) ? 0 : set_speed_3;
    set_speed_4 = (set_speed_4 > 1000) ? 1000 : set_speed_4;
    set_speed_4 = (set_speed_4 < 0) ? 0 : set_speed_4;
    
    //防止转速上升过快
    set_speed_1 = (set_speed_1 > last_speed_1 + ACCEL_THRESHOLD) ? (last_speed_1 + ACCEL_THRESHOLD) : set_speed_1;
    set_speed_2 = (set_speed_2 > last_speed_2 + ACCEL_THRESHOLD) ? (last_speed_2 + ACCEL_THRESHOLD) : set_speed_2;
    set_speed_3 = (set_speed_3 > last_speed_3 + ACCEL_THRESHOLD) ? (last_speed_3 + ACCEL_THRESHOLD) : set_speed_3;
    set_speed_4 = (set_speed_4 > last_speed_4 + ACCEL_THRESHOLD) ? (last_speed_4 + ACCEL_THRESHOLD) : set_speed_4;
    
    //在LCD屏上显示提示信息（设定的转速）
    char tmp_buf[16];
    sprintf(tmp_buf, "[1]%4d [2]%4d ", set_speed_1, set_speed_2);
    LCD_Position(0, 0);
    LCD_PrintString(tmp_buf);
    sprintf(tmp_buf, "[3]%4d [4]%4d ", set_speed_3, set_speed_4);
    LCD_Position(1, 0);
    LCD_PrintString(tmp_buf);

    //设定转速
    Motor_v_1 = set_speed_1;
    Motor_v_2 = set_speed_2;
    Motor_v_3 = set_speed_3;
    Motor_v_4 = set_speed_4;
    
    last_speed_1 = set_speed_1;
    last_speed_2 = set_speed_2;
    last_speed_3 = set_speed_3;
    last_speed_4 = set_speed_4;*/
}
    
#endif
