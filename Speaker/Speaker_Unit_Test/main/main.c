#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "string.h"
#include "speaker.h"

void app_main(void) {
    
    // 初始化音頻系統
    initialize_audio_system();

    // 設置播放文件
    set_audio("/spiffs/adf_music.mp3");

    // 播放
    play_audio();

    // 控制音量
    set_volume(-15);
    
    while (1) {

        // 監測播放狀態
        handle_audio_events();

        if(get_audio_state() == AEL_STATE_INIT) {
            stop_audio();
            set_audio("/spiffs/adf_music.mp3");
            play_audio();
        }

    }

    // 停止並銷毀
    terminate_audio();
}
