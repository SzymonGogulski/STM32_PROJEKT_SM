// STM32 libs
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "crc.h"
#include "usart.h"
#include "gpio.h"
#include "dma.h"
#include "math.h"
#include "arm_math.h"
#include "rtc.h"

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

// obliczanie CRC
unsigned int calculateCRC(char* data, uint32_t dataSize);

// FUNKCJE I ZMIENNE ZEGAROWE
extern TIM_HandleTypeDef htim4;
extern RTC_HandleTypeDef hrtc;
void MX_TIM_Init(void);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

// ZMIENNE I FUNKCJE DO TRANSMISJI DANYCH
void SendMessage(const char *message);
#define SendBuffer_SIZE 120
char SendBuffer[SendBuffer_SIZE];

// ZMIENNE I FUNKCJE ODBIERANIA DANYCH I ICH PRZETWARZANIA
#define ReceiveBuffer_SIZE 120
#define MainBuffer_SIZE 120
uint8_t ReceiveBuffer[ReceiveBuffer_SIZE];
uint8_t MainBuffer[MainBuffer_SIZE];
int ProcessDataFlag;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

// RTC
void MX_RTC_Init(void);
RTC_TimeTypeDef time;
RTC_DateTypeDef date;

// POMIARY, NASTAWY I ZMIENNE REGULATORA
typedef struct {
    float Tref;
    float Kp;
    float Ki;
    float Kd;
} Settings;
void InitializeSettings(Settings *data);
void SaveSettings(Settings *data);
Settings data, temp;
arm_pid_instance_f32 PID;
float Temperature, Pressure, Humidity;
float Tref;
float error;
float32_t R;
float U;
int set_comp;
const int D_PWM = 1250;
uint32_t calculateCRCValue;
uint32_t receivedCRCValue;


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
	// Inicjalizacja RTC
    HAL_RTC_Init(&hrtc);
	MX_RTC_Init();
	// Inicjalizacja tim4
	MX_TIM_Init();
	HAL_TIM_Base_Start_IT(&htim4);

	// Inicjalizacja PWM
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);

	// Inicjalizacja regulatora PID i zmiennych
	InitializeSettings(&data);
	Tref = data.Tref;
	PID.Kp = data.Kp;
	PID.Ki = data.Ki;
	PID.Kd = data.Kd;
	arm_pid_init_f32(&PID, 1);

	// Odbieranie nastaw z GUI
	HAL_UARTEx_ReceiveToIdle_DMA(&huart3, ReceiveBuffer, ReceiveBuffer_SIZE);
	// Inicjalizacja CRC
    HAL_CRC_Init(&hcrc);
	MX_CRC_Init();



	while(1){
	}
}

// FUNKCJE UZYTKOWNIKA -----------------------------------------
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){

	// PRZERWANIE ZEGARA
	// 1) PRZETWORZENIE DANYCH UART
	// Dodaj 1.2) ZAPISANIE NOWYCH NASTAW DO EPROM
	// 2) POMIAR
	// 3) WYSLANIE POMIARU
	// 4) ALGORYM REGULACJI PID
	// Dodaj 4.2) FILTR CYFROWY

	if(htim->Instance == TIM4){
		HAL_RTC_GetTime(&hrtc,  &time,  RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc,  &date,  RTC_FORMAT_BIN);

		//Processing danych
		if(ProcessDataFlag == 1){
			calculateCRCValue = calculateCRC((char*) MainBuffer, sizeof(MainBuffer));

			// tfloat;
			if (MainBuffer[0] == 't') {
				// aktualną Tref zapisujemy do zmiennej
				temp.Tref = Tref;
				sscanf((char*)&MainBuffer[1], "%f;CRC:%lu;", &Tref, &receivedCRCValue);

				// jeśli CRC nie są identyczne to przypisujemy poprzednią Tref zamiast odebranej z GUI
				if(calculateCRCValue != receivedCRCValue) {
					Tref = temp.Tref;
				}
			}
			// pfloat,float,float;
			else if (MainBuffer[0] == 'p') {
				// aktualne nastawy zapisujemy do zmiennych
				temp.Kp = PID.Kp;
				temp.Ki = PID.Ki;
				temp.Kd = PID.Kd;
				sscanf((char*)&MainBuffer[1], "%f,%f,%f;CRC:%lu;", &PID.Kp, &PID.Ki, &PID.Kd, &receivedCRCValue);

				// jeśli CRC nie są identyczne to przypisujemy poprzednie nastawy PID, zamiast odebranych z GUI
				if(calculateCRCValue != receivedCRCValue) {
					PID.Kp = temp.Kp;
					PID.Ki = temp.Ki;
					PID.Kd = temp.Kd;
				}

				PID.A0 = PID.Kp + PID.Ki + PID.Kd;
				PID.A1 = -PID.Kp - 2.0*PID.Kd;
				PID.A2 = PID.Kd;
				arm_pid_init_f32(&PID, 0);
			}
			ProcessDataFlag = 0;
		}

		// Pomiar
		BMP280_Measure();

		// Obliczanie CRC oraz wysyłanie pomiaru do terminala
		sprintf(SendBuffer, "[%d:%d:%d] %2.2f, %2.2f, %d;", time.Hours , time.Minutes, time.Seconds, Temperature, Tref, (int)(U*100.0));
		// obliczenie CRC
		calculateCRCValue = calculateCRC(SendBuffer, sizeof(SendBuffer));
		sprintf(SendBuffer + strlen(SendBuffer), "CRC:%lu;\r\n", calculateCRCValue);
		SendMessage(SendBuffer);

		// Zamkniety uklad regulacji z regulatorem PID
		//Uchyb regulacji
		error = Tref - Temperature;
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

	// Wczytanie ustawien zapisanych w EEPROM

	// Inicjalizacja ustawien domyslnych
    data->Tref = 25.0;
    data->Kp = 1.0;
    data->Ki = 0.0;
    data->Kd = 0.0;
}

void SaveSettings(Settings *data){

	// Zapisywanie danych do EEPROM
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
//	HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
//	HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
	// Wysylanie wiadomosci do UART
	if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)message, strlen(message)) != HAL_OK) {
		Error_Handler();
	}
}

unsigned int calculateCRC(char* data, uint32_t dataSize)
{
    return HAL_CRC_Calculate(&hcrc, (uint32_t*)data, dataSize);
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

// FUNKCJE SYSTEMOWE -----------------------------------------
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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
