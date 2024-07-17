/*
  Florian Klug-Göri

  Code taken from

  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-post-image-photo-server/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "AsyncJson.h"
#include <ArduinoJson.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define PIN_PIR           13
#define LED_PIN           14

const uint8_t ENABLE_LED = 1;
const bool ON = true;
const bool OFF = false;
const char* FILENAME = "/capture.jpg";
const auto timerInterval = 20000;

const char* ssid = WIFI_NAME;
const char* password = WIFI_PW;

// where to upload the taken images to
String serverName = "10.0.0.253";
String serverPath = "/upload";
const int serverPort = 5000;

bool PIRSensorEnabled = true;
bool lightState = OFF;
bool captureImage = false;

WiFiClient client;
AsyncWebServer server(80);


#ifdef WEBHOOK
HTTPClient http;
#endif



void switchLight(bool state) {
  if (state) {
    digitalWrite(LED_PIN, HIGH);
    lightState = ON;
  } else {
    digitalWrite(LED_PIN, LOW);
    lightState = OFF;
  }
}


String sendPhoto() {
  String getAll;
  String getBody;

  Serial.println("Capturing image");
  
  camera_fb_t * fb = NULL;
  
  // Warm-up loop to discard first few frames
  for (int i = 0; i < 100; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Failed to capture frame");
      continue;
    }
    esp_camera_fb_return(fb);
  }
  fb = esp_camera_fb_get();

  switchLight(OFF);
  if(!fb) {
    Serial.println("Camera capture failed");
    ESP.restart();
  }


  // save the captured image to spiffs in case we want it served by the webserver
  File file = SPIFFS.open(FILENAME, FILE_WRITE);

  // Insert the data in the photo file
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.print("The picture has been saved");
  }
  // Close the file
  file.close();

  
  Serial.println("Connecting to " + serverName);

  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");    
    String head = "--SHICEBoundary\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--SHICEBoundary--\r\n";
    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;
  
    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=SHICEBoundary");
    client.println();
    client.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        client.write(fbBuf, remainder);
      }
    }   
    client.print(tail);
    
    esp_camera_fb_return(fb);
    
    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;
    Serial.println("Image uploaded. Waiting for response.");
    while ((startTimer + timoutTimer) > millis()) {
      Serial.print(".");
      delay(100);      
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length()==0) { state=true; }
          getAll = "";
        }
        else if (c != '\r') { getAll += String(c); }
        if (state==true) { getBody += String(c); }
        startTimer = millis();
      }
      if (getBody.length()>0) { break; }
    }
    Serial.println("Got response");
    client.stop();
    Serial.println(getBody);
  } else {
    esp_camera_fb_return(fb);
    getBody = "Connection to " + serverName +  " failed.";
  }
  return getBody;
  
}

String buildJsonConfigObject() {
  String jsonResponse = "{\"PIRSensorEnabled\":";
  jsonResponse += PIRSensorEnabled ? "true" : "false";
  jsonResponse += ",\"lightState\":";
  jsonResponse += lightState ? "true" : "false";
  jsonResponse += "}";
  return jsonResponse;
}


void setup() {
  // disable brown-out detection
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  WiFi.setHostname(NAME);
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  

  // init with high specs to pre-allocate larger buffers
  if(psramFound()){
    Serial.println("PSRAM found");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 3;
  } else {
    Serial.println("PSRAM not found");
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t *s = esp_camera_sensor_get();
  // adjust according to orientation of your cam
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
  

  pinMode(PIN_PIR, INPUT); 
  pinMode (LED_PIN, OUTPUT);

  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
  }

  //////////////////////
  // webserver callbacks
  //////////////////////

  // http://HOSTNAME/light?state={on|off}
  // turns the light on or off
  server.on("/light", HTTP_GET, [](AsyncWebServerRequest * request) {
    String state = request->getParam("state")->value();
    if (state == "on") {
      switchLight(ON);
      request->send(200, "text/plain", "Light turned on");
    } else if (state == "off") {
      switchLight(OFF);
      request->send(200, "text/plain", "Light turned off");
    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  // http://HOSTNAME/pir_sensor?state={on|off}
  // en-/disables the PIR sensor
  server.on("/pir_sensor", HTTP_GET, [](AsyncWebServerRequest * request) {
    String state = request->getParam("state")->value();
    if (state == "on") {
      PIRSensorEnabled = true;
      request->send(200, "text/plain", "PIR sensor enabled");
    } else if (state == "off") {
      PIRSensorEnabled = false;
      request->send(200, "text/plain", "PIR sensor disabled");
    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  // http://HOSTNAME/capture
  // capture a new image and save to SPIFFS
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    captureImage = true;
    request->send(200, "text/plain", "Taking Photo");
  });

  // http://HOSTNAME/latest_image
  // download the last image that was saved to SPIFFS
  server.on("/latest_image", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILENAME, "image/jpg", false);
  });

  // http://HOSTNAME/state
  // get a JSON response of the current state
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest * request) {
    String jsonResponse = buildJsonConfigObject();
    request->send(200, "application/json", jsonResponse);
  });

  // http://HOSTNAME/set_state
  // handler for handling the configuration request via a POST request with JSON body
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/set_state", [](AsyncWebServerRequest *request, JsonVariant &json) {
    const JsonObject& jsonObj = json.as<JsonObject>();
    if (jsonObj.containsKey("PIRSensorEnabled")) {
      PIRSensorEnabled = jsonObj["PIRSensorEnabled"];
      Serial.printf("PIR sensor state set to %s\n", PIRSensorEnabled ? "enabled" : "disabled");
    }
    if (jsonObj.containsKey("lightState")) {
      if (jsonObj["lightState"]) {
        switchLight(ON);
      }
      else {
        switchLight(OFF);
      }
    }
    String jsonResponse = buildJsonConfigObject();
    request->send(200, "application/json", jsonResponse);
  });
  server.addHandler(handler);


  server.begin();
  Serial.println("Web server started");

  // OTA stuff
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

void loop() {
  if (PIRSensorEnabled && digitalRead(PIN_PIR) == HIGH) { 
    captureImage = true;
  }
  
  if (captureImage) {
    // blink shortly to indicate begin of the capture process
    switchLight(ON);
    delay(250);
    switchLight(OFF);

    if (ENABLE_LED) {
      switchLight(ON);
    }

    // delay for 1st half of interval before capturing
    delay(timerInterval/2);

    // call webhook first if defined
    #ifdef WEBHOOK
    http.begin(WEBHOOK);
    // Send HTTP POST request
    int httpResponseCode = http.POST("");
    // we do not care about the response, so just go on
    http.end();
    #endif
    
    sendPhoto();
    
    // delay for 2nd half of interval after capturing
    delay(timerInterval/2);
    captureImage = false;
  }
  
  ArduinoOTA.handle();
}
