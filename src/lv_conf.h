#ifndef LV_CONF_H
#define LV_CONF_H

/*---- Display ----*/
#define LV_COLOR_DEPTH          16
#define LV_HOR_RES_MAX          240
#define LV_VER_RES_MAX          240

/*---- Memory ----*/
#define LV_MEM_SIZE             (20U * 1024U)
#define LV_MEMCPY_MEMSET_STD    1
#define LV_ATTRIBUTE_FAST_MEM

/*---- GPU ----*/
#define LV_USE_GPU              0
#define LV_USE_GPU_STM32_DMA2D  0

/*---- Logging ----*/
#define LV_USE_LOG              0

/*---- Features ----*/
#define LV_DISP_DEF_REFR_PERIOD  30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM           0
#define LV_USE_PERF_MONITOR      0
#define LV_USE_ASSERT_NULL       0
#define LV_USE_ASSERT_MALLOC     0
#define LV_USE_ASSERT_STYLE      0
#define LV_USE_ASSERT_MEM_INTEGRITY 0

/*---- Drawing ----*/
#define LV_DRAW_COMPLEX          1
#define LV_SHADOW_CACHE_SIZE     0
#define LV_IMG_CACHE_DEF_SIZE    1
#define LV_CIRCLE_CACHE_SIZE     4
#define LV_LAYER_SIMPLE_BUF_SIZE (24U * 1024U)
#define LV_IMG_CF_INDEXED         0
#define LV_GPU_DMA2D_CMSIS_INCLUDE 0

/*---- Fonts ----*/
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_DEFAULT          &lv_font_montserrat_14

/*---- Extra widgets ----*/
#define LV_USE_ANIMIMG           0
#define LV_USE_CALENDAR          0
#define LV_USE_CHART             0
#define LV_USE_COLORWHEEL        0
#define LV_USE_IMGBTN            0
#define LV_USE_KEYBOARD          0
#define LV_USE_LED               0
#define LV_USE_LIST              1
#define LV_USE_METER             0
#define LV_USE_MSGBOX            1
#define LV_USE_SPINBOX           0
#define LV_USE_SPINNER           0
#define LV_USE_TABVIEW           0
#define LV_USE_TILEVIEW          0
#define LV_USE_WIN               0

/*---- Themes ----*/
#define LV_USE_THEME_DEFAULT     1
#define LV_THEME_DEFAULT_GROW    1

/*---- Filesystem ----*/
#define LV_USE_FS_FATFS           0
#define LV_FS_FATFS_LETTER        '0'
#define LV_FS_FATFS_CACHE_SIZE    4096

/*---- Demos ----*/
#define LV_USE_DEMO_WIDGETS       0
#define LV_USE_DEMO_BENCHMARK     0
#define LV_USE_DEMO_STRESS        0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_MUSIC         0

/*---- Image decoders ----*/
#define LV_USE_SJPG             0
#define LV_IMG_CACHE_DEF_SIZE    1

/*---- Others ----*/
#define LV_SPRINTF_CUSTOM         0
#define LV_USE_USER_DATA          0
#define LV_USE_LARGE_COORD        0
#define LV_USE_MONKEY             0
#define LV_USE_GRIDNAV            0

#endif /* LV_CONF_H */
