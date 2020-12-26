#ifndef RAWR_PA_H
#define RAWR_PA_H

#define RAWR_PA_NAME_MAX 256

typedef enum rawr_pa_rate_e {
    RAWR_AUDIO_RATE_8000 = 8000,
    RAWR_AUDIO_RATE_9600 = 9600,
    RAWR_AUDIO_RATE_11025 = 11025,
    RAWR_AUDIO_RATE_12000 = 12000,
    RAWR_AUDIO_RATE_16000 = 16000,
    RAWR_AUDIO_RATE_22050 = 22050,
    RAWR_AUDIO_RATE_24000 = 24000,
    RAWR_AUDIO_RATE_32000 = 32000,
    RAWR_AUDIO_RATE_44100 = 44100,
    RAWR_AUDIO_RATE_48000 = 48000,
    RAWR_AUDIO_RATE_88200 = 88200,
    RAWR_AUDIO_RATE_96000 = 96000,
    RAWR_AUDIO_RATE_192000 = 192000,
} rawr_pa_rate_t;

int rawr_pa_setup(void);
int rawr_pa_cleanup(void);

int rawr_pa_dev_default_input(void);
int rawr_pa_dev_default_output(void);
const char *rawr_pa_dev_name(int id);
int rawr_pa_dev_rate_supported(int rate);

#endif
