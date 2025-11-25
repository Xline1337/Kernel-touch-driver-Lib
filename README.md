# Kernel-touch-driver-Lib
触摸检测原理:ls等命令在/sys/class/input/等设备目录,在目录eventX存在时返回"Permission denied"
eventX目录不存在,则返回"no such file or directory",
所以可以构造"event+数字"遍历设备目录,判断设备数量是否增加或异常
过检测原理:直接将触摸事件注入到系统触摸屏设备节点(/dev/input/eventX)

兼容性:应该是-能用内核触摸的能用此触摸库,不能用内核触摸的不能用此触摸库
>tips:完整版在release自取
