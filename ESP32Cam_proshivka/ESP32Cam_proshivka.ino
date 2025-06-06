/* Бібліотеки Arduino */
#include "ArduinoJson.h"       // Бібліотека для роботи з об'єктами JSON
#include "base64.h"            // Бібліотека для кодування та декодування Base64
#include "esp_camera.h"        // Основна бібліотека для роботи з камерою ESP32
#include <WiFiClient.h>        // Бібліотека для створення WiFi клієнтських з'єднань
#include "SPIFFS.h"            // Бібліотека для роботи з файловою системою SPIFFS (Serial Peripheral Interface Flash File System)
#include "ESPAsyncWebSrv.h"    // Бібліотека для створення асинхронного веб-сервера
#include "WiFi.h"              // Бібліотека для управління WiFi з'єднанням
#include <ESPmDNS.h>           // Бібліотека для роботи з mDNS (дозволяє звертатися до пристрою за іменем у локальній мережі, наприклад, esp32.local)
#include "string.h"            // Стандартна бібліотека C для роботи з рядками
#include <HTTPClient.h>        // Бібліотека для здійснення HTTP запитів
#include <WiFiClientSecure.h>  // Бібліотека для створення захищених (HTTPS) клієнтських WiFi з'єднань

/* Власні файли включення */
#include "secrets.h"           // Файл для зберігання конфіденційних даних (ім'я та пароль WiFi, ключі API тощо). Створюється користувачем.
#include "camera_pins.h"       // Файл з визначенням пінів камери для конкретної плати ESP32-CAM. Створюється користувачем.
#include "html.h"              // Файл, що містить HTML-код для веб-сторінки. Створюється користувачем.

/* Глобальні змінні */

// Закоментовані змінні для періодичної перевірки WiFi з'єднання
// unsigned long wifiCheckPreviousMillis = 0; // Час останньої перевірки WiFi
// unsigned long wifiCheckInterval = 60000;   // Інтервал перевірки WiFi (у мілісекундах)

// Змінні для фотографування через заданий інтервал
unsigned long picturePreviousMillis = 0; // Час останнього фотографування
unsigned long pictureInterval = 60000;   // Інтервал для фотографування (у мілісекундах, тут 1 хвилина)

// Змінні-флаги для управління станами
boolean takeNewPhoto = false;           // Флаг: чи потрібно зробити нове фото
boolean sendToAWS = false;              // Флаг: чи потрібно відправити фото на AWS (одноразово)
boolean sendToAWSEveryInterval = false; // Флаг: чи потрібно відправляти фото на AWS періодично

// Створення об'єктів класів
AsyncWebServer server(80);     // Об'єкт асинхронного веб-сервера, що слухає порт 80
WiFiClientSecure client;       // Об'єкт для захищеного (HTTPS) WiFi клієнта

// Змінні для параметрів обрізки та повороту зображення
unsigned short cropLeft = 0;     // Координата X лівого верхнього кута для обрізки
unsigned short cropTop = 0;      // Координата Y лівого верхнього кута для обрізки
unsigned short cropWidth = 1600; // Ширина області обрізки
unsigned short cropHeight = 1200;// Висота області обрізки
short rotateAngle = 0;           // Кут повороту зображення (в градусах)

// Закоментований код для використання зовнішньої PSRAM (SPI RAM) з бібліотекою ArduinoJSON.
// Це може бути корисно при роботі з великими JSON-документами, щоб не перевантажувати вбудовану пам'ять.
// struct SpiRamAllocator {
//  void* allocate(size_t size) {
//    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
//  }

//  void deallocate(void* pointer) {
//    heap_caps_free(pointer);
//  }

//  void* reallocate(void* ptr, size_t new_size) {
//    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
//  }
// };
// using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

// Функція для підключення до мережі WiFi
void connectWifi(){
  // Встановлення режиму ESP32 як WiFi станції (клієнта)
  WiFi.mode(WIFI_STA);
  // Розрив будь-якого попереднього з'єднання для чистого підключення
  WiFi.disconnect();

  // Початок підключення до WiFi з використанням SSID та пароля, визначених у файлі secrets.h
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  Serial.print("Підключення до WiFi з SSID: "); Serial.println(WIFI_SSID);
  Serial.print("Підключення ..");
  // Цикл очікування, доки статус з'єднання не стане WL_CONNECTED (підключено)
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.'); // Виведення крапки кожні 500 мс для індикації процесу
    delay(500);
  }
  Serial.println("Успішно!");
  Serial.print("IP Адреса: "); Serial.println(WiFi.localIP()); // Виведення отриманої IP-адреси
}

// Функція для сканування доступних мереж WiFi
void scanWifi(){
  Serial.println("Сканування WiFi... ");

  // Функція WiFi.scanNetworks() повертає кількість знайдених мереж
  int n = WiFi.scanNetworks();
  Serial.println("Сканування завершено!");
  if (n == 0) {
    Serial.println("мереж не знайдено");
  } else {
    Serial.print(n);
    Serial.println(" мереж знайдено");
    // Виведення інформації про кожну знайдену мережу
    for (int i = 0; i < n; ++i) {
      // Виведення номеру, SSID, рівня сигналу (RSSI) та типу шифрування
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i)); // Ім'я мережі
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i)); // Рівень сигналу
      Serial.print(")");
      // Виведення "*" якщо мережа захищена, або пробілу, якщо відкрита
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10); // Невелика затримка
    }
  }
}

// Функція для конфігурації камери
void cameraConfig() {
  camera_config_t config; // Створення структури конфігурації камери

  /* Апаратне налаштування пінів камери */
  // Ці значення зазвичай визначаються у файлі camera_pins.h залежно від моделі плати
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
  config.pin_sccb_sda = SIOD_GPIO_NUM; // Пін SDA для I2C (SCCB)
  config.pin_sccb_scl = SIOC_GPIO_NUM; // Пін SCL для I2C (SCCB)
  config.pin_pwdn = PWDN_GPIO_NUM;     // Пін Power Down
  config.pin_reset = RESET_GPIO_NUM;   // Пін Reset

  config.xclk_freq_hz = 20000000; // Тактова частота камери (20 МГц)

  /* Налаштування параметрів камери */
  config.pixel_format = PIXFORMAT_JPEG; // Формат пікселів: JPEG (добре підходить для більшості випадків)
  // config.pixel_format = PIXFORMAT_RGB565; // Альтернативний формат, якщо потрібна обрізка або інші маніпуляції з сирими даними
  config.fb_location = CAMERA_FB_IN_PSRAM; // Місце зберігання буфера кадру: у зовнішній PSRAM (якщо доступна)
  config.jpeg_quality = 10; // Якість JPEG зображення (від 10 до 63, менше значення - вища якість)
  config.fb_count = 2;      // Кількість буферів кадру (2 дозволяє швидше отримувати кадри)
  config.frame_size = FRAMESIZE_UXGA; // Розмір кадру: UXGA (1600x1200)
  config.grab_mode = CAMERA_GRAB_LATEST; // Режим захоплення кадру: завжди брати найновіший кадр
  // config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Альтернативний режим: брати кадр, коли попередній буфер порожній

  // Закоментований блок для вибору меншого розміру кадру, якщо камера не підтримує PSRAM
  // або якщо потрібно заощадити пам'ять.
  // if(psramFound()){
  //     config.frame_size = FRAMESIZE_HD; // Розмір кадру HD (1280x720)
  //     config.jpeg_quality = 10;         // Якість JPEG
  //     config.fb_count = 2;              // Кількість буферів
  //     config.grab_mode = CAMERA_GRAB_LATEST;
  // }
  // else {
  //     config.frame_size = FRAMESIZE_VGA;  // Розмір кадру VGA (640x480)
  //     config.jpeg_quality = 12;         // Якість JPEG
  //     config.fb_count = 1;              // Кількість буферів
  // }

  // Ініціалізація камери з заданою конфігурацією
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Помилка ініціалізації камери: 0x%x\n", err); // Виведення коду помилки
    return; // Вихід з функції, якщо ініціалізація не вдалася
  }

  /* Налаштування параметрів зображення сенсора камери */
  sensor_t * s = esp_camera_sensor_get(); // Отримання вказівника на об'єкт сенсора

  s->set_brightness(s, 1);     // Встановлення яскравості (від -2 до 2)
  s->set_contrast(s, 0);       // Встановлення контрастності (від -2 до 2)
  s->set_saturation(s, 0);     // Встановлення насиченості (від -2 до 2)
  s->set_special_effect(s, 0); // Встановлення спеціального ефекту (0 - Без ефекту, 1 - Негатив, 2 - Відтінки сірого, 3 - Червоний відтінок, 4 - Зелений відтінок, 5 - Синій відтінок, 6 - Сепія)
  s->set_whitebal(s, 1);       // Увімкнення/вимкнення автоматичного балансу білого (0 - вимк., 1 - увімк.)
  s->set_awb_gain(s, 1);       // Увімкнення/вимкнення посилення для автоматичного балансу білого (0 - вимк., 1 - увімк.)
  s->set_wb_mode(s, 0);        // Режим балансу білого (0-4, якщо awb_gain увімкнено: 0 - Авто, 1 - Сонячно, 2 - Хмарно, 3 - Офіс, 4 - Вдома)
  s->set_exposure_ctrl(s, 1);  // Увімкнення/вимкнення автоматичного контролю експозиції (0 - вимк., 1 - увімк.)
  s->set_aec2(s, 0);           // Увімкнення/вимкнення AEC2 (Advanced Exposure Control) (0 - вимк., 1 - увімк.)
  s->set_ae_level(s, 0);       // Рівень автоматичної експозиції (від -2 до 2)
  s->set_aec_value(s, 300);    // Значення AEC (Automatic Exposure Control) (від 0 до 1200)
  s->set_gain_ctrl(s, 1);      // Увімкнення/вимкнення автоматичного контролю посилення (AGC) (0 - вимк., 1 - увімк.)
  s->set_agc_gain(s, 0);       // Значення посилення AGC (від 0 до 30)
  s->set_gainceiling(s, (gainceiling_t)0); // Максимальне значення посилення (від 0 до 6)
  s->set_bpc(s, 0);            // Увімкнення/вимкнення корекції "битих" пікселів (Black Pixel Correction) (0 - вимк., 1 - увімк.)
  s->set_wpc(s, 1);            // Увімкнення/вимкнення корекції "білих" пікселів (White Pixel Correction) (0 - вимк., 1 - увімк.)
  s->set_raw_gma(s, 1);        // Увімкнення/вимкнення гамма-корекції для RAW даних (0 - вимк., 1 - увімк.)
  s->set_lenc(s, 1);           // Увімкнення/вимкнення корекції віньєтування (Lens Correction) (0 - вимк., 1 - увімк.)
  s->set_hmirror(s, 0);        // Горизонтальне дзеркальне відображення (0 - вимк., 1 - увімк.)
  s->set_vflip(s, 0);          // Вертикальне дзеркальне відображення (0 - вимк., 1 - увімк.)
  s->set_dcw(s, 1);            // Увімкнення/вимкнення DCW (Downsampling Correction for Color Artifacts) (0 - вимк., 1 - увімк.)
  s->set_colorbar(s, 0);       // Відображення тестової кольорової смуги (0 - вимк., 1 - увімк.)
}

// Функція для відправки фото на сервер AWS S3 (або інший сумісний API)
void sendPhotoToS3(){
  // Перевірка статусу WiFi з'єднання
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Неможливо відправити: Немає підключення до WiFi!");
    return; // Вихід, якщо немає WiFi
  }
  else{
    // Захоплення кадру з камери
    camera_fb_t *fb = esp_camera_fb_get(); // Отримання вказівника на буфер кадру
    if(!fb) { // Перевірка, чи кадр успішно захоплений
      Serial.println("Помилка захоплення кадру з камери");
      return; // Вихід, якщо не вдалося захопити кадр
    } else {
      Serial.println("Кадр з камери успішно захоплено!");
    }

    // const char *data = (const char *)fb->buf; // Можна використовувати, якщо потрібен const char*
    // Виведення метаданих зображення
    Serial.print("Розмір зображення: ");
    Serial.println(fb->len); // Довжина буфера (розмір файлу в байтах)
    Serial.print("Розміри->ширина: ");
    Serial.print(fb->width); // Ширина зображення в пікселях
    Serial.print(" висота: ");
    Serial.println(fb->height); // Висота зображення в пікселях
    Serial.println("\n\n");

    HTTPClient http; // Створення об'єкта HTTPClient

    Serial.println("Відправка на AWS...");
    // Формування URL для API. Базовий URL береться з AWS_REST_API (визначено в secrets.h).
    // Додаються параметри обрізки та повороту.
    String url = String(AWS_REST_API) + "/" + String(cropLeft) + "/" + String(cropTop) + "/" + String(cropWidth) + "/" + String(cropHeight) + "/" + String(rotateAngle);
    http.begin(client, url); // Ініціалізація HTTP запиту з використанням захищеного клієнта та URL
    Serial.println(url); // Виведення URL в монітор порту
    http.addHeader("Content-Type", "image/jpg"); // Додавання заголовка Content-Type, що вказує на тип даних (зображення JPEG)
    // http.addHeader("Content-Encoding", "base64"); // Закоментовано: заголовок для Base64 кодування, якщо потрібно

    // Serial.println(dataToSend); // Для відладки, якщо дані готуються окремо
    // Відправка POST-запиту з тілом, що містить байти зображення (fb->buf) та їх кількість (fb->len)
    int httpResponseCode = http.POST(fb->buf, fb->len);

    if(httpResponseCode > 0){ // Якщо код відповіді позитивний (наприклад, 200 OK, 201 Created)
      String response = http.getString(); // Отримання тіла відповіді від сервера
      Serial.print("Код відповіді: "); Serial.println(httpResponseCode); // Виведення коду відповіді
      Serial.println(response); // Виведення тіла відповіді
    }
    else{ // Якщо виникла помилка при відправці POST-запиту
      Serial.print("Помилка при відправці POST: ");
      Serial.print("Код відповіді: "); Serial.println(httpResponseCode); // Виведення коду помилки
    }
    esp_camera_fb_return(fb); // Важливо! Звільнення буфера кадру, щоб пам'ять могла бути використана повторно.
  }
}

// Функція для виведення поточних значень параметрів обрізки та повороту
void checkCropDim(){
  Serial.println("Отримані дані для обрізки: ");
  Serial.print("X: "); Serial.println(cropLeft);
  Serial.print("Y: "); Serial.println(cropTop);
  Serial.print("Ширина: "); Serial.println(cropWidth);
  Serial.print("Висота: "); Serial.println(cropHeight);
  Serial.print("Кут повороту: "); Serial.println(rotateAngle);
}

// Функція для налаштування та запуску веб-сервера
void beginServer(){
  // Обробник для кореневого шляху ("/") - головна сторінка
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    // Відправка HTML-коду сторінки (змінна index_html, ймовірно, визначена у html.h)
    request->send_P(200, "text/html", index_html);
  });

  // Обробник для шляху "/capture" - зробити нове фото
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Роблю фото"); // Відповідь клієнту
    takeNewPhoto = true; // Встановлення флагу для захоплення нового фото в головному циклі
  });

  // Обробник для шляху "/startInterval" - почати періодичну відправку фото на AWS
  server.on("/startInterval", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Відправка на AWS кожні N секунд інтервалу");
    sendToAWSEveryInterval = true; // Встановлення флагу для періодичної відправки
    Serial.print("Почнемо відправляти фото на AWS кожні "); Serial.print(pictureInterval); Serial.println(" мілісекунд");
  });

  // Обробник для шляху "/stopInterval" - зупинити періодичну відправку фото на AWS
  server.on("/stopInterval", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Зупинка інтервальної відправки на AWS");
    sendToAWSEveryInterval = false; // Скидання флагу періодичної відправки
    Serial.println("Зупинено відправку на AWS з інтервалами");
  });

  // Обробник для шляху "/saved-photo.jpg" - віддати збережене фото
  server.on("/saved-photo.jpg", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Відправка фото");
    // Відправка файлу з SPIFFS. FILE_PHOTO - це константа з ім'ям файлу фото.
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Обробник для шляху "/send-aws" - одноразово відправити фото на AWS
  server.on("/send-aws", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Відправка на AWS");
    sendToAWS = true; // Встановлення флагу для одноразової відправки
    Serial.println("Початок тестової відправки на AWS");
  });

  // Обробник для шляху "/cropData" - отримання параметрів обрізки та повороту
  server.on("/cropData", HTTP_GET, [](AsyncWebServerRequest * request) {
    // Отримання значень параметрів з GET-запиту
    String left = request->getParam("cropLeft")->value();
    String top = request->getParam("cropTop")->value();
    String width = request->getParam("cropWidth")->value();
    String height = request->getParam("cropHeight")->value();
    String rotate = request->getParam("rotate")->value();
    // Перетворення рядкових значень у числові та збереження у глобальні змінні
    cropLeft = left.toInt();
    cropTop = top.toInt();
    cropWidth = width.toInt();
    cropHeight = height.toInt();
    rotateAngle = rotate.toInt();
    request->send_P(200, "text/plain", "Отримано дані для обрізки");
    checkCropDim(); // Виклик функції для виведення отриманих значень
  });

  // Запуск веб-сервера
  server.begin();
}

// Функція для перевірки, чи фото було успішно збережено у файловій системі
// fs: посилання на об'єкт файлової системи (наприклад, SPIFFS)
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO ); // Відкриття файлу фото (FILE_PHOTO - константа з ім'ям файлу)
  unsigned int pic_sz = f_pic.size(); // Отримання розміру файлу
  return ( pic_sz > 100 ); // Повертає true, якщо розмір файлу більший за 100 байт (проста перевірка, що файл не порожній)
}

// Функція для захоплення фото з камери та збереження його у SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // Вказівник на буфер кадру, ініціалізований як NULL
  bool ok = 0;             // Булева змінна, що вказує, чи було фото зроблено коректно

  do { // Цикл do-while для повторення спроби, доки фото не буде успішно зроблено та збережено
    // Зробити фото з камери
    Serial.println("Роблю фото...");

    fb = esp_camera_fb_get(); // Отримання кадру з камери
    if (!fb) { // Перевірка, чи кадр успішно захоплений
      Serial.println("Помилка захоплення кадру з камери");
      return; // Вихід з функції, якщо не вдалося захопити кадр
    }

    // Ім'я файлу фото (FILE_PHOTO - константа)
    Serial.printf("Ім'я файлу фото: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // Відкриття файлу у SPIFFS для запису

    // Запис даних у файл фото
    if (!file) { // Перевірка, чи файл успішно відкрито
      Serial.println("Не вдалося відкрити файл для запису");
    }
    else {
      file.write(fb->buf, fb->len); // Запис даних кадру (fb->buf - буфер, fb->len - довжина)
      Serial.print("Фото було збережено у ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Розмір: ");
      Serial.print(file.size()); // Виведення розміру збереженого файлу
      Serial.println(" байт");
    }
    // Закриття файлу
    file.close();
    esp_camera_fb_return(fb); // Обов'язково звільнити буфер кадру

    // Перевірка, чи файл був коректно збережений у SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok ); // Повторювати, якщо фото не було збережено коректно (наприклад, файл замалий)
}

// Функція налаштування, виконується один раз при старті мікроконтролера
void setup(){
  Serial.begin(115200); // Ініціалізація серійного порту для виводу діагностичної інформації
  Serial.println("Конфігурація...");
  delay(500); // Невелика затримка

  // Ініціалізація файлової системи SPIFFS
  // true - форматувати SPIFFS, якщо не вдається змонтувати
  if (!SPIFFS.begin(true)) {
    Serial.println("Сталася помилка під час монтування SPIFFS");
    ESP.restart(); // Перезавантаження ESP32, якщо SPIFFS не вдалося змонтувати
  }
  else {
    delay(500);
    Serial.println("SPIFFS успішно змонтовано");
  }

  cameraConfig(); // Виклик функції конфігурації камери
  delay(500);
  capturePhotoSaveSpiffs(); // Зробити та зберегти перше фото при старті
  delay(2000); // Затримка

  // scanWifi(); // Закоментований виклик сканування WiFi мереж
  // delay(500);

  connectWifi(); // Підключення до WiFi
  delay(500);
  delay(100);
  beginServer(); // Запуск веб-сервера
  delay(100);
  // Встановлення незахищеного режиму для HTTPS клієнта.
  // Це вимикає перевірку SSL сертифікатів сервера.
  // НЕ РЕКОМЕНДУЄТЬСЯ для використання у продакшені.
  // Для безпечного з'єднання потрібно використовувати валідні сертифікати.
  client.setInsecure();
  Serial.println("Готово до використання!\n");
}

// Головний цикл програми, виконується безперервно
void loop(){
  unsigned long currentMillis = millis(); // Отримання поточного часу в мілісекундах

  // Періодична відправка фото, якщо увімкнено sendToAWSEveryInterval
  if(currentMillis - picturePreviousMillis >= pictureInterval && sendToAWSEveryInterval){
    // Перевірка WiFi з'єднання перед відправкою
    if(WiFi.status() != WL_CONNECTED){
      Serial.print(millis());
      Serial.println("Перепідключення до WiFi...");
      WiFi.disconnect(); // Розрив поточного з'єднання
      WiFi.reconnect();  // Спроба перепідключення
    }
    else{ // Якщо WiFi підключено
      sendPhotoToS3(); // Відправка фото на S3
      // sendToAWS = false; // Цей рядок може бути зайвим тут, якщо sendToAWS керує лише одноразовою відправкою
    }
    picturePreviousMillis = currentMillis; // Оновлення часу останньої відправки
  }

  // Якщо встановлено флаг takeNewPhoto (наприклад, через веб-інтерфейс)
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs(); // Зробити нове фото та зберегти його
    takeNewPhoto = false;     // Скинути флаг
  }

  // Якщо встановлено флаг sendToAWS (для одноразової відправки)
  if (sendToAWS) {
    sendPhotoToS3();      // Відправити фото на S3
    sendToAWS = false;    // Скинути флаг
  }

  delay(2); // Дуже коротка затримка для стабільності та можливості обробки інших задач (якщо вони є)
}