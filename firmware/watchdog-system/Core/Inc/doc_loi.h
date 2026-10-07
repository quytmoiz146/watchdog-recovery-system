

#ifndef INC_DOC_LOI_H_
#define INC_DOC_LOI_H_

#include "main.h"
#include "string.h"
#include "stdio.h"
#include "lcd.h"

typedef enum{
	FAULT_NONE,
	FAULT_LOOP,
	FAULT_SENSOR,
	FAULT_HARDFAULT
}FaultCode;

typedef struct{
	const char* cause;
	uint32_t count;
	FaultCode fault;
}ResetInfor;

void doc_loi_Init(ResetInfor *infor);

void ghi_lai_loi(FaultCode f);

void hien_thi_loi(ResetInfor *infor);

#endif /* INC_DOC_LOI_H_ */
