#include "esp_camera.h"
#include "FS.h"
#include "SPI.h"
#include "SD_MMC.h"
#include "EEPROM.h"
#include "driver/rtc_io.h"

// camera module
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

#define ID_ADDRESS            0x00
#define COUNT_ADDRESS         0x01
#define ID_BYTE               0xAA
#define EEPROM_SIZE           0x0F

#define TIME_TO_SLEEP  10            //deep sleep duration in seconds
#define uS_TO_S_FACTOR 1000000ULL

uint16_t nextImageNumber = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");

  pinMode(4, INPUT);              //GPIO for LED flash
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4);  //diable pin hold if it was enabled before sleeping

  camera_config_t config;
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;


  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  //initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(5000);
    ESP.restart();
    return;
  }

  //  if(!SD_MMC.begin("/sdcard",true))
  if (!SD_MMC.begin())
  {
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  //initialize EEPROM & get file number
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Exiting now");
    while (1);  //wait here as something is not right
  }

  /*ERASE EEPROM BYTES START*/
  /*
    Serial.println("Erasing EEPROM...");
    for(int i = 0; i < EEPROM_SIZE; i++)
    {
    EEPROM.write(i, 0xFF);
    EEPROM.commit();
    delay(20);
    }
    Serial.println("Erased");
    while(1);
  */
  /*ERASE EEPROM BYTES END*/

  if (EEPROM.read(ID_ADDRESS) != ID_BYTE)   //there will not be a valid picture number
  {
    Serial.println("Initializing ID byte & restarting picture count");
    nextImageNumber = 0;
    EEPROM.write(ID_ADDRESS, ID_BYTE);
    EEPROM.commit();
  }
  else                                      //obtain next picture number
  {
    EEPROM.get(COUNT_ADDRESS, nextImageNumber);
    nextImageNumber +=  1;
    Serial.print("Next image number:");
    Serial.println(nextImageNumber);
  }

  //take new image
  camera_fb_t * fb = NULL;
  //obtain camera frame buffer
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    Serial.println("Exiting now");
    while (1);  //wait here as something is not right
  }

  //save to SD card
  //generate file path
  String path = "/IMG" + String(nextImageNumber) + ".jpg";

  fs::FS &fs = SD_MMC;

  //create new file
  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to create file");
    Serial.println("Exiting now");
    while (1);  //wait here as something is not right
  }
  else
  {
    file.write(fb->buf, fb->len);
    EEPROM.put(COUNT_ADDRESS, nextImageNumber);
    EEPROM.commit();
  }
  file.close();

  //return camera frame buffer
  esp_camera_fb_return(fb);
  Serial.printf("Image saved: %s\n", path.c_str());

  pinMode(4, OUTPUT);              //GPIO for LED flash
  digitalWrite(4, LOW);            //turn OFF flash LED
  rtc_gpio_hold_en(GPIO_NUM_4);    //make sure flash is held LOW in sleep
  delay(500);
  Serial.println("Entering deep sleep mode");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void loop()
{


}
