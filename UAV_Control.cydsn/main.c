#include <project.h>
#include "Motors.h"
#include "BlueTooth.h"
#include "Control.h"
#include <stdio.h>
#include "math.h"
#include "JY901.h"

//LCD显示
void LCD_Disp(char *str, unsigned char row, unsigned char col) {
    LCD_Position(row, col);
    LCD_PrintString(str);
}

#ifndef ABS
    #define ABS(x) (((x)>0)?(x):(-(x)))
#endif

//电机速度控制
volatile int Motor_v_1, Motor_v_2,  Motor_v_3,  Motor_v_4;
int last_Motor_v_1, last_Motor_v_2, last_Motor_v_3, last_Motor_v_4;

CY_ISR(Timer_1_Interrupt_Handler) {
    Timer_1_STATUS;
    if(Motor_v_1 != last_Motor_v_1 || Motor_v_1 == 0) {
        Set_Motor_1_Speed(Motor_v_1);
        last_Motor_v_1 = Motor_v_1;
    }
    if(Motor_v_2 != last_Motor_v_2 || Motor_v_2 == 0) {
        Set_Motor_2_Speed(Motor_v_2);
        last_Motor_v_2 = Motor_v_2;
    }
    if(Motor_v_3 != last_Motor_v_3 || Motor_v_3 == 0) {
        Set_Motor_3_Speed(Motor_v_3);
        last_Motor_v_3 = Motor_v_3;
    }
    if(Motor_v_4 != last_Motor_v_4 || Motor_v_4 == 0) {
        Set_Motor_4_Speed(Motor_v_4);
        last_Motor_v_4 = Motor_v_4;
    }
}

//蓝牙串口接收与解码
uint8 BTRxBuf[64];
uint8 BTRxBufLen;
uint8 tmp;
int flag = 0;
extern int connection_lost_time;

CY_ISR(Bluetooth_RX_Interrupt_Handler) {
    tmp = UART_Bluetooth_ReadRxData();
    if(tmp == 0xa5 && flag == 0) { //遇到起始位，开始接收
        BTRxBuf[0] = 0xa5;
        BTRxBufLen = 1;
        flag = 1;
    }
    else 
        if(flag == 1 && BTRxBufLen < 15) { //固定接收起始位之后的15个字节
            BTRxBuf[BTRxBufLen] = tmp;
            BTRxBufLen++;
        }
        else
            if(flag == 1 && BTRxBufLen == 15) {
                connection_lost_time = 0;
                Process_Bluetooth_Message(BTRxBuf, BTRxBufLen); //转入解码函数
                BTRxBufLen = 0; //清空缓冲区
                flag = 0;
            }
}

//JY901串口接收与解码
uint8 JY901RxBuf[64];
uint8 JY901RxBufLen;
uint8 tmp2;
int flag_55 = 0;
int flag_rec = 0;
int decode_success;

CY_ISR(JY901_RX_Interrupt_Handler) {
    tmp2 = UART_JY901_ReadRxData();
    
    if(tmp2 == 0x55 && flag_55 == 0) //接收到数据起始位
        flag_55 = 1;
    else
        if(tmp2 >= 0x51 && tmp2 <= 0x54 && flag_55 == 1 && flag_rec == 0) { //接收到数据类型标志位
            JY901RxBuf[0] = 0x55;
            JY901RxBuf[1] = tmp2;
            JY901RxBufLen = 2;
            flag_rec = 1;
            flag_55 = 1;
        }
        else
            if(flag_rec == 1) { //储存特定长度的数据
                JY901RxBuf[JY901RxBufLen++] = tmp2;
                if(JY901RxBufLen == 11) { //接收完一段完整数据
                    decode_success = Decode_JY901_Data(JY901RxBuf, JY901RxBufLen);
                    JY901RxBufLen = 0;
                    flag_rec = 0;
                    flag_55 = 0;
                }
            }
            else
                flag_55 = flag_rec = 0; //其他情况清空标志位
}

//向串口输出JY901解码得到的信息，仅供调试
char DisplayBuffer[128];
CY_ISR(JY901_Debug_Display) {
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("===== Debug Data =====\n");
    while(USBUART_1_CDCIsReady() == 0u);
    sprintf(DisplayBuffer, "AccelX=%f, AccelY=%f, AccelZ=%f\n", AccelX, AccelY, AccelZ);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    sprintf(DisplayBuffer, "OmegaX=%f, OmegaY=%f, OmegaZ=%f\n", OmegaX, OmegaY, OmegaZ);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    sprintf(DisplayBuffer, "Roll=%f, Pitch=%f, Yaw=%f\n", Roll, Pitch, Yaw);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    sprintf(DisplayBuffer, "Hx=%d, Hy=%d, Hz=%d\n", MagX, MagY, MagZ);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    sprintf(DisplayBuffer, "Temp=%f\n\n", Temperature);
    USBUART_1_PutString(DisplayBuffer);
}

//定时运行控制算法函数
int launch_control_cnt = 0;
CY_ISR(Launch_Control) {
    launch_control_cnt++;
    if(launch_control_cnt >= 4) {
        Control_Main();
        launch_control_cnt = 0;
    }
}

//主函数
char tmpbuf[10];
volatile int Debug_Mode = 0; //0表示进入实际运行状态，1表示调试模式
int main() {
    CyGlobalIntEnable;
    
    //LCD显示相关
    LCD_Start();
    LCD_Disp("Init     ", 0, 0);
    
    //定时器相关，控制PWM和控制算法
    isr_Timer_1_StartEx(Timer_1_Interrupt_Handler);
    if(!Debug_Mode)
        isr_Ctrl_StartEx(Control_Main);
    Timer_1_Start();
    Initialize_All_PWM();
    
    //USBUART相关，用于输出调试信息
    USBUART_1_Start(0u, USBUART_1_3V_OPERATION);
    while(!USBUART_1_GetConfiguration());
    USBUART_1_CDC_Init();
    
    //蓝牙相关
    BTRxBufLen = 0;
    UART_Bluetooth_Start();
    UART_Bluetooth_Init();
    isr_RX_StartEx(Bluetooth_RX_Interrupt_Handler);
    
    //JY901串口相关
    JY901RxBufLen = 0;
    UART_JY901_Start();
    UART_JY901_Init();
    isr_jyRX_StartEx(JY901_RX_Interrupt_Handler);
    Init_JY901_Data();
    
    //初始化控制算法变量
    Control_Func_InitVariable();

    //初始电机停转，保持10秒，调试状态下可以接上电池
    LCD_Disp("Start    ", 0, 0);
    Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    BT_Throttle = 0;
    BT_Pitch = 512;
    BT_Roll = 512;
    BT_Yaw = 512;
    if(Debug_Mode) {
        CyDelay(10000);
        LCD_Disp("Ready    ", 0, 0);
    }
    else {
        CyDelay(1000);
    }
    
    //仅供测试，四个电机轮流转动1秒，用于检查电机是否正常工作以及顺序是否正确
    /*int velocity = 300;  //0-1000
    Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    Motor_v_1 = velocity;
    LCD_Disp("Run 1    ", 0, 0);
    CyDelay(1000);
    
    Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    Motor_v_2 = velocity;
    LCD_Disp("Run 2    ", 0, 0);
    CyDelay(1000);
    
    Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    Motor_v_3 = velocity;
    LCD_Disp("Run 3    ", 0, 0);
    CyDelay(1000);
    
    Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
    Motor_v_4 = velocity;
    LCD_Disp("Run 4    ", 0, 0);
    CyDelay(1000);*/
    
    Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;

    for(;;) {
        if(Debug_Mode) {
            JY901_Debug_Display(); //通过串口向上位机输出JY901数据
            Control_Main();
            CyDelay(500);
        }
        else {
            if(connection_lost_time >= 30)
                LCD_Disp("Stop    ", 1, 0);
            else
                LCD_Disp("Control ", 1, 0);
        }
    }
}
