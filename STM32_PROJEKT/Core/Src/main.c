// STM32 libs
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "dma.h"
#include "math.h"
#include "arm_math.h"
// STD libs
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
// USER libs
#include "BMP280_STM32.h"

// FUNKCJE SYSTEMOWE
void SystemClock_Config(void);

// OBSLUGA CZUJNIKA
void SensorConfiguration(void);

// FUNKCJE I ZMIENNE ZEGAROWE
extern TIM_HandleTypeDef htim4;
void MX_TIM_Init(void);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

// ZMIENNE I FUNKCJE DO TRANSMISJI DANYCH
void SendMessage(const char *message);
#define SendBuffer_SIZE 20
char SendBuffer[SendBuffer_SIZE];

// ZMIENNE I FUNKCJE ODBIERANIA DANYCH I ICH PRZETWARZANIA
#define ReceiveBuffer_SIZE 64
#define MainBuffer_SIZE 64
uint8_t ReceiveBuffer[ReceiveBuffer_SIZE];
uint8_t MainBuffer[MainBuffer_SIZE];
int ProcessDataFlag = 0;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

// POMIARY, NASTAWY I ZMIENNE REGULATORA
float Temperature, Pressure, Humidity;
float Tref = 25.0f; //temperatura zadana
float Kp = 1.0f;
float Ki = 0.0f;
float Kd = 0.0f;
arm_pid_instance_f32 PID;
float error = 0.0;
float U = 0.0;
int set_comp = 0;
float32_t R = 0.0;
int D_PWM = 1250;

int main(void)
{
	// Inicjalizacja peryferiów
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART3_UART_Init();
	MX_I2C1_Init();

	// Konfiguracja czujnika
	SensorConfiguration();

	// Inicjalizacja tim4
	MX_TIM_Init();
	HAL_TIM_Base_Start_IT(&htim4);

	// Inicjalizacja PWM
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);

	// Inicjalizacja regulatora PID
	PID.Kp = Kp;
	PID.Ki = Ki;
	PID.Kd = Kd;
	arm_pid_init_f32(&PID, 1);

	// Odbieranie nastaw z GUI
	HAL_UARTEx_ReceiveToIdle_DMA(&huart3, ReceiveBuffer, ReceiveBuffer_SIZE);

	while(1){}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	// Przerwanie zegara
	// 1) PRZETWORZENIE DANYCH UART
	// 1) POMIAR
	// 2) WYSLANIE POMIARU
	// 4) ALGORYM REGULACJI PID

	if(htim->Instance == TIM4){

		//Processing danych
		if(ProcessDataFlag == 1){

			// tfloat;
			if (MainBuffer[0] == 't') {
				sscanf((char*)&MainBuffer[1], "%f;", &Tref);
			}
			// pfloat,float,float;
			else if (MainBuffer[0] == 'p') {
				sscanf((char*)&MainBuffer[1], "%f,%f,%f;", &PID.Kp, &PID.Ki, &PID.Kd);
//				PID.Kp = Kp;
//				PID.Ki = Ki;
//				PID.Kd = Kd;
				PID.A0 = PID.Kp + PID.Ki + PID.Kd;
				PID.A1 = -PID.Kp - 2.0*PID.Kd;
				PID.A2 = PID.Kd;
				arm_pid_init_f32(&PID, 0);
			}
			ProcessDataFlag = 0;
		}

		// Pomiar
		BMP280_Measure();

		// Wyślij pomiar do terminala
		sprintf(SendBuffer, "%2.2f, %2.2f, %d;\r\n", Temperature, Tref, (int)(U*100.0));
		SendMessage(SendBuffer);

		// Zamknięty układ regulacji z regulatorem PID
		//Uchyb regulacji
		error = Tref - Temperature;
		// sygnał sterujący z regulatora
		R = arm_pid_f32(&PID, error);
		U = R/10.0;
		// Saturacja sygnału U
		U = (U <= 1.0) ? U : 1.0;
		U = (U >= 0.0) ? U : 0.0;
		// Przeliczenie U na set_compare
		set_comp = U * D_PWM;
		// Zadanie wypełnienia PWM
		__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, set_comp);
	}
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){

	// Funkcja do odbierania paczek danych z UART
    if(huart->Instance == USART3)
    {
    	if(ProcessDataFlag == 0){
    		memset(MainBuffer, '\000', MainBuffer_SIZE);
    		memcpy(MainBuffer, ReceiveBuffer, ReceiveBuffer_SIZE);
    		memset(ReceiveBuffer, '\000', ReceiveBuffer_SIZE);
    		ProcessDataFlag = 1;

    		HAL_UARTEx_ReceiveToIdle_DMA(&huart3, ReceiveBuffer, ReceiveBuffer_SIZE);
    	}

    	HAL_UARTEx_ReceiveToIdle_DMA(&huart3, ReceiveBuffer, ReceiveBuffer_SIZE);
    }
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

	// Konfiguracja czujnika
	int ret = BMP280_Config(OSRS_16, OSRS_16, OSRS_OFF, MODE_NORMAL, T_SB_1000, IIR_16);

	if (ret>=0){
		HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
	}else{
		HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
	}
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
