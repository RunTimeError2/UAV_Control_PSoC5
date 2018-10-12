/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
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
//小数显示（不知道为什么sprintf不能用%f）
void Convert_Float2String(char *str, float x) {
    uint8 symbol = (x > 0.0) ? 1 : 0;
    float num = ABS(x);
    int int_part = (int)num;
    int float_2part = (int)((num - int_part)*100.0); //取两位小数
    if(float_2part >= 10)
        sprintf(str, "%c%d.%d", (symbol == 1)?'+':'-', int_part, float_2part);
    else
        if(float_2part > 0)
            sprintf(str, "%c%d.0%d", (symbol == 1)?'+':'-', int_part, float_2part);
        else
            sprintf(str, "%c%d.00", (symbol == 1)?'+':'-', int_part);
}

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

//蓝牙接收与解码
uint8 BTRxBuf[64];
uint8 BTRxBufLen;
uint8 tmp;
int flag = 0;

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
                LCD_Disp("GetData!", 1, 2); //输出提示信息
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

//在LCD屏上显示角度
char LCDBuffer[16];
int float2int_tmp;
char symbol;
float f_tmp;
void Float28Str(float f) {
    f_tmp = f;
    symbol = (f_tmp < 0) ? '-' : ' ';
    f_tmp = (f_tmp < 0) ? (-f_tmp) : f_tmp;
    float2int_tmp = (int)(f_tmp*1000.0);
    sprintf(LCDBuffer, "%c%3d.%d", symbol, float2int_tmp/1000, float2int_tmp%1000);
    LCDBuffer[8] = '\0';
}

void LCD_DisplayAngle() {
    Float28Str(Roll);
    LCD_Disp(LCDBuffer, 0, 0);
    Float28Str(Pitch);
    LCD_Disp(LCDBuffer, 0, 8);
    Float28Str(Yaw);
    LCD_Disp(LCDBuffer, 1, 0);
}

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

char DisplayBuffer[128];
CY_ISR(JY901_Debug_Display) {
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("===== Debug Data =====\n");
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("AccelX=");
    Convert_Float2String(DisplayBuffer, AccelX);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", AccelY=");
    Convert_Float2String(DisplayBuffer, AccelY);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", AccelZ=");
    Convert_Float2String(DisplayBuffer, AccelZ);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("\nOmegaX=");
    Convert_Float2String(DisplayBuffer, OmegaX);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", OmegaY=");
    Convert_Float2String(DisplayBuffer, OmegaY);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", OmegaZ=");
    Convert_Float2String(DisplayBuffer, OmegaZ);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("\nRoll=");
    Convert_Float2String(DisplayBuffer, Roll);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", Pitch=");
    Convert_Float2String(DisplayBuffer, Pitch);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", Yaw=");
    Convert_Float2String(DisplayBuffer, Yaw);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("\nHx=");
    Convert_Float2String(DisplayBuffer, MagX);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", Hy=");
    Convert_Float2String(DisplayBuffer, MagY);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(", Hz=");
    Convert_Float2String(DisplayBuffer, MagZ);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("\nTemp=");
    Convert_Float2String(DisplayBuffer, Temperature);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString(DisplayBuffer);
    while(USBUART_1_CDCIsReady() == 0u);
    USBUART_1_PutString("\n\n");
}

//主函数
char tmpbuf[10];
int main()
{
    CyGlobalIntEnable; /* Enable global interrupts. */
    
    //LCD显示相关
    LCD_Start();
    LCD_Disp("Init     ", 0, 0);
    
    //定时器/PWM相关
    isr_Timer_1_StartEx(Timer_1_Interrupt_Handler);
    Timer_1_Start();
    Initialize_All_PWM();
    
    //USBUART相关
    USBUART_1_Start(0u, USBUART_1_3V_OPERATION);
    while(!USBUART_1_GetConfiguration());
    USBUART_1_CDC_Init();
    
    //USBUART_1_PutData(buffer, length);
    //USBUART_1_PutString(str);
    
    //蓝牙相关
    BTRxBufLen = 0;
    UART_Bluetooth_Start();
    UART_Bluetooth_Init();
    //isr_RX_StartEx(Bluetooth_RX_Interrupt_Handler); //=================
    
    //JY901串口相关
    JY901RxBufLen = 0;
    UART_JY901_Start();
    UART_JY901_Init();
    isr_jyRX_StartEx(JY901_RX_Interrupt_Handler);
    Init_JY901_Data();
    
    //初始化控制算法变量
    Control_Func_InitVariable();
    
    //控制算法定时器
    //isr_Ctrl_StartEx(Control_Main);
    //Timer_Control_Start();
    
    LCD_Disp("Start    ", 0, 0);
    Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;

    //设置电机停转
    for(;;) {
        BT_Throttle = 300;
        BT_Pitch = 512;
        BT_Roll = 512;
        BT_Yaw = 512;
        //JY901_Debug_Display(); //每隔1秒通过串口向上位机输出JY901数据
        //Motor_v_1 = Motor_v_2 = Motor_v_3 = Motor_v_4 = 0;
        //CyDelay(500);
        Control_Main();
        CyDelay(100);
    }
}
