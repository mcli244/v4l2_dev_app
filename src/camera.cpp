#include "camera.h"
#include "log.h"
#include <thread>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

#define BUFFER_COUNT 4


struct MyVideoBuffer
{
    void *data;
    size_t frame_size;
    int len;
};

static enum v4l2_buf_type capture_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
static struct MyVideoBuffer mVideoBuffer[BUFFER_COUNT];
static struct v4l2_capability cap;
static struct v4l2_cropcap cropcap;
static struct v4l2_crop crop;

static struct v4l2_frmivalenum fival;
static struct v4l2_format g_v4l2_fmt;

static struct v4l2_format v4l2_fmt;

camera::camera()
{

}

camera::~camera()
{

}

static int xioctl(int fd, int request, void *arg) {
  int r;

  do {
    r = ioctl(fd, request, arg);
  } while (-1 == r && EINTR == errno);

  return r;
}

bool _isCapture(int fd)
{
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
        LOG_E("Get video capability error! [%s]", strerror(errno));
        return false;
    }

    LOG_D("device_caps:0x%x.", cap.device_caps);
    if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)){
        LOG_E("Video device not support capture! [%s]", strerror(errno));
        return false;
    }
    LOG_D("Support capture!");

    return true;
}

void _showSupportFmt(int fd)
{
    // 获取驱动支持的格式
    static struct v4l2_fmtdesc fmtdesc;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    LOG_I("============ VIDIOC_ENUM_FMT ==================");
    for (int i = 0; ; i++)
    {
        fmtdesc.index = i;
        if (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
        {
            LOG_I("%02d 0x%x \t %s", i, fmtdesc.pixelformat, fmtdesc.description);
        }
        else
            break;
    }	
}

void _showCurFmt(int fd, enum v4l2_buf_type _capture_buf_type)
{
    struct v4l2_format _v4l2_fmt;
    int ret;

    memset(&_v4l2_fmt, 0, sizeof(_v4l2_fmt));
    _v4l2_fmt.type = _capture_buf_type; 
    ret = xioctl(fd, VIDIOC_G_FMT, &_v4l2_fmt);
    if (ret != 0)
    {
        LOG_E("VIDIOC_G_FMT ret:%d [%s]", ret, strerror(errno));
        return;
    }
    LOG_I("============ VIDIOC_G_FMT ==================");
    LOG_I("width:%d height:%d pixelformat:0x%x type:0x%x", 
        _v4l2_fmt.fmt.pix.width, _v4l2_fmt.fmt.pix.height, 
        _v4l2_fmt.fmt.pix.pixelformat, _v4l2_fmt.type);
}

void _showFrameSize(int fd, int pixel_format)
{
    static struct v4l2_frmsizeenum frmsize;
    LOG_I("============ VIDIOC_ENUM_FRAMESIZES ==================");
    memset(&frmsize, 0, sizeof(frmsize));
	frmsize.pixel_format = pixel_format;
	frmsize.index = 0;
    for (int i = 0; ; i++)
    {
        frmsize.index = i;
        if (0 == xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize))
        {
            LOG_I("%02d 0x%x \t %d x %d", i, frmsize.pixel_format, frmsize.discrete.width, frmsize.discrete.height);
            // 枚举与此分辨率对应的帧间隔（帧率）
            struct v4l2_frmivalenum frmival;
            frmival.index = 0;
            frmival.pixel_format = frmsize.pixel_format;
            frmival.width = frmsize.discrete.width;
            frmival.height = frmsize.discrete.height;

            while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                if (frmival.discrete.numerator != 0 && frmival.discrete.denominator != 0) {
                    LOG_I("  Frame Interval: %u/%u, FPS: %.2f",
                        frmival.discrete.numerator, frmival.discrete.denominator,
                        (float)frmival.discrete.denominator / frmival.discrete.numerator);
                }
                // 如果是步进式帧间隔（stepwise）
                else if (frmival.stepwise.min.numerator != 0 && frmival.stepwise.min.denominator != 0) {
                    LOG_I("  Stepwise Frame Interval:");
                    LOG_I("    Min: %u/%u, Max: %u/%u, Step: %u/%u",
                        frmival.stepwise.min.numerator, frmival.stepwise.min.denominator,
                        frmival.stepwise.max.numerator, frmival.stepwise.max.denominator,
                        frmival.stepwise.step.numerator, frmival.stepwise.step.denominator);
                }
                frmival.index ++;
            }
        }
        else
            break;
    }	
}

bool _setFrameRate(int fd, int fps) {
    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));

    // 获取当前流参数
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_PARM, &streamparm) < 0) {
        perror("VIDIOC_G_PARM");
        return false;
    }

    // 设置帧率
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = fps;

    if (ioctl(fd, VIDIOC_S_PARM, &streamparm) < 0) {
        perror("VIDIOC_S_PARM");
        return false;
    }

    LOG_I("Frame rate set to %d fps", fps);
    return 0;
}

int _getBrightness(int fd)
{
    struct v4l2_control control;
    // 获取当前亮度值
    control.id = V4L2_CID_BRIGHTNESS;
    if (ioctl(fd, VIDIOC_G_CTRL, &control) == 0) {
        printf("当前亮度: %d\n", control.value);
    } else {
        perror("获取亮度失败");
    }
    return control.value;
}

int _setBrightness(int fd, int brightness)
{
    struct v4l2_control control;
    // 设置亮度值
    control.id = V4L2_CID_BRIGHTNESS;
    control.value = brightness;
    if (ioctl(fd, VIDIOC_S_CTRL, &control) == 0) {
        printf("亮度设置成功\n");
    } else {
        perror("设置亮度失败");
    }
    return control.value;
}


bool requestBuffer(int _fd, MyVideoBuffer *mVideoBuffer, int size)
{
    // 申请缓存，用于存储相机输出的图像
    int ret;

    struct v4l2_requestbuffers req;
    req.count = size;                       // 缓存数量
    req.type = capture_buf_type;
    req.memory = V4L2_MEMORY_MMAP;          // 内存映射方式，由驱动层申请内存，应用层用指针映射过去即可
    ret = xioctl(_fd, VIDIOC_REQBUFS, &req);
    if (ret < 0)
    {
        LOG_E("VIDIOC_REQBUFS failed ret:%d [%s]", ret, strerror(errno));
        return false;
    }
    LOG_D("============ VIDIOC_REQBUFS ==================");
    LOG_D("count:%d type:0x%08X memory:0x%08X", req.count, req.type, req.memory);
    
    for(int i=0; i<size; i++)
    {
        // 定义v4l2_buffer结构，每个buffer需要初始化为什么格式？在buffer结构的成员指定好
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(buffer));
        buffer.index = i; // 初始化第0~3个buffer中的第i个
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.type = capture_buf_type; //支持的设备视频输入类型

        if (-1 == xioctl(_fd, VIDIOC_QUERYBUF, &buffer))  // 从驱动中取出VIDIOC_REQBUFS时申请的内存缓存信息 "buffer.m.offset"
		{
            LOG_E("get %d VIDIOC_QUERYBUF error [%s]", i, strerror(errno));
			goto _exit;
        }

        // 映射buffer的驱动空间地址到应用层的指针并且保存指针。
        mVideoBuffer[i].len = buffer.length;
        mVideoBuffer[i].frame_size = v4l2_fmt.fmt.pix.width * v4l2_fmt.fmt.pix.height * 1;  // 8bit灰度数据
        mVideoBuffer[i].data= mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, buffer.m.offset);
        LOG_D("mVideoBuffer[i].len:%d mVideoBuffer[i].frame_size:%d", mVideoBuffer[i].len, mVideoBuffer[i].frame_size);

        if (-1 == xioctl(_fd, VIDIOC_QBUF, &buffer)) // 将这个信息放回驱动
		{
            LOG_E("set %d VIDIOC_QUERYBUF to line error [%s]", i, strerror(errno));
			goto _exit;
        }
    }

    LOG_D("============ VIDIOC_QUERYBUF/VIDIOC_QUERYBUF ==================");
    for(int i=0; i<size; i++){
        LOG_D("buffer:%p len:%d", mVideoBuffer[i].data, mVideoBuffer[i].len);
    }

    return true;

_exit:
_free:
    for(int i=0; i<size; i++) 
        munmap(mVideoBuffer[i].data, mVideoBuffer[i].len);
    return false;
}

bool freeBuffer(MyVideoBuffer *mVideoBuffer, int size)
{
    for(int i=0; i<size; i++) 
        munmap(mVideoBuffer[i].data, mVideoBuffer[i].len);
    return true;
}


bool camera::init(const char *cam)
{
    int ret;

    _fd = open(cam, O_RDWR);
    if (_fd < 0){
        LOG_E("open video device:[%s] fail [%s]", cam, strerror(errno));
        return false;
    }

    if(!_isCapture(_fd)){
        close(_fd);
        return false;
    }

    _showSupportFmt(_fd);   // 获取该设备支持的所有格式
    _showCurFmt(_fd, capture_buf_type);     // 获取当前使用的格式
    _showFrameSize(_fd, V4L2_PIX_FMT_RGB24);    // 获取支持的帧分辨率

    // _getBrightness(_fd);
    // _setBrightness(_fd, 128);
    // _getBrightness(_fd);

    // 从支持的格式中选择一个格式设置到驱动
    v4l2_fmt.type = capture_buf_type;
    // 重要！！ 分辨率不能随意设置，需要从驱动支持的分辨率下取一个
    v4l2_fmt.fmt.pix.width = WIDTH;                     // 宽度
    v4l2_fmt.fmt.pix.height = HEIGHT;                    // 高度
    v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // 像素格式
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;
    ret = xioctl(_fd, VIDIOC_S_FMT, &v4l2_fmt);
    if (0 != ret)
    {
        LOG_E("set VIDIOC_S_FMT failed ret:%d [%s]", ret, strerror(errno));
        close(_fd);
        return false;
    }

    LOG_D("VIDIOC_S_FMT succ");
    _showCurFmt(_fd, capture_buf_type);     // 获取当前使用的格式
    _setFrameRate(_fd, 30);

    if(false == requestBuffer(_fd, mVideoBuffer, BUFFER_COUNT))
    {
        LOG_E("requestBuffer failed!");
        return false;
    }

    std::thread p(std::bind(&camera::run, this));
    p.detach();
    
    return true;
}

bool camera::deinit(void)
{
    freeBuffer(mVideoBuffer, BUFFER_COUNT);
    close(_fd);
    LOG_D("camera is closed! ");

    return true;
}

void camera::run(void)
{
    fd_set fds;
	struct timeval tv;
    int cnt = 0;

	FD_ZERO(&fds);
	FD_SET(_fd, &fds);
    int frameId = 0;
    uint8_t rgb_buf[WIDTH*HEIGHT*3 + 10];

    LOG_D("camera::run in");
    _isRunning = true;
    while(_isRunning)
    {
        if(_isStop)
        {
            usleep(10*1000);
            continue;
        }

        FD_ZERO(&fds);
		FD_SET(_fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		
		if(0 >= select(_fd + 1, &fds, NULL, NULL, &tv)) 
			continue;
        
        // 获取图像，主要是获取索引
        struct v4l2_buffer t_buffer;
		memset(&t_buffer, 0, sizeof(t_buffer));
		t_buffer.type = capture_buf_type; 
		t_buffer.memory = V4L2_MEMORY_MMAP;
		if(-1 == xioctl(_fd, VIDIOC_DQBUF, &t_buffer)) 
            continue;
            
        // 判断获取的状态有没有出错
		if (t_buffer.flags & V4L2_BUF_FLAG_ERROR) 
		{
			LOG_E("v4l2 buf error! buf flag 0x%x, index=%d", t_buffer.flags, t_buffer.index);
			continue;
		}

        LOG_D("index:%d bytesused:%d flags:0x%x field:0x%x t_buffer.m.offset:0x%x", 
            t_buffer.index, t_buffer.bytesused, t_buffer.flags, t_buffer.field, t_buffer.m.offset);

        if(_cameraCallback)
        {
            _cameraCallback(_user, (const char *)mVideoBuffer[t_buffer.index].data, t_buffer.bytesused, v4l2_fmt.fmt.pix.width, v4l2_fmt.fmt.pix.height);
        }
            
        // 处理完后，把这个缓冲器再还回驱动层。实际上是再次加入到驱动层的队列中去。
        if (-1 == xioctl(_fd, VIDIOC_QBUF, &t_buffer)) // 将这个信息放回驱动
		{
            LOG_E("set %d VIDIOC_QUERYBUF to line error [%s]", t_buffer.index, strerror(errno));
            continue;
        }
    }
    _isRunning = false;
    LOG_D("camera::run exit");
}

bool camera::start()
{
    if(!_isStop)
    {
        LOG_W("The camera was already running!");
        return true;
    }

    LOG_D("start!");
    if(false == requestBuffer(_fd, mVideoBuffer, BUFFER_COUNT))
    {
        LOG_E("requestBuffer failed!");
        return false;
    }

    // 通知驱动，启动摄像头
	if (-1 == xioctl(_fd, VIDIOC_STREAMON, &capture_buf_type)) 
	{
		LOG_E("set VIDIOC_STREAMON error [%s]", strerror(errno));
		goto _free;
	}

    _isStop = false;

    return true;

_free:
    freeBuffer(mVideoBuffer, BUFFER_COUNT);
    return false;
}

bool camera::stop()
{
    if(_isStop)
    {
        LOG_E("This camera is not working");
        return false;
    }

    if(-1 == xioctl(_fd, VIDIOC_STREAMOFF, &capture_buf_type)) 
	{
        LOG_E("set VIDIOC_STREAMOFF error [%s]", strerror(errno));
        return false;
    }
    freeBuffer(mVideoBuffer, BUFFER_COUNT);

    _isStop = true;

    return true;
}
