#include "Kalman.h"; // бибилиотека фильтрации (стабилизации) измерений
#include <SPI.h> // для работы с SD картой
#include <SD.h> // для работы с SD картой

double measurement, filteredMeasurement, averageMeasurement; // измерения: прямое, фильтрованное и среднее за период
Kalman myFilter(0.15,15,1023,0); // сам фильтр, параметры 0.15, 15 и 1023 - экспериментально подобраны

unsigned long doMeasurementFuture = 0; // расчетное время делать измерение
const word doMeasurementInterval = 200; // период (мс) прямых измерений pH метра
const word lastMeasurementsPeriod = 60; // период (сек) для усреднения фильтрованных измерений
const word lastMeasurementsNumber = (lastMeasurementsPeriod*1000)/doMeasurementInterval; // количество фильтрованных измерений за период для усреднения
double lastMeasurements[lastMeasurementsNumber]; // массив последних фильтрованных измерений за период для усреденения
word count = 0; // счетчик для записи в массив последних фильтрованных измерений

unsigned long doCalcWriteFuture = 0; // расчетное время расчитывать среднее и записывать на SD карту

const word chipSelect = 53; // подключение SD шилда линия CS
const word ledPin = 2; // подключение светодиода индикации работы
const word pHPin = 0; // подключение pH метра

char dataFileName[50];

void doMeasurement(void) { // измерение аналогового сигнала, фильтрация, заполнение массива последних измерений для последующего расчета среднего за период
  if (doMeasurementFuture <= millis()) { // настало время делать измерение 
                                         // если же не настало время делать измерения, то функция завершится мгновенно и ничего не делает
    doMeasurementFuture = millis() + doMeasurementInterval; // сразу рассчитываем время для следующего измерения
    
    measurement = (double) analogRead(pHPin); // прямое измерение pH метра
    filteredMeasurement = myFilter.getFilteredValue(measurement); // фильтрация измерения
    lastMeasurements[count] = filteredMeasurement; // запись очередного фильтрованного измерения в массив

    if (count % 10 == 0) Serial.print("."); // вывод в отладочную консоль прогресс бар о том, что программа работает
    count++; // подготовка к следуюущей записи
    if (count == lastMeasurementsNumber) count = 0; // если массив кончился, то начинаем записывать в него с начала (нумерация элементов массива с нуля)
  }
}

void doCalcWrite(void) {
  if (doCalcWriteFuture <= millis()) { // настало время рассчитать среднее и записать на SD карту
                                       // если же не настало время делать измерения, то функция завершится мгновенно и ничего не делает
    doCalcWriteFuture = millis() + lastMeasurementsPeriod*1000; // сразу рассчитываем время для следующих расчета и записи

    // включаем индикатор подготовки и записи на SD карту на 5 секунд - в это время выключать плату нельзя
    noTone(ledPin); // выключаем свечение, чтобы последюущая команда включения отработала наверняка, иначе она может быть проигнорирована
    tone(ledPin, 490, 5000); // включаем свечение на 5 сек с частотой 490 Гц и 50% заполнением ШИМ - половинная яркость

    // расчет среднего по массиву последних записанных фильтрованных измерений
    double sum = 0;
    for (int i=0; i<lastMeasurementsNumber; i++) sum += lastMeasurements[i];
    averageMeasurement = sum / lastMeasurementsNumber;
    
    Serial.println();
    Serial.println(averageMeasurement); // вывод в отладочную консоль

    File dataFile = SD.open(dataFileName, FILE_WRITE); // открываем файл
    if (dataFile) { // если файл готов, записываем среднее значение и закрываем файл, чтобы данные точно записались на SD карту
      dataFile.println(averageMeasurement);
      dataFile.close();
    }
    else { // если проблема с SD картой, пишем ошибку в консоль
      Serial.println("Error with datalog file.");
    }
  }
}

void setup() {
  Serial.begin(115200); // открываем консоль отладки
  
  Serial.println("\r\n\r\n");
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) Serial.println(" Failed.\r\n\r\n"); // проверяем SD карту
  else Serial.println(" OK.\r\n\r\n");

  Serial.print("dataFileName: "); // находим первое свободное имя файла в формате datalog<N>.csv где <N> - число от 1 до 100
  for (int i=1; i<100; i++) {
    sprintf(dataFileName, "datalog%d.csv", i);
    if (!SD.exists(dataFileName)) {
      Serial.println(dataFileName);
      break;
    }
  }

  pinMode(ledPin, OUTPUT); // настраиваем пин светодиода
  noTone(ledPin); // и выключаем светодиод

  doMeasurementFuture = millis() + doMeasurementInterval; // вычисляем время первого измерения
  doCalcWriteFuture = millis() + lastMeasurementsPeriod*1000; // вычисляем время первого усреднения
}

void loop() {
  doMeasurement(); // измерения каждые doMeasurementInterval = 200 миллисекунд, накапливается массив данных
  doCalcWrite(); // усреднение массива каждые lastMeasurementsPeriod = 60 секунд, и запись на SD карту
}
