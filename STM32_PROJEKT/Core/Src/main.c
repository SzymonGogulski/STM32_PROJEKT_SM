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
#include "FLASH_SECTOR.h"
#include "stdint.h"

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
int ProcessDataFlag;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

// POMIARY, NASTAWY I ZMIENNE REGULATORA
typedef struct {
    float Tref;
    float Kp;
    float Ki;
    float Kd;
} Settings;
Settings data;
arm_pid_instance_f32 PID;
float Temperature, Pressure, Humidity;
//float Tref;
float error;
float32_t R;
float U;
int set_comp;
const int D_PWM = 1250;

// OBSLUGA PAMIECI FLASH
void InitializeSettings(Settings *data);
void SaveSettings(Settings *data);
uint32_t SettingsAddress = 0x08080000;

// FILTR CYFROWY DOLNPORZEPUSTOWY 10HZ
// PARAMETRY FILTRÓW
//gain and SOS matrix:
//    0.0676
//
//    1.0000    0.9922         0    1.0000    0.2310         0
//    1.0000    2.0144    1.0144    1.0000    0.4847    0.1051
//    1.0000    2.0034    1.0034    1.0000    0.5621    0.2818
//    1.0000    1.9901    0.9901    1.0000    0.7309    0.6667
//{1.0000,0.9922,0,-0.2310,0,
//1.0000,2.0144,1.0144,-0.4847,-0.1051,
//1.0000,2.0034,1.0034,-0.5621,-0.2818,
//1.0000,1.9901,0.9901,-0.7309,-0.6667};
// IIR
//float32_t iir_coeffs[] = {	1.0000,	0.9922,	0,		-0.2310,	0,
//							1.0000,	2.0144,	1.0144,	-0.4847,	-0.1051,
//							1.0000,	2.0034,	1.0034,	-0.5621,	-0.2818,
//							1.0000,	1.9901,	0.9901,	-0.7309,	-0.6667};
//float32_t iir_state[16] = {0};
//float32_t iir_gain = 0.0676;
//arm_biquad_casd_df1_inst_f32 iir_filter = {.numStages=4, .pState=iir_state, .pCoeffs=iir_coeffs};
//
//float TemperatureFiltered; // WYJŚCIE FILTRU IIR



int main(void){
	// Inicjalizacja peryferiów
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART3_UART_Init();
	MX_I2C1_Init();

	// Konfiguracja czujnika
	SensorConfiguration();

	// Inicjalizacja regulatora PID i zmiennych
	// Wczytanie poprzednich nastaw z FLASH


	InitializeSettings(&data);
	PID.Kp = data.Kp;
	PID.Ki = data.Ki;
	PID.Kd = data.Kd;
	arm_pid_init_f32(&PID, 1);

	// Inicjalizacja tim4
	MX_TIM_Init();
	HAL_TIM_Base_Start_IT(&htim4);

	// Inicjalizacja PWM
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);

	// Odbieranie nastaw z GUI
	HAL_UARTEx_ReceiveToIdle_DMA(&huart3, ReceiveBuffer, ReceiveBuffer_SIZE);

	while(1){
		//Processing danych
		if(ProcessDataFlag == 1){

			// tfloat;
			if (MainBuffer[0] == 't') {
				float tempT;
				sscanf((char*)&MainBuffer[1], "%f;", &tempT);
				if (tempT >= 25.0 && tempT <= 75.0){
					data.Tref = tempT;
					SaveSettings(&data);
				}
			}
			// pfloat,float,float;
			else if (MainBuffer[0] == 'p') {
				sscanf((char*)&MainBuffer[1], "%f,%f,%f;", &data.Kp, &data.Ki, &data.Kd);
				PID.Kp = data.Kp;
				PID.Ki = data.Ki;
				PID.Kd = data.Kd;
				PID.A0 = PID.Kp + PID.Ki + PID.Kd;
				PID.A1 = -PID.Kp - 2.0*PID.Kd;
				PID.A2 = PID.Kd;
				arm_pid_init_f32(&PID, 0);
				SaveSettings(&data);
			}
			ProcessDataFlag = 0;
		}}
}

// FUNKCJE UZYTKOWNIKA -----------------------------------------
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){

	// PRZERWANIE ZEGARA
	// 1) POMIAR
	// 2) FILTR CYFROWY
	// 3) WYSLANIE POMIARU
	// 4) ALGORYM REGULACJI PID

	if(htim->Instance == TIM4){

		// Pomiar
		BMP280_Measure();

//		arm_biquad_cascade_df1_f32(&iir_filter, &Temperature, &TemperatureFiltered, 1);
//		TemperatureFiltered = TemperatureFiltered * iir_gain;

		// Wyslij pomiar do terminala
		sprintf(SendBuffer, "%2.2f, %2.2f, %d;\r\n", Temperature, data.Tref, (int)(U*100.0)); //TemperatureFiltered
		SendMessage(SendBuffer);

		// Zamkniety uklad regulacji z regulatorem PID
		//Uchyb regulacji
		error = data.Tref - Temperature; //TempeartureFiltered
		// sygnal sterujacy z regulatora
		R = arm_pid_f32(&PID, error);
		U = R/10.0;
		// Saturacja sygnalu U
		U = (U <= 1.0) ? U : 1.0;
		U = (U >= 0.0) ? U : 0.0;
		// Przeliczenie U na set_compare
		set_comp = U * D_PWM;
		// Zadanie wypelnienia PWM
		__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, set_comp);
	}
}

void InitializeSettings(Settings *data) {

	float tempTref;
	float tempKp;
	float tempKi;
	float tempKd;

	// Wczytanie ustawien zapisanych w FLASH
	tempTref = Flash_Read_NUM(SettingsAddress);
	tempKp = Flash_Read_NUM(SettingsAddress + 4);
	tempKi = Flash_Read_NUM(SettingsAddress + 8);
	tempKd = Flash_Read_NUM(SettingsAddress + 12);


    // Sprawdzenie czy wczytane ustawienia są poprawne
    if (tempTref < 25.0 || tempTref > 75.0 ||
    		tempKp < 0.0 || tempKp > 1000.0 ||
			tempKi < 0.0 || tempKi > 1000.0 ||
			tempKd < 0.0 || tempKd > 1000.0)
    {
    	// jeżeli ustawienia są niepoprawne to:
    	//Inicjalizacja ustawien domyslnych
        data->Tref = 25.0;
        data->Kp = 1.0;
        data->Ki = 0.0;
        data->Kd = 0.0;

        // Zapisanie ustawien domyslnych
    	float floatArray[4] = {25.0, 1.0, 0.0, 0.0};
    	Flash_Write_NUM(0x08080000, floatArray);
    }
    else{
    	// Wczytanie poprawynych danych do zmiennych
        data->Tref = tempTref;
        data->Kp = tempKp;
        data->Ki = tempKi;
        data->Kd = tempKd;
    }

}

void SaveSettings(Settings *data){

	float floatArray[4] = {data->Tref, data->Kp, data->Ki, data->Kd};
	Flash_Write_NUM(0x08080000, floatArray);
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

	// Wysylanie wiadomosci do UART
	if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)message, strlen(message)) != HAL_OK) {
		Error_Handler();
	}
}

void MX_TIM_Init(void){

	// Redefinicja funkcji bibliotecznej MX_TIM4_Init(); Okres PWM = 500ms
	// 719 -> 100ms
	// 539 -> 75ms
	// 3599 -> 500ms
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

// FUNKCJE SYSTEMOWE -----------------------------------------
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
