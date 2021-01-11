#include "rawr/net.h"
#include "rawr/error.h"

#include "re.h"

int rawr_Net_Setup()
{
    return libre_init();
}

void rawr_Net_Cleanup()
{
    libre_close();
}
