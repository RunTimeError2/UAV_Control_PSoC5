#ifndef JY901_H_
#define JY901_H_
    
#include <project.h>
    
#ifndef ABS
    #define ABS(x) (((x)>0)?(x):(-(x)))
#endif

volatile float AccelX, AccelY, AccelZ;     //三轴加速度，单位为g
volatile float OmegaX, OmegaY, OmegaZ;     //三轴角速度，单位为°/s
volatile float Temperature;                //测量温度
volatile float Roll, Pitch, Yaw;           //三轴角度，单位为°
/* 
 *  角度正方向定义：
 *  抬机头Pitch正
 *  左滚转Roll正
 *  俯视逆时针转Yaw增大
 */
volatile int MagX, MagY, MagZ;             //三轴磁场
volatile int Pressure, Height;             //气压和高度，单位为Pa和cm
int i, sum;
    
void Init_JY901_Data() {
    AccelX = AccelY = AccelZ = 0.0;
    OmegaX = OmegaY = OmegaZ = 0.0;
    Temperature = 0.0;
    Roll = Pitch = Yaw = 0.0;
    MagX = MagY = MagZ = 0.0;
}
    
int Decode_JY901_Data(uint8 *buf, int len) {
    if(len != 11 || buf[0] != 0x55)
        return 0; //无效数据错误
    sum = 0;
    for(i = 0; i < 10; i++)
        sum += buf[i];
    if(buf[10] != (sum & 0xff))
        return 1; //校验无效错误
    
    switch(buf[1]) {
        case 0x51: {
            AccelX = (float)((short)((buf[3] << 8) | buf[2]))/32768.0*16.0;
            AccelY = (float)((short)((buf[5] << 8) | buf[4]))/32768.0*16.0;
            AccelZ = (float)((short)((buf[7] << 8) | buf[6]))/32768.0*16.0;
            Temperature = ((buf[9] << 8) | buf[8])/100.0;    
            return 0x51;
            break;
        }
        case 0x52: {
            OmegaX = (float)((short)((buf[3] << 8) | buf[2]))/32768.0*2000.0;
            OmegaY = (float)((short)((buf[5] << 8) | buf[4]))/32768.0*2000.0;
            OmegaZ = (float)((short)((buf[7] << 8) | buf[6]))/32768.0*2000.0;
            Temperature = ((buf[9] << 8) | buf[8])/100.0;
            return 0x52;
            break;
        }
        case 0x53: {
            Roll  = (float)((short)((buf[3] << 8) | buf[2]))/32768.0*180.0;
            Pitch = (float)((short)((buf[5] << 8) | buf[4]))/32768.0*180.0;
            Yaw   = (float)((short)((buf[7] << 8) | buf[6]))/32768.0*180.0;
            Temperature = ((buf[9] << 8) | buf[8])/100.0;
            
            /*LCD_Position(0, 0);
            LCD_PrintDecUint16((short)ABS(Roll));
            LCD_PrintString("     ");
            LCD_Position(0, 5);
            LCD_PrintDecUint16((short)ABS(Pitch));
            LCD_PrintString("     ");
            LCD_Position(0, 10);
            LCD_PrintDecUint16((short)ABS(Yaw));
            LCD_PrintString("     ");*/
                    
            return 0x53;
            break;
        }
        case 0x54: {
            MagX = ((buf[3] << 8) | buf[2]);
            MagY = ((buf[5] << 8) | buf[4]);
            MagZ = ((buf[7] << 8) | buf[6]);
            Temperature = ((buf[9] << 8) | buf[8])/100.0;
            return 0x54;
            break;
        }
        case 0x56: {
            Pressure = (buf[5] << 24) | (buf[4] << 16) | (buf[3] << 8) | buf[2];
            Height = (buf[9] << 24) | (buf[8] << 16) | (buf[7] << 8) | buf[6];
            return 0x56;
            break;
        }
        default: {
            return 0;
            break;
        }
    }
    return 0;
}
    
#endif
 