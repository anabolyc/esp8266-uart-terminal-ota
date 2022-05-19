#ifndef WIRELESS_TERMINAL_APPLICATION_H_INCLUDED
#define WIRELESS_TERMINAL_APPLICATION_H_INCLUDED

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266FtpServer.h>
#include <WiFiManager.h>  
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <Ticker.h>
#include <memory>
#include <map>

#include "logger.hpp"
#include "jsonconfig.hpp"

static const char AFTER_SAVING_MSG[] PROGMEM = "Rebooting...\n"
"Please plug-in 'Wireless Terminal' to device and connect with terminal (23 port)";

static const char WELCOME_STRING[] PROGMEM = "\nWireless terminal (SW version: 0.9)";
static const char INDEX_PAGE_FILENAME[] PROGMEM = "index.html";
static const char CONFIG_FILENAME[] PROGMEM = "/config.json";
static const char HTTP_TEXT_PLAIN[] PROGMEM = "text/plain";
static const char HTTP_TEXT_HTML[] PROGMEM = "text/html";
static const char HTTP_NOT_FOUND_TEXT[] PROGMEM = "Not Found";
static const char HTTP_CONF_LINK[] PROGMEM = "/configure";
static const char HTTP_SAVE_LINK[] PROGMEM = "/save";
static const char HTTP_ROOT_LINK[] PROGMEM = "/";
static const char FTP_LOGIN_[] PROGMEM = "admin";
static const char FTP_PASSWORD_[] PROGMEM = "admin";

const IPAddress DEFAULT_AP_GATEWAY(0, 0, 0, 0);
const IPAddress DEFAULT_AP_MASK(255, 255, 255, 0);
const size_t STACK_MAX_SIZE = 512;

#define DEFAULT_TERMINAL_SERVER_PORT 23
#define WEB_SERVER_PORT 80
#define DNS_SERVER_PORT 53
#define CONFIG_SIZE 2048
#define HTTP_SERVER_OK_ 200
#define HTTP_SERVER_NOT_FOUND_ 404
#define REBOOT_DELAY 3000
#define RX_BUFFER_SIZE 1024
#define LINE_MAX 80
#define WEBSOCKET_PORT_ 81
#define PIN_UART0_RX 13   
#define PIN_UART0_TX 15

#define PIN_STATUS_LED 12
#define PIN_STATUS_LED_ON HIGH
#define PIN_STATUS_LED_RGB 

#ifdef PIN_STATUS_LED_RGB
#include <FastLED.h>
#endif

#define LED_STATUSES_COUNT 6
#define LED_STATUSES_STATES_COUNT 2

enum ApplicationState {
    NOT_READY,          // 0
    READY,              // 1
    CLIENT_CONNECTED,   // 2
    CLIENT_RX,          // 3
    CLIENT_TX,          // 4
    ERROR               // 5
};

class Application
{
public:
    Application();
    void initialize();
    void mainloop();

protected:
    ApplicationState _state;

    Configuration *_settings;
    WiFiClient _terminalClient;
    WiFiServer *_terminalServer;
    ESP8266WebServer *_WebServer;
    WiFiManager* _wifiManager;

    WebSocketsServer *_webSockServer;

    FtpServer *_FTPServer;

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
    String getContentType(const String &filename);
    bool handleFileRead(String path);
    void handleNotFound();
    void handleWebConsole();

    void handleTerminalClient();
    void handleSettingsSave();
    void handleGetSettings();
    bool handleRoot();
    void startWifi();
    void halt();
    void setState(ApplicationState);

private:
    bool _terminalClientAlreadyConnected = false;
};

#endif