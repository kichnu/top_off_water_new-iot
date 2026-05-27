#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>
#include <DFRobot_DF1201S.h>

// ============================================================
// AudioPlayer — obsługa DFPlayer Pro (DFR0768) przez UART1.
//
// Pliki audio na flash DFPlayera (wgraj w tej kolejności przez USB):
//   001.mp3 — ERROR_DAILY_LIMIT     — dzienny limit wody osiągnięty
//   002.mp3 — ERROR_RED_ALERT       — anomalia szybkości dolewania
//   003.mp3 — ERROR_BOTH            — limit dobowy i anomalia
//   004.mp3 — ERROR_LOW_RESERVOIR   — krytycznie niski poziom wody
//   005.mp3 — WARN_LOW_RESERVOIR    — niski poziom wody (ostrzeżenie)
//   006.mp3 — PROVISIONING          — odliczanie + "system gotowy do konfiguracji"
//
// Błędy (STATE_ERROR): odtwarzane w pętli z przerwą AUDIO_REPEAT_INTERVAL_MS.
// Ostrzeżenia (isLowReservoirWarning): odtwarzane z przerwą, system działa.
// Priorytety: błąd > ostrzeżenie.
// ============================================================

#define AUDIO_VOLUME               15    // Głośność 0–30

// Przerwy między powtórzeniami per komunikat [ms]
// Pierwsze odtworzenie zawsze natychmiastowe (przy zmianie alarmu/warning).
#define AUDIO_REPEAT_DAILY_LIMIT_MS        60000   // 001 — dzienny limit osiągnięty
#define AUDIO_REPEAT_RED_ALERT_MS          60000   // 002 — anomalia szybkości
#define AUDIO_REPEAT_BOTH_ERRORS_MS        60000   // 003 — limit + anomalia
#define AUDIO_REPEAT_LOW_RESERVOIR_CRIT_MS 60000   // 004 — krytycznie niski poziom
#define AUDIO_REPEAT_LOW_RESERVOIR_WARN_MS 120000  // 005 — ostrzeżenie niski poziom

#define AUDIO_TRACK_WARN_LOW_RESERVOIR  5   // numer pliku dla ostrzeżenia
#define AUDIO_TRACK_PROVISIONING        6   // numer pliku dla trybu provisioning

class AudioPlayer {
public:
    void init();    // Wywołaj z setup() po initPeristalticPump()
    void update();  // Wywołaj z loop() co ~100 ms
    void stop();    // Zatrzymaj odtwarzanie

    // Wyciszenie alarmu głosowego — zatrzymuje bieżące odtwarzanie i blokuje nowe.
    // Mute reset automatycznie gdy wszystkie alarmy ustąpią (desired==0),
    // dzięki czemu kolejny alarm zawsze odtworzy się natychmiastowo.
    void setMuted(bool mute);
    bool isMuted() const { return muted; }

    bool    isInitialized() const { return initialized; }
    uint8_t getVolume()     const { return volume; }
    void    setVolume(uint8_t vol);

private:
    DFRobot_DF1201S df;
    uint32_t lastPlayMs;
    uint8_t  currentTrack;   // 0 = nic nie gra
    bool     initialized;
    bool     muted;          // true = alarm wyciszony przez użytkownika
    uint8_t  volume;         // głośność alarmu 3–30 (persystowana w FRAM)

    void playTrack(uint8_t track);
    uint32_t repeatIntervalMs(uint8_t track) const;
};

extern AudioPlayer audioPlayer;

#endif // AUDIO_PLAYER_H
