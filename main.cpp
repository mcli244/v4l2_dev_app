#include <iostream>
#include "log.h"

#include "camera.h"

using namespace std;

int cnt = 0;
void cameraCb(void *user, const char *dat, size_t size, int width, int height)
{
    LOG_I("%02d cameraCb size:%d width:%d height:%d", cnt++, size, width, height);
    if(cnt > 10)
    {
        LOG_I("cameraCb exit");
        camera *cam = (camera *)user;
        cam->stop();
    }
}

int main(int argc, char **argv)
{
    Log::get_instance()->init("v4l2_dev_test.log");
    LOG_I("v4l2 dev test demo");

    if(argc < 2)
    {
        LOG_E("usage: %s /dev/videoX", argv[0]);
        return -1;
    }

    camera cam;
    if(false == cam.init(argv[1]))
    {
        LOG_E("camera init failed!");
        return -1;
    }

    cam.setCb(cameraCb, &cam);

    if(false == cam.start())
    {
        LOG_E("camera start failed!");
        return -1;
    }

    LOG_I("camera start success!");
    
    cam.run();
    cam.stop();
    cam.deinit();

    return 0;
}
