#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h" 		 	 
//#include "key.h"     
//#include "exti.h"
//#include "malloc.h"
//#include "sdio_sdcard.h"     
//#include "ff.h"  
//#include "exfuns.h"    
#include "uc8088_spi.h"
#include "wdg.h"

#include <stdbool.h>
//#include <string.h>
volatile bool which_buf = 0;
volatile u8  send_flag = 1;
volatile u16 str_len[2] = {0};
u8 Buffer[2][SPI_BUF_LEN] = {0};
//const char str_OK[]="SEND OK";

void ByteChange(register u8 *pBuf, s16 len)
{
	register s16 i;
	register u8 ucTmp;
	
	for(i = 0; i < len; i += 4)
	{
			ucTmp = pBuf[i+3];
			pBuf[i+3] = pBuf[i];
			pBuf[i] = ucTmp;
			ucTmp = pBuf[i+2];
			pBuf[i+2] = pBuf[i+1];
			pBuf[i+1] = ucTmp;
	}
}

void ML302_init()
{
	printf("AT\r\n");			
	delay_ms(10);
	printf("AT+CPIN?\r\n");		//查询SIM卡状态
	delay_ms(10);
	printf("AT+CSQ\r\n");			//查询信号质量， 小于10说明信号差
	delay_ms(10);
//	printf("AT+CGDCONT=1,\"IP\",\"CMIOT\"\r\n");		//设置APN
//	delay_ms(10);
	printf("AT+CGACT=1,1\r\n");		//激活PDP
	delay_ms(20);
	printf("AT+MIPOPEN=1,\"TCP\",\"server.natappfree.cc\",43328\r\n");	//连接服务器
//		printf("AT+MIPOPEN=1,\"TCP\",\"47.104.157.10\",15884\r\n");	//连接服务器
	delay_ms(20);
	
//	printf("AT+CMUX=0,0,6,127,10,3,30,10,2\r\n");	//波特率设置为230400
	delay_ms(10);
	printf("ATE0\r\n");				//关闭回显
	delay_ms(10);
	memset(USART_RX_BUF, 0, USART_REC_LEN);
	USART_RX_STA = 0;
}


//int ML302_send_result()
//{
//	u32 time_out = 1e6;
//	while(time_out && !(USART_RX_STA & 0x8000))		//等待 ML302 返回发送结果
//	{	time_out--; LED1=!LED1;}
//	if(!time_out)
//		printf("***---%s---%x****", USART_RX_BUF, USART_RX_STA);
//	USART_RX_STA = 0;
//	if (NULL == strstr(USART_RX_BUF, str_OK))
//	{
//		delay_ms(1);
//		printf("-*-*-*%s---%x*-*-*", USART_RX_BUF, USART_RX_STA);
//		memset(USART_RX_BUF, 0, USART_REC_LEN);
//		return -1;
//	}
//	else
//	{
//		memset(USART_RX_BUF, 0, USART_REC_LEN);
//		return 0;
//	}
//}

void uart_send_data_2_ML302(register u8 *TX_BUF, u16 len)
{
	char num_char[5];
	register u16 i;
	sprintf(num_char, "%d", len);
	printf("AT+MIPSEND=1,%s\r\n", num_char);
	//TX_BUF[len] = '\0';
	//printf("%s", TX_BUF);
	for(i=0; i<len; i++){
		USART1->DR = TX_BUF[i];
		while((USART1->SR&0X40)==0);//等待该字符发送完毕   
	}
}


void Resend()
{
		printf("Resend\r\n");
		send_flag = 0;
		uart_send_data_2_ML302(Buffer[!which_buf], str_len[!which_buf]);
}

//FATFS fs;													/* FatFs文件系统对象 */
//FIL fnew;													/* 文件对象 */
//FRESULT res_flash;                /* 文件操作结果 */
//UINT fnum;            					  /* 文件成功读写数量 */
////BYTE ReadBuffer[1024]={0};        /* 读缓冲区 */
//BYTE WriteBuffer[] = "随便写到文件中123abc";         /* 写缓冲区*/

int main(void)
{		
	volatile u8  rp_OK, wp_OK, cnt, wp_stop_flag;
	volatile u16 tmp, tmp1, len;
	volatile u32 rrp;
	u32 wp, rp;

	delay_init();	    	 //延时函数初始化	  
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	uart_init(230400);	 	//串口初始化为230400
 	LED_Init();		  			//初始化与LED连接的硬件接口
	//KEY_Init();					//初始化按键
	uc8088_init();		//8088初始化
	
	printf("halt cpu\t");
	rp = uc8088_read_u32(0x1a107018);
	printf("read test = %x\r\n", rp);

	wp = uc8088_read_u32(Buf_addr);
	rp = uc8088_read_u32(Buf_addr + 4);
	printf("wp = %d,   rp = %d\r\n", wp, rp); 
	LED0 = 1;
	ML302_init();
	wp = 65536;
	rrp = rp;

	wp_stop_flag = 0;
	IWDG_Init(5,625);    //与分频数为128,重载值为625,溢出时间为2s
	send_flag = 1;

	while(1){
		if (send_flag == 2){
				//Resend();
				printf("\r\nThe return value is not 'OK'!\r\n");
				send_flag = 1;
		}

		cnt = 0;
		do
		{
			uc8088_read_2_u32(Buf_addr, &wp, &rp);
			if (rp == rrp && wp < SPI_BUF_LEN)
			{
				rp_OK = 1;
				len = (rp <= wp) ? (wp - rp) : (SPI_BUF_LEN - rp + wp);
				if (len < 64){
					wp_OK = 0;
					wp_stop_flag = 1;
				}
				else if((len + str_len[which_buf]) > SPI_BUF_LEN)
					wp_OK = 0;
				else 
					wp_OK = 1;
				break;
			}

			if(cnt++ > 5)
			{
				wp_OK = 0;
				printf("rrp = %d,  rp = %d,  wp = %d!\r\n", rrp, rp, wp);
				break;
			}
		}while(1);
		
		if(rp_OK == 1 && wp_OK == 1){
			LED1 = 0;
			printf("    rp = %d,  wp = %d\r\n", rp, wp);
			
			if (rp < wp){
				tmp = uc8088_read_memory(Buf_addr + 8 + rp, Buffer[which_buf] + str_len[which_buf], len);
				tmp1 = rp+tmp;
				str_len[which_buf] += tmp;
			}
			else{
				tmp = uc8088_read_memory(Buf_addr + 8 + rp, Buffer[which_buf]+ str_len[which_buf], SPI_BUF_LEN - rp);
				tmp1 = uc8088_read_memory(Buf_addr + 8, Buffer[which_buf]+ str_len[which_buf] + tmp, wp);
				tmp += tmp1;
				str_len[which_buf] += tmp;
			}
			
			ByteChange(Buffer[which_buf] + str_len[which_buf] - tmp, tmp);		//字节翻转
			
			cnt = 0;  	rp_OK=0;		wp_OK=0;  rrp = 65536;
			
			do{
					uc8088_write_u32(Buf_addr + 4, tmp1);
					rrp = uc8088_read_u32(Buf_addr + 4);
					
					if(cnt++ > 5)		//修改读指针失败
					{
						printf("write rp = %d, not eq %d, so is error!\r\n", rrp, tmp1);
						break;
					}
			}while(rrp != tmp1);
		}
		
		//收到1KB以上内容就发给ML302
		if(str_len[which_buf] > 1024 && send_flag == 1)
		{
//			send_flag = 0;
//			LED0 = 0;			//灯亮  忙, STM32 不能读取uc8088, 故可能丢数据
//			ByteChange(Buffer[which_buf], str_len[which_buf]);		//字节翻转
//			cnt = 0;
//			do{
			uart_send_data_2_ML302(Buffer[which_buf], str_len[which_buf]);
			
//				if (cnt++ > 5){
//					printf("send to ML302 error\r\n");
//					break;
//				}
//			}while(ML302_send_result());
//			LED0 = 1;
			send_flag = 0;
			which_buf = !which_buf;
			str_len[which_buf] = 0;
//			memset(Buffer[which_buf], 0, SPI_BUF_LEN);
			IWDG_Feed();		//喂狗
		}
		
		if(wp_stop_flag)		//空闲
		{
			LED1 = 1;
			wp_stop_flag = 0;
			IWDG_Feed();		//喂狗
		}
		
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
