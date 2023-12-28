// STM32 libs
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "math.h"
// STD libs
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
// USER libs
#include "BMP280_STM32.h"


// DEKLARACJE FUNKCJI
void SystemClock_Config(void);
void SensorConfiguration(void);
void MX_TIM_Init(void);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void SendMessage(const char *message);

// ZMIENNE GLOBALNE
extern TIM_HandleTypeDef htim4;
float Temperature, Pressure, Humidity;
float Tref = 0.0f;

// ZMIENNE DO OBSLUGI UART
char result[20];
char rx_Buffer[20] = "00000000000000000000";
uint8_t rx_Received;
uint8_t rx_Index = 0;

int main(void)
{
	// Inicjalizacja peryferiów
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART3_UART_Init();
	MX_I2C1_Init();

	// Konfiguracja czujnika
	SensorConfiguration();

	// Inicjalizacja tim4
	MX_TIM_Init();
	HAL_TIM_Base_Start_IT(&htim4);

	// Wypełnienie PWM 1000/10000*100% = 10%
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 1000);

	// Zadawanie Tref przez UART
	HAL_UART_Receive_IT(&huart3, &rx_Received, 1);

	while(1){}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	// Przerwanie zegara
	// Tutaj dodaj:
	// 1) POMIAR
	// 2) WYSLANIE POMIARU
	// 4) ALGORYM REGULACJI PID

	if(htim->Instance == TIM4){
		// Pomiar
		BMP280_Measure();

		// Wyślij pomiar do terminala
		sprintf(result, "%2.1f;\r\n", (float)Temperature);
		SendMessage(result);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	// Odbieranie nastaw przez UART
	if(rx_Received == ';'){
		char ext[rx_Index];
		strncpy(ext, rx_Buffer, rx_Index);
		Tref = atof(ext);

		rx_Index = 0;
		memset(rx_Buffer, '0', sizeof(rx_Buffer));
		rx_Received = 0;
	}
	else{
		rx_Buffer[rx_Index] = (char)rx_Received;
		rx_Index++;
	}
	HAL_UART_Receive_IT(&huart3, (uint8_t *)&rx_Received, 1);
}

void SendMessage(const char *message){
	// Wysyłanie wiadomości do UART

	if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)message, strlen(message)) != HAL_OK) {
		Error_Handler();
	}
}

void MX_TIM_Init(void){
	// Redefinicja funkcji bibliotecznej MX_TIM4_Init(); Okres PWM = 500ms

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 3599;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 9999;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 500;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}

	HAL_TIM_MspPostInit(&htim4);
}

void SensorConfiguration(void){

	// Konfiguracja
	int ret = BMP280_Config(OSRS_16, OSRS_16, OSRS_OFF, MODE_NORMAL, T_SB_1000, IIR_16);

	// Wiadomosc do terminala
	HAL_Delay(100);
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
