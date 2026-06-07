#include "AsyncBuzzer.h"

AsyncBuzzer::AsyncBuzzer(uint8_t ledcChannel, uint8_t resolution)
    : pin(0), ledcChannel(ledcChannel), resolution(resolution),
      volume(128), playing(false), onCompleteCb(nullptr),
      queue(nullptr), taskHandle(nullptr) {}

AsyncBuzzer::~AsyncBuzzer() {
    end();
}

void AsyncBuzzer::begin(uint8_t pin) {
    this->pin = pin;

    ledcSetup(ledcChannel, 2000, resolution);
    ledcAttachPin(pin, ledcChannel);
    ledcWrite(ledcChannel, 0);

    queue = xQueueCreate(1, sizeof(QueueItem));
    if (!queue) return;

#if CONFIG_FREERTOS_UNICORE
    xTaskCreate(taskFunc, "buzzer", 2048, this, 1, &taskHandle);
#else
    xTaskCreatePinnedToCore(taskFunc, "buzzer", 2048, this, 1, &taskHandle, 1);
#endif
}

void AsyncBuzzer::end() {
    if (taskHandle) {
        QueueItem item = { CMD_EXIT };
        xQueueOverwrite(queue, &item);
        vTaskDelay(pdMS_TO_TICKS(20));
        taskHandle = nullptr;
    }
    if (queue) {
        vQueueDelete(queue);
        queue = nullptr;
    }
    ledcDetachPin(pin);
    ledcWrite(ledcChannel, 0);
}

void AsyncBuzzer::beep(uint16_t freq, uint32_t durationMs, bool force) {
    if (!queue) return;
    if (!force && (playing || uxQueueMessagesWaiting(queue) > 0)) return;
    QueueItem item = { CMD_BEEP };
    item.freq = freq;
    item.durationMs = durationMs;
    xQueueOverwrite(queue, &item);
}

void AsyncBuzzer::beep(uint16_t freq, uint32_t onMs, uint32_t offMs,
                        uint16_t beeps, uint32_t pauseMs, uint8_t cycles,
                        bool force) {
    if (!queue) return;
    if (!force && (playing || uxQueueMessagesWaiting(queue) > 0)) return;
    QueueItem item = { CMD_BEEP_PATTERN };
    item.freq = freq;
    item.durationMs = onMs;
    item.offMs = offMs;
    item.beeps = beeps;
    item.pauseMs = pauseMs;
    item.cycles = cycles;
    xQueueOverwrite(queue, &item);
}

void AsyncBuzzer::playMelody(const char* melodyStr, bool force) {
    if (!queue || !melodyStr) return;
    if (!force && (playing || uxQueueMessagesWaiting(queue) > 0)) return;
    QueueItem item = { CMD_MELODY };
    strncpy(item.melody, melodyStr, sizeof(item.melody) - 1);
    item.melody[sizeof(item.melody) - 1] = '\0';
    xQueueOverwrite(queue, &item);
}

void AsyncBuzzer::stop() {
    if (!queue) return;
    QueueItem item = { CMD_STOP };
    xQueueOverwrite(queue, &item);
}

bool AsyncBuzzer::isPlaying() {
    return playing;
}

void AsyncBuzzer::setVolume(uint8_t vol) {
    volume = vol;
}

void AsyncBuzzer::onComplete(void (*cb)()) {
    onCompleteCb = cb;
}

void AsyncBuzzer::taskFunc(void* pv) {
    auto* self = static_cast<AsyncBuzzer*>(pv);
    QueueItem item;

    while (true) {
        xQueueReceive(self->queue, &item, portMAX_DELAY);

        switch (item.type) {
            case CMD_EXIT:
                vTaskDelete(nullptr);
                return;

            case CMD_STOP:
                ledcWrite(self->ledcChannel, 0);
                continue;

            default:
                break;
        }

        self->playing = true;
        self->processItem(item);
        self->playing = false;

        if (self->onCompleteCb) {
            self->onCompleteCb();
        }
    }
}

void AsyncBuzzer::processItem(const QueueItem& item) {
    switch (item.type) {
        case CMD_BEEP:
            playTone(item.freq, item.durationMs);
            break;

        case CMD_BEEP_PATTERN:
            for (uint8_t c = 0; c < item.cycles; c++) {
                for (uint16_t b = 0; b < item.beeps; b++) {
                    if (checkNewCommand()) return;
                    playTone(item.freq, item.durationMs);
                    if (checkNewCommand()) return;
                    if (item.offMs > 0) vTaskDelay(pdMS_TO_TICKS(item.offMs));
                }
                if (item.pauseMs > 0 && c < item.cycles - 1) {
                    if (checkNewCommand()) return;
                    vTaskDelay(pdMS_TO_TICKS(item.pauseMs));
                }
            }
            break;

        case CMD_MELODY: {
            int p[6] = {0};
            int n = sscanf(item.melody, "%d %d %d %d %d %d",
                           &p[0], &p[1], &p[2], &p[3], &p[4], &p[5]);
            if (n < 6) break;
            QueueItem pi = { CMD_BEEP_PATTERN };
            pi.freq = (uint16_t)p[0];
            pi.durationMs = (uint32_t)p[1];
            pi.offMs = (uint32_t)p[2];
            pi.beeps = (uint16_t)p[3];
            pi.pauseMs = (uint32_t)p[4];
            pi.cycles = (uint8_t)p[5];
            processItem(pi);
            break;
        }

        default:
            break;
    }
}

bool AsyncBuzzer::checkNewCommand() {
    return uxQueueMessagesWaiting(queue) > 0;
}

void AsyncBuzzer::playTone(uint16_t freq, uint32_t durationMs) {
    if (freq > 0) {
        ledcSetup(ledcChannel, freq, resolution);
        ledcWrite(ledcChannel, volume);
    }
    if (durationMs > 0) {
        vTaskDelay(pdMS_TO_TICKS(durationMs));
    }
    ledcWrite(ledcChannel, 0);
}
