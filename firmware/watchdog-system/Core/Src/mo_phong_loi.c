#include "doc_loi.h"
#include "system_status.h"

uint32_t tick = 0;

uint8_t loi_1 = 0;
uint8_t loi_2 = 0;
uint8_t loi_3 = 0;
uint8_t loi_4 = 0;


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if(GPIO_Pin == GPIO_PIN_0){
		if(HAL_GetTick() - tick > 200){
			loi_1 = 1;
			tick = HAL_GetTick();
		}
	}else if(GPIO_Pin == GPIO_PIN_1){
		if(HAL_GetTick() - tick > 200){
			loi_2 = 1;
			tick = HAL_GetTick();
		}
	}else if(GPIO_Pin == GPIO_PIN_2){
		if(HAL_GetTick() - tick > 200){
			loi_3 = 1;
			tick = HAL_GetTick();
		}
	}else if(GPIO_Pin == GPIO_PIN_3){
		if(HAL_GetTick() - tick > 200){
			loi_4 = 1;
			tick = HAL_GetTick();
		}
	}
}

void quet_loi(void){
	if(loi_1){
		Status_LogMessage("[FAULT] PA0: Mo phong treo vong lap vo han (while(1))...");
		Status_SetHeartbeat(HB_FAULT);
		ghi_lai_loi(FAULT_LOOP);
		while(1);
	}else if(loi_2){
		Status_LogMessage("[FAULT] PA1: Mo phong cho cam bien DHT11 phan hoi vo han...");
		Status_SetHeartbeat(HB_FAULT);
		ghi_lai_loi(FAULT_SENSOR);
		/* Gia lap cho tin hieu cam bien ma khong co timeout -> khong bao khoe -> IWDG reset */
		while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET || 1);
	}else if(loi_3){
		Status_LogMessage("[FAULT] PA2: Mo phong HardFault (truy cap dia chi sai 0xFFFFFFF0)...");
		Status_SetHeartbeat(HB_FAULT);
		ghi_lai_loi(FAULT_HARDFAULT);
		*(volatile uint32_t *)0xFFFFFFF0 = 1;
	}
}
