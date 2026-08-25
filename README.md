# ESP32-S3 MP4 播放器

基于鹿小班 ESP32-S3 (R8N16) 的多功能媒体播放器，支持图片/视频/文本浏览、WiFi 投屏、网络时钟和天气。

## 外设接线

### TFT 彩屏 (ST7789 240x240)

| TFT | ESP32-S3 GPIO |
|-----|---------------|
| VCC | 3.3V |
| GND | GND |
| SCL | 21 |
| SDA | 47 |
| CS  | 41 |
| DC  | 40 |
| BL  | 42 |

### SD 卡模块 (SPI)

| SD 模块 | ESP32-S3 GPIO |
|---------|---------------|
| VCC     | **3.3V**      |
| GND     | GND           |
| CS      | 10            |
| SCK     | 14            |
| MOSI    | 15            |
| MISO    | 16            |

### 双轴摇杆 (ADC + GPIO)

| 摇杆 | ESP32-S3 GPIO |
|------|---------------|
| VCC  | 3.3V          |
| GND  | GND           |
| VRX  | 4             |
| VRY  | 5             |
| SW   | 6             |

## 标签页功能

| 序号 | 标签     | 功能                            | 操作                          |
|------|----------|--------------------------------|-------------------------------|
| 0    | Home     | 网络时钟 + 日期 + 天气          | 默认首页                       |
| 1    | RAW      | 浏览 .raw 图片 (240x240 RGB565) | ENTER 打开, 左右切换, 长按退出  |
| 2    | TXT      | 阅读 .txt 文本 (UTF-8, 中文)    | ENTER 打开, 上下翻页, 长按退出  |
| 3    | VID      | 播放 .vid 视频                  | ENTER 播放, 左右跳帧, 长按退出  |
| 4    | Stream   | WiFi 接收电脑画面投屏           | ENTER 进入接收, 长按退出        |

### 摇杆操作

| 动作         | 功能           |
|-------------|----------------|
| 上/下        | 切换标签页      |
| 左/右        | 选择文件/翻页   |
| 短按 (按键)  | 确认/打开       |
| 长按 500ms   | 返回/退出       |

## 文件格式

### .raw 图片
- 240x240 像素 RGB565 原始数据
- 115,200 字节 (240 x 240 x 2)
- 用小端字节序存储
- 工具: `tools/convert_jpeg.py` — JPEG 转 .raw

### .vid 视频
- 自定义格式: 8 字节头 + 帧数据
- 头部: 4 字节帧数 (uint32 LE) + 4 字节 FPS×100 (uint32 LE)
- 每帧 115,200 字节 RGB565
- 工具: `tools/convert_video.py` — MP4/GIF 转 .vid

### .txt 文本
- UTF-8 编码
- 支持中文 (内嵌 407 汉字字库)
- 自动换行

## WiFi 投屏

### ESP32 端
1. 从 `include/secrets.example.h` 复制生成本机 `include/secrets.h`，填写本地 WiFi 名称和密码（该文件不会上传）
2. 切换到第 4 个标签 "Stream"
3. 屏幕会显示 ESP32 的 IP 地址
4. 按摇杆 ENTER 进入接收模式

### 电脑端推流

Node.js 发送器已内置，依赖已安装。

```bash
# 切换到项目 tools 目录
cd C:\Users\13957\Documents\PlatformIO\Projects\mp4

# 推流 (默认 15fps)
C:\Users\13957\nodejs\node-v22.14.0-win-x64\node.exe tools\screen_sender.js <ESP32的IP>

# 指定帧率
C:\Users\13957\nodejs\node-v22.14.0-win-x64\node.exe tools\screen_sender.js 192.168.43.62 20

# 低延迟参数 (高帧率)
C:\Users\13957\nodejs\node-v22.14.0-win-x64\node.exe tools\screen_sender.js 192.168.43.62 25
```

> 电脑必须和 ESP32 连接**同一个 Wi-Fi**。网络名称和密码只写入本机忽略文件 `include/secrets.h`，不要写入 README、源码或提交记录。

## 技术架构

| 模块       | 方案                                        |
|-----------|---------------------------------------------|
| TFT 显示   | LovyanGFX, SPI3_HOST, rgb_order=true        |
| SD 卡      | Arduino SD.h, SPI2_HOST, 400KHz, 3.3V 供电  |
| 文件系统    | POSIX API (opendir/readdir/fopen/fread)     |
| UI         | LVGL 8.2.0, 240x240, 16-bit color           |
| 摇杆输入    | ESP-IDF ADC, center=1800, deadzone=300       |
| WiFi       | ESP32 内置, STA 模式                         |
| 投屏协议    | TCP:8888, 原始 RGB565 115KB/帧              |
| NTP 时钟   | configTime (ntp.aliyun.com), UTC+8           |
| 天气       | wttr.in API, 每 30 分钟刷新                  |
| 中文字库    | cn_font, 407 字符, 16x16 点阵               |

## 编译 & 烧录

```bash
pio run          # 编译
pio run -t upload # 烧录
```

PlatformIO 板型: `4d_systems_esp32s3_gen4_r8n16`

## 工程交接与验证

- [交接文档：架构、运行路径、网络边界与排障](docs/HANDOVER.md)
- [验证清单：SD、文件浏览、网络服务和投屏](docs/VALIDATION.md)
- `include/secrets.example.h` 是公开的网络配置模板；实际 `include/secrets.h` 只保存在本机。

## 简历项目描述

**ESP32-S3 多媒体播放器**：基于 ESP32-S3 和 LVGL 实现 240×240 嵌入式媒体 UI，集成 SPI TFT、SD 卡文件系统、中文文本/图片/自定义视频格式浏览、Wi-Fi TCP RGB565 投屏、NTP 授时与天气服务，并完成本地凭据隔离和资源受限下的交互调度。
