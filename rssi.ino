#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- 硬體腳位設定 ---
#define REED_PIN 5      // 磁簧開關
#define ALARM_LED 2     // 內建 LED (用來代替蜂鳴器顯示警報狀態)

// --- UUID 設定 ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" 
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 傳送 (Notify)
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 接收 (Write)

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastSendTime = 0;

// --- 藍牙連線與斷線回調 ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

// --- 接收手機指令回調 ---
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        // 收到手機確認警報
        if (rxValue == "ALARM_ON") {
           Serial.println(">>> 接收到指令：啟動警報狀態！");
           digitalWrite(ALARM_LED, HIGH); // 亮燈表示警報中
        }
        // 收到手機重置指令 (可選)
        else if (rxValue == "RESET") {
           Serial.println(">>> 接收到指令：解除警報");
           digitalWrite(ALARM_LED, LOW);
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  
  // 初始化腳位
  pinMode(REED_PIN, INPUT_PULLUP); // 沒磁鐵時為 HIGH，有磁鐵為 LOW
  pinMode(ALARM_LED, OUTPUT);
  digitalWrite(ALARM_LED, LOW);

  // 初始化 BLE
  BLEDevice::init("ESP32_Smart_Guard"); 
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                       BLECharacteristic::PROPERTY_WRITE
                     );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);  // <--- 關鍵！必須廣播你的 UUID
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); 
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE 已啟動 (含 UUID 廣播)，等待連線...");
}

void loop() {
  // 檢查連線狀態
  if (deviceConnected) {
    int sensorState = digitalRead(REED_PIN);
    unsigned long currentMillis = millis();

    // 如果磁鐵移開 (HIGH) 且距離上次發送超過 1 秒
    // 我們持續發送 "OPEN" 讓 App 知道現在是不安全的狀態
    if (sensorState == HIGH) {
      if (currentMillis - lastSendTime > 1000) {
        String msg = "OPEN";
        pTxCharacteristic->setValue(msg.c_str());
        pTxCharacteristic->notify();
        Serial.println("磁鐵移開：發送 OPEN 訊號");
        lastSendTime = currentMillis;
      }
    } 
    else {
      // 磁鐵吸住時，關閉 LED
      digitalWrite(ALARM_LED, LOW);
    }
  }

  // 處理斷線重連
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); 
      pServer->startAdvertising(); 
      Serial.println("斷線，重新廣播...");
      oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
      Serial.println("手機已連線");
  }
  delay(20); 
}