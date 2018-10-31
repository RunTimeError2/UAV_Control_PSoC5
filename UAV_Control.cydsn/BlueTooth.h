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
    
    //遥控器控制的偏转（设定期望角度数据），上限角度30度，角速度40rad/s
    /*Desire_angle_pitch = -((float)BT_Pitch - 512.0) * 30.0 / 512.0;
    Desire_angle_roll = ((float)BT_Roll - 512.0) * 30.0 / 512.0;
    Desire_w_yaw = ((float)BT_Yaw - 512.0) * 40.0 / 512.0;*/
}
    
#endif
