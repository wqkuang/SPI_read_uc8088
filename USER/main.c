#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h" 		 	 
#include "key.h"     
#include "exti.h"
//#include "malloc.h"
//#include "sdio_sdcard.h"     
//#include "ff.h"  
//#include "exfuns.h"    
#include "uc8088_spi.h"

u8 key0_flag = 0;
u8 Buffer[SPI_BUF_LEN] = {0};
u8 send_cmd[16] = "AT+MIPSEND=1,\0";
const char *str_OK="SEND OK";

void ML302_init()
{
	printf("AT\r\n");			
	delay_ms(10);
	printf("AT+CPIN?\r\n");		//查询SIM卡状态
	delay_ms(50);
	printf("AT+CSQ\r\n");			//查询信号质量， 小于10说明信号差
	delay_ms(50);
//	printf("AT+CGDCONT=1,\"IP\",\"CMIOT\"\r\n");		//设置APN
//	delay_ms(10);
	printf("AT+CGACT=1,1\r\n");		//激活PDP
	delay_ms(100);
	printf("AT+MIPOPEN=1,\"TCP\",\"server.natappfree.cc\",42451\r\n");	//连接服务器
	delay_ms(100);
	printf("ATE0\r\n");				//关闭回显
	delay_ms(10);
	memset(USART_RX_BUF, 0, USART_REC_LEN);
	USART_RX_STA = 0;
}


int ML302_send_result()
{
	u32 time_out = 1e7;
	while(time_out && !(USART_RX_STA & 0x8000))		//等待 ML302 返回发送结果
	{	time_out--; LED1=!LED1;}
	if(!time_out)
		printf("***---%s---%x****", USART_RX_BUF, USART_RX_STA);
	USART_RX_STA = 0;
	if (NULL == strstr(USART_RX_BUF, str_OK))
	{
		delay_ms(1);
		//printf("***---%s---%x****", USART_RX_BUF, USART_RX_STA);
		memset(USART_RX_BUF, 0, USART_REC_LEN);
		return -1;
	}
	else
	{
		memset(USART_RX_BUF, 0, USART_REC_LEN);
		return 0;
	}
}

void uart_send_data_2_ML302(u8 *TX_BUF, u16 len)
{
	char num_char[4];
//	u16 t;
	sprintf(num_char, "%d", len);
	printf("%s%s\r\n", send_cmd, num_char);
	TX_BUF[len] = '\0';
	printf("%s", TX_BUF);
//	for(t=0; t<len; t++)
//	{
//		USART_SendData(USART1, *(TX_BUF + t));//向串口1发送数据
//		while(USART_GetFlagStatus(USART1,USART_FLAG_TC)!=SET);//等待发送结束
//	}
}

/************************************************
程序功能，存储8088串口打印的数据到SD卡
************************************************/
//FATFS fs;													/* FatFs文件系统对象 */
//FIL fnew;													/* 文件对象 */
//FRESULT res_flash;                /* 文件操作结果 */
//UINT fnum;            					  /* 文件成功读写数量 */
////BYTE ReadBuffer[1024]={0};        /* 读缓冲区 */
//BYTE WriteBuffer[] = "随便写到文件中123abc";         /* 写缓冲区*/

int main(void)
{	 
	u8 one_char, first_flag=0;
	u16 tmp, i;
	u32 wp, rp, rrp;
	delay_init();	    	 //延时函数初始化	  
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	uart_init(115200);	 	//串口初始化为115200	
 	LED_Init();		  			//初始化与LED连接的硬件接口
	KEY_Init();					//初始化按键
	uc8088_init();		//8088初始化
	
	printf("halt cpu\t");
	rp = uc8088_read_u32(0x1a107018);
	printf("read test = %x\r\n", rp);
	tmp = uc8088_read_u8(0x1a107018);
	printf("tmp = %x\r\n", tmp);
	tmp = uc8088_read_u8(0x1a107019);
	printf("tmp = %x\r\n", tmp);
	tmp = uc8088_read_u8(0x1a10701a);
	printf("tmp = %x\r\n", tmp);
	 tmp = uc8088_read_u8(0x1a10701b);
	printf("tmp = %x\r\n", tmp);
	
	wp = uc8088_read_u16(Buf_addr);
	printf("wp = %x\r\n", wp);
	wp = uc8088_read_u16(Buf_addr+2);
	printf("wp = %x\r\n", wp);
	wp = 0;

	wp = uc8088_read_u32(Buf_addr);
	rp = uc8088_read_u32(Buf_addr + 4);
	printf("wp = %d,   rp = %d\r\n", wp, rp); 
	LED0 = 0;
	ML302_init();
	wp = 0;
	rp = 65536;
	while(1){
		
		rp = uc8088_read_u32(Buf_addr + 4);
		if (rp != rrp && first_flag)		//保证rp绝对正确
		{
			//printf("error rp = %d,  right rp = %d\r\n", rp, rrp);
			continue;
		}
		wp = uc8088_read_u32(Buf_addr);
		if (wp > SPI_BUF_LEN || wp < 8)		//避开0值，因为可能抢占失败返回0
			continue;
		//printf("rp = %d,   wp = %d\r\n", rp, wp);
		if (rp < wp){
			if (wp - rp < 16)
				continue;
			tmp = uc8088_read_memory(Buf_addr + 8 + rp, Buffer, wp - rp);
			if (tmp > SPI_BUF_LEN){
				//printf("error1 : tmp = %d > SPI_BUF_LEN\r\n", tmp);
				tmp = 0;
				continue;
			}
			
			if(tmp){
				//printf("rp = %d,   wp = %d, 111  tmp = %d\r\n", rp, wp, tmp);
				rrp = 65536;
				do{
					uc8088_write_u32(Buf_addr + 4, rp+tmp);
					rrp = uc8088_read_u32(Buf_addr + 4);
				}while(rrp != (rp+tmp));
				
				for(i=0; i<tmp; i+=4){
					one_char = Buffer[i+3];
					Buffer[i+3] = Buffer[i];
					Buffer[i] = one_char;
					one_char = Buffer[i+2];
					Buffer[i+2] = Buffer[i+1];
					Buffer[i+1] = one_char;
				}
				//Buffer[tmp] = '\0';
				
				rp = 65536;
				//printf("%s", Buffer);
				do{
					uart_send_data_2_ML302(Buffer, tmp);
				}while(ML302_send_result());
			}
		}
		else if(rp > wp){
			if (SPI_BUF_LEN - rp + wp < 16)
				continue;
			tmp = uc8088_read_memory(Buf_addr + 8 + rp, Buffer, SPI_BUF_LEN - rp);
			i = uc8088_read_memory(Buf_addr + 8, Buffer + tmp, wp);
				
			tmp += i;
			if (tmp > SPI_BUF_LEN){
				//printf("error2 : tmp = %d > SPI_BUF_LEN\r\n", tmp);
				tmp = 0;
				continue;
			}
			
			if (tmp){
				//printf("rp = %d,   wp = %d, 222  tmp = %d\r\n", rp, wp, tmp);
				rrp = 65536;
				do{
					uc8088_write_u32(Buf_addr + 4, i);
					rrp = uc8088_read_u32(Buf_addr + 4);
				}while(rrp != i);
				
				for(i=0; i<tmp; i+=4){
					one_char = Buffer[i+3];
					Buffer[i+3] = Buffer[i];
					Buffer[i] = one_char;
					one_char = Buffer[i+2];
					Buffer[i+2] = Buffer[i+1];
					Buffer[i+1] = one_char;
				}
				//Buffer[tmp] = '\0';
				
				rp = 65536;
				//printf("%s", Buffer);
				do{
					uart_send_data_2_ML302(Buffer, tmp);
				}while(ML302_send_result());
			}
		}
		LED0=!LED0;//DS0闪烁
		LED1=!LED1;
//		Buffer[0] = '\0';
		first_flag |= 0xff;
		delay_ms(10);
	}
	
}


// 
// int main(void)
// {	 
//	char USART_RX_BUF1[11], USART_RX_BUF2[11];
//	u16 USART_RX_cnt2, USART_RX_cnt1;
//	u32 total,free;
//	u32 wp, rp;
//	delay_init();	    	 //延时函数初始化	  
//  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
//	uart_init(115200);	 	//串口初始化为115200	
// 	LED_Init();		  			//初始化与LED连接的硬件接口
//	KEY_Init();					//初始化按键
//	uc8088_init();		//8088初始化
//	EXTIX_Init(); //外部中断初始化
// 	my_mem_init(SRAMIN);		//初始化内部内存池

//	while(SD_Init())//检测不到SD卡
//	{
//		printf("SD Card Error!");
//		delay_ms(200);					
//		LED0=!LED0;//DS0闪烁
//	}
//	
// 	exfuns_init();							//为fatfs相关变量申请内存				 
//  f_mount(fs[0],"0:",1); 					//挂载SD卡 	
//		  
//	while(exf_getfree((u8*)'0', &total, &free))	//得到SD卡的总容量和剩余容量
//	{
//		printf("SD Card Fatfs Error!\n");
//		delay_ms(200);
//		LED0=!LED0;//DS0闪烁
//	}													  			    
// 					
//	printf("SD Total Size: %d    MB\r\n", total>>10);	//显示SD卡总容量 MB
//	printf("SD  Free Size: %d    MB\r\n", free>>10);	//显示SD卡剩余容量 MB	
//	
//	printf("\r\n******即将进行文件写入测试... ******\r\n");	
//	res_flash = f_open(&fnew, "0:gps_test_data\\out_test.log",FA_CREATE_ALWAYS | FA_WRITE );
//	if ( res_flash == FR_OK )
//	{
//		printf("》打开/创建FatFs读写测试文件.txt文件成功，向文件写入数据。\r\n");
//		LED0 = 1;
//		LED1 = 1;
//	}
//	else
//	{	
//		printf("！！打开/创建文件失败。\r\n");
//		LED0 = 0;
//		while(1)
//		{
//			delay_ms(300);
//			LED1 = ~LED1;
//		}
//	}
//	
//	printf("halt cpu\t");
//	rp = uc8088_read_u32(0x1a107018);
//	wp = uc8088_read_u32(config_enable_Addr);
//	printf("rp = %x,\t wp = %x\r\n", rp, wp);
//	
//	total = 0;
//	while(1)
//	{
//		
//		if (key0_flag)	// 按键按下
//		{
//			LED0 = 0;
//			LED1 = 0;
//			printf("文件保存成功\r\n");
//			while(1){
//				delay_ms(1000);
//				LED0 = ~LED0;
//				LED1 = ~LED1;
//			}
//		}
//		
//		// 如果写BUF2 已经写满
//		if(USART_RX_cnt2 == USART_REC_LEN){
//			res_flash = f_write(&fnew, USART_RX_BUF2, USART_REC_LEN, &fnum);
//			if (res_flash==FR_OK && fnum == USART_REC_LEN){
//				LED0 = 1;
//				LED1 = 0;
//				USART_RX_cnt2 = 0;
//			}
//			else
//				printf("！！文件写入失败：(res_flash = %d), fnum = %d\n",res_flash, fnum); 
//		}
//		else if(USART_RX_cnt1 == USART_REC_LEN){
//			res_flash = f_write(&fnew, USART_RX_BUF1, USART_REC_LEN, &fnum);
//			if (res_flash==FR_OK && fnum == USART_REC_LEN){
//				LED0 = 0;
//				LED1 = 1;
//				USART_RX_cnt1 = 0;
//			}
//			else
//				printf("！！文件写入失败：(res_flash = %d), fnum = %d\n",res_flash, fnum); 
//		}
//	} 
//}

////外部中断0服务程序 
//void EXTI0_IRQHandler(void)
//{
//	delay_ms(10);//消抖
//	if(WK_UP==1)	 	 //WK_UP按键
//	{				 
//		if (key0_flag == 0)
//		{
//			/* 不再读写，关闭文件 */
//			f_close(&fnew);	
//			/* 不再使用文件系统，取消挂载文件系统 */
//			f_mount(NULL,"0:",1);
//		}
//		
//		LED0 = 1;
//		LED1 = 1;
//		key0_flag = 1;
//	}
//	EXTI_ClearITPendingBit(EXTI_Line0); //清除LINE0上的中断标志位  
//}

//void EXTI4_IRQHandler(void)
//{
//	delay_ms(10);//消抖
//	if(KEY0==0)	 //按键KEY0
//	{
//		if (key0_flag == 0)
//		{
//			/* 不再读写，关闭文件 */
//			f_close(&fnew);	
//			/* 不再使用文件系统，取消挂载文件系统 */
//			f_mount(NULL,"0:",1);
//		}
//		
//		LED0=!LED0;
//		LED1=!LED1; 
//		key0_flag = 1;
//	}		 
//	EXTI_ClearITPendingBit(EXTI_Line4);  //清除LINE4上的中断标志位  
//}
// 
