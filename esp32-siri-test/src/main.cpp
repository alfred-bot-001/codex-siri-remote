#include <Arduino.h>
#include <NimBLEDevice.h>
#include <atomic>
#include <map>

// Diagnostic USB serial transport; audio capture is explicitly armed and bounded.
static const NimBLEAdvertisedDevice* candidate = nullptr;
static std::atomic<bool> pending{false};
static std::atomic<unsigned> reports{0}, buttons{0};
static NimBLEClient* client = nullptr;
static std::map<uint16_t, uint8_t> reportIds;
static std::string lockedIdentity;
static uint32_t heartbeat = 0;
static NimBLEAddress targetAddress;
static bool haveTarget = false;
static std::atomic<bool> retry{false};
static unsigned retryCount = 0;
static uint32_t retryAt = 0;
struct AudioPacket { uint8_t length; uint8_t data[128]; };
static QueueHandle_t audioQueue;
static std::atomic<uint32_t> audioUntil{0};
static std::atomic<unsigned> audioPackets{0}, audioDropped{0};

class ConnectionEvents : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient*) override { Serial.println("LINK_CONNECTED"); }
    void onDisconnect(NimBLEClient*, int reason) override {
        Serial.printf("DISCONNECTED reason=%d; send s to scan same target again\n", reason);
        retry = true;
        audioUntil = 0;
    }
    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        Serial.printf("AUTH encrypted=%d bonded=%d authenticated=%d\n",
                      info.isEncrypted(), info.isBonded(), info.isAuthenticated());
    }
} connectionEvents;

class ScanEvents : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (pending || device->getRSSI() < -60 ||
            !device->isAdvertisingService(NimBLEUUID(uint16_t(0x1812)))) return;
        std::string m = device->getManufacturerData();
        // Apple company ID, HID prefix, and the identity embedded in pairing advertisements.
        if (m.size() < 14 || uint8_t(m[0]) != 0x4c || uint8_t(m[1]) != 0 ||
            uint8_t(m[2]) != 7 || uint8_t(m[3]) != 13) return;
        char identity[18];
        snprintf(identity, sizeof(identity), "%02X:%02X:%02X:%02X:%02X:%02X",
                 uint8_t(m[8]), uint8_t(m[9]), uint8_t(m[10]),
                 uint8_t(m[11]), uint8_t(m[12]), uint8_t(m[13]));
        if (!lockedIdentity.empty() && lockedIdentity != identity) return;
        lockedIdentity = identity;
        Serial.printf("SIRI_FOUND address=%s identity=%s rssi=%d name=%s\n",
                      device->getAddress().toString().c_str(), identity,
                      device->getRSSI(), device->getName().c_str());
        NimBLEDevice::getScan()->stop();
        candidate = device;
        pending = true;
    }
    void onScanEnd(const NimBLEScanResults&, int reason) override {
        Serial.printf("SCAN_ENDED reason=%d\n", reason);
    }
} scanEvents;

static void notification(NimBLERemoteCharacteristic* characteristic, uint8_t* data,
                         size_t size, bool) {
    ++reports;
    auto it = reportIds.find(characteristic->getHandle());
    uint8_t id = it == reportIds.end() ? 0 : it->second;
    if (id == 0xfb && size >= 2) {
        ++buttons;
        Serial.printf("BUTTON mask=0x%04X length=%u\n",
                      unsigned(data[0] | (data[1] << 8)), unsigned(size));
    } else if (id == 0xfa && audioUntil && int32_t(audioUntil.load() - millis()) > 0) {
        if (size > 128) { ++audioDropped; return; }
        AudioPacket packet;
        packet.length = size;
        memcpy(packet.data, data, size);
        if (xQueueSend(audioQueue, &packet, 0) == pdTRUE) ++audioPackets;
        else ++audioDropped;
    }
}

static bool inspectRemote(bool reconnect = false) {
    if (!reconnect) {
        targetAddress = candidate->getAddress();
        haveTarget = true;
        retryCount = 0;
    }
    if (client) { NimBLEDevice::deleteClient(client); client = nullptr; }
    reportIds.clear();
    client = NimBLEDevice::createClient();
    client->setClientCallbacks(&connectionEvents, false);
    client->setConnectTimeout(10000);
    // Voice produces a frame every 20 ms; use a 15 ms BLE connection interval.
    client->setConnectionParams(12, 12, 0, 200);
    if (!client->connect(targetAddress)) {
        Serial.printf("CONNECT_FAILED error=%d\n", client->getLastError());
        return false;
    }
    bool secure = client->secureConnection();
    Serial.printf("SECURE_CONNECTION result=%d error=%d\n", secure, client->getLastError());
    if (!secure) { client->disconnect(); return false; }
    Serial.printf("BLE_PARAMETERS interval_units=%u latency=%u mtu=%u\n",
                  client->getConnInfo().getConnInterval(), client->getConnInfo().getConnLatency(),
                  client->getMTU());
    auto* hid = client->getService(NimBLEUUID(uint16_t(0x1812)));
    if (!hid) { Serial.println("HID_SERVICE_MISSING"); client->disconnect(); return false; }
    unsigned subscribed = 0;
    // Enumerate by handle: multiple Report characteristics share UUID 0x2A4D.
    const auto& chars = hid->getCharacteristics(true);
    for (auto* ch : chars) {
        if (ch->getUUID() != NimBLEUUID(uint16_t(0x2a4d))) continue;
        auto* ref = ch->getDescriptor(NimBLEUUID(uint16_t(0x2908)));
        if (!ref) continue;
        auto value = ref->readValue();
        if (value.size() < 2) continue;
        uint8_t id = value[0], type = value[1];
        reportIds[ch->getHandle()] = id;
        Serial.printf("HID_REPORT handle=%u id=0x%02X type=%u\n", ch->getHandle(), id, type);
    }
    // Audio bytes are forwarded only during a bounded, explicitly armed test.
    for (auto* ch : chars) {
        auto it = reportIds.find(ch->getHandle());
        if (it == reportIds.end() || (it->second != 0xfb && it->second != 0xfa) || !ch->canNotify()) continue;
        bool ok = ch->subscribe(true, notification);
        Serial.printf("REPORT_SUBSCRIBE id=0x%02X result=%d\n", it->second, ok);
        if (ok) ++subscribed;
    }
    // A2854 requires the input-enable byte on its writable feature reports.
    for (auto* ch : chars) {
        if (ch->getUUID() != NimBLEUUID(uint16_t(0x2a4d))) continue;
        auto* ref = ch->getDescriptor(NimBLEUUID(uint16_t(0x2908)));
        if (!ref) continue;
        auto value = ref->readValue();
        if (value.size() < 2 || (value[1] != 2 && value[1] != 3)) continue;
        if (!ch->canWrite() && !ch->canWriteNoResponse()) continue;
        uint8_t enable = 0xaf;
        bool ok = ch->writeValue(&enable, 1, !ch->canWriteNoResponse());
        Serial.printf("INPUT_ENABLE id=0x%02X result=%d\n", value[0], ok);
    }
    Serial.printf("READY_FOR_BUTTON_TEST subscriptions=%u bonds=%d\n",
                  subscribed, NimBLEDevice::getNumBonds());
    return subscribed > 0;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.printf("SIRI_BLE_TEST build=4 psram=%u\n", ESP.getPsramSize());
    audioQueue = xQueueCreate(64, sizeof(AudioPacket));
    if (!audioQueue) { Serial.println("FATAL_AUDIO_QUEUE"); while (true) delay(1000); }
    NimBLEDevice::init("Siri-ESP32-Test");
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setMTU(185);
    auto* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanEvents, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(90);
    for (int i = 0; i < NimBLEDevice::getNumBonds(); ++i) {
        auto address = NimBLEDevice::getBondedAddress(i);
        // This diagnostic run has verified this exact physical remote.
        if (address.toString() == "28:2d:7f:3f:50:6e") {
            targetAddress = address;
            lockedIdentity = "28:2D:7F:3F:50:6E";
            haveTarget = true;
            retry = true;
            Serial.println("RESTORING_VERIFIED_REMOTE_BOND");
        }
    }
    Serial.println("IDLE: send s for 120-second Siri-only scan; d to disconnect");
}

void loop() {
    if (Serial.available()) {
        char command = Serial.read();
        if (command == 's' && !pending && (!client || !client->isConnected())) {
            Serial.println("SCANNING_SIRI_PAIRING_MODE 120s");
            NimBLEDevice::getScan()->start(120000, false, true);
        } else if (command == 'd') {
            NimBLEDevice::getScan()->stop();
            if (client && client->isConnected()) client->disconnect();
            haveTarget = false;
            retry = false;
            audioUntil = 0;
        } else if (command == 'm' && client && client->isConnected()) {
            xQueueReset(audioQueue);
            audioPackets = 0;
            audioDropped = 0;
            audioUntil = millis() + 60000;
            Serial.println("MIC_ARMED maximum=60_seconds");
        } else if (command == 'x') {
            audioUntil = 0;
            xQueueReset(audioQueue);
            Serial.printf("MIC_STOPPED packets=%u dropped=%u\n", audioPackets.load(), audioDropped.load());
        }
    }
    if (pending.exchange(false)) inspectRemote();
    if (audioUntil && int32_t(millis() - audioUntil.load()) >= 0) {
        audioUntil = 0;
        Serial.printf("MIC_TIMEOUT packets=%u dropped=%u\n", audioPackets.load(), audioDropped.load());
    }
    AudioPacket packet;
    while (xQueueReceive(audioQueue, &packet, 0) == pdTRUE) {
        char line[270];
        unsigned offset = snprintf(line, sizeof(line), "AUDIO ");
        for (unsigned i = 0; i < packet.length; ++i)
            offset += snprintf(line + offset, sizeof(line) - offset, "%02x", packet.data[i]);
        line[offset++] = '\n';
        Serial.write(reinterpret_cast<uint8_t*>(line), offset);
    }
    if (retry.exchange(false) && haveTarget) retryAt = millis() + 3000;
    if (haveTarget && retryAt && int32_t(millis() - retryAt) >= 0 &&
        (!client || !client->isConnected()) && retryCount < 5) {
        retryAt = 0;
        ++retryCount;
        NimBLEDevice::getScan()->stop();
        Serial.printf("RECONNECT_SAME_REMOTE attempt=%u\n", retryCount);
        if (!inspectRemote(true)) retryAt = millis() + 3000;
    }
    if (millis() - heartbeat >= 10000) {
        heartbeat = millis();
        Serial.printf("STATUS connected=%d notifications=%u buttons=%u free_heap=%u\n",
                      client && client->isConnected(), reports.load(), buttons.load(), ESP.getFreeHeap());
    }
    delay(10);
}
