#define LGFX_M5STACK

// LGFX_AUTODETECT
// #define LGFX_M5STACK               // M5Stack (Basic / Gray / Go / Fire)
// #define LGFX_M5STACK_CORE2         // M5Stack Core2
// #define LGFX_M5STACK_COREINK       // M5Stack CoreInk
// #define LGFX_M5STICK_C             // M5Stick C / CPlus
// #define LGFX_M5PAPER               // M5Paper
// #define LGFX_ODROID_GO             // ODROID-GO
// #define LGFX_TTGO_TS               // TTGO TS
// #define LGFX_TTGO_TWATCH           // TTGO T-Watch
// #define LGFX_TTGO_TWRISTBAND       // TTGO T-Wristband
// #define LGFX_DDUINO32_XS           // DSTIKE D-duino-32 XS
// #define LGFX_LOLIN_D32_PRO         // LoLin D32 Pro
// #define LGFX_ESP_WROVER_KIT        // ESP-WROVER-KIT
// #define LGFX_WIFIBOY_PRO           // WiFiBoy Pro
// #define LGFX_WIFIBOY_MINI          // WiFiBoy mini
// #define LGFX_MAKERFABS_TOUCHCAMERA // Makerfabs Touch with Camera
// #define LGFX_MAKERFABS_MAKEPYTHON  // Makerfabs MakePython
// #define LGFX_WIO_TERMINAL          // Wio Terminal

#include <HTTPClient.h>
#include <ArduinoJson.h>           /* Arduino library manager */
#include <LovyanGFX.hpp>           /* Arduino library manager */

#include "setup.h"
#include "cert.h"

struct SpiRamAllocator {
    void* allocate(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    void deallocate(void* pointer) {
        heap_caps_free(pointer);
    }

    void* reallocate(void* ptr, size_t new_size) {
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    }
};

using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

const size_t JSON_BUFFER_SIZE = 1048576;

static LGFX lcd;

void setup() {
    ESP_LOGI(TAG, "\n\nShitcoin_Ticker for esp32 - (c) 2021 by Cellie\n");

    if (ESP.getFreePsram() < JSON_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Not enough SPIRAM found. (%i bytes required) System halted.", JSON_BUFFER_SIZE);
        while (true) delay(1000);
    }

    lcd.init();
    lcd.setBrightness(10);

#if defined( LGFX_ESP_WROVER_KIT )
    lcd.setRotation(1);
#endif

    lcd.setTextDatum(textdatum_t::top_center);
    lcd.setTextColor(0xFFFF00U, 0x000000U);
    lcd.setFont(&fonts::Font4);
    lcd.drawString("connecting WiFi",  lcd.width() / 2,  0);

    ESP_LOGI(TAG, "connecting to %s\n", SSID);

    WiFi.begin(SSID, PSK);
    WiFi.setSleep(false);
    while (!WiFi.isConnected())
        delay(10);

    //set the time
    ESP_LOGI(TAG, "syncing NTP");
    lcd.drawString("         syncing ntp         ",  lcd.width() / 2,  0);

    /* sync the clock with ntp */
    configTzTime(TIMEZONE, NTP_POOL);

    struct tm now;

    while (!getLocalTime(&now, 0))
        delay(10);

    ESP_LOGI(TAG, "synced NTP");
    //lcd.setFont(&fonts::Font7);
    lcd.setTextColor(0xFFFF00U, 0x000000U);
    lcd.drawString("    " + coinName + " @ kraken.com" + "    ", lcd.width() / 2,  0);

    getTradesSince(time(NULL) - (10 * 60));
}

void loop() {
    static const int DELAY_SECONDS {1};

    delay(DELAY_SECONDS * 1000);
    getTradesSince(time(NULL) - DELAY_SECONDS);
}

void getTradesSince(const time_t tm) {

    static const String BASE_URL {"https://api.kraken.com/0/public/Trades?pair=" + coinName + "&since="};

    WiFiClientSecure secureClient;

    if (USE_SSL)
        secureClient.setCACert(CA_CERTIFICATE);
    else
        secureClient.setInsecure();

    {   // extra scopingblock to make sure HTTPClient is destroyed before secureClient
        HTTPClient http;

        http.setConnectTimeout(2500);
        http.useHTTP10(true);

        if (!http.begin(secureClient, BASE_URL + String(tm).c_str())) {
            ESP_LOGE(TAG, "Could not connect ");
            return;
        }

        http.setTimeout(1000);

        const int HTTP_RESULT {http.GET()};

        switch (HTTP_RESULT) {
            case HTTP_CODE_OK :
                {
                    static SpiRamJsonDocument doc(JSON_BUFFER_SIZE);
                    const DeserializationError JSON_ERROR {deserializeJson(doc, http.getStream())};

                    if (JSON_ERROR) {
                        ESP_LOGE(TAG, "deserializeJson() failed: %s", JSON_ERROR.c_str());
                    }
                    else {
                        //Serial.begin(115200);
                        //serializeJsonPretty(doc, Serial);  //might take very long time with doc in PSRAM
                        //Serial.flush();
                        //Serial.end();

                        //find the last trade
                        int currentTrade {0};
                        while (!doc["result"][coinName][currentTrade][0].isNull())
                            currentTrade++;

                        static String lastPrice{""};

                        if (currentTrade--) {
                            lastPrice = doc["result"][coinName][currentTrade][0].as<String>();
                            ESP_LOGI(TAG, "last price: %s", lastPrice.c_str());
                            displayPrice(lastPrice, true);
                        }
                        else {
                            displayPrice(lastPrice, false);
                        }
                    }
                    break;
                }
            default :
                ESP_LOGW(TAG, "[HTTP] GET... failed, error: %i - %s", HTTP_RESULT, http.errorToString(HTTP_RESULT).c_str());
        }
        http.end();
    }
    secureClient.stop();
    secureClient.flush();
}

void displayPrice(const String& priceStr, const bool refreshDate) {

    static float lastPrice {0};

    const float currentPrice {strtof(priceStr.c_str(), NULL)};

    const int color {(lastPrice > currentPrice) ? TFT_RED : (lastPrice < currentPrice) ? TFT_GREEN : TFT_WHITE};

    lastPrice = currentPrice;

    int length {6};
    switch (priceStr.indexOf('.')) {
        case 1 :
        case 5 : length = 5;
    }

    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(color, 0x000000U);
    lcd.setFont(&fonts::Font8);
    lcd.drawString(priceStr.substring(0, length), lcd.width() / 2, lcd.height() / 2);

    if (!refreshDate) return;

    // display the date/time footer
    time_t rawtime;
    time(&rawtime);

    const struct tm* timeinfo {
        localtime(&rawtime)
    };

    lcd.setTextDatum(textdatum_t::top_center);
    lcd.setFont(&fonts::Roboto_Thin_24);
    lcd.setTextColor(0xFFFF00U, 0x000000U);
    lcd.drawString("Last price received:", lcd.width() / 2, 180);
    lcd.drawString("  " + String(asctime(timeinfo)) + "  ", lcd.width() / 2, 210);
}
