#include "application.hpp"

uint32_t led_time_on = 0;
uint32_t led_time_off = 0;
Ticker *_blinker = new Ticker();

#ifdef PIN_STATUS_LED_RGB
uint32_t led_color = 0;
#define PIN_STATUS_LED_COUNT 1
CRGB leds[PIN_STATUS_LED_COUNT];
#endif

void changeStatusLedStateOn();
void changeStatusLedStateOff();

#ifdef PIN_STATUS_LED_RGB
static uint32_t StatusLedBlinkTimings[LED_STATUSES_COUNT][LED_STATUSES_STATES_COUNT + 1] = {
    // on, off - in milliseconds, color (R,G,B)
    {1024, 16, 0x000080}, // NOT_READY
    {16, 1024, 0x808000}, // READY
    {256, 256, 0x800080}, // CLIENT_CONNECTED
    {256, 0, 0x008000}, // CLIENT_RX
    {256, 0, 0x000080}, // CLIENT_TX
    {128, 128, 0x800000}  // ERROR
};
#else
static uint32_t StatusLedBlinkTimings[LED_STATUSES_COUNT][LED_STATUSES_STATES_COUNT] = {
    // on, off - in milliseconds
    {1024, 16}, // NOT_READY
    {16, 1024}, // READY
    {256, 256}, // CLIENT_CONNECTED
    {256, 0}, // CLIENT_RX
    {256, 0}, // CLIENT_TX
    {128, 128}, // ERROR
};
#endif

void changeStatusLedStateOn()
{
    #ifdef PIN_STATUS_LED_RGB
    for (uint8_t i = 0; i < PIN_STATUS_LED_COUNT; i++)
        leds[0] = led_color;
    FastLED.show();
    #else 
    digitalWrite(PIN_STATUS_LED, PIN_STATUS_LED_ON);
    #endif
    _blinker->once_ms(led_time_on, changeStatusLedStateOff);
}

void changeStatusLedStateOff()
{
    #ifdef PIN_STATUS_LED_RGB
    for (uint8_t i = 0; i < PIN_STATUS_LED_COUNT; i++)
        leds[0] = 0x0;
    FastLED.show();
    #else 
    digitalWrite(PIN_STATUS_LED, !PIN_STATUS_LED_ON);
    #endif
    _blinker->once_ms(led_time_off, changeStatusLedStateOn);
}

Application::Application()
{
    _settings = new Configuration();
    _terminalServer = new WiFiServer(DEFAULT_TERMINAL_SERVER_PORT);

    _webSockServer = new WebSocketsServer(WEBSOCKET_PORT_);

    _WebServer = new ESP8266WebServer(WEB_SERVER_PORT);
    _FTPServer = new FtpServer();
    _wifiManager = new WiFiManager();

    setState(NOT_READY);
}

void Application::setState(ApplicationState state) 
{
    if (state == _state)
        return;

    logger->printf("Status changed to %d\n", state);
    _state = state;

    if (_blinker->active())
        _blinker->detach();

    led_time_on = StatusLedBlinkTimings[(int)state][0];
    led_time_off = StatusLedBlinkTimings[(int)state][1];
    #ifdef PIN_STATUS_LED_RGB
    led_color = StatusLedBlinkTimings[(int)state][2];
    #endif

    if (led_time_off == 0) {
        // no need to switch off
        #ifdef PIN_STATUS_LED_RGB
        for (uint8_t i = 0; i < PIN_STATUS_LED_COUNT; i++)
            leds[0] = led_color;
        FastLED.show();
        #else 
        digitalWrite(PIN_STATUS_LED, PIN_STATUS_LED_ON);
        #endif
    } else if (led_time_on == 0) {
        // no need to switch on
        #ifdef PIN_STATUS_LED_RGB
        for (uint8_t i = 0; i < PIN_STATUS_LED_COUNT; i++)
            leds[0] = 0;
        FastLED.show();
        #else 
        digitalWrite(PIN_STATUS_LED, !PIN_STATUS_LED_ON);
        #endif
    } else {
        // trigger blinking
        changeStatusLedStateOn();
    }
}

void Application::startWifi() {
    String ssid = "ESP-" + String(ESP.getChipId());
    WiFi.hostname(ssid);

    if(!_wifiManager->autoConnect()) {
        logger->println("Failed to connect and hit timeout");
        ESP.reset();
    } else {
        logger->print("Connected: ");
        logger->println(WiFi.localIP());
    }
}

void Application::halt()
{
    setState(ERROR);
    logger->println("fatal error - rebooting...");
    delay(REBOOT_DELAY);

    ESP.reset();
}

String Application::getContentType(const String &filename)
{
    if (filename.endsWith(".html"))
        return "text/html";

    else if (filename.endsWith(".css"))
        return "text/css";

    else if (filename.endsWith(".js"))
        return "application/javascript";

    else if (filename.endsWith(".ico"))
        return "image/x-icon";

    return HTTP_TEXT_PLAIN;
}

void Application::handleNotFound()
{
    if (!handleFileRead(_WebServer->uri()))
    {
        _WebServer->send(HTTP_SERVER_NOT_FOUND_, FPSTR(HTTP_TEXT_PLAIN), FPSTR(HTTP_NOT_FOUND_TEXT));
    }
}

bool Application::handleFileRead(String path)
{
    bool success = false;
    logger->println("handle file read: " + path);
    if (path.endsWith("/"))
        path += "index.html";

    String contentType = getContentType(path);
    if (LittleFS.exists(path))
    {
        // _WebServer->sendHeader("Cache-Control", "public, max-age=2678400\r\n");
        File file = LittleFS.open(path, "r");
        _WebServer->streamFile(file, contentType);
        file.close();

        logger->println(String("Sent file: ") + path);
        success = true;
    }
    else
        logger->println(String("File Not Found: ") + path);

    return success;
}

void Application::handleSettingsSave()
{
    logger->println("handle settings save");
    std::map<String, String> settingsMap;
    for (int i = 0; i < _WebServer->args(); i++)
    {
        settingsMap[_WebServer->argName(i)] = _WebServer->arg(i);
    }
    _settings->fromMapping(settingsMap);
    JSONConfig::write(FPSTR(CONFIG_FILENAME), *_settings, CONFIG_SIZE);
    _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_TEXT_PLAIN), FPSTR(AFTER_SAVING_MSG));

    delay(REBOOT_DELAY);
    ESP.reset();
}

void Application::handleGetSettings()
{
    logger->println("handle get settings");
    String serializedData;
    DynamicJsonDocument doc(CONFIG_SIZE);

    _settings->serialize(doc);
    serializeJson(doc, serializedData);
    _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_TEXT_PLAIN), serializedData);
}

void Application::handleTerminalClient()
{
    if (_terminalServer->hasClient())
    {
        if (_terminalClient.connected())
        {
            _terminalClient.println("new client connected, current connection aborted");
            _terminalClient.stop();
        }
        _terminalClient = _terminalServer->available();
        logger->println("Client connected to telnet server");
        setState(CLIENT_CONNECTED);
    }

    while (_terminalClient.available() && Serial.availableForWrite() > 0) {
        ApplicationState prev_state = _state;
        setState(CLIENT_TX);
        Serial.write(_terminalClient.read());
        setState(prev_state);
    }

    size_t maxToTcp = 0;
    if (_terminalClient)
    {
        size_t bytesCount = _terminalClient.availableForWrite();
        if (bytesCount)
        {
            if (!maxToTcp)
                maxToTcp = bytesCount;
            else
                maxToTcp = std::min(maxToTcp, bytesCount);
        }
        else
            logger->println("client is congested");
    }
    size_t bufferLen = std::min((size_t)Serial.available(), maxToTcp);
    bufferLen = std::min(bufferLen, STACK_MAX_SIZE);
    if (bufferLen)
    {
        ApplicationState prev_state = _state;
        setState(CLIENT_RX);
        uint8_t buffer[bufferLen];
        int serialGotBytesCount = Serial.readBytes(buffer, bufferLen);
        if (_terminalClient.availableForWrite() >= serialGotBytesCount)
        {
            size_t sent = _terminalClient.write(buffer, serialGotBytesCount);
            if (sent != bufferLen)
            {
                logger->printf("len mismatch: available:%zd serial-read:%zd tcp-write:%zd\n",
                               bufferLen, serialGotBytesCount, sent);
            }
        }
        setState(prev_state);
    }
}

void Application::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    if (type == WStype_CONNECTED)
    {
        logger->printf("ws client #%u connected from:", num);
        logger->println(_webSockServer->remoteIP(num));
        setState(CLIENT_CONNECTED);
    }
    else if (type == WStype_DISCONNECTED)
    {
        logger->printf("ws client #%u disconnected\n", num);
        setState(READY);
    }
    else if (type == WStype_TEXT)
    {
        ApplicationState prev_state = _state;
        setState(CLIENT_TX);
        logger->write(payload, length);
        Serial.write(payload, length);
        setState(prev_state);
    }
}

void Application::initialize()
{
    #ifdef PIN_STATUS_LED_RGB
    FastLED.addLeds<WS2812B, PIN_STATUS_LED, GRB>(leds, PIN_STATUS_LED_COUNT);
    #else
    pinMode(PIN_STATUS_LED, OUTPUT);
    #endif

    logger->begin(DEFAULT_BAUD_LOGGER);
    logger->println(FPSTR(WELCOME_STRING));

    if (!LittleFS.begin())
    {
        logger->println("failed to mount FS");
        halt();
    }

    JSONConfig::read(FPSTR(CONFIG_FILENAME), *_settings, CONFIG_SIZE);
    Serial.begin(_settings->serialBaud);
    Serial.pins(PIN_UART0_TX, PIN_UART0_RX);
    Serial.flush();
    Serial.setRxBufferSize(RX_BUFFER_SIZE);

    startWifi();
    
    setState(READY);
    
    _WebServer->on(FPSTR(HTTP_SAVE_LINK), [&]() mutable
                   { this->handleSettingsSave(); });
    _WebServer->on(FPSTR(HTTP_CONF_LINK), [&]() mutable
                   { this->handleGetSettings(); });
    _WebServer->onNotFound([&]() mutable
                           { handleNotFound(); });
    _WebServer->begin();

    _terminalServer->begin();
    _terminalServer->setNoDelay(true);
    _FTPServer->begin(FPSTR(FTP_LOGIN_), FPSTR(FTP_PASSWORD_));

    _webSockServer->begin();
    _webSockServer->onEvent([&](uint8_t num, WStype_t type, uint8_t *payload, size_t length) mutable
                            { this->handleWebSocketEvent(num, type, payload, length); });
}

void Application::mainloop()
{
    _webSockServer->loop();
    _FTPServer->handleFTP();
    _WebServer->handleClient();
    this->handleTerminalClient();
    this->handleWebConsole();
}

void Application::handleWebConsole()
{
    if (_terminalClient.connected())
    {
        if (!_terminalClientAlreadyConnected)
        {
            _terminalClientAlreadyConnected = true;
            _webSockServer->broadcastTXT("[[;red;]terminal client connected]");
            setState(CLIENT_CONNECTED);
        }
        return; /* exit */
    }
    else
    {
        if (_terminalClientAlreadyConnected)
        {
            _terminalClientAlreadyConnected = false;
            _webSockServer->broadcastTXT("[[;red;]terminal client disconnected]");
            setState(READY);
        }
    }

    ApplicationState prev_state = _state;
    static String line;
    while (Serial.available())
    {
        setState(CLIENT_RX);
        char chr = Serial.read();
        if ((chr == '\n') || (line.length() >= LINE_MAX))
        {
            _webSockServer->broadcastTXT(line);
            line.clear();
        }
        if (chr != '\n')
            line += chr;
    }

    setState(prev_state);
}