#include "Control.h"

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
int set_speed_1, set_speed_2, set_speed_3, set_speed_4; //储存设定的速度
float Error_roll, Error_pitch, Error_w_yaw; //三轴角度/Yaw角速度的误差
float last_Error_roll, last_Error_pitch, last_Error_w_yaw; //上一时刻的角度误差，用于求差分
float int_Error_roll, int_Error_pitch, int_Error_w_yaw; //误差的积分

float Desire_w_roll, Desire_w_pitch; //期望的Roll和Pitch的角速度
float Error_w_roll, Error_w_pitch; //Roll和Pitch角速度的误差
float last_Error_w_roll, last_Error_w_pitch; //上一时刻的Roll和Pitch角速度误差，用于求差分
float int_Error_w_roll, int_Error_w_pitch; //角速度误差的积分
float Control_Roll, Control_Pitch, Control_Yaw;

//PID相关参数
#define angle_P_roll  10.0 //PID参数有待调整
#define angle_I_roll  0.0
#define angle_D_roll  0.0
#define angle_P_pitch 10.0
#define angle_I_pitch 0.0
#define angle_D_pitch 0.0
#define Omega_P_roll  10.0
#define Omega_I_roll  0.0
#define Omega_D_roll  0.0
#define Omega_P_pitch 10.0
#define Omega_I_pitch 0.0
#define Omega_D_pitch 0.0
#define Omega_P_yaw   0.0
#define Omega_I_yaw   0.0
#define Omega_D_yaw   0.0

//积分上限
#define Angle_Int_Sup 1000.0
#define Omega_Int_Sup 1000.0

//期望偏角/角速度限制
#define Angle_Roll_Sup   30.0
#define Angle_Roll_Inf  -30.0
#define Angle_Pitch_Sup  30.0
#define Angle_Pitch_Inf -30.0
#define Omega_Yaw_Sup    40.0
#define Omega_Yaw_Inf   -40.0

//电机转速
extern int Motor_v_1, Motor_v_2,  Motor_v_3,  Motor_v_4;

int tmpcount = 0; //================================

//初始化所有变量
void Control_Func_InitVariable() {
    set_speed_1 = set_speed_2 = set_speed_3 = set_speed_4 = 0;
    Error_roll = Error_pitch = Error_w_yaw = Error_w_pitch = Error_w_roll = 0.0;
    last_Error_roll = last_Error_pitch = last_Error_w_yaw = last_Error_w_pitch = last_Error_w_roll = 0.0;
    int_Error_pitch = int_Error_roll = int_Error_w_pitch = int_Error_w_roll = int_Error_w_yaw = 0.0;
    Desire_w_pitch = Desire_w_roll = 0.0;
    Control_Roll = Control_Pitch = Control_Yaw = 0.0;
}

//主要控制算法
void Control_Main() {
    //设定根据油门大小设定转速，并留出控制余量
    set_speed_1 = BT_Throttle * 900 / 1024;
    set_speed_2 = BT_Throttle * 900 / 1024;
    set_speed_3 = BT_Throttle * 900 / 1024;
    set_speed_4 = BT_Throttle * 900 / 1024;
    
    //解码蓝牙传输的设定角度
    Desire_angle_pitch = -((float)BT_Pitch - 512.0) * 30.0 / 512.0;
    Desire_angle_roll = ((float)BT_Roll - 512.0) * 30.0 / 512.0;
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
    //实际旋转方向和电机编号需要进一步确认
    set_speed_1 +=  Control_Roll + Control_Pitch + Control_Yaw;
    set_speed_2 += -Control_Roll + Control_Pitch - Control_Yaw;
    set_speed_3 += -Control_Roll - Control_Pitch + Control_Yaw;
    set_speed_4 +=  Control_Roll - Control_Pitch - Control_Yaw;
    
    //限制电机转速在0-1000
    set_speed_1 = (set_speed_1 < 0)?(0):((set_speed_1 > 1000)?(1000):(set_speed_1));
    set_speed_2 = (set_speed_2 < 0)?(0):((set_speed_2 > 1000)?(1000):(set_speed_2));
    set_speed_3 = (set_speed_3 < 0)?(0):((set_speed_3 > 1000)?(1000):(set_speed_3));
    set_speed_4 = (set_speed_4 < 0)?(0):((set_speed_4 > 1000)?(1000):(set_speed_4));
    
    LCD_Position(0, 0);
    LCD_PrintDecUint16(set_speed_1); //=========================
    LCD_PrintString("     ");
    LCD_Position(0, 4);
    LCD_PrintDecUint16(set_speed_2);
    LCD_PrintString("     ");
    LCD_Position(0, 8);
    LCD_PrintDecUint16(set_speed_3);
    LCD_PrintString("     ");
    LCD_Position(0, 12);
    LCD_PrintDecUint16(set_speed_4);
    LCD_PrintString("     ");
    
    /*LCD_Position(1, 0);
    LCD_PrintDecUint16((int)(Desire_w_roll*10.0)); //=========================
    LCD_Position(1, 5);
    LCD_PrintDecUint16((int)(Desire_w_pitch*10.0));
    LCD_Position(1, 10);
    LCD_PrintDecUint16((int)(Desire_w_yaw*10.0));*/
    tmpcount++;
    if(tmpcount >= 100)
        tmpcount = 0;
    LCD_Position(1, 0);
    LCD_PrintDecUint16(tmpcount);
    LCD_PrintString("     ");
    
    //Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    /*Motor_v_1 = set_speed_1;
    Motor_v_2 = set_speed_2;
    Motor_v_3 = set_speed_3;
    Motor_v_4 = set_speed_4;*/
}
