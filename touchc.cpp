#include "touchc.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"
#include "linux/input.h"
#include "linux/uinput.h"
#include "spinlock.h"
#include "sys/ioctl.h"
#include "vector2.h"
// 将物理坐标转换为屏幕坐标(或逻辑坐标),并进行比例缩放
Vector2 TouchManager::ConvertPhysicalToScreenAndScale(const Vector2 &corrd,
                                                      const Vector2 &scale) {
    Vector2 scale_pos = {corrd.x * scale.x, corrd.y * scale.y};
    Vector2 pos{0, 0};

    switch (screen_orientation) {
        case 0: pos = {scale_pos.x, scale_pos.y}; break;
        case 1: pos = {scale_pos.y, screen_size.y - scale_pos.x}; break;
        case 3: pos = {screen_size.x - scale_pos.y, scale_pos.x}; break;
        default:
            pos = {screen_size.y - scale_pos.x, screen_size.x - scale_pos.y};
            break;
    }
    return pos;
}
// 获取所有输入设备的节点路径
std::vector<std::string> TouchManager::ScanInputDevicesPath() {
    std::vector<std::string> input_device_paths;
    input_device_paths.reserve(18);
    DIR *input_dir = opendir("/dev/input/");
    if (input_dir == nullptr) {
        std::cerr << "打开/dev/input/失败!" << std::endl;
        return input_device_paths;
    }
    struct dirent *file_entry;
    while ((file_entry = readdir(input_dir)) != nullptr) {
        const std::string filename = file_entry->d_name;
        if (filename.find("event") == 0) {
            input_device_paths.emplace_back("/dev/input/" + filename);
        }
    }
    closedir(input_dir);
    return input_device_paths;
}

// 检查指定位是否被设置能力
bool TouchManager::CheckKeyBit(unsigned int bit,
                               const unsigned long *arr) const noexcept {
    static constexpr auto long_bits = sizeof(unsigned long) * kByteBitSize;
    if (bit >= sizeof(unsigned long) * kEvByteSize) { return false; }
    const auto mask = 1UL << (bit % long_bits);
    return (arr[bit / long_bits] & mask) != 0;
}

// 检查是否为触摸设备
bool TouchManager::CheckIsTouchDevice(int fd) const noexcept {
    std::array<unsigned long, kEvByteSize> abs_bits{};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits.data()) < 0) {
        return false;
    }
    bool has_slot = CheckKeyBit(ABS_MT_SLOT, abs_bits.data());
    bool has_tracking_id = CheckKeyBit(ABS_MT_TRACKING_ID, abs_bits.data());
    bool has_pos_x = CheckKeyBit(ABS_MT_POSITION_X, abs_bits.data());
    bool has_pos_y = CheckKeyBit(ABS_MT_POSITION_Y, abs_bits.data());

    return has_slot && has_tracking_id && has_pos_x && has_pos_y;
}

// 获取系统触摸设备的数据
void TouchManager::SetTouchDevices(
    const std::vector<std::string> &input_device_paths) {
    for (const auto &path : input_device_paths) {
        int fd = open(path.c_str(), O_RDWR);
        if (fd == -1) { continue; }
        if (CheckIsTouchDevice(fd)) {
            input_absinfo abs_x, abs_y;
            ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x);
            ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y);
            touch_device.scale = {screen_size.y / abs_x.maximum,
                                  screen_size.x / abs_y.maximum};
            touch_device.fd = fd;
            touch_device.path = path;
            break;
        } else {
            close(fd);
        }
    }
}
// 设置触摸点回调函数
void TouchManager::SetHandleTouchPointCallback(
    HandleTouchPointCallback callback) {
    handle_touch_point_callback = callback;
}

// 读取触摸事件
void TouchManager::ReadTouchEvent() {
    TouchDevice &device = touch_device;
    struct input_event input_events[788];  // 较大的缓冲区
    memset(input_events, 0, sizeof(input_events));
    int current_slot = 0;
    while (true) {
        if (!initialized) { return; }
        size_t read_size = read(device.fd, &input_events, sizeof(input_events));
        if (read_size <= 0 || (read_size % sizeof(struct input_event) != 0)) {
            continue;
        }
        int event_count = read_size / sizeof(struct input_event);

        for (int i = 0; i < event_count; i++) {
            struct input_event &event = input_events[i];
            if (event.type == EV_ABS) {
                if (event.code == ABS_MT_SLOT) {
                    current_slot = event.value;
                    continue;
                }
                if (event.code == ABS_MT_TRACKING_ID) {
                    device.touch_point[current_slot].tracking_id = event.value;
                    continue;
                }
                if (event.code == ABS_MT_POSITION_X) {
                    device.touch_point[current_slot].pos.x = event.value;
                    continue;
                }
                if (event.code == ABS_MT_POSITION_Y) {
                    device.touch_point[current_slot].pos.y = event.value;
                    continue;
                }
            }
            if (event.type == EV_SYN && event.code == SYN_REPORT) {
                if (ImGui::GetCurrentContext() == nullptr) { continue; }
                ImGuiIO &io = ImGui::GetIO();
                if (device.touch_point[current_slot].tracking_id >= 0) {
                    const auto pos = ConvertPhysicalToScreenAndScale(
                        device.touch_point[current_slot].pos, device.scale);

                    if (handle_touch_point_callback != nullptr) {
                        handle_touch_point_callback(pos);
                    }
                    io.MousePos = ImVec2(pos.x, pos.y);
                    io.MouseDown[0] = true;
                } else {
                    io.MouseDown[0] = false;
                }
            }
        }
    }
}

void TouchManager::Init(const Vector2 &screen_size, bool is_physical_pos) {
    Close();
    this->is_physical_pos = is_physical_pos;
    touch_point.slot = 9;
    if (screen_size.x > screen_size.y) {
        this->screen_size = screen_size;
    } else {
        this->screen_size = {screen_size.y, screen_size.x};
    }

    const std::vector<std::string> input_devices_path = ScanInputDevicesPath();
    if (input_devices_path.empty()) {
        std::cerr << "未找到输入设备!" << std::endl;
        return;
    }

    SetTouchDevices(input_devices_path);
    if (touch_device.fd == -1) {
        std::cerr << "未找到触摸设备!" << std::endl;
        return;
    }

    initialized = true;

    std::thread handle_touch_device_thread(&TouchManager::ReadTouchEvent, this);
    handle_touch_device_thread.detach();

    std::thread confirm_has_up_thread(&TouchManager::ConfirmHasUp, this);
    confirm_has_up_thread.detach();
    return;
}

/*
添加此函数原因:在很小概率下,有抬起事件没有被上传,导致卡屏
作用：确保有抬起事件
*/

// 修复 ConfirmHasUp 的忙等待和锁范围
void TouchManager::ConfirmHasUp() {
    while (initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // antiCPU爆炸
        
        if (is_uploading.load()) continue;
        
        
    }
}

// 2. 读写分离，或至少使用非阻塞+select
void TouchManager::ReadTouchEvent() {
    // 设置非阻塞
    int flags = fcntl(device.fd, F_GETFL, 0);
    fcntl(device.fd, F_SETFL, flags | O_NONBLOCK);
    
    fd_set fds;
    while (initialized) {
        FD_ZERO(&fds);
        FD_SET(device.fd, &fds);
        struct timeval tv = {0, 10000};  // 10ms timeout
        
        if (select(device.fd + 1, &fds, nullptr, nullptr, &tv) > 0) {
            // 只在有数据时读，避免阻塞写入时机
            read(...);
        }
    }
}

// 3. 使用不同 slot，避免硬件冲突
touch_point.slot = 15;  // 用最大slot，减少冲突概率（如果驱动支持）

void TouchManager::Close() {
    if (!initialized) { return; }
    initialized = false;
    close(touch_device.fd);
    touch_device.fd = -1;
}

void TouchManager::UploadTouchEvent() {
    struct input_event events[221];
    memset(events, 0, sizeof(events));
    static bool isFirstDown = true;
    int event_count = 0;

    if (touch_point.tracking_id != -1) {
        events[event_count].type = EV_ABS;
        events[event_count].code = ABS_MT_SLOT;
        events[event_count].value = touch_point.slot;
        event_count++;

        events[event_count].type = EV_ABS;
        events[event_count].code = ABS_MT_TRACKING_ID;
        events[event_count].value = touch_point.tracking_id;
        event_count++;

        events[event_count].type = EV_ABS;
        events[event_count].code = ABS_MT_POSITION_X;
        events[event_count].value = (int)touch_point.pos.x;
        event_count++;

        events[event_count].type = EV_ABS;
        events[event_count].code = ABS_MT_POSITION_Y;
        events[event_count].value = (int)touch_point.pos.y;
        event_count++;

        if (isFirstDown) { isFirstDown = false; }
    } else {
        if (!isFirstDown) {
            events[event_count].type = EV_ABS;
            events[event_count].code = ABS_MT_SLOT;
            events[event_count].value = touch_point.slot;
            event_count++;

            events[event_count].type = EV_ABS;
            events[event_count].code = ABS_MT_TRACKING_ID;
            events[event_count].value = -1;
            event_count++;
            isFirstDown = true;
        }
    }
    events[event_count].type = EV_SYN;
    events[event_count].code = SYN_REPORT;
    events[event_count].value = 0;
    event_count++;

    write(touch_device.fd, events, sizeof(struct input_event) * event_count);
}
Vector2 TouchManager::ConvertScreenToPhysicalNoScale(Vector2 screen_pos) {
    Vector2 physical_pos = {0, 113};
    switch (screen_orientation) {
        case 0:
            physical_pos.x = screen_pos.x;
            physical_pos.y = screen_pos.y;
            break;
        case 1:
            physical_pos.x = screen_size.y - screen_pos.y;
            physical_pos.y = screen_pos.x;
            break;
        case 3:
            physical_pos.x = screen_pos.y;
            physical_pos.y = screen_size.x - screen_pos.x;
            break;
        default:
            physical_pos.x = screen_size.y - screen_pos.y;
            physical_pos.y = screen_size.x - screen_pos.x;
            break;
    }
    return physical_pos;
}

void TouchManager::Down(const Vector2 &pos) {
    std::lock_guard<spinlock> guard(device_lock);
    is_uploading.store(true);
    touch_point.tracking_id = 5200;
    Vector2 physical_pos;
    if (is_physical_pos) {
        physical_pos = Vector2(pos.x, pos.y);
    } else {
        physical_pos = ConvertScreenToPhysicalNoScale(pos);
    }
    touch_point.pos = {physical_pos.x / touch_device.scale.x,
                       physical_pos.y / touch_device.scale.y};
    UploadTouchEvent();
}

void TouchManager::Move(const Vector2 &pos) { Down(pos); }
void TouchManager::Up() {
    std::lock_guard<spinlock> guard(device_lock);
    touch_point.tracking_id = -1;
    UploadTouchEvent();
    is_uploading.store(false);
}

void TouchManager::SetScreenOrientation(int screen_orientation) {
    this->screen_orientation = screen_orientation;
}