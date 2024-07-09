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
    set_volume(-25);

    int time = xTaskGetTickCount();
    int index = 0;
    
    while (1) {
        // 監測播放狀態
        handle_audio_events();

        if(xTaskGetTickCount() - time > 15000 / portTICK_PERIOD_MS) {

            index += 1;
            printf("case : %d\n", index+1);
            
            switch(index%=3)
            {
                case 0:
                stop_audio();
                set_audio("/spiffs/adf_music.mp3");
                play_audio();
                break;
                case 1:
                stop_audio();
                set_audio("/spiffs/Recreation.mp3");
                play_audio();
                break;
                case 2:
                stop_audio();
                set_audio("/spiffs/song_pixel.mp3");
                play_audio();
                break;
            }
            
            time = xTaskGetTickCount();
        }
    }

    // 停止並銷毀
    terminate_audio();
}
