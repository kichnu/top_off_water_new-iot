#include "audio_player.h"
#include "hardware_pins.h"
#include "fram_controller.h"
#include "../algorithm/water_algorithm.h"
#include "../core/logging.h"

AudioPlayer audioPlayer;

void AudioPlayer::setVolume(uint8_t vol) {
    vol = (vol < AUDIO_VOLUME_MIN) ? AUDIO_VOLUME_MIN
        : (vol > AUDIO_VOLUME_MAX) ? AUDIO_VOLUME_MAX : vol;
    volume = vol;
    if (initialized) df.setVol(volume);
    saveAudioConfigToFRAM(volume);
}

void AudioPlayer::init() {
    initialized  = false;
    currentTrack = 0;
    lastPlayMs   = 0;
    muted        = false;
    volume       = AUDIO_VOLUME_DEFAULT;

    // Wczytaj zapisaną głośność z FRAM (jeśli dostępna)
    uint8_t savedVol;
    if (loadAudioConfigFromFRAM(savedVol)) {
        volume = savedVol;
        LOG_INFO("AudioPlayer: volume loaded from FRAM: %d", volume);
    }

    // TX-only: GPIO20 (UART0 RX) blokuje UART1 RX przez periman — df.begin() timeout.
    // Komendy wysyłamy przez TX (GPIO8), odpowiedzi nie odbieramy.
    Serial1.begin(115200, SERIAL_8N1, -1, DFPLAYER_TX_PIN);

    // begin() zwróci false (brak ACK z RX), ale stream jest ustawiony — komendy działają
    df.begin(Serial1);
    initialized = true;

    df.setVol(volume);
    df.switchFunction(df.MUSIC);
    delay(2000);                  // odczekaj na dźwięk startowy
    df.setPlayMode(df.SINGLE);
    df.setPrompt(false);
    df.setLED(false);

    LOG_INFO("AudioPlayer: OK TX-only (vol=%d, GPIO TX=%d)", AUDIO_VOLUME, DFPLAYER_TX_PIN);
}

void AudioPlayer::playTrack(uint8_t track) {
    if (!initialized || track == 0) return;
    df.playFileNum(track);
    currentTrack = track;
    lastPlayMs   = millis();
    LOG_INFO("AudioPlayer: track %d", track);
}

void AudioPlayer::update() {
    if (!initialized) return;

    // Wyznacz żądany utwór — błąd ma priorytet nad ostrzeżeniem
    uint8_t desired = 0;
    if (waterAlgorithm.getState() == STATE_ERROR) {
        desired = (uint8_t)waterAlgorithm.getLastError();   // kody 1–4 = pliki 1–4
    } else if (waterAlgorithm.isLowReservoirWarning()) {
        desired = AUDIO_TRACK_WARN_LOW_RESERVOIR;           // plik 5
    }

    // Brak alarmu: zatrzymaj, wyczyść mute (reset dla następnego alarmu)
    if (desired == 0) {
        if (currentTrack != 0) {
            df.pause();
            currentTrack = 0;
        }
        muted = false;
        return;
    }

    // Alarm wyciszony: zatrzymaj jeśli coś gra i nie startuj
    if (muted) {
        if (currentTrack != 0) {
            df.pause();
            currentTrack = 0;
        }
        return;
    }

    // Zmiana alarmu → natychmiastowe odtworzenie nowego
    if (desired != currentTrack) {
        playTrack(desired);
        return;
    }

    // TX-only: bez isPlaying() — powtarzaj po upływie repeat interval
    if (millis() - lastPlayMs >= repeatIntervalMs(currentTrack)) {
        playTrack(currentTrack);
    }
}

void AudioPlayer::setMuted(bool mute) {
    muted = mute;
    LOG_INFO("AudioPlayer: alarm %s", mute ? "MUTED" : "UNMUTED");
}

uint32_t AudioPlayer::repeatIntervalMs(uint8_t track) const {
    switch (track) {
        case 1: return AUDIO_REPEAT_DAILY_LIMIT_MS;
        case 2: return AUDIO_REPEAT_RED_ALERT_MS;
        case 3: return AUDIO_REPEAT_BOTH_ERRORS_MS;
        case 4: return AUDIO_REPEAT_LOW_RESERVOIR_CRIT_MS;
        case 5: return AUDIO_REPEAT_LOW_RESERVOIR_WARN_MS;
        default: return 60000;
    }
}

void AudioPlayer::stop() {
    if (!initialized) return;
    df.pause();
    currentTrack = 0;
    LOG_INFO("AudioPlayer: stopped");
}
