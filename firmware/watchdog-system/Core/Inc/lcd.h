
#ifndef LCD_H_
#define LCD_H_

#include "stm32f1xx_hal.h"

#define LCD_ADDRESS				(0x27 << 1)

//D7 D6 D5 D4 Backlight E RW RS

#define LCD_BACKLIGHT   		(1 << 3)
#define LCD_EN_BIT  			(1 << 2)
#define LCD_RW_BIT    			(1 << 1)
#define LCD_RS_BIT    			(1 << 0)

//Backlight E RW RS: 1 0 0 0
#define LCD_SEND_CMD		LCD_BACKLIGHT
//Backlight E RW RS: 1 0 0 1
#define LCD_SEND_DATA		LCD_BACKLIGHT | LCD_RS_BIT

//LCD command
#define LCD_CMD_CLEAR_DISPLAY	(0x01)
#define LCD_CMD_RETURN_HOME		(0x02)
#define LCD_CMD_4BIT_2L_5X8    	(0x28)
#define LCD_CMD_ENTRY_INCRE		(0x06)
#define LCD_CMD_DIS_CTRL		(0x0E)

#define LCD_I2C_TIMEOUT 		100

void lcd_init(I2C_HandleTypeDef *i2c);
void lcd_send_cmd(uint8_t cmd);
void lcd_display_clear(void);
void lcd_display_return_home(void);
void lcd_print_char(uint8_t data_char);
void lcd_print_string(char *message);
void lcd_set_cursor(uint8_t row, uint8_t column);

#endif /* LCD_H_ */
