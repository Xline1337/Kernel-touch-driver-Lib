#pragma once
//频道:https://t.me/colanb113
// by cola,基于普通触摸库二改,原创,和kpm橘子的内核触摸效果一样,c++风格

/*
触摸检测原理:ls等命令在/sys/class/input/等设备目录,在目录eventX存在时返回"Permission denied"
eventX目录不存在,则返回"no such file or directory",
所以可以构造"event+数字"遍历设备目录,判断设备数量是否增加或异常
过检测原理:直接将触摸事件注入到系统触摸屏设备节点(/dev/input/eventX)
*/

//兼容性:应该是-能用内核触摸的能用此触摸库,不能用内核触摸的不能用此触摸库

/*
public函数的简单使用说明:
1.初始化:TouchManager& touch_manager = TouchManager::GetInstance();
    touch_manager.Init(renderer.screen_abs_size,false);
    [第二个参数:imgui的坐标为屏幕坐标(或逻辑坐标),部分挂通过充电口方向计算物理坐标,需要设置为true]
*/
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "spinlock.h"
#include "vector2.h"

class TouchManager {
private:
    using HandleTouchPointCallback = std::function<void(const Vector2)>;
    TouchManager() = default;
    TouchManager(const TouchManager&) = delete;
    TouchManager& operator=(const TouchManager&) = delete;
    TouchManager(TouchManager&&) = delete;
    TouchManager& operator=(TouchManager&&) = delete;
    Vector2 ConvertPhysicalToScreenAndScale(const Vector2& corrd,
                                            const Vector2& scale);
    std::vector<std::string> ScanInputDevicesPath();
    bool CheckKeyBit(unsigned int bit, const unsigned long* arr) const noexcept;
    bool CheckIsTouchDevice(int fd) const noexcept;
    void SetTouchDevices(const std::vector<std::string>& input_device_paths);
    Vector2 ConvertScreenToPhysicalNoScale(Vector2 screen_pos);
    void ReadTouchEvent();
    void UploadTouchEvent();
    void ConfirmHasUp();
    struct TouchPoint {
        int slot = -1;
        int tracking_id = -1;
        Vector2 pos{};
    };
    struct TouchDevice {
        int fd = -1;
        std::string path = "";
        Vector2 scale = {0, 0};
        TouchPoint touch_point[10];
    };
    std::atomic<bool> is_uploading{false};
    bool initialized = false;
    spinlock device_lock;
    int screen_orientation = 0;
    Vector2 screen_size{0, 0};
    const int kGrab = 1, kUnGrab = 0;
    bool is_physical_pos = false;
    TouchPoint touch_point;
    HandleTouchPointCallback handle_touch_point_callback = nullptr;
    static constexpr int kByteBitSize = 8;
    static constexpr int kEvByteSize = 64;
    TouchDevice touch_device;
public:
    static TouchManager& GetInstance() {
        static TouchManager instance;
        return instance;
    };
    void Init(const Vector2& screen_size,bool is_physical_pos);
    void Close();
    void Down(const Vector2& pos);
    void Move(const Vector2& pos);
    void Up();
    void SetScreenOrientation(int orientation);
    void SetHandleTouchPointCallback(HandleTouchPointCallback callback);
};