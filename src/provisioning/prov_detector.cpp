#include "prov_detector.h"
#include "prov_config.h"
#include "../core/logging.h"
#include "../hardware/hardware_pins.h"
#include "../hardware/audio_player.h"
#include <DFRobot_DF1201S.h>

bool checkProvisioningButton() {
    pinMode(PROV_BUTTON_PIN, INPUT_PULLUP);

    LOG_INFO("Checking provisioning button on GPIO%d...", PROV_BUTTON_PIN);

    delay(PROV_BUTTON_DEBOUNCE_MS);

    if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
        LOG_INFO("Provisioning button not pressed - normal boot");
        return false;
    }

    LOG_INFO("Provisioning button pressed - checking hold time (%dms)...", PROV_BUTTON_HOLD_MS);

    // pressStart starts immediately so total hold time = PROV_BUTTON_HOLD_MS from button detection
    unsigned long pressStart = millis();

    // Init DFPlayer Pro and play provisioning track (starts ~0.5-1s after pressStart)
    DFRobot_DF1201S df;
    Serial1.begin(115200, SERIAL_8N1, -1, DFPLAYER_TX_PIN);  // TX-only, brak RX (GPIO20 conflict)
    if (df.begin(Serial1)) {
        df.setVol(AUDIO_VOLUME);
        df.switchFunction(df.MUSIC);
        df.setPlayMode(df.SINGLE);
        df.playFileNum(PROV_AUDIO_TRACK);
        LOG_INFO("AudioPlayer: provisioning track %d playing", PROV_AUDIO_TRACK);
    } else {
        LOG_WARNING("AudioPlayer: DFPlayer not found - continuing without audio");
    }

    while (millis() - pressStart < PROV_BUTTON_HOLD_MS) {
        if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
            unsigned long heldTime = millis() - pressStart;
            df.pause();
            LOG_INFO("Button released after %lums (required: %dms)", heldTime, PROV_BUTTON_HOLD_MS);
            return false;
        }
        delay(5);
    }

    // Audio continues playing ("system gotowy do konfiguracji") while provisioning mode inits
    LOG_INFO("Provisioning button held for %dms - entering provisioning mode", PROV_BUTTON_HOLD_MS);
    return true;
}
