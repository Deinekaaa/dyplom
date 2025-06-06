/* Бібліотеки */
#include "ArduinoJson.h"      // Робота з JSON
#include "base64.h"           // Кодування та декодування Base64
#include "esp_camera.h"       // Робота з камерою ESP32
#include <WiFiClient.h>       // Створення WiFi клієнтських з'єднань
#include "SPIFFS.h"           // Робота з файловою системою SPIFFS
#include "ESPAsyncWebSrv.h"   // Створення асинхронного веб-сервера
#include "WiFi.h"             // Управління WiFi з'єднанням
#include <ESPmDNS.h>          // Робота з mDNS (для доступу за іменем .local)
#include "string.h"           // Робота з рядками
#include <HTTPClient.h>       // Здійснення HTTP запитів
#include <WiFiClientSecure.h> // Створення захищених HTTPS з'єднань

/* Власні файли */
#include "secrets.h"          // Конфіденційні дані (паролі, ключі API)
#include "camera_pins.h"      // Піни для плати ESP32-CAM
#include "html.h"             // HTML-код для веб-сторінки

/* Глобальні змінні */

// Змінні для фотографування за інтервалом
unsigned long picturePreviousMillis = 0;
unsigned long pictureInterval = 60000;  // Інтервал для фотографування (1 хвилина)

// Флаги для управління станами
boolean takeNewPhoto = false;           // Флаг для створення нового фото
boolean sendToAWS = false;              // Флаг для одноразової відправки фото на AWS
boolean sendToAWSEveryInterval = false; // Флаг для періодичної відправки фото на AWS

// Об'єкти класів
AsyncWebServer server(80);              // Асинхронний веб-сервер на 80 порту
WiFiClientSecure client;                // Клієнт для захищених HTTPS з'єднань

// Параметри обрізки та повороту зображення
unsigned short cropLeft = 0;
unsigned short cropTop = 0;
unsigned short cropWidth = 1600;
unsigned short cropHeight = 1200;
short rotateAngle = 0;

// Функція підключення до WiFi
void connectWifi(){
  WiFi.mode(WIFI_STA);        // Встановлюємо режим WiFi-клієнта (станції)
  WiFi.disconnect();          // Розриваємо попереднє з'єднання

  WiFi.begin(WIFI_SSID, WIFI_PASSWD); // Починаємо підключення
  Serial.print("Підключення до WiFi з SSID: "); Serial.println(WIFI_SSID);
  Serial.print("Підключення ..");
  
  while (WiFi.status() != WL_CONNECTED) { // Очікуємо на підключення
    Serial.print('.');
    delay(500);
  }
  Serial.println("Успішно!");
  Serial.print("IP Адреса: "); Serial.println(WiFi.localIP()); // Виводимо IP-адресу
}

// Функція сканування доступних мереж WiFi
void scanWifi(){
  Serial.println("Сканування WiFi... ");
  int n = WiFi.scanNetworks(); // Кількість знайдених мереж
  Serial.println("Сканування завершено!");
  
  if (n == 0) {
    Serial.println("мереж не знайдено");
  } else {
    Serial.print(n);
    Serial.println(" мереж знайдено");
    for (int i = 0; i < n; ++i) {
      // Виводимо номер, SSID, рівень сигналу (RSSI) та тип шифрування
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10);
    }
  }
}

// Функція конфігурації камери
void cameraConfig() {
  camera_config_t config; // Структура конфігурації камери

  /* Апаратне налаштування пінів */
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000; // Тактова частота камери (20 МГц)

  /* Налаштування параметрів зображення */
  config.pixel_format = PIXFORMAT_JPEG;     // Формат пікселів
  config.fb_location = CAMERA_FB_IN_PSRAM;  // Зберігати буфер кадру у PSRAM
  config.jpeg_quality = 10;                 // Якість JPEG (10-63, менше - краще)
  config.fb_count = 2;                      // Кількість буферів кадру
  config.frame_size = FRAMESIZE_UXGA;       // Розмір кадру: 1600x1200
  config.grab_mode = CAMERA_GRAB_LATEST;    // Режим захоплення: завжди брати найновіший кадр

  // Ініціалізація камери
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Помилка ініціалізації камери: 0x%x\n", err);
    return;
  }

  /* Налаштування сенсора камери */
  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 1);       // Яскравість (-2..2)
  s->set_contrast(s, 0);         // Контрастність (-2..2)
  s->set_saturation(s, 0);       // Насиченість (-2..2)
  s->set_special_effect(s, 0);   // Спецефект (0: немає, 1: негатив, 2: сірий...)
  s->set_whitebal(s, 1);         // Автоматичний баланс білого (1: увімк.)
  s->set_awb_gain(s, 1);         // Посилення для авто балансу білого (1: увімк.)
  s->set_wb_mode(s, 0);          // Режим балансу білого (0: авто, 1: сонячно...)
  s->set_exposure_ctrl(s, 1);    // Автоматичний контроль експозиції (1: увімк.)
  s->set_aec2(s, 0);             // AEC2 (0: вимк.)
  s->set_ae_level(s, 0);         // Рівень авто експозиції (-2..2)
  s->set_aec_value(s, 300);      // Значення AEC (0-1200)
  s->set_gain_ctrl(s, 1);        // Автоматичний контроль посилення (1: увімк.)
  s->set_agc_gain(s, 0);         // Значення посилення AGC (0-30)
  s->set_gainceiling(s, (gainceiling_t)0); // Максимальне посилення (0-6)
  s->set_bpc(s, 0);              // Корекція "битих" пікселів (0: вимк.)
  s->set_wpc(s, 1);              // Корекція "білих" пікселів (1: увімк.)
  s->set_raw_gma(s, 1);          // Гамма-корекція (1: увімк.)
  s->set_lenc(s, 1);             // Корекція віньєтування (1: увімк.)
  s->set_hmirror(s, 0);          // Горизонтальне дзеркало (0: вимк.)
  s->set_vflip(s, 0);            // Вертикальне дзеркало (0: вимк.)
  s->set_dcw(s, 1);              // DCW (1: увімк.)
  s->set_colorbar(s, 0);         // Тестова кольорова смуга (0: вимк.)
}

// Функція відправки фото на сервер
void sendPhotoToS3(){
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Неможливо відправити: немає підключення до WiFi!");
    return;
  }
  
  camera_fb_t *fb = esp_camera_fb_get(); // Отримуємо вказівник на буфер кадру
  if(!fb) {
    Serial.println("Помилка захоплення кадру з камери");
    return;
  }
  
  Serial.println("Кадр з камери успішно захоплено!");
  Serial.print("Розмір зображення: "); Serial.println(fb->len);
  Serial.print("Розміри->ширина: "); Serial.print(fb->width);
  Serial.print(" висота: "); Serial.println(fb->height);

  HTTPClient http;

  Serial.println("Відправка на AWS...");
  // Формуємо URL з параметрами обрізки та повороту
  String url = String(AWS_REST_API) + "/" + String(cropLeft) + "/" + String(cropTop) + "/" + String(cropWidth) + "/" + String(cropHeight) + "/" + String(rotateAngle);
  http.begin(client, url); // Ініціалізуємо HTTP запит
  Serial.println(url);
  http.addHeader("Content-Type", "image/jpg"); // Вказуємо тип даних

  // Відправляємо POST-запит з байтами зображення
  int httpResponseCode = http.POST(fb->buf, fb->len);

  if(httpResponseCode > 0){
    String response = http.getString();
    Serial.print("Код відповіді: "); Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else{
    Serial.print("Помилка при відправці POST: ");
    Serial.print("Код відповіді: "); Serial.println(httpResponseCode);
  }
  esp_camera_fb_return(fb); // Звільняємо буфер кадру
}

// Функція для виведення поточних значень обрізки та повороту
void checkCropDim(){
  Serial.println("Отримані дані для обрізки: ");
  Serial.print("X: "); Serial.println(cropLeft);
  Serial.print("Y: "); Serial.println(cropTop);
  Serial.print("Ширина: "); Serial.println(cropWidth);
  Serial.print("Висота: "); Serial.println(cropHeight);
  Serial.print("Кут повороту: "); Serial.println(rotateAngle);
}

// Функція налаштування та запуску веб-сервера
void beginServer(){
  // Обробник для головної сторінки ("/")
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  // Обробник для "/capture": робить нове фото
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Роблю фото");
    takeNewPhoto = true;
  });

  // Обробник для "/startInterval": починає періодичну відправку
  server.on("/startInterval", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Відправка на AWS кожні N секунд інтервалу");
    sendToAWSEveryInterval = true;
    Serial.print("Почнемо відправляти фото на AWS кожні "); Serial.print(pictureInterval); Serial.println(" мілісекунд");
  });

  // Обробник для "/stopInterval": зупиняє періодичну відправку
  server.on("/stopInterval", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Зупинка інтервальної відправки на AWS");
    sendToAWSEveryInterval = false;
    Serial.println("Зупинено відправку на AWS з інтервалами");
  });

  // Обробник для "/saved-photo.jpg": віддає збережене фото
  server.on("/saved-photo.jpg", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Відправка фото");
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Обробник для "/send-aws": одноразово відправляє фото на AWS
  server.on("/send-aws", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Відправка на AWS");
    sendToAWS = true;
    Serial.println("Початок тестової відправки на AWS");
  });

  // Обробник для "/cropData": отримує параметри обрізки та повороту
  server.on("/cropData", HTTP_GET, [](AsyncWebServerRequest * request) {
    String left = request->getParam("cropLeft")->value();
    String top = request->getParam("cropTop")->value();
    String width = request->getParam("cropWidth")->value();
    String height = request->getParam("cropHeight")->value();
    String rotate = request->getParam("rotate")->value();
    
    cropLeft = left.toInt();
    cropTop = top.toInt();
    cropWidth = width.toInt();
    cropHeight = height.toInt();
    rotateAngle = rotate.toInt();
    
    request->send_P(200, "text/plain", "Отримано дані для обрізки");
    checkCropDim();
  });

  server.begin(); // Запуск веб-сервера
}

// Функція перевірки, чи фото збережено у файловій системі
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 ); // Повертає true, якщо розмір файлу більший за 100 байт
}

// Функція захоплення фото та збереження у SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL;
  bool ok = 0;

  do { // Повторюємо, доки фото не буде успішно збережено
    Serial.println("Роблю фото...");
    fb = esp_camera_fb_get(); // Отримуємо кадр з камери
    if (!fb) {
      Serial.println("Помилка захоплення кадру з камери");
      return;
    }

    Serial.printf("Ім'я файлу фото: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // Відкриваємо файл для запису

    if (!file) {
      Serial.println("Не вдалося відкрити файл для запису");
    }
    else {
      file.write(fb->buf, fb->len); // Записуємо дані кадру
      Serial.print("Фото було збережено у ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Розмір: ");
      Serial.print(file.size());
      Serial.println(" байт");
    }
    file.close();
    esp_camera_fb_return(fb); // Звільняємо буфер кадру

    ok = checkPhoto(SPIFFS); // Перевіряємо, чи файл збережено коректно
  } while ( !ok );
}

// Функція налаштування, виконується один раз
void setup(){
  Serial.begin(115200);
  Serial.println("Конфігурація...");
  delay(500);

  // Ініціалізація файлової системи SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Сталася помилка під час монтування SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS успішно змонтовано");
  }

  cameraConfig();
  delay(500);
  capturePhotoSaveSpiffs(); // Робимо перше фото при старті
  delay(2000);

  connectWifi();
  delay(600);
  beginServer();
  delay(100);
  
  // Вимикаємо перевірку SSL-сертифіката для HTTPS-запитів
  client.setInsecure();
  
  Serial.println("Готово до використання!\n");
}

// Головний цикл програми
void loop(){
  unsigned long currentMillis = millis();

  // Обробка періодичної відправки фото
  if(currentMillis - picturePreviousMillis >= pictureInterval && sendToAWSEveryInterval){
    if(WiFi.status() != WL_CONNECTED){
      Serial.print(millis());
      Serial.println("Перепідключення до WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
    else{
      sendPhotoToS3();
    }
    picturePreviousMillis = currentMillis; // Оновлюємо час останньої відправки
  }

  // Обробка флагу для створення нового фото
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }

  // Обробка флагу для одноразової відправки
  if (sendToAWS) {
    sendPhotoToS3();
    sendToAWS = false;
  }

  delay(2);
}