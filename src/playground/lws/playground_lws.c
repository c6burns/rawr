#include <libwebsockets.h>

#include <string.h>
#include <signal.h>

int main(int argc, const char **argv)
{
    // this will definitely not work, but it will force the linker
    struct lws_context_creation_info info;
    struct lws_context *context;
    context = lws_create_context(&info);
    return 0;
}
