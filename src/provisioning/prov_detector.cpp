#include "prov_detector.h"
#include "prov_config.h"
#include "../core/logging.h"
#include "../hardware/hardware_pins.h"

bool checkProvisioningButton() {
    pinMode(PROV_BUTTON_PIN, INPUT_PULLUP);

    LOG_INFO("Checking provisioning button on GPIO%d...", PROV_BUTTON_PIN);

    delay(PROV_BUTTON_DEBOUNCE_MS);

    if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
        LOG_INFO("Provisioning button not pressed - normal boot");
        return false;
    }

    LOG_INFO("Provisioning button pressed - checking hold time (%dms)...", PROV_BUTTON_HOLD_MS);

    unsigned long pressStart = millis();

    while (millis() - pressStart < PROV_BUTTON_HOLD_MS) {
        if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
            unsigned long heldTime = millis() - pressStart;
            LOG_INFO("Button released after %lums (required: %dms)", heldTime, PROV_BUTTON_HOLD_MS);
            return false;
        }
        delay(5);
    }

    LOG_INFO("Provisioning button held for %dms - entering provisioning mode", PROV_BUTTON_HOLD_MS);
    return true;
}
