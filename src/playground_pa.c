#include "rawr/pa.h"

#include "mn/log.h"

int main(void)
{
    mn_log_trace("testing portaudio ...");

    rawr_pa_setup();

    int default_input = rawr_pa_dev_default_input();
    mn_log_info("default input: %s (%d)", rawr_pa_dev_name(default_input), default_input);

    int default_output = rawr_pa_dev_default_output();
    mn_log_info("default output: %s (%d)", rawr_pa_dev_name(default_output), default_output);

    rawr_pa_cleanup();

    return 0;
}
