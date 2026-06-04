#ifndef ASYNC_BUZZER_H
#define ASYNC_BUZZER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class AsyncBuzzer {
public:
    AsyncBuzzer(uint8_t ledcChannel = 0, uint8_t resolution = 8);
    ~AsyncBuzzer();

    void begin(uint8_t pin);
    void end();

    void beep(uint16_t freq, uint32_t durationMs);
    void beep(uint16_t freq, uint32_t onMs, uint32_t offMs,
              uint16_t beeps, uint32_t pauseMs, uint8_t cycles);
    void playMelody(const char* melodyStr);
    void stop();
    bool isPlaying();

    void setVolume(uint8_t vol);
    void onComplete(void (*cb)());

private:
    enum CmdType : uint8_t {
        CMD_NONE = 0,
        CMD_BEEP,
        CMD_BEEP_PATTERN,
        CMD_MELODY,
        CMD_STOP,
        CMD_EXIT
    };

    struct QueueItem {
        CmdType type;
        uint16_t freq;
        uint32_t durationMs;
        uint32_t offMs;
        uint32_t pauseMs;
        uint16_t beeps;
        uint8_t cycles;
        char melody[48];
    };

    uint8_t pin;
    uint8_t ledcChannel;
    uint8_t resolution;
    uint8_t volume;
    volatile bool playing;
    void (*onCompleteCb)();

    QueueHandle_t queue;
    TaskHandle_t taskHandle;

    static void taskFunc(void* pv);
    void processItem(const QueueItem& item);
    void playTone(uint16_t freq, uint32_t durationMs);
    bool checkNewCommand();
};

#endif
