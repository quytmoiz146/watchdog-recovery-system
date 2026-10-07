#include "doc_loi.h"

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
		ghi_lai_loi(FAULT_LOOP);
		while(1);
	}else if(loi_2){
		ghi_lai_loi(FAULT_SENSOR);
		while(1);
	}else if(loi_3){
		ghi_lai_loi(FAULT_HARDFAULT);
		*(volatile uint32_t *)0xFFFFFFF0 = 1;
	}
}
