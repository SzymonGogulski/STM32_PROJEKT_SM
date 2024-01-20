# Projekt zaliczeniowy - Systemy Mikroprocesorowe
### Szymon Gogulski 147403 <br />Alan Grądecki 151126

Projekt przedstawia realizacje systemu automatycznej regulacji temperatury. Jako obiekt 
sterowania użyliśmy grzałkę ceramiczną. 

### Spełnione założenia podstawowe
- Cykliczny pomiar wartości mierzonej (Ts=500ms).
- Sterowanie regulowaną zmienną w zakresie 25°C - 75°C.
- Wykorzystanie algorytmu regulacji PID.
- Uchyb ustalony poniżej 5% zakresu regulacji.
- Zadawanie wartości referencyjnej poprzez kominukacje szeregową.
- Podgląd aktualych wartości sygnału: pomiarowego, referencyjnego i sterującego.

### Spełnione założenia dodatkowe
- Aplikacja GUI
    - komunikuje się z STM poprzez UART
    - wyświetla badane sygnały
    - zadaje nastawy regulatora
    - mierzy uchyb ustalony
- Zapis nastaw regulatora do pamięci Flash
- Identyfikacja modelu matematycznego obiektu w Matlab

### Użyte technologie
- STM32
- C
- C#
- Matlab
- Python

### Zdjęcia połączeń
![1](images/1.jpg)
![2](images/2.jpg)
### Zrzut GUI
![GUI](images/GUI.png)

### Schemat układu regulacji
![SchematUAR](images/schematUAR.png)

### Pętla regulacji
```
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){

	// PRZERWANIE ZEGARA
	// 1) POMIAR
	// 2) FILTR CYFROWY (nie aktywny)
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
```

### Schemat elektryczny
![SchematElektryczny](images/schematelektryczny.png)

### Lista elementów elektrycznych
- STM32 Nucleo F746ZG
- Czujnik BMP280
- Grzałka 3,6Ω; 40W
- Zasilacz 12V; 2,5A
- Tranzystor N-MOSFET IRF520N
- 2 x Rezystor 100Ω
- 2 x Rezystor 10kΩ
- Rezystor 330Ω
- Dioda LED R
- Tranzystor N-MOSFET 2N7000
- Bateria 9V

### Możliwe usprawnienia
**1. Podstawa czasu/ Filtr**
    - Zmniejszyć podstawe czasową Ts.
    - Zrealizować filtr cyfrowy IIR sygnału pomiarowego (dla mniejszego Ts).
**2. RTC**
    - Dodać Time Stamp pomiarów poprzez zegar RTC.
    - Dodać synchronizacje RTC poprzez SNTP/NTP.
    - Dodać okienko czas pomiaru w GUI.
**3. CRC**
    - Dodać system sum kontrolnych CRC w komunikacji pomiędzy STM i GUI.
**4. Komunikacja UART**
    - Przenieść wysyłanie pomiarów UART z przerwania TIM do pętli głównej.
    - Wysyłać 50 pomiarów w jednej paczce zamiast każdy pojedyńczo.
**5. Flash**
    - Usprawnić zapis do pamięci Flash.
**6. Regulacja**
    - Największym problemem naszego układu regulacji jest znaczne początkowe przeregulowanie. <br/>Konieczne jest znalezienie lepszych nastaw PID. Możliwe, że jest to cecha charakterystyczna <br/> układów regulacji temperatury.
    
### Zewnętrze biblioteki
CMSIS 5.7.0: https://www.keil.arm.com/packs/cmsis-arm/versions/
BMP 280: https://github.com/ProjectoOfficial/STM32/tree/main/STM32_I2C
FLASH: https://github.com/controllerstech/STM32/tree/master/FLASH_PROGRAM/F4%20SERIES


