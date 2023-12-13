#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "BMP280_STM32.h"

void SystemClock_Config(void);
void SensorConfiguration(void);

float Temperature, Pressure, Humidity;

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART3_UART_Init();
	MX_TIM4_Init();
	MX_I2C1_Init();

	SensorConfiguration();

	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);

	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);

	while (1)
	{
		// Pomiar
		BMP280_Measure();

		// Wiadomosc do terminala
		HAL_Delay(500);
		char data[255] = "";
		sprintf(data, "Pressure: %2.1f Temperature %2.1f C \r\n", (float)Pressure, (float)Temperature);
		HAL_UART_Transmit(&huart3, (uint8_t*)data, strlen(data), HAL_MAX_DELAY);
		HAL_Delay(100);
	}
}

void SensorConfiguration(void){

	// Konfiguracja
	int ret = BMP280_Config(OSRS_16, OSRS_16, OSRS_OFF, MODE_NORMAL, T_SB_1000, IIR_16);

	// Wiadomosc do terminala
	HAL_Delay(500);
	char msg[] = "Start up. \r\n";
	HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
	if (ret>=0){
		sprintf(msg, "Correct %d\r\n", (int) ret);
		HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
	}else{
		sprintf(msg, "Fail %d\r\n", (int) ret);
		HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
	}
	HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 50);
}

void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	HAL_PWR_EnableBkUpAccess();

	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 72;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 3;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}

void Error_Handler(void)
{
	__disable_irq();
	while(1){}
}

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{}
#endif
