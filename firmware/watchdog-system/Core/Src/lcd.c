#include "lcd.h"


static void lcd_enable(uint8_t data);
static void udelay(uint8_t);

I2C_HandleTypeDef *lcd_i2c;



void lcd_init(I2C_HandleTypeDef *i2c){
	lcd_i2c = i2c;

	HAL_Delay(40);

	//Cấu hình cho LCD
	//RS RW D7 D6 D5 D4 là 0 0 0 0 1 1
	//Gửi qua PCF8574T
	//D7 D6 D5 D4 Backlight  EN RW RS
	//0  0  1  1  1          0  0  0 =>
	uint8_t data = 0x30 | LCD_SEND_CMD;

	for(int i = 0; i < 3; i++){
		HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data, 1, LCD_I2C_TIMEOUT);
		lcd_enable(data);
		HAL_Delay(5);
	}


	data = 0x20 | LCD_SEND_CMD;
	HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data, 1, LCD_I2C_TIMEOUT);
	lcd_enable(data);

	//function set command
	lcd_send_cmd(LCD_CMD_4BIT_2L_5X8);

	//display on and cursor on
	lcd_send_cmd(LCD_CMD_DIS_CTRL);

	//clear display
	lcd_display_clear();

	//Entry mode set
	lcd_send_cmd(LCD_CMD_ENTRY_INCRE);
}

void lcd_display_clear(void){
	lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY);

	HAL_Delay(2);
}


void lcd_send_cmd(uint8_t cmd){
	uint8_t high_nibble = (cmd & 0xF0);
	uint8_t low_nibble = ((cmd & 0x0F) << 4);

	//RS = 0 để gửi cmd
	//RnW = 0 để ghi
	//D7 D6 D5 D4 Backlight E RW RS

	uint8_t data = high_nibble | LCD_SEND_CMD | LCD_BACKLIGHT;
	HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data, 1, LCD_I2C_TIMEOUT);

	lcd_enable(data);

	data = low_nibble | LCD_SEND_CMD | LCD_BACKLIGHT;

	HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data, 1, LCD_I2C_TIMEOUT);

	lcd_enable(data);
}


void lcd_print_char(uint8_t data_char){
	uint8_t high_nibble = (data_char & 0xF0);
	uint8_t low_nibble = ((data_char & 0x0F) << 4);

	//RS = 1 để gửi data
	//RnW = 0 để ghi
	//D7 D6 D5 D4 Backlight E RW RS

	uint8_t data = high_nibble | LCD_SEND_DATA | LCD_BACKLIGHT;
	HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data, 1, LCD_I2C_TIMEOUT);

	lcd_enable(data);

	data = low_nibble | LCD_SEND_DATA | LCD_BACKLIGHT;

	HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data, 1, LCD_I2C_TIMEOUT);

	lcd_enable(data);
}


void lcd_print_string(char *message){
	uint8_t current_char;
	while(*message != '\0'){
		current_char = (uint8_t)*message++;
		lcd_print_char(current_char);
	}
}


void lcd_display_return_home(void){
	lcd_send_cmd(LCD_CMD_RETURN_HOME);

	HAL_Delay(2);
}


void lcd_set_cursor(uint8_t row, uint8_t column){
	if(column > 0){
		column--;
	}
	uint8_t cmd = 0;
	if(row == 1){
		cmd = column | 0x80;
	}else if(row == 2){
		cmd = column | 0xC0;
	}else if(row < 1 || row > 2) return;
	lcd_send_cmd(cmd);
}


static void lcd_enable(uint8_t data){
	//D7 D6 D5 D4 Backlight E RW RS
	//EN = 1

	uint8_t data_en = data | LCD_EN_BIT;
	HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data_en, 1, LCD_I2C_TIMEOUT);
	udelay(150);

	//EN = 0
	HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDRESS, &data, 1, LCD_I2C_TIMEOUT);
	udelay(150);
}
static void udelay(uint8_t t){
	for(volatile int i = 0; i < t; i++);
}
