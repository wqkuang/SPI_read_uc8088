#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h" 		 	 
#include "key.h"     
#include "exti.h"
#include "malloc.h"
#include "sdio_sdcard.h"     
#include "ff.h"  
#include "exfuns.h"    

 
/************************************************
程序功能，存储8088串口打印的数据到SD卡
************************************************/
//FATFS fs;													/* FatFs文件系统对象 */
FIL fnew;													/* 文件对象 */
FRESULT res_flash;                /* 文件操作结果 */
UINT fnum;            					  /* 文件成功读写数量 */
//BYTE ReadBuffer[1024]={0};        /* 读缓冲区 */
BYTE WriteBuffer[] = "随便写到文件中123abc";         /* 写缓冲区*/

 int main(void)
 {	 
	u32 total,free;

	delay_init();	    	 //延时函数初始化	  
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	uart_init(115200);	 	//串口初始化为115200	
 	LED_Init();		  			//初始化与LED连接的硬件接口
	KEY_Init();					//初始化按键
	EXTIX_Init(); //外部中断初始化
 	my_mem_init(SRAMIN);		//初始化内部内存池

	while(SD_Init())//检测不到SD卡
	{
		printf("SD Card Error!");
		delay_ms(200);					
		LED0=!LED0;//DS0闪烁
	}
	
 	exfuns_init();							//为fatfs相关变量申请内存				 
  f_mount(fs[0],"0:",1); 					//挂载SD卡 	
		  
	while(exf_getfree((u8*)'0', &total, &free))	//得到SD卡的总容量和剩余容量
	{
		printf("SD Card Fatfs Error!\n");
		delay_ms(200);
		LED0=!LED0;//DS0闪烁
	}													  			    
 					
	printf("SD Total Size: %d    MB\r\n", total>>10);	//显示SD卡总容量 MB
	printf("SD  Free Size: %d    MB\r\n", free>>10);	//显示SD卡剩余容量 MB	
	
	printf("\r\n******即将进行文件写入测试... ******\r\n");	
	res_flash = f_open(&fnew, "0:gps_test_data\\out_test.log",FA_CREATE_ALWAYS | FA_WRITE );
	if ( res_flash == FR_OK )
	{
		printf("》打开/创建FatFs读写测试文件.txt文件成功，向文件写入数据。\r\n");
		LED0 = 1;
		LED1 = 1;
    /* 将指定存储区内容写入到文件内 */
//		res_flash=f_write(&fnew,WriteBuffer,sizeof(WriteBuffer),&fnum);
//    if(res_flash==FR_OK)
//    {
//     printf("》文件写入成功，写入字节数据：%d\n",fnum);
//      printf("》向文件写入的数据为：\r\n%s\r\n",WriteBuffer);
//		}
//    else
//    {
//      printf("！！文件写入失败：(%d)\n",res_flash);
//    }    
//		/* 不再读写，关闭文件 */
//    f_close(&fnew);
	}
	else
	{	
		printf("！！打开/创建文件失败。\r\n");
		LED0 = 0;
		while(1)
		{
			delay_ms(300);
			LED1 = ~LED1;
		}
	}
	
/*------------------- 文件系统测试：读测试 ------------------------------------*/
//	printf("****** 即将进行文件读取测试... ******\r\n");
//	res_flash = f_open(&fnew, "0:gps_test_data\\FatFs读写测试文件.txt", FA_OPEN_EXISTING | FA_READ); 	 
//	if(res_flash == FR_OK)
//	{
//		printf("》打开文件成功。\r\n");
//		res_flash = f_read(&fnew, ReadBuffer, sizeof(ReadBuffer), &fnum); 
//    if(res_flash==FR_OK)
//    {
//      printf("》文件读取成功,读到字节数据：%d\r\n",fnum);
//      printf("》读取得的文件数据为：\r\n%s \r\n", ReadBuffer);	
//    }
//    else
//    {
//      printf("！！文件读取失败：(%d)\n",res_flash);
//    }		
//	}
//	else
//	{
//		printf("！！打开文件失败。\r\n");
//	}
	
	total = 0;
	while(1)
	{
		
		if (key0_flag)	// 按键按下
		{
			/* 不再读写，关闭文件 */
			f_close(&fnew);	
			/* 不再使用文件系统，取消挂载文件系统 */
			f_mount(NULL,"0:",1);
			LED0 = 0;
			LED1 = 0;
			printf("文件保存成功\r\n");
			while(1){
				delay_ms(1000);
				LED0 = ~LED0;
				LED1 = ~LED1;
			}
		}
		if (write_slow_flag)	// BUF中的数据没来得及写入SD卡
		{
			/* 不再读写，关闭文件 */
			f_close(&fnew);	
			/* 不再使用文件系统，取消挂载文件系统 */
			f_mount(NULL,"0:",1);
			LED0 = 1;
			LED1 = 1;
			printf("读取BUF 太慢\r\n");
			while(1){
			}
		}
		// 如果现在在写BUF1 ， 且USART_RX_cnt2 已经写满
		if((witch_BUF == 1) && (USART_RX_cnt2 == USART_REC_LEN)){
			res_flash=f_write(&fnew, USART_RX_BUF2, USART_REC_LEN, &fnum);
			if (fnum == USART_REC_LEN){
				USART_RX_cnt2 = 0;
				LED0 = 1;
				LED1 = 0;
				//printf("%c", USART_RX_BUF2[511]);
			}
			else
				printf("！！文件写入失败：(%d)\n",res_flash); 
		}
		else if(witch_BUF == 2 && USART_RX_cnt1 == USART_REC_LEN){
			res_flash=f_write(&fnew, USART_RX_BUF1, USART_REC_LEN, &fnum);
			if (fnum == USART_REC_LEN){
				USART_RX_cnt1 = 0;
				LED0 = 0;
				LED1 = 1;
				//printf("%s", USART_RX_BUF1);
			}
			else
				printf("！！文件写入失败：(%d)\n",res_flash); 
		}
	} 
}


