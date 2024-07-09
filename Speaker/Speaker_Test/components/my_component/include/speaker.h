#ifndef MY_COMPONENT_H
#define MY_COMPONENT_H

#include "audio_pipeline.h"
#include "esp_peripherals.h"

// Initialize audio system
void initialize_audio_system();

// set audio file
void set_audio(const char *file_path, int pipeline_index);

// play audio (run the pipeline)
void play_audio(int pipeline_index);

// Read audio playing status (waiting time for 100ms)
// Modify the audio info automatically
// When the audio finish playing, stop the pipeline and reset
void handle_audio_events();

// Get audio state from i2s_stream_writer
// Usually got : AEL_STATE_INIT, AEL_STATE_RUNNING, AEL_STATE_FINISHED, AEL_STATE_PAUSED, AEL_STATE_STOPPED
int get_audio_state(int pipeline_index);

// Pause the audio
void pause_audio(int pipeline_index);

// Resume the audio
void resume_audio(int pipeline_index);

// Stop the audio
void stop_audio(int pipeline_index);

// Terminate the audio system
void terminate_audio();

// set audio volume, default number is 0
void set_volume(int volume);

#endif // MY_COMPONENT_H