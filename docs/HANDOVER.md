# ESP32-S3 多媒体播放器交接文档

更新时间：2026-08-25  
目标板：4D Systems ESP32-S3 Gen4 R8N16（PlatformIO: `4d_systems_esp32s3_gen4_r8n16`）

## 1. 系统能力

项目使用 ST7789 240×240 TFT 和 LVGL 构建本地媒体 UI，通过 SPI 读取 SD 卡，支持 RGB565 图片、自定义 `.vid` 视频和 UTF-8 文本；Wi-Fi 侧提供 TCP 投屏接收、NTP 校时和天气查询。

```text
SD(SPI) -> FatFS/POSIX files -> LVGL UI -> ST7789 TFT
Wi-Fi  -> NTP / weather / TCP:8888 RGB565 stream -> TFT direct frame push
Joystick -> LVGL key events -> page/file navigation
```

## 2. 硬件接口

| 模块 | ESP32-S3 GPIO | 说明 |
|---|---|---|
| ST7789 SCLK / MOSI / DC / CS / BL | 21 / 47 / 40 / 41 / 42 | LovyanGFX、40 MHz 写屏 |
| SD CS / SCK / MISO / MOSI | 10 / 14 / 16 / 15 | SPI，固件以 400 kHz 初始化 |
| 摇杆 VRX / VRY / SW | 4 / 5 / 6 | UI 导航、确认、退出 |
| Wi-Fi 投屏 | TCP `8888` | 单帧 240×240×2 RGB565 原始数据 |

TFT、SD 和摇杆均使用 3.3 V 逻辑。SD 模块不得把 MISO/MOSI/SCK/CS 上拉到 5 V。

## 3. 文件和网络协议

| 类型 | 要求 |
|---|---|
| `.raw` | 240×240 RGB565、小端、115200 字节 |
| `.vid` | 8 字节头（帧数、FPS×100）+ RGB565 帧数据 |
| `.txt` | UTF-8，使用内嵌中文点阵字库 |
| 投屏 | TCP 8888，连续原始 RGB565 帧；不带图像容器头 |

天气接口与 NTP 仅是演示级网络服务：无网络时本地文件浏览、TFT 和摇杆仍应可用。网络参数只从被 Git 忽略的 `include/secrets.h` 读取。

## 4. 日常开发与恢复

1. 构建/烧录后，先确认 TFT 点亮和 SD 可挂载。
2. 在 SD 卡分别放入一张 `.raw`、一段 `.vid` 和一个 UTF-8 `.txt`，逐项浏览。
3. 需要投屏时让电脑与板卡处于同一网络，进入 Stream 页面后再运行 `tools/screen_sender.js`。
4. 显示异常先核对 TFT 与 SD 的独立 SPI 总线配置；若 SD 初始化失败，固件会显示错误色块并停止，以避免在未挂载文件系统时继续访问媒体。

## 5. 公开仓库卫生

不提交 `.pio/`、`node_modules/`、本地 Wi-Fi 密码或屏幕录制素材。提交源代码、转换脚本、公开配置模板、文档与可公开的演示资源即可。
