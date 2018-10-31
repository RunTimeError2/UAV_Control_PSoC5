#include "Control.h"
#include <stdio.h>
#include "math.h"

//来自蓝牙的数据
extern int BT_Throttle, BT_Yaw, BT_Pitch, BT_Roll;
extern float roll_angle_offset, pitch_angle_offset;
extern float Desire_angle_pitch, Desire_angle_roll, Desire_w_yaw;
//来自JY901的数据
extern float AccelX, AccelY, AccelZ;     //三轴加速度，单位为g
extern float OmegaX, OmegaY, OmegaZ;     //三轴角速度，单位为°/s
extern float Temperature;                //测量温度
extern float Roll, Pitch, Yaw;           //三轴角度，单位为°
extern int MagX, MagY, MagZ;             //三轴磁场

//其他临时变量
float set_power_1, set_power_2, set_power_3, set_power_4; //储存设定的动力，范围0-10000
float Error_roll, Error_pitch, Error_w_yaw; //三轴角度/Yaw角速度的误差
float last_Error_roll, last_Error_pitch, last_Error_w_yaw; //上一时刻的角度误差，用于求差分
float int_Error_roll, int_Error_pitch, int_Error_w_yaw; //误差的积分

float Desire_w_roll, Desire_w_pitch; //期望的Roll和Pitch的角速度
float Error_w_roll, Error_w_pitch; //Roll和Pitch角速度的误差
float last_Error_w_roll, last_Error_w_pitch; //上一时刻的Roll和Pitch角速度误差，用于求差分
float int_Error_w_roll, int_Error_w_pitch; //角速度误差的积分
float Control_Roll, Control_Pitch, Control_Yaw;

float last_power_1, last_power_2, last_power_3, last_power_4;
#define MAX_POWER_ACCELERATION 150

//PID相关参数
// 全为正数
#define angle_P_roll  0.5 //PID参数有待调整
#define angle_I_roll  0.000
#define angle_D_roll  1.0
#define angle_P_pitch 0.5
#define angle_I_pitch 0.000
#define angle_D_pitch 1.0
#define Omega_P_roll  10.0
#define Omega_I_roll  0.00
#define Omega_D_roll  15.0
#define Omega_P_pitch 10.0
#define Omega_I_pitch 0.00
#define Omega_D_pitch 15.0
#define Omega_P_yaw   5.0
#define Omega_I_yaw   0.00
#define Omega_D_yaw   12.0

//积分上限
#define Angle_Int_Sup 5000.0
#define Omega_Int_Sup 5000.0

//期望偏角/角速度限制
#define Angle_Roll_Sup   30.0
#define Angle_Roll_Inf  -30.0
#define Angle_Pitch_Sup  30.0
#define Angle_Pitch_Inf -30.0
#define Omega_Yaw_Sup    40.0
#define Omega_Yaw_Inf   -40.0

//电机转速
extern int Motor_v_1, Motor_v_2,  Motor_v_3,  Motor_v_4;

//计算与蓝牙断开连接的时间
volatile int connection_lost_time = 0;

//初始化所有变量
void Control_Func_InitVariable() {
    set_power_1 = set_power_2 = set_power_3 = set_power_4 = 0;
    Error_roll = Error_pitch = Error_w_yaw = Error_w_pitch = Error_w_roll = 0.0;
    last_Error_roll = last_Error_pitch = last_Error_w_yaw = last_Error_w_pitch = last_Error_w_roll = 0.0;
    int_Error_pitch = int_Error_roll = int_Error_w_pitch = int_Error_w_roll = int_Error_w_yaw = 0.0;
    Desire_w_pitch = Desire_w_roll = 0.0;
    Control_Roll = Control_Pitch = Control_Yaw = 0.0;
}

extern int Debug_Mode;
char displayBuffer[16];
//主要控制算法
void Control_Main() {
    //设定根据油门大小设定动力，并留出控制余量
    set_power_1 = BT_Throttle * 9000 / 1024;
    set_power_2 = BT_Throttle * 9000 / 1024;
    set_power_3 = BT_Throttle * 9000 / 1024;
    set_power_4 = BT_Throttle * 9000 / 1024;
    
    //解码蓝牙传输的设定角度
    Desire_angle_pitch = ((float)BT_Roll - 512.0) * 30.0 / 512.0;
    Desire_angle_roll = ((float)BT_Pitch - 512.0) * 30.0 / 512.0;
    Desire_w_yaw = ((float)BT_Yaw - 512.0) * 40.0 / 512.0;
    
    //应用角度偏置
    Roll += roll_angle_offset;
    Pitch += pitch_angle_offset;
    
    //PID控制
    //对于Roll和Pitch，使用两层闭环PID
    //第一层根据角度的误差生成期望角速度
    //第二层根据角速度误差进行控制
    //对于Yaw，因为无法精确计算角度，仅对角速度进行一层闭环的PID控制
    
    //限制期望偏角
    Desire_angle_pitch = (Desire_angle_pitch < Angle_Pitch_Inf)?(Angle_Pitch_Inf):((Desire_angle_pitch > Angle_Pitch_Sup)?(Angle_Pitch_Sup):(Desire_angle_pitch));
    Desire_angle_roll = (Desire_angle_roll < Angle_Roll_Inf)?(Angle_Roll_Inf):((Desire_angle_roll > Angle_Roll_Sup)?(Angle_Roll_Sup):(Desire_angle_roll));
    Desire_w_yaw = (Desire_w_yaw < Omega_Yaw_Inf)?(Omega_Yaw_Inf):((Desire_w_yaw > Omega_Yaw_Sup)?(Omega_Yaw_Sup):(Desire_w_yaw));
    
    //对Pitch和Roll角度两层PID控制
    //计算角度误差
    Error_roll = Desire_angle_roll - Roll;
    Error_pitch = Desire_angle_pitch - Pitch;
    
    //积分和积分限幅
    int_Error_roll += Error_roll;
    int_Error_pitch += Error_pitch;
    int_Error_roll = (int_Error_roll > Angle_Int_Sup)?(Angle_Int_Sup):((int_Error_roll < -Angle_Int_Sup)?(-Angle_Int_Sup):(int_Error_roll));
    int_Error_pitch = (int_Error_pitch > Angle_Int_Sup)?(Angle_Int_Sup):((int_Error_pitch < -Angle_Int_Sup)?(-Angle_Int_Sup):(int_Error_pitch));
    
    //PID生成期望角速度
    Desire_w_roll = Error_roll*angle_P_roll + int_Error_roll*angle_I_roll + (Error_roll - last_Error_roll)*angle_D_roll;
    Desire_w_pitch = Error_pitch*angle_P_roll + int_Error_pitch*angle_I_roll + (Error_pitch - last_Error_pitch)*angle_D_pitch;
    
    //更新角度误差
    last_Error_roll = Error_roll;
    last_Error_pitch = Error_pitch;
    
    //计算角速度误差
    Error_w_roll = Desire_w_roll - OmegaX; //具体的轴与角速度的对应关系有待测量
    Error_w_pitch = Desire_w_pitch - OmegaY;
    
    //角速度误差积分和积分限幅
    int_Error_w_roll += Error_w_roll;
    int_Error_w_pitch += Error_w_pitch;
    int_Error_w_roll = (int_Error_w_roll > Omega_Int_Sup)?(Omega_Int_Sup):((int_Error_w_roll < -Omega_Int_Sup)?(-Omega_Int_Sup):(int_Error_w_roll));
    int_Error_w_pitch = (int_Error_w_pitch > Omega_Int_Sup)?(Omega_Int_Sup):((int_Error_w_pitch < -Omega_Int_Sup)?(-Omega_Int_Sup):(int_Error_w_pitch));
    
    //PID生成控制量
    Control_Roll = Error_w_roll*Omega_P_roll + int_Error_w_roll*Omega_I_roll + (Error_w_roll - last_Error_w_roll)*Omega_D_roll;
    Control_Pitch = Error_w_pitch*Omega_P_pitch + int_Error_w_pitch*Omega_I_pitch + (Error_w_pitch - last_Error_w_pitch)*Omega_D_pitch;
    
    //更新角速度误差
    last_Error_w_roll = Error_w_roll;
    last_Error_w_pitch = Error_w_pitch;

    //对yaw角度的处理，形成一个PID闭环
    //因为对yaw角度测量不准，因此只考虑角速度
    Error_w_yaw = Desire_w_yaw - OmegaZ;
    int_Error_w_yaw += Error_w_yaw;
    int_Error_w_yaw = (int_Error_w_yaw > Omega_Int_Sup)?(Omega_Int_Sup):((int_Error_w_yaw < -Omega_Int_Sup)?(-Omega_Int_Sup):(int_Error_w_yaw));
    Control_Yaw = Error_w_yaw*Omega_P_yaw + int_Error_w_yaw*Omega_I_yaw + (Error_w_yaw - last_Error_w_yaw)*Omega_D_yaw;
    last_Error_w_yaw = Error_w_yaw;
    
    //控制量转化为电机转速变化
    set_power_1 +=  Control_Roll + Control_Pitch + Control_Yaw;
    set_power_2 += -Control_Roll + Control_Pitch - Control_Yaw;
    set_power_3 += -Control_Roll - Control_Pitch + Control_Yaw;
    set_power_4 +=  Control_Roll - Control_Pitch - Control_Yaw;
    
    //限制目标动力在0-10000
    set_power_1 = (set_power_1 < 0)?(0):((set_power_1 > 10000)?(10000):(set_power_1));
    set_power_2 = (set_power_2 < 0)?(0):((set_power_2 > 10000)?(10000):(set_power_2));
    set_power_3 = (set_power_3 < 0)?(0):((set_power_3 > 10000)?(10000):(set_power_3));
    set_power_4 = (set_power_4 < 0)?(0):((set_power_4 > 10000)?(10000):(set_power_4));

    connection_lost_time++;
    if(connection_lost_time >= 50)
        connection_lost_time = 50;
    
    //限制动力增加速率，提高安全性
    if(set_power_1 > last_power_1 + MAX_POWER_ACCELERATION)
        set_power_1 = last_power_1 + MAX_POWER_ACCELERATION;
    if(set_power_2 > last_power_2 + MAX_POWER_ACCELERATION)
        set_power_2 = last_power_2 + MAX_POWER_ACCELERATION;
    if(set_power_3 > last_power_3 + MAX_POWER_ACCELERATION)
        set_power_3 = last_power_3 + MAX_POWER_ACCELERATION;
    if(set_power_4 > last_power_4 + MAX_POWER_ACCELERATION)
        set_power_4 = last_power_4 + MAX_POWER_ACCELERATION;
    
    last_power_1 = set_power_1;
    last_power_2 = set_power_2;
    last_power_3 = set_power_3;
    last_power_4 = set_power_4;
    
    //Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    //动力转速换算，因为推力与转速的平方成正比
    //从动力0-10000换算到转速0-1000
    if(connection_lost_time <= 30) { //一定时间接收不到蓝牙控制信号就自动停止
        Motor_v_1 = (int)(10.0 * sqrt(set_power_1));
        Motor_v_2 = (int)(10.0 * sqrt(set_power_2));
        Motor_v_3 = (int)(10.0 * sqrt(set_power_3));
        Motor_v_4 = (int)(10.0 * sqrt(set_power_4));
    }
    else
        Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    
    //向LCD屏输出3个控制量，仅供调试
    if(Debug_Mode) {
        LCD_Position(1, 0);
        sprintf(displayBuffer, "%d", (int)Control_Roll);
        LCD_PrintString(displayBuffer);
        LCD_PrintString("     ");
        LCD_Position(1, 5);
        sprintf(displayBuffer, "%d", (int)Control_Pitch);
        LCD_PrintString(displayBuffer);
        LCD_PrintString("     ");
        LCD_Position(1, 10);
        sprintf(displayBuffer, "%d", (int)Control_Yaw);
        LCD_PrintString(displayBuffer);
        LCD_PrintString("     ");
    }
    
    //向LCD屏输出设定的动力，仅供调试
    if(Debug_Mode) {
        LCD_Position(0, 0);
        LCD_PrintDecUint16((int)(set_power_1 / 10.0));
        LCD_PrintString("     ");
        LCD_Position(0, 4);
        LCD_PrintDecUint16((int)(set_power_2 / 10.0));
        LCD_PrintString("     ");
        LCD_Position(0, 8);
        LCD_PrintDecUint16((int)(set_power_3 / 10.0));
        LCD_PrintString("     ");
        LCD_Position(0, 12);
        LCD_PrintDecUint16((int)(set_power_4 / 10.0));
        LCD_PrintString("     ");
    }
    
    //向LCD屏输出设定的电机转速（PWM），仅供调试
    /*if(Debug_Mode) {
        LCD_Position(1, 0);
        LCD_PrintDecUint16(Motor_v_1);
        LCD_PrintString("     ");
        LCD_Position(1, 4);
        LCD_PrintDecUint16(Motor_v_2);
        LCD_PrintString("     ");
        LCD_Position(1, 8);
        LCD_PrintDecUint16(Motor_v_3);
        LCD_PrintString("     ");
        LCD_Position(1, 12);
        LCD_PrintDecUint16(Motor_v_4);
        LCD_PrintString("     ");
    }*/
}
