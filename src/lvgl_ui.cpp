#include "lvgl_ui.h"
#include "lv_port_fs.h"
#include "tft_display.h"
#include <Arduino.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include "net_services.h"

static lv_obj_t   *g_scr;
static lv_obj_t   *g_cont_home;
static lv_obj_t   *g_cont_img;
static lv_obj_t   *g_cont_txt;
static lv_obj_t   *g_cont_vid;
static lv_obj_t   *g_cont_stream;
static lv_obj_t   *g_label_stream;
static lv_obj_t   *g_label_time;
static lv_obj_t   *g_label_date;
static lv_obj_t   *g_label_weather;
static lv_obj_t   *g_label_status;
static lv_group_t *g_group;
static int         g_cur_tab;

#define MAX_FILES 40
static lv_obj_t *g_btns_img[MAX_FILES];
static lv_obj_t *g_btns_txt[MAX_FILES];
static lv_obj_t *g_btns_vid[MAX_FILES];
static char      g_names_img[MAX_FILES][64];
static char      g_names_txt[MAX_FILES][64];
static char      g_names_vid[MAX_FILES][64];
static int       g_cnt_img, g_cnt_txt, g_cnt_vid;
static int       g_focus_img, g_focus_txt, g_focus_vid;

#define VIEW_NONE   0
#define VIEW_RAW    1
#define VIEW_TEXT   2
#define VIEW_VID    3
#define VIEW_STREAM 4
static int g_view_mode = VIEW_NONE;

static uint8_t  g_txt_buf[4100];
static unsigned int g_txt_len;
static int      g_txt_line_start[256];
static int      g_txt_line_count;
static int      g_txt_scroll;

/* Video state */
static FILE   *g_vid_file;
static int     g_vid_frames;
static int     g_vid_frame_idx;
static int     g_vid_frame_delay;
static uint32_t g_vid_next_frame_ms;

static lv_style_t style_focus;

/* ====================== helpers ====================== */

static int str_ends(const char *s, const char *ext)
{
    int sl = strlen(s), el = strlen(ext);
    if (sl < el) return 0;
    for (int i = 0; i < el; i++)
        if (s[sl - el + i] != ext[i]) return 0;
    return 1;
}

static void set_focus(lv_obj_t *btn, int on)
{
    if (on) lv_obj_add_style(btn, &style_focus, 0);
    else    lv_obj_remove_style(btn, &style_focus, 0);
}

static int build_list(lv_obj_t *parent, lv_obj_t **btns, char names[][64],
                       const char *ext)
{
    for (int i = 0; i < MAX_FILES; i++) {
        if (btns[i]) { lv_obj_del(btns[i]); btns[i] = NULL; }
    }

    DIR *dir = opendir("/sd");
    if (!dir) return 0;

    int cnt = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && cnt < MAX_FILES) {
        if (entry->d_type == DT_DIR) continue;
        if (!str_ends(entry->d_name, ext)) continue;

        strncpy(names[cnt], entry->d_name, 63); names[cnt][63] = '\0';

        char fullpath[128];
        snprintf(fullpath, sizeof(fullpath), "/sd/%s", entry->d_name);
        struct stat st;
        uint64_t fsize = 0;
        if (stat(fullpath, &st) == 0) fsize = st.st_size;

        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 236, 34);
        lv_obj_t *lb = lv_label_create(btn);
        lv_label_set_text_fmt(lb, "%s  (%llu KB)", entry->d_name, (unsigned long long)(fsize / 1024));
        lv_obj_set_style_text_color(lb, lv_color_white(), 0);
        lv_obj_center(lb);
        btns[cnt] = btn;
        cnt++;
    }
    closedir(dir);
    return cnt;
}

/* ====================== tab switching ====================== */

static void show_tab(int tab)
{
    g_cur_tab = tab;
    lv_group_remove_all_objs(g_group);
    lv_obj_add_flag(g_cont_home, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_cont_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_cont_txt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_cont_vid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_cont_stream, LV_OBJ_FLAG_HIDDEN);

    if (tab == 0) {
        lv_obj_clear_flag(g_cont_home, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(g_label_status, "Home  U/D:switch tab");
    } else if (tab == 1) {
        lv_obj_clear_flag(g_cont_img, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < g_cnt_img; i++) {
            lv_group_add_obj(g_group, g_btns_img[i]);
            set_focus(g_btns_img[i], i == g_focus_img);
        }
        if (g_cnt_img > 0) lv_group_focus_obj(g_btns_img[g_focus_img]);
        lv_label_set_text_fmt(g_label_status, "%d images  L/R:sel  U/D:tab  press:open  hold:back", g_cnt_img);
    } else if (tab == 2) {
        lv_obj_clear_flag(g_cont_txt, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < g_cnt_txt; i++) {
            lv_group_add_obj(g_group, g_btns_txt[i]);
            set_focus(g_btns_txt[i], i == g_focus_txt);
        }
        if (g_cnt_txt > 0) lv_group_focus_obj(g_btns_txt[g_focus_txt]);
        lv_label_set_text_fmt(g_label_status, "%d texts  L/R:sel  U/D:tab  press:open  hold:back", g_cnt_txt);
    } else if (tab == 4) {
        lv_obj_clear_flag(g_cont_stream, LV_OBJ_FLAG_HIDDEN);
        /* Show WiFi status with IP */
        extern const char *wifi_stream_ip();
        extern bool wifi_stream_connected();
        if (wifi_stream_connected()) {
            lv_label_set_text_fmt(g_label_stream,
                "WiFi Connected\nIP: %s\nPort: 8888\n\nPress ENTER to start",
                wifi_stream_ip());
        } else {
            lv_label_set_text(g_label_stream,
                "WiFi: TEST\nConnecting...\n\nIP: ---\n\nPress ENTER to start");
        }
        lv_obj_center(g_label_stream);
        lv_label_set_text_fmt(g_label_status, "Press ENTER to start stream");
    } else {
        lv_obj_clear_flag(g_cont_vid, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < g_cnt_vid; i++) {
            lv_group_add_obj(g_group, g_btns_vid[i]);
            set_focus(g_btns_vid[i], i == g_focus_vid);
        }
        if (g_cnt_vid > 0) lv_group_focus_obj(g_btns_vid[g_focus_vid]);
        lv_label_set_text_fmt(g_label_status, "%d videos  L/R:sel  U/D:tab  press:open  hold:back", g_cnt_vid);
    }
}

static void nav_focus(int right)
{
    int cnt; int *fidx; lv_obj_t **btns;
    if (g_cur_tab == 1)      { cnt = g_cnt_img; fidx = &g_focus_img; btns = g_btns_img; }
    else if (g_cur_tab == 2) { cnt = g_cnt_txt; fidx = &g_focus_txt; btns = g_btns_txt; }
    else if (g_cur_tab == 3) { cnt = g_cnt_vid; fidx = &g_focus_vid; btns = g_btns_vid; }
    else return;
    if (cnt < 2) return;
    set_focus(btns[*fidx], 0);
    *fidx = right ? (*fidx + 1) % cnt : (*fidx - 1 + cnt) % cnt;
    set_focus(btns[*fidx], 1);
    lv_group_focus_obj(btns[*fidx]);
}

/* ====================== viewers ====================== */

static void view_raw_image(const char *fname)
{
    char path[128]; snprintf(path, sizeof(path), "/sd/%s", fname);
    FILE *f = fopen(path, "rb");
    if (!f) { g_view_mode = VIEW_NONE; return; }
    TFT_FillScreen(COLOR_BLACK);
    TFT_StreamStart();
    uint16_t buf[2048];
    int remain = 240 * 240;
    while (remain > 0) {
        int n = (remain < 2048) ? remain : 2048;
        fread(buf, n * 2, 1, f);
        TFT_StreamPush(buf, n);
        remain -= n;
    }
    TFT_StreamEnd();
    fclose(f);
    g_view_mode = VIEW_RAW;
}

static void compute_lines(void)
{
    g_txt_line_count = 0; g_txt_line_start[0] = 0;
    unsigned int pos = 0; int cur_w = 0;
    while (pos < g_txt_len && g_txt_line_count < 255) {
        uint8_t c = g_txt_buf[pos];
        uint32_t cp = 0; int char_w;
        if (c < 0x80) { cp = c; pos++; char_w = (cp == '\n' || cp == '\r') ? 0 : 8; }
        else if ((c & 0xE0) == 0xC0 && pos + 1 < g_txt_len) { cp = ((c & 0x1F) << 6) | (g_txt_buf[pos+1] & 0x3F); pos += 2; char_w = 16; }
        else if ((c & 0xF0) == 0xE0 && pos + 2 < g_txt_len) { cp = ((c & 0x0F) << 12) | ((g_txt_buf[pos+1] & 0x3F) << 6) | (g_txt_buf[pos+2] & 0x3F); pos += 3; char_w = 16; }
        else { pos++; char_w = 0; }
        if (cp == '\r') continue;
        if (cp == '\n' || cur_w + char_w > TFT_WIDTH) {
            g_txt_line_start[++g_txt_line_count] = (cp == '\n') ? pos : pos - (char_w > 0 ? (c < 0x80 ? 1 : 3) : 0);
            cur_w = 0; if (cp == '\n') continue;
        }
        cur_w += char_w;
    }
}

static void render_text_page(void)
{
    TFT_FillScreen(COLOR_BLACK); TFT_SetTextColor(COLOR_WHITE, COLOR_BLACK);
    int lines = TFT_HEIGHT / 16;
    for (int i = 0; i < lines && (g_txt_scroll + i) < g_txt_line_count; i++) {
        int off = g_txt_line_start[g_txt_scroll + i];
        int end = (g_txt_scroll + i + 1 < g_txt_line_count) ? g_txt_line_start[g_txt_scroll + i + 1] : (int)g_txt_len;
        if (end > off + 1 && g_txt_buf[end-1] == '\n') end--;
        if (end > off + 1 && g_txt_buf[end-1] == '\r') end--;
        if (end > off) TFT_DrawUTF8(0, i * 16, g_txt_buf + off, (unsigned int)(end - off));
    }
}

static void view_text_file(const char *fname)
{
    char path[128]; snprintf(path, sizeof(path), "/sd/%s", fname);
    FILE *f = fopen(path, "rb");
    g_txt_len = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        unsigned int sz = (unsigned int)ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 4095) sz = 4095;
        g_txt_len = fread(g_txt_buf, 1, sz, f);
        fclose(f);
    }
    if (g_txt_len == 0) {
        TFT_FillScreen(COLOR_BLACK); TFT_SetTextColor(COLOR_WHITE, COLOR_BLACK);
        TFT_DrawString(0, 0, "(empty)"); g_view_mode = VIEW_RAW; return;
    }
    compute_lines(); g_txt_scroll = 0; render_text_page();
    g_view_mode = VIEW_TEXT;
}

/* ====================== video player ====================== */

static void show_vid_frame(void)
{
    unsigned int offset = 8 + (unsigned int)g_vid_frame_idx * 240 * 240 * 2;
    fseek(g_vid_file, offset, SEEK_SET);

    TFT_StreamStart();

    #define VID_CHUNK 20400
    static uint16_t vid_buf[VID_CHUNK];
    int remain = 240 * 240;

    while (remain > 0) {
        int n = (remain < VID_CHUNK) ? remain : VID_CHUNK;
        fread(vid_buf, n * 2, 1, g_vid_file);
        TFT_StreamPush(vid_buf, n);
        remain -= n;
    }

    TFT_StreamEnd();

    if ((g_vid_frame_idx & 7) == 0) {
        TFT_SetTextColor(COLOR_WHITE, COLOR_BLACK);
        char fstr[16];
        snprintf(fstr, sizeof(fstr), " %d/%d ", g_vid_frame_idx + 1, g_vid_frames);
        TFT_DrawString(0, 0, fstr);
    }

    g_vid_next_frame_ms = millis() + g_vid_frame_delay;
}

static void view_video_file(const char *fname)
{
    char path[128]; snprintf(path, sizeof(path), "/sd/%s", fname);
    g_vid_file = fopen(path, "rb");
    if (!g_vid_file) { g_view_mode = VIEW_NONE; return; }

    uint8_t hdr[8];
    fread(hdr, 8, 1, g_vid_file);
    g_vid_frames = (int)(hdr[0] | ((uint32_t)hdr[1] << 8)
                       | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24));
    uint32_t fps100 = hdr[4] | ((uint32_t)hdr[5] << 8)
                    | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    if (fps100 >= 100 && fps100 <= 10000)
        g_vid_frame_delay = 100000 / fps100;
    else
        g_vid_frame_delay = 125;
    g_vid_frame_idx = 0;
    g_vid_next_frame_ms = 0;

    TFT_FillScreen(COLOR_BLACK);
    show_vid_frame();
    g_view_mode = VIEW_VID;
}

/* ====================== key handling ====================== */

void lvgl_ui_handle_key(uint32_t key)
{
    if (g_view_mode == VIEW_TEXT) {
        if (key == LV_KEY_ESC) { g_view_mode = VIEW_NONE; lv_obj_invalidate(lv_scr_act()); show_tab(g_cur_tab); }
        else if (key == LV_KEY_NEXT && g_txt_scroll + 15 < g_txt_line_count) { g_txt_scroll++; render_text_page(); }
        else if (key == LV_KEY_PREV && g_txt_scroll > 0) { g_txt_scroll--; render_text_page(); }
        else if ((key == LV_KEY_LEFT || key == LV_KEY_RIGHT) && g_cnt_txt > 1) {
            g_focus_txt = (key == LV_KEY_RIGHT) ? (g_focus_txt + 1) % g_cnt_txt : (g_focus_txt - 1 + g_cnt_txt) % g_cnt_txt;
            if (g_names_txt[g_focus_txt][0]) view_text_file(g_names_txt[g_focus_txt]);
        }
        return;
    }
    if (g_view_mode == VIEW_RAW) {
        if (key == LV_KEY_ESC) { g_view_mode = VIEW_NONE; lv_obj_invalidate(lv_scr_act()); show_tab(g_cur_tab); }
        else if ((key == LV_KEY_LEFT || key == LV_KEY_RIGHT) && g_cnt_img > 1) {
            g_focus_img = (key == LV_KEY_RIGHT) ? (g_focus_img + 1) % g_cnt_img : (g_focus_img - 1 + g_cnt_img) % g_cnt_img;
            if (g_names_img[g_focus_img][0]) view_raw_image(g_names_img[g_focus_img]);
        }
        return;
    }
    if (g_view_mode == VIEW_VID) {
        if (key == LV_KEY_ESC) {
            fclose(g_vid_file); g_view_mode = VIEW_NONE;
            lv_obj_invalidate(lv_scr_act()); show_tab(g_cur_tab);
        } else if (key == LV_KEY_RIGHT && g_vid_frame_idx + 1 < g_vid_frames) {
            g_vid_frame_idx++; show_vid_frame();
        } else if (key == LV_KEY_LEFT && g_vid_frame_idx > 0) {
            g_vid_frame_idx--; show_vid_frame();
        }
        return;
    }
    if (g_view_mode == VIEW_STREAM) {
        if (key == LV_KEY_ESC) {
            g_view_mode = VIEW_NONE;
            lv_obj_invalidate(lv_scr_act()); show_tab(g_cur_tab);
        }
        return;
    }

    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) { nav_focus(key == LV_KEY_RIGHT); return; }

    /* Stream tab: ENTER starts stream */
    if (key == LV_KEY_ENTER && g_cur_tab == 4) {
        g_view_mode = VIEW_STREAM;
        return;
    }
    if (key == LV_KEY_PREV) {
        int t = (g_cur_tab - 1 + 5) % 5; show_tab(t); return;
    }
    if (key == LV_KEY_NEXT) {
        int t = (g_cur_tab + 1) % 5; show_tab(t); return;
    }
    if (key == LV_KEY_ENTER) {
        int idx; char *name;
        if (g_cur_tab == 1)      { idx = g_focus_img; name = g_names_img[idx]; }
        else if (g_cur_tab == 2) { idx = g_focus_txt; name = g_names_txt[idx]; }
        else                     { idx = g_focus_vid; name = g_names_vid[idx]; }
        if (name[0]) {
            if (g_cur_tab == 1)      view_raw_image(name);
            else if (g_cur_tab == 2) view_text_file(name);
            else                     view_video_file(name);
        }
        return;
    }
}

void lvgl_ui_show_debug(uint16_t x, uint16_t y, uint8_t sw) { (void)x; (void)y; (void)sw; }
int lvgl_ui_is_viewing(void) { return (g_view_mode == VIEW_RAW || g_view_mode == VIEW_TEXT || g_view_mode == VIEW_VID || g_view_mode == VIEW_STREAM); }
int lvgl_ui_is_streaming(void) { return (g_view_mode == VIEW_STREAM); }

void lvgl_ui_tick(void)
{
    if (g_view_mode == VIEW_VID && g_vid_frames > 1) {
        if (millis() >= g_vid_next_frame_ms) {
            g_vid_frame_idx++;
            if (g_vid_frame_idx >= g_vid_frames) g_vid_frame_idx = 0;
            show_vid_frame();
        }
    }

    /* Refresh home tab every second */
    static uint32_t last_home = 0;
    if (g_cur_tab == 0 && g_view_mode == VIEW_NONE && millis() - last_home > 1000) {
        last_home = millis();
        if (net_time_synced()) {
            int h, m, s, y, mo, d, wd;
            net_time_get(&h, &m, &s, &y, &mo, &d, &wd);
            const char *wdays[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            lv_label_set_text_fmt(g_label_time, "%02d:%02d:%02d", h, m, s);
            lv_label_set_text_fmt(g_label_date, "%04d-%02d-%02d %s", y, mo, d, wdays[wd]);
        }
        lv_label_set_text(g_label_weather, net_weather_text());
    }
}

/* ====================== init ====================== */

void lvgl_ui_init(void)
{
    g_scr = lv_scr_act();
    lv_obj_set_style_bg_color(g_scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(g_scr);
    lv_label_set_text(title, "File Browser");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    g_label_status = lv_label_create(g_scr);
    lv_obj_set_style_text_color(g_label_status, lv_color_hex(0x888888), 0);
    lv_obj_align(g_label_status, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_style_init(&style_focus);
    lv_style_set_bg_color(&style_focus, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_opa(&style_focus, LV_OPA_70);

#define MAKE_PAGE(cnt_name, btns, names, ext) do { \
    g_cont_##cnt_name = lv_obj_create(g_scr); \
    lv_obj_set_size(g_cont_##cnt_name, 240, 200); lv_obj_set_pos(g_cont_##cnt_name, 0, 18); \
    lv_obj_set_style_border_width(g_cont_##cnt_name, 0, 0); \
    lv_obj_set_style_pad_all(g_cont_##cnt_name, 2, 0); \
    lv_obj_set_style_bg_opa(g_cont_##cnt_name, LV_OPA_TRANSP, 0); \
    lv_obj_set_flex_flow(g_cont_##cnt_name, LV_FLEX_FLOW_COLUMN); \
    memset(btns, 0, sizeof(btns)); \
    memset(names, 0, sizeof(names)); \
    g_cnt_##cnt_name = build_list(g_cont_##cnt_name, btns, names, ext); \
} while(0)

    /* Home tab (tab 0) */
    g_cont_home = lv_obj_create(g_scr);
    lv_obj_set_size(g_cont_home, 240, 198); lv_obj_set_pos(g_cont_home, 0, 14);
    lv_obj_set_style_border_width(g_cont_home, 0, 0);
    lv_obj_set_style_pad_all(g_cont_home, 4, 0);
    lv_obj_set_style_bg_opa(g_cont_home, LV_OPA_TRANSP, 0);
    g_label_time = lv_label_create(g_cont_home);
    lv_obj_set_style_text_color(g_label_time, lv_color_white(), 0);
    lv_label_set_text(g_label_time, "--:--:--");
    g_label_date = lv_label_create(g_cont_home);
    lv_obj_set_style_text_color(g_label_date, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(g_label_date, LV_ALIGN_TOP_MID, 0, 18);
    g_label_weather = lv_label_create(g_cont_home);
    lv_obj_set_style_text_color(g_label_weather, lv_color_hex(0x88CCFF), 0);
    lv_obj_align(g_label_weather, LV_ALIGN_TOP_MID, 0, 36);

    MAKE_PAGE(img, g_btns_img, g_names_img, ".raw");
    MAKE_PAGE(txt, g_btns_txt, g_names_txt, ".txt");
    MAKE_PAGE(vid, g_btns_vid, g_names_vid, ".vid");

    /* Stream tab */
    g_cont_stream = lv_obj_create(g_scr);
    lv_obj_set_size(g_cont_stream, 240, 200); lv_obj_set_pos(g_cont_stream, 0, 18);
    lv_obj_set_style_border_width(g_cont_stream, 0, 0);
    lv_obj_set_style_bg_opa(g_cont_stream, LV_OPA_TRANSP, 0);
    g_label_stream = lv_label_create(g_cont_stream);
    lv_obj_set_style_text_color(g_label_stream, lv_color_white(), 0);
    lv_obj_center(g_label_stream);

    g_group = lv_group_get_default();
    if (!g_group) g_group = lv_group_create();
    lv_group_set_default(g_group);

    g_focus_img = 0; g_focus_txt = 0; g_focus_vid = 0;
    show_tab(0);
}
