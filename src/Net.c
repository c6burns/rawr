#include "rawr/Net.h"
#include "rawr/Error.h"
#include <stdint.h>
#include "re.h"

int rawr_Net_Setup()
{
    return libre_init();
}

void rawr_Net_Cleanup()
{
    libre_close();
}
