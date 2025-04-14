#pragma once
#include <iostream>
#include <functional>

/**
    相机采集
 */

using namespace std;

class camera
{
    using CameraCallback = function<void(void *user, const char *dat, size_t size, int width, int height)>;
public:
    camera();
    ~camera();

    bool init(const char *cam);
    bool deinit(void);
    void setCb(const CameraCallback &cb, void *user){ _cameraCallback = cb; _user = user;};
    bool start(void);
    bool stop(void);
    bool isStop(void){return _isStop;};
    void run(void);
    
private:
    const int WIDTH = 416;
    const int HEIGHT = 480;

private:
    int _fd;
    CameraCallback _cameraCallback;
    void *_user;
    int _isRunning = false;
    bool _isStop = true;
};
