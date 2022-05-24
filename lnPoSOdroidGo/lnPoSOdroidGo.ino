#include <odroid_go.h>

#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <AutoConnect.h>
#include "Bitcoin.h"
#include <Hash.h>

using WebServerClass = WebServer;
fs::SPIFFSFS &FlashFS = SPIFFS;

#define PIN_BLUE_LED 2

#define FORMAT_ON_FAIL true
#define PARAM_FILE "/elements.json"
#define KEY_FILE "/thekey.txt"
#define INVOICE_FILE "/invoice.txt"

// Variables
String lnurl;
String currency;
String lncurrency;
String key;
String preparedURL;
String baseURL;
String apPassword = "ToTheMoon1"; // default WiFi AP password
String masterKey;
String lnbitsServer;
String invoice;
String baseURLPoS;
String secretPoS;
String currencyPoS;
String baseURLATM;
String secretATM;
String currencyATM;
String lnurlATMMS;
String dataIn = "0";
String noSats = "0";
String qrData;
String dataId;
String addressNo;
String pinToShow;
String originalPinToShow;
char menuItems[4][12] = {"LNPoS", "LNURLPoS", "OnChain", "LNURLATM"};
String menuItemLabels[4] = {"Lightning (online)", "Lightning (offline)", "Bitcoin (on-chain)", "Lightning ATM"};
int menuItemCheck[4] = {0, 0, 0, 0};
String selection;
int menuItemNo = 0;
int randomPin;
int calNum = 1;
int sumFlag = 0;
int converted = 0;
bool onchainCheck = false;
bool lnCheck = false;
bool lnurlCheck = false;
bool unConfirmed = true;
bool selected = false;
bool lnurlCheckPoS = false;
bool lnurlCheckATM = false;
String lnurlATMPin;

String inputAmount = "0000.00";
int inputPos = 3;
String invoiceNo;

// Custom access point pages
static const char PAGE_ELEMENTS[] PROGMEM = R"(
{
  "uri": "/posconfig",
  "title": "PoS Options",
  "menu": true,
  "element": [
    {
      "name": "text",
      "type": "ACText",
      "value": "LNPoS options",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;"
    },
    {
      "name": "password",
      "type": "ACInput",
      "label": "Password for PoS AP WiFi",
      "value": "ToTheMoon1"
    },

    {
      "name": "offline",
      "type": "ACText",
      "value": "Onchain *optional",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;"
    },
    {
      "name": "masterkey",
      "type": "ACInput",
      "label": "Master Public Key"
    },

    {
      "name": "heading1",
      "type": "ACText",
      "value": "Lightning *optional",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;"
    },
    {
      "name": "server",
      "type": "ACInput",
      "label": "LNbits Server"
    },
    {
      "name": "invoice",
      "type": "ACInput",
      "label": "Wallet Invoice Key"
    },
    {
      "name": "lncurrency",
      "type": "ACInput",
      "label": "PoS Currency ie EUR"
    },
    {
      "name": "heading2",
      "type": "ACText",
      "value": "Offline Lightning *optional",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;"
    },
    {
      "name": "lnurlpos",
      "type": "ACInput",
      "label": "LNURLPoS String"
    },
    {
      "name": "heading3",
      "type": "ACText",
      "value": "Offline Lightning *optional",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;"
    },
    {
      "name": "lnurlatm",
      "type": "ACInput",
      "label": "LNURLATM String"
    },
    {
      "name": "lnurlatmms",
      "type": "ACInput",
      "value": "mempool.space",
      "label": "mempool.space server"
    },
    {
      "name": "lnurlatmpin",
      "type": "ACInput",
      "value": "878787",
      "label": "LNURLATM pin String"
    },
    {
      "name": "load",
      "type": "ACSubmit",
      "value": "Load",
      "uri": "/posconfig"
    },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "Save",
      "uri": "/save"
    },
    {
      "name": "adjust_width",
      "type": "ACElement",
      "value": "<script type='text/javascript'>window.onload=function(){var t=document.querySelectorAll('input[]');for(i=0;i<t.length;i++){var e=t[i].getAttribute('placeholder');e&&t[i].setAttribute('size',e.length*.8)}};</script>"
    }
  ]
 }
)";

static const char PAGE_SAVE[] PROGMEM = R"(
{
  "uri": "/save",
  "title": "Elements",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "format": "Elements have been saved to %s",
      "style": "font-family:Arial;font-size:18px;font-weight:400;color:#191970"
    },
    {
      "name": "validated",
      "type": "ACText",
      "style": "color:red"
    },
    {
      "name": "echo",
      "type": "ACText",
      "style": "font-family:monospace;font-size:small;white-space:pre;"
    },
    {
      "name": "ok",
      "type": "ACSubmit",
      "value": "OK",
      "uri": "/posconfig"
    }
  ]
}
)";

SHA256 h;
WebServerClass server;
AutoConnect portal(server);
AutoConnectConfig config;
AutoConnectAux elementsAux;
AutoConnectAux saveAux;

void setup()
{
  // put your setup code here, to run once:
  GO.begin();
  GO.Speaker.mute();

  pinMode(PIN_BLUE_LED, OUTPUT);
  digitalWrite(PIN_BLUE_LED, LOW);

  logo();

  h.begin();

  FlashFS.begin(FORMAT_ON_FAIL);
  SPIFFS.begin(true);
  // Get the saved details and store in global variables
  File paramFile = FlashFS.open(PARAM_FILE, "r");
  if (paramFile)
  {
    StaticJsonDocument<2500>
        doc;
    DeserializationError error = deserializeJson(doc, paramFile.readString());

    JsonObject passRoot = doc[0];
    const char *apPasswordChar = passRoot["value"];
    const char *apNameChar = passRoot["name"];
    if (String(apPasswordChar) != "" && String(apNameChar) == "password")
    {
      apPassword = apPasswordChar;
    }
    JsonObject maRoot = doc[1];
    const char *masterKeyChar = maRoot["value"];
    masterKey = masterKeyChar;
    if (masterKey != "")
    {
      menuItemCheck[2] = 1;
    }
    JsonObject serverRoot = doc[2];
    const char *serverChar = serverRoot["value"];
    lnbitsServer = serverChar;
    JsonObject invoiceRoot = doc[3];
    const char *invoiceChar = invoiceRoot["value"];
    invoice = invoiceChar;
    if (invoice != "")
    {
      menuItemCheck[0] = 1;
    }
    JsonObject lncurrencyRoot = doc[4];
    const char *lncurrencyChar = lncurrencyRoot["value"];
    lncurrency = lncurrencyChar;
    JsonObject lnurlPoSRoot = doc[5];
    const char *lnurlPoSChar = lnurlPoSRoot["value"];
    String lnurlPoS = lnurlPoSChar;
    baseURLPoS = getValue(lnurlPoS, ',', 0);
    secretPoS = getValue(lnurlPoS, ',', 1);
    currencyPoS = getValue(lnurlPoS, ',', 2);
    if (secretPoS != "")
    {
      menuItemCheck[1] = 1;
    }
    JsonObject lnurlATMRoot = doc[6];
    const char *lnurlATMChar = lnurlATMRoot["value"];
    String lnurlATM = lnurlATMChar;
    baseURLATM = getValue(lnurlATM, ',', 0);
    secretATM = getValue(lnurlATM, ',', 1);
    currencyATM = getValue(lnurlATM, ',', 2);
    if (secretATM != "")
    {
      menuItemCheck[3] = 1;
    }
    JsonObject lnurlATMMSRoot = doc[7];
    const char *lnurlATMMSChar = lnurlATMMSRoot["value"];
    lnurlATMMS = lnurlATMMSChar;
    JsonObject lnurlATMPinRoot = doc[8];
    const char *lnurlATMPinChar = lnurlATMPinRoot["value"];
    lnurlATMPin = lnurlATMPinChar;

    originalPinToShow = "";
    for (int i = 0; i < lnurlATMPin.length(); i++)
    {
      originalPinToShow += "0";
    }
  }

  paramFile.close();

  // Handle access point traffic

  server.on("/", []()
            {
    String content = "<h1>LNPoS</br>Free open-source bitcoin PoS</h1>";
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content); });

  elementsAux.load(FPSTR(PAGE_ELEMENTS));
  elementsAux.on([](AutoConnectAux &aux, PageArgument &arg)
                 {
    File param = FlashFS.open(PARAM_FILE, "r");
    if (param)
    {
      aux.loadElement(param, {"password", "masterkey", "server", "invoice", "lncurrency", "lnurlpos", "lnurlatm", "lnurlatmms", "lnurlatmpin"});
      param.close();
    }
    if (portal.where() == "/posconfig")
    {
      File param = FlashFS.open(PARAM_FILE, "r");
      if (param)
      {
        aux.loadElement(param, {"password", "masterkey", "server", "invoice", "lncurrency", "lnurlpos", "lnurlatm", "lnurlatmms", "lnurlatmpin"});
        param.close();
      }
    }
    return String(); });

  saveAux.load(FPSTR(PAGE_SAVE));
  saveAux.on([](AutoConnectAux &aux, PageArgument &arg)
             {
    aux["caption"].value = PARAM_FILE;
    File param = FlashFS.open(PARAM_FILE, "w");
    if (param)
    {
      // Save as a loadable set for parameters.
      elementsAux.saveElement(param, {"password", "masterkey", "server", "invoice", "lncurrency", "lnurlpos", "lnurlatm", "lnurlatmms", "lnurlatmpin"});
      param.close();
      // Read the saved elements again to display.
      param = FlashFS.open(PARAM_FILE, "r");
      aux["echo"].value = param.readString();
      param.close();
    }
    else
    {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String(); });

  /*
  config.auth = AC_AUTH_BASIC;
  config.authScope = AC_AUTHSCOPE_AUX;
  config.username = "lnPoS";
  config.password = apPassword;
  */

  config.ticker = true;
  config.autoReconnect = true;
  config.apid = "PoS-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  config.psk = apPassword;
  config.menuItems = AC_MENUITEM_CONFIGNEW | AC_MENUITEM_OPENSSIDS | AC_MENUITEM_RESET;
  config.reconnectInterval = 1;
  config.title = "LNPoS";
  int timer = 0;

  // Give few seconds to trigger portal
  while (timer < 2000)
  {
    GO.update();

    if (GO.BtnA.isPressed())
    {
      portalLaunch();
      config.immediateStart = true;
      portal.join({elementsAux, saveAux});
      portal.config(config);
      portal.begin();
      while (true)
      {
        portal.handleClient();
      }
    }

    timer = timer + 200;
    delay(200);
  }

  if (menuItemCheck[0])
  {
    portal.join({elementsAux, saveAux});
    config.autoRise = false;
    portal.config(config);
    portal.begin();
  }
}

void loop()
{
  // put your main code here, to run repeatedly:

  GO.lcd.clear();
  GO.Speaker.mute();
  pinMode(PIN_BLUE_LED, OUTPUT);
  digitalWrite(PIN_BLUE_LED, LOW);

  noSats = "0";
  dataIn = "0";
  inputAmount = "0000.00";

  unConfirmed = true;
  int menuItemsAmount = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    menuItemCheck[0] = 0;
  }

  for (int i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); i++)
  {
    if (menuItemCheck[i] == 1)
    {
      menuItemsAmount++;
      selection = menuItems[i];
    }
  }

  // If no methods available
  if (menuItemsAmount < 1)
  {
    error("No config found", "Restart device and hold A to set up");
    delay(10000000);
  }

  drawHeader();

  // If only one payment method available skip menu
  if (menuItemsAmount == 1)
  {
    showPaymentMethod(selection);
  }
  // If more than one payment method available trigger menu
  else
  {
    while (unConfirmed)
    {
      menuLoop();
      showPaymentMethod(selection);
    }
  }

  delay(10);
}

void showPaymentMethod(String paymentMethod)
{
  if (paymentMethod == "LNPoS")
  {
    lnMain();
  }
  if (paymentMethod == "OnChain")
  {
    onchainMain();
  }
  if (paymentMethod == "LNURLPoS")
  {
    lnurlPoSMain();
  }
  if (paymentMethod == "LNURLATM")
  {
    lnurlATMMain();
  }
}

void logo()
{
  GO.lcd.clear();

  GO.lcd.setTextWrap(1);
  GO.lcd.setTextFont(2);
  GO.lcd.setTextSize(4);

  GO.lcd.setCursor(40, 20);
  GO.lcd.setTextColor(ORANGE);
  GO.lcd.print("Bitcoin");

  GO.lcd.setTextSize(2);
  GO.lcd.setCursor(40, 80);
  GO.lcd.print("Point of Sale");

  GO.lcd.setTextSize(1);
  GO.lcd.setTextColor(WHITE);

  GO.lcd.drawLine(40, 120, 280, 120, WHITE);

  GO.lcd.setCursor(40, 126);
  GO.lcd.print("Powered by LNbits");

  GO.lcd.setTextColor(DARKGREY);
  GO.lcd.setCursor(40, 210);
  GO.lcd.print("Hold A while powering on to set up");
}

void portalLaunch()
{
  GO.lcd.clear();
  GO.lcd.setTextColor(PURPLE);
  GO.lcd.setTextSize(2);

  GO.lcd.setCursor(20, 20);
  GO.lcd.print("Setup mode - WiFi AP");

  GO.lcd.setTextColor(WHITE);
  GO.lcd.setCursor(20, 60);
  GO.lcd.print("Please connect");

  GO.lcd.setCursor(20, 90);
  GO.lcd.print("to this WiFi:");

  GO.lcd.setTextColor(ORANGE);
  GO.lcd.setCursor(20, 130);
  GO.lcd.print(config.apid);

  GO.lcd.setTextColor(WHITE);
  GO.lcd.setCursor(20, 200);
  GO.lcd.setTextSize(1);
  GO.lcd.print("Turn system off & on again when finished.");
}

void error(String message, String additional)
{
  drawHeaderMessage("Uh oh!");
  clearContent();
  GO.lcd.setTextColor(RED);
  GO.lcd.setTextSize(2);
  GO.lcd.setCursor(20, 40);
  GO.lcd.print(message);
  if (additional != "")
  {
    GO.lcd.setTextColor(WHITE);
    GO.lcd.setCursor(20, 80);
    GO.lcd.setTextSize(1);
    GO.lcd.print(additional);
  }
}

void showMessage(String message, int color = WHITE)
{
  clearContent();
  GO.lcd.setTextWrap(1);
  GO.lcd.setTextColor(color);
  GO.lcd.setTextSize(2);
  GO.lcd.setTextWrap(1);
  GO.lcd.setCursor(20, 40);
  GO.lcd.print(message);
}

void processing(String message)
{
  drawHeaderMessage("Please wait...");
  showMessage(message);
}

void complete()
{
  GO.lcd.clear();
  drawHeaderMessage("");
  showMessage("Complete!", GREEN);
}

void drawButtonTriangle(int x, int color = WHITE)
{
  int y = 230;
  GO.lcd.fillTriangle(x, y, x + 10, y, x + 5, y + 6, color);
}

void drawMenuButton()
{
  GO.lcd.setTextColor(WHITE);
  GO.lcd.setTextSize(1);
  GO.lcd.setCursor(15, 210);
  GO.lcd.print("MENU");
  drawButtonTriangle(15);
}

void drawInvoiceButton()
{
  int invoiceColor = WHITE;

  if (inputAmount == "0000.00")
  {
    invoiceColor = DARKGREY;
  }

  GO.lcd.setTextColor(invoiceColor);
  GO.lcd.setCursor(260, 210);
  GO.lcd.print("INVOICE");
  drawButtonTriangle(295, invoiceColor);
}

void drawWithdrawButton()
{
  int withdrawColor = WHITE;

  if (inputAmount == "0000.00")
  {
    withdrawColor = DARKGREY;
  }

  GO.lcd.setTextColor(withdrawColor);
  GO.lcd.setCursor(244, 210);
  GO.lcd.print("WITHDRAW");
  drawButtonTriangle(295, withdrawColor);
}

void drawDpad(int x, int y, int color = WHITE, int size = 12)
{
  int usize = size / 3;

  GO.lcd.drawLine(x, y + usize, x + usize, y + usize, color);
  GO.lcd.drawLine(x + usize, y + usize, x + usize, y, color);
  GO.lcd.drawLine(x + usize, y, x + (usize * 2), y, color);
  GO.lcd.drawLine(x + (usize * 2), y, x + (usize * 2), y + usize, color);
  GO.lcd.drawLine(x + (usize * 2), y + usize, x + size, y + usize, color);
  GO.lcd.drawLine(x + size, y + usize, x + size, y + (usize * 2), color);
  GO.lcd.drawLine(x + size, y + (usize * 2), x + (usize * 2), y + (usize * 2), color);
  GO.lcd.drawLine(x + (usize * 2), y + (usize * 2), x + (usize * 2), y + size, color);
  GO.lcd.drawLine(x + (usize * 2), y + size, x + usize, y + size, color);
  GO.lcd.drawLine(x + usize, y + size, x + usize, y + (usize * 2), color);
  GO.lcd.drawLine(x + usize, y + (usize * 2), x, y + (usize * 2), color);
  GO.lcd.drawLine(x, y + (usize * 2), x, y + usize, color);
}

void drawButton(int x, int y, String button = "A", int color = WHITE)
{
  GO.lcd.setTextFont(2);
  GO.lcd.setTextColor(color);
  GO.lcd.setTextSize(1);
  GO.lcd.setCursor(x + 3, y);
  GO.lcd.print(button);
  GO.lcd.drawCircle(x + 6, y + 8, 9, color);
}

void menuLoop()
{
  drawHeaderMessage("Select mode");

  GO.lcd.setTextSize(1);
  GO.lcd.setCursor(15, 210);
  GO.lcd.print("CHANGE");
  drawButtonTriangle(15);

  GO.lcd.setCursor(260, 210);
  GO.lcd.print("SELECT");
  drawButtonTriangle(295, WHITE);

  selection = "";
  selected = true;

  GO.lcd.setTextFont(1);

  while (selected)
  {
    if (menuItemCheck[0] == 0 && menuItemNo == 0)
    {
      menuItemNo = menuItemNo + 1;
    }

    GO.lcd.setTextSize(2);

    for (int i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); i++)
    {

      if (menuItemCheck[i] == 1)
      {
        if (menuItems[i] == menuItems[menuItemNo])
        {
          GO.lcd.setTextColor(GREEN);
          selection = menuItems[i];
        }
        else
        {
          GO.lcd.setTextColor(WHITE);
        }

        GO.lcd.setCursor(20, 44 + (i * 34));
        GO.lcd.print(menuItemLabels[i]);
      }
    }

    bool btnloop = true;
    while (btnloop)
    {
      GO.update();

      if (GO.BtnMenu.isPressed())
      {
        menuItemNo = menuItemNo + 1;
        if (menuItemCheck[menuItemNo] == 0)
        {
          menuItemNo = menuItemNo + 1;
        }
        if (menuItemNo >= (sizeof(menuItems) / sizeof(menuItems[0])))
        {
          menuItemNo = 0;
        }
        btnloop = false;
      }

      if (GO.BtnStart.isPressed())
      {
        selected = false;
        btnloop = false;
        delay(300);
      }

      delay(150);
    }
  }

  GO.lcd.setTextFont(2);
}

void getSats()
{
  WiFiClientSecure client;
  lnbitsServer.toLowerCase();
  Serial.println(lnbitsServer);
  if (lnbitsServer.substring(0, 8) == "https://")
  {
    Serial.println(lnbitsServer.substring(8, lnbitsServer.length()));
    lnbitsServer = lnbitsServer.substring(8, lnbitsServer.length());
  }
  client.setInsecure(); // Some versions of WiFiClientSecure need this
  const char *lnbitsServerChar = lnbitsServer.c_str();
  const char *invoiceChar = invoice.c_str();
  const char *lncurrencyChar = lncurrency.c_str();

  if (!client.connect(lnbitsServerChar, 443))
  {
    Serial.println("failed");
    error("SERVER DOWN", lnbitsServerChar);
    delay(3000);
  }

  String toPost = "{\"amount\" : 1, \"from\" :\"" + String(lncurrencyChar) + "\"}";
  String url = "/api/v1/conversion";
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + String(lnbitsServerChar) + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "X-Api-Key: " + String(invoiceChar) + " \r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n" +
               "Content-Length: " + toPost.length() + "\r\n" +
               "\r\n" +
               toPost + "\n");

  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (line == "\r")
    {
      break;
    }
    if (line == "\r")
    {
      break;
    }
  }
  String line = client.readString();
  StaticJsonDocument<150> doc;
  DeserializationError error = deserializeJson(doc, line);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  converted = doc["sats"];
}

void lnMain()
{
  inputAmount = "0000.00";
  inputPos = 3;

  if (converted == 0)
  {
    processing("Fetching BTC/" + lncurrency + " rate");
    getSats();
  }

  isLNMoneyNumber(true);

  while (unConfirmed)
  {
    GO.update();

    getKeypad(false, false, true, false);

    if (GO.BtnMenu.isPressed())
    {
      unConfirmed = false;
    }

    if (inputAmount != "0000.00" && GO.BtnStart.isPressed())
    {
      processing("Fetching invoice");
      String memo = getInvoice();
      delay(1000);

      qrShowCodeln(memo);

      while (unConfirmed)
      {
        int timer = 0;
        unConfirmed = checkInvoice();

        if (!unConfirmed)
        {
          complete();
          timer = 50000;
          delay(3000);
        }

        while (timer < 4000)
        {
          GO.update();

          if (GO.BtnMenu.isPressed())
          {
            noSats = "0";
            dataIn = "0";
            inputAmount = "0000.00";

            unConfirmed = false;
            timer = 5000;
          }

          delay(200);
          timer = timer + 100;
        }
      }

      noSats = "0";
      dataIn = "0";
      inputAmount = "0000.00";
    }

    delay(50);
  }
}

void drawNumberInputHelp()
{
  GO.lcd.setTextFont(2);
  GO.lcd.setTextColor(WHITE);
  GO.lcd.setTextSize(1);
  GO.lcd.setCursor(20, 40);
  GO.lcd.print("Use     and");
  drawDpad(48, 40, WHITE, 15);
  drawButton(102, 40, "A", WHITE);
  drawButton(126, 40, "B", WHITE);
}

void isLNMoneyNumber(bool cleared)
{
  clearContent();

  drawHeaderMessage("Enter amount in " + lncurrency);
  drawNumberInputHelp();

  if (!cleared)
  {
    noSats = String(converted * dataIn.toFloat());
  }
  else
  {
    noSats = "0";
    dataIn = "0";
    inputAmount = "0000.00";
  }

  GO.lcd.setTextSize(2);

  GO.lcd.setCursor(20, 80);
  GO.lcd.print(lncurrency + ":");

  // GO.lcd.setTextColor(RED);
  GO.lcd.setCursor(110, 80);
  GO.lcd.print(inputAmount);

  int rctOffset = inputPos > 4 ? -6 : 0;
  GO.lcd.drawRect(110 + rctOffset + (inputPos * 16), 110, 14, 2, WHITE);

  GO.lcd.setTextColor(WHITE);
  GO.lcd.setCursor(20, 120);
  GO.lcd.print("SATS:");

  GO.lcd.setTextColor(GREEN);
  GO.lcd.setCursor(110, 120);
  GO.lcd.print(noSats.toInt());

  drawMenuButton();
  drawInvoiceButton();
}

// Onchain payment method

String getAddressNo()
{
  File file = SPIFFS.open(KEY_FILE);
  if (file)
  {
    addressNo = file.readString();
    addressNo = String(addressNo.toInt() + 1);
    file.close();
    file = SPIFFS.open(KEY_FILE, FILE_WRITE);
    file.print(addressNo);
    file.close();
  }
  else
  {
    file.close();
    file = SPIFFS.open(KEY_FILE, FILE_WRITE);
    addressNo = "1";
    file.print(addressNo);
    file.close();
  }
  return addressNo;
}

void onchainMain()
{
  inputScreenOnChain();

  while (unConfirmed)
  {
    GO.update();

    if (GO.BtnMenu.isPressed())
    {
      unConfirmed = false;
    }

    if (GO.BtnStart.isPressed())
    {
      addressNo = getAddressNo();

      HDPublicKey xpub(masterKey);
      HDPublicKey pub;
      pub = xpub.child(0).child(addressNo.toInt());
      Serial.println(pub.address());

      qrData = pub.address();
      qrShowCodeOnchain(true);

      GO.lcd.setTextColor(WHITE);
      GO.lcd.setTextSize(1);

      GO.lcd.setCursor(20, 4);
      GO.lcd.print("Address #" + String(addressNo));

      drawMenuButton();

      GO.lcd.setCursor(220, 210);
      GO.lcd.print("Check address");
      drawButtonTriangle(295, WHITE);

      while (unConfirmed)
      {
        GO.update();

        if (GO.BtnMenu.isPressed())
        {
          unConfirmed = false;
        }
        if (GO.BtnStart.isPressed())
        {
          while (unConfirmed)
          {
            qrData = "https://" + lnurlATMMS + "/address/" + qrData;
            qrShowCodeOnchain(false);

            GO.lcd.setCursor(64, 0);
            GO.lcd.print("Scan to check address balance");

            drawMenuButton();

            while (unConfirmed)
            {
              GO.update();

              if (GO.BtnMenu.isPressed())
              {
                unConfirmed = false;
              }

              delay(50);
            }
          }
        }

        delay(50);
      }

      delay(50);
    }

    delay(50);
  }
}

// LN offline

void showPin()
{
  GO.lcd.clearDisplay();

  GO.lcd.setTextColor(WHITE);
  GO.lcd.setTextSize(2);
  GO.lcd.setCursor(90, 50);
  GO.lcd.println("PROOF PIN");

  GO.lcd.setTextSize(1);
  GO.lcd.setTextFont(7);
  GO.lcd.setCursor(90, 90);
  GO.lcd.setTextColor(GREEN);
  GO.lcd.println(randomPin);

  GO.lcd.setTextFont(2);
  drawMenuButton();
}

void lnurlPoSMain()
{
  pinToShow = "";

  inputAmount = "0000.00";
  inputPos = 3;

  isLNURLMoneyNumber(true);

  while (unConfirmed)
  {
    GO.update();

    getKeypad(false, false, false, false);

    if (GO.BtnMenu.isPressed())
    {
      unConfirmed = false;
    }
    else if (inputAmount != "0000.00" && GO.BtnStart.isPressed())
    {
      makeLNURL();
      qrShowCodeLNURL();

      GO.lcd.setCursor(246, 210);
      GO.lcd.print("SHOW PIN");
      drawButtonTriangle(295, WHITE);

      while (unConfirmed)
      {
        GO.update();

        if (GO.BtnStart.isPressed())
        {
          showPin();

          while (unConfirmed)
          {
            GO.update();

            if (GO.BtnMenu.isPressed())
            {
              unConfirmed = false;
            }

            delay(50);
          }
        }
        else if (GO.BtnMenu.isPressed())
        {
          unConfirmed = false;
        }

        delay(50);
      }
    }

    delay(50);
  }
}

void isLNURLMoneyNumber(bool cleared)
{
  clearContent();

  if (cleared)
  {
    dataIn = "0";
    inputAmount = "0000.00";
  }

  drawHeaderMessage("Enter amount in " + currencyPoS);
  drawNumberInputHelp();

  GO.lcd.setTextSize(2);

  GO.lcd.setCursor(20, 80);
  GO.lcd.print(currencyPoS + ":");

  GO.lcd.setCursor(110, 80);
  GO.lcd.print(inputAmount);

  int rctOffset = inputPos > 4 ? -6 : 0;
  GO.lcd.drawRect(110 + rctOffset + (inputPos * 16), 110, 14, 2, WHITE);

  drawMenuButton();
  drawInvoiceButton();
}

// ATM

void lnurlATMMain()
{
  dataIn = "";

  pinToShow = originalPinToShow;

  inputAmount = "0000.00";
  inputPos = 0;

  isATMMoneyPin(true);

  while (unConfirmed)
  {
    GO.update();

    getKeypad(true, false, false, false);

    if (GO.BtnMenu.isPressed())
    {
      unConfirmed = false;
    }

    // if (GO.BtnStart.isPressed())
    // {
    //   isATMMoneyPin(true);
    // }

    // if (pinToShow.length() == lnurlATMPin.length() && pinToShow != lnurlATMPin)

    if (GO.BtnStart.isPressed())
    {
      if (pinToShow != lnurlATMPin)
      {
        error("WRONG PIN", "");
        delay(1500);
        pinToShow = originalPinToShow;
        dataIn = "";
        inputPos = 0;
        isATMMoneyPin(true);
      }
      else if (pinToShow == lnurlATMPin)
      {
        isATMMoneyNumber(true);
        dataIn = "";
        while (unConfirmed)
        {
          GO.update();

          getKeypad(false, false, false, true);

          if (GO.BtnMenu.isPressed())
          {
            unConfirmed = false;
          }
          if (inputAmount != "0000.00" && GO.BtnStart.isPressed())
          {
            makeLNURL();
            // qrShowCodeLNURL(" *MENU");
            qrShowCodeLNURL();

            while (unConfirmed)
            {
              GO.update();

              getKeypad(false, true, false, false);
              if (GO.BtnMenu.isPressed())
              {
                unConfirmed = false;
              }

              delay(50);
            }
          }

          delay(50);
        }
      }
    }

    delay(50);
  }
}

void isATMMoneyPin(bool cleared)
{
  clearContent();

  drawHeaderMessage("Enter secret PIN");
  drawNumberInputHelp();

  GO.lcd.setTextSize(2);

  GO.lcd.setCursor(20, 80);
  GO.lcd.print("PIN:");

  GO.lcd.setCursor(110, 80);
  GO.lcd.print(pinToShow);

  GO.lcd.drawRect(110 + (inputPos * 16), 110, 14, 2, WHITE);

  drawMenuButton();

  GO.lcd.setCursor(260, 210);
  GO.lcd.print("CONFIRM");
  drawButtonTriangle(295, WHITE);

  if (cleared)
  {
    pinToShow = originalPinToShow;
    dataIn = "";
  }
}

void isATMMoneyNumber(bool cleared)
{
  clearContent();

  drawHeaderMessage("Enter amount in " + currencyATM);
  drawNumberInputHelp();

  GO.lcd.setTextSize(2);

  GO.lcd.setCursor(20, 80);
  GO.lcd.print(currencyATM + ":");

  GO.lcd.setCursor(110, 80);
  GO.lcd.print(inputAmount);

  int rctOffset = inputPos > 4 ? -6 : 0;
  GO.lcd.drawRect(110 + rctOffset + (inputPos * 16), 110, 14, 2, WHITE);

  drawMenuButton();
  drawWithdrawButton();

  if (cleared)
  {
    dataIn = "0";
    inputAmount = "0000.00";
  }
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void getKeypad(bool isATMPin, bool justKey, bool isLN, bool isATMNum)
{
  bool keyPressed = false;

  char currentChar = isATMPin ? pinToShow[inputPos] : inputAmount[inputPos];
  char newChar;

  GO.lcd.setCursor(20, 160);

  // D-Pad up or A
  if (GO.JOY_Y.isAxisPressed() == 2 || GO.BtnA.isPressed())
  {
    if (currentChar == '0')
    {
      newChar = '1';
    }
    else if (currentChar == '1')
    {
      newChar = '2';
    }
    else if (currentChar == '2')
    {
      newChar = '3';
    }
    else if (currentChar == '3')
    {
      newChar = '4';
    }
    else if (currentChar == '4')
    {
      newChar = '5';
    }
    else if (currentChar == '5')
    {
      newChar = '6';
    }
    else if (currentChar == '6')
    {
      newChar = '7';
    }
    else if (currentChar == '7')
    {
      newChar = '8';
    }
    else if (currentChar == '8')
    {
      newChar = '9';
    }
    else if (currentChar == '9')
    {
      newChar = '0';
    }

    isATMPin ? pinToShow.setCharAt(inputPos, newChar) : inputAmount.setCharAt(inputPos, newChar);
    keyPressed = true;
  }

  // D-Pad down or B
  if (GO.JOY_Y.isAxisPressed() == 1 || GO.BtnB.isPressed())
  {
    if (currentChar == '0')
    {
      newChar = '9';
    }
    else if (currentChar == '1')
    {
      newChar = '0';
    }
    else if (currentChar == '2')
    {
      newChar = '1';
    }
    else if (currentChar == '3')
    {
      newChar = '2';
    }
    else if (currentChar == '4')
    {
      newChar = '3';
    }
    else if (currentChar == '5')
    {
      newChar = '4';
    }
    else if (currentChar == '6')
    {
      newChar = '5';
    }
    else if (currentChar == '7')
    {
      newChar = '6';
    }
    else if (currentChar == '8')
    {
      newChar = '7';
    }
    else if (currentChar == '9')
    {
      newChar = '8';
    }
    isATMPin ? pinToShow.setCharAt(inputPos, newChar) : inputAmount.setCharAt(inputPos, newChar);
    keyPressed = true;
  }

  // D-Pad left
  if (GO.JOY_X.isAxisPressed() == 2)
  {
    inputPos--;
    if (inputPos < 0)
    {
      inputPos = isATMPin ? pinToShow.length() - 1 : inputAmount.length() - 1;
    }
    if (!isATMPin && inputPos == 4)
    {
      inputPos = 3;
    }
    keyPressed = true;
  }

  // D-Pad right
  if (GO.JOY_X.isAxisPressed() == 1)
  {
    inputPos++;
    int maxPos = isATMPin ? pinToShow.length() : inputAmount.length();
    if (inputPos >= maxPos)
    {
      inputPos = 0;
    }
    if (!isATMPin && inputPos == 4)
    {
      inputPos = 5;
    }
    keyPressed = true;
  }

  if (keyPressed)
  {
    dataIn = isATMPin ? pinToShow : inputAmount;
    if (isLN)
    {
      isLNMoneyNumber(false);
    }
    else if (isATMPin)
    {
      isATMMoneyPin(false);
    }
    else if (justKey)
    {
    }
    else if (isATMNum)
    {
      isATMMoneyNumber(false);
    }
    else
    {
      isLNURLMoneyNumber(false);
    }
  }
}

String getInvoiceMemo()
{
  File file = SPIFFS.open(INVOICE_FILE);
  if (file)
  {
    invoiceNo = file.readString();
    invoiceNo = String(invoiceNo.toInt() + 1);
    file.close();
    file = SPIFFS.open(INVOICE_FILE, FILE_WRITE);
    file.print(invoiceNo);
    file.close();
  }
  else
  {
    file.close();
    file = SPIFFS.open(INVOICE_FILE, FILE_WRITE);
    invoiceNo = "1";
    file.print(invoiceNo);
    file.close();
  }
  String memo = "LNPoS-" + String((uint32_t)ESP.getEfuseMac(), HEX) + "--" + invoiceNo;
  return memo;
}

String getInvoice()
{
  WiFiClientSecure client;
  lnbitsServer.toLowerCase();
  if (lnbitsServer.substring(0, 8) == "https://")
  {
    lnbitsServer = lnbitsServer.substring(8, lnbitsServer.length());
  }
  client.setInsecure(); // Some versions of WiFiClientSecure need this
  const char *lnbitsServerChar = lnbitsServer.c_str();
  const char *invoiceChar = invoice.c_str();

  if (!client.connect(lnbitsServerChar, 443))
  {
    Serial.println("failed");
    error("SERVER DOWN", lnbitsServerChar);
    delay(3000);
    return "SERVER DOWN";
  }

  String memo = getInvoiceMemo();
  String toPost = "{\"out\": false,\"amount\" : " + String(noSats.toInt()) + ", \"memo\" :\"" + memo + "\"}";
  String url = "/api/v1/payments";
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + lnbitsServerChar + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "X-Api-Key: " + invoiceChar + " \r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n" +
               "Content-Length: " + toPost.length() + "\r\n" +
               "\r\n" +
               toPost + "\n");

  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r")
    {
      break;
    }
    if (line == "\r")
    {
      break;
    }
  }
  String line = client.readString();

  StaticJsonDocument<1000> doc;
  DeserializationError error = deserializeJson(doc, line);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return error.f_str();
  }
  const char *payment_hash = doc["checking_id"];
  const char *payment_request = doc["payment_request"];
  qrData = payment_request;
  dataId = payment_hash;
  Serial.println(qrData);

  return memo;
}

bool checkInvoice()
{
  WiFiClientSecure client;
  client.setInsecure(); // Some versions of WiFiClientSecure need this

  const char *lnbitsServerChar = lnbitsServer.c_str();
  const char *invoiceChar = invoice.c_str();
  if (!client.connect(lnbitsServerChar, 443))
  {
    error("SERVER DOWN", lnbitsServerChar);
    delay(3000);
    return false;
  }

  const String url = "/api/v1/payments/";
  client.print(String("GET ") + url + dataId + " HTTP/1.1\r\n" +
               "Host: " + lnbitsServerChar + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n\r\n");
  while (client.connected())
  {
    const String line = client.readStringUntil('\n');
    if (line == "\r")
    {
      break;
    }
  }

  const String line = client.readString();
  Serial.println(line);
  StaticJsonDocument<2000> doc;

  DeserializationError error = deserializeJson(doc, line);
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return false;
  }
  if (doc["paid"])
  {
    unConfirmed = false;
  }

  return unConfirmed;
}

void qrShowCodeln(String memo)
{
  GO.lcd.fillScreen(BLACK);

  qrData.toUpperCase();
  const char *qrDataChar = qrData.c_str();

  GO.lcd.qrcode(qrDataChar, 60, 0, 200, 11);

  drawMenuButton();

  GO.lcd.setCursor(100, 210);
  GO.lcd.print("INVOICE: " + memo);
}

void qrShowCodeLNURL()
{
  GO.lcd.fillScreen(BLACK);

  qrData.toUpperCase();
  const char *qrDataChar = qrData.c_str();

  GO.lcd.qrcode(qrDataChar, 60, 0, 200, 6);

  drawMenuButton();
}

void qrShowCodeOnchain(bool anAddress)
{
  GO.lcd.fillScreen(BLACK);

  int version = anAddress ? 2 : 4;

  if (anAddress)
  {
    qrData.toUpperCase();
  }

  const char *qrDataChar = qrData.c_str();

  GO.lcd.qrcode(qrDataChar, 70, 24, 180, version);
}

void inputScreenOnChain()
{
  clearContent();

  GO.lcd.setTextColor(WHITE);
  GO.lcd.setTextSize(2);
  GO.lcd.setCursor(60, 60);
  GO.lcd.println("XPUB: ***" + masterKey.substring(masterKey.length() - 5));

  drawMenuButton();

  GO.lcd.setCursor(200, 210);
  GO.lcd.print("Generate address");
  drawButtonTriangle(295, WHITE);
}

void drawWiFiStatus()
{
  int color = WHITE;

  if (WiFi.status() != WL_CONNECTED)
  {
    color = RED;
  }

  int x = 268;
  int y = 12;
  int r = 10;

  GO.lcd.fillCircle(x, y, r, color);
  GO.lcd.fillCircle(x, y, r - 2, BLACK);

  GO.lcd.fillCircle(x, y, r - 4, color);
  GO.lcd.fillCircle(x, y, r - 6, BLACK);

  GO.lcd.fillTriangle(x - 12, y - 10, x - 12, y, x - 1, y, BLACK);
  GO.lcd.fillRect(x - 12, y - 2, 24, 20, BLACK);
  GO.lcd.fillTriangle(x + 1, y, x + 12, y - 10, x + 12, y, BLACK);

  GO.lcd.fillCircle(x, y + 1, r - 8, color);
}

void drawBattery()
{
  static esp_adc_cal_characteristics_t adc_chars;
  double voltage;

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  // why 1100? -> (https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/adc.html)
  // Per design the ADC reference voltage is 1100mV, however the true reference voltage can range from
  // 1000mV to 1200mV amongst different ESP32s.

  uint32_t adc_reading = 0;
  for (int i = 0; i < 32; i++)
  {
    adc_reading += adc1_get_raw((adc1_channel_t)ADC1_CHANNEL_0);
    delay(1);
  }

  voltage = esp_adc_cal_raw_to_voltage(adc_reading / 32., &adc_chars) * 2. / 1000.;

  int percent = voltage / 4.2 * 100.;
  if (percent > 100)
    percent = 100;

  int x = 286;
  int y = 3;
  int w = 20;

  GO.lcd.drawRect(x, y, w, 12, WHITE);             // battery body
  GO.lcd.fillRect(x + 1, y + 1, w - 2, 10, BLACK); // fill with black to enable redrawing
  GO.lcd.fillRect(x + w, y + 2, 3, 8, WHITE);      // battery knob

  int fill_c = GREEN;
  if (percent < 50)
    fill_c = ORANGE;
  if (percent < 20)
    fill_c = RED;

  int fill_w = round(((w)*percent) / 100);
  GO.lcd.fillRect(x + 2, y + 2, fill_w - 4, 8, fill_c); // battery fill
}

void drawHeader()
{
  drawWiFiStatus();
  drawBattery();

  GO.lcd.drawLine(0, 20, 320, 20, DARKGREY);
}

void drawHeaderMessage(String message)
{
  GO.lcd.fillRect(20, 0, 200, 19, BLACK);
  GO.lcd.setTextFont(2);
  GO.lcd.setTextColor(WHITE);
  GO.lcd.setTextSize(1);
  GO.lcd.setCursor(20, 1);
  GO.lcd.print(message);
}

void clearContent()
{
  GO.lcd.fillRect(0, 21, 320, 240, BLACK);
}

void makeLNURL()
{
  randomPin = random(1000, 9999);
  byte nonce[8];
  for (int i = 0; i < 8; i++)
  {
    nonce[i] = random(256);
  }

  float amount = dataIn.toFloat() * 100;

  byte payload[51]; // 51 bytes is max one can get with xor-encryption
  if (selection == "LNURLPoS")
  {
    size_t payload_len = xor_encrypt(payload, sizeof(payload), (uint8_t *)secretPoS.c_str(), secretPoS.length(), nonce, sizeof(nonce), randomPin, amount);
    preparedURL = baseURLPoS + "?p=";
    preparedURL += toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);
  }
  else
  {
    size_t payload_len = xor_encrypt(payload, sizeof(payload), (uint8_t *)secretATM.c_str(), secretATM.length(), nonce, sizeof(nonce), randomPin, amount);
    preparedURL = baseURLATM + "?atm=1&p=";
    preparedURL += toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);
  }

  Serial.println(preparedURL);
  char Buf[200];
  preparedURL.toCharArray(Buf, 200);
  char *url = Buf;
  byte *data = (byte *)calloc(strlen(url) * 2, sizeof(byte));
  size_t len = 0;
  int res = convert_bits(data, &len, 5, (byte *)url, strlen(url), 8, 1);
  char *charLnurl = (char *)calloc(strlen(url) * 2, sizeof(byte));
  bech32_encode(charLnurl, "lnurl", data, len);
  to_upper(charLnurl);
  qrData = charLnurl;
  Serial.println(qrData);
}

int xor_encrypt(uint8_t *output, size_t outlen, uint8_t *key, size_t keylen, uint8_t *nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents)
{
  // check we have space for all the data:
  // <variant_byte><len|nonce><len|payload:{pin}{amount}><hmac>
  if (outlen < 2 + nonce_len + 1 + lenVarInt(pin) + 1 + lenVarInt(amount_in_cents) + 8)
  {
    return 0;
  }

  int cur = 0;
  output[cur] = 1; // variant: XOR encryption
  cur++;

  // nonce_len | nonce
  output[cur] = nonce_len;
  cur++;
  memcpy(output + cur, nonce, nonce_len);
  cur += nonce_len;

  // payload, unxored first - <pin><currency byte><amount>
  int payload_len = lenVarInt(pin) + 1 + lenVarInt(amount_in_cents);
  output[cur] = (uint8_t)payload_len;
  cur++;
  uint8_t *payload = output + cur;                                 // pointer to the start of the payload
  cur += writeVarInt(pin, output + cur, outlen - cur);             // pin code
  cur += writeVarInt(amount_in_cents, output + cur, outlen - cur); // amount
  cur++;

  // xor it with round key
  uint8_t hmacresult[32];
  SHA256 h;
  h.beginHMAC(key, keylen);
  h.write((uint8_t *)"Round secret:", 13);
  h.write(nonce, nonce_len);
  h.endHMAC(hmacresult);
  for (int i = 0; i < payload_len; i++)
  {
    payload[i] = payload[i] ^ hmacresult[i];
  }

  // add hmac to authenticate
  h.beginHMAC(key, keylen);
  h.write((uint8_t *)"Data:", 5);
  h.write(output, cur);
  h.endHMAC(hmacresult);
  memcpy(output + cur, hmacresult, 8);
  cur += 8;

  // return number of bytes written to the output
  return cur;
}

void to_upper(char *arr)
{
  for (size_t i = 0; i < strlen(arr); i++)
  {
    if (arr[i] >= 'a' && arr[i] <= 'z')
    {
      arr[i] = arr[i] - 'a' + 'A';
    }
  }
}
