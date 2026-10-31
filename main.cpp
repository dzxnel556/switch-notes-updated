// Switch Notes - rounded UI with a Settings tab and themes (libnx + SDL2 + SDL2_ttf)
// Notes are plain .txt files in sdmc:/switch/notes/. Settings: sdmc:/config/switch-notes/settings.ini
// Font: the Switch's built-in sans font, or sdmc:/config/switch-notes/font.ttf if you add one.
//
// L / R switch tab.
// Notes tab:    Up/Down select | A edit | X new | Y delete (press twice) | ZL/ZR or right stick scroll
// Settings tab: Up/Down choose a theme (applies instantly)
// + quits

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <ctime>
#include <string>
#include <vector>

#define NOTES_DIR     "sdmc:/switch/notes/"
#define CONFIG_DIR    "sdmc:/config/switch-notes/"
#define SETTINGS_FILE CONFIG_DIR "settings.ini"
#define FONT_PATH     CONFIG_DIR "font.ttf"
#define MAX_CHARS     1000

struct Theme {
    const char* name;
    SDL_Color bg, card, sel, text, sub, selText, selSub, div, red;
};

static const Theme THEMES[] = {
    {"Light",  {244, 245, 247, 255}, {255, 255, 255, 255}, { 79, 124, 255, 255}, { 31,  36,  48, 255}, {120, 128, 144, 255},
               {255, 255, 255, 255}, {214, 226, 255, 255}, {230, 232, 236, 255}, {214,  64,  64, 255}},
    {"Dark",   { 18,  19,  24, 255}, { 30,  32,  40, 255}, { 79, 124, 255, 255}, {236, 238, 243, 255}, {140, 148, 165, 255},
               {255, 255, 255, 255}, {214, 226, 255, 255}, { 50,  53,  64, 255}, {240, 100, 100, 255}},
    {"Amber",  {253, 248, 236, 255}, {255, 255, 255, 255}, {245, 180,   0, 255}, { 50,  40,  20, 255}, {140, 125,  95, 255},
               { 40,  30,   0, 255}, {110,  80,   0, 255}, {240, 230, 205, 255}, {200,  60,  40, 255}},
    {"Forest", {238, 245, 240, 255}, {255, 255, 255, 255}, { 46, 125,  80, 255}, { 24,  40,  30, 255}, {100, 125, 110, 255},
               {255, 255, 255, 255}, {200, 230, 212, 255}, {218, 230, 222, 255}, {200,  60,  60, 255}},
};
static const int THEME_COUNT = (int)(sizeof(THEMES) / sizeof(THEMES[0]));

struct Note {
    std::string file, title, text, stamp;
};

static std::string read_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    std::string s;
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

static bool write_file(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return true;
}

static int load_theme() {
    std::string s = read_file(SETTINGS_FILE);
    size_t p = s.find("theme=");
    if (p == std::string::npos) return 0;
    int v = atoi(s.c_str() + p + 6);
    return v >= 0 && v < THEME_COUNT ? v : 0;
}

static void save_theme(int idx) {
    write_file(SETTINGS_FILE, "theme=" + std::to_string(idx) + "\n");
}

static std::vector<Note> load_notes() {
    std::vector<std::string> names;
    if (DIR* d = opendir(NOTES_DIR)) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".txt") names.push_back(n);
        }
        closedir(d);
    }
    std::sort(names.begin(), names.end(), std::greater<std::string>());
    std::vector<Note> out;
    for (auto& n : names) {
        Note note;
        note.file = n;
        note.text = read_file(std::string(NOTES_DIR) + n);
        size_t nl = note.text.find('\n');
        note.title = nl == std::string::npos ? note.text : note.text.substr(0, nl);
        if (note.title.size() > 200) note.title.resize(200);
        if (note.title.empty()) note.title = "Empty note";
        long long t = 0;
        if (sscanf(n.c_str(), "note_%lld.txt", &t) == 1 && t > 0) {
            time_t tt = (time_t)t;
            char buf[48];
            strftime(buf, sizeof buf, "%d %b %Y, %H:%M", localtime(&tt));
            note.stamp = buf;
        }
        out.push_back(note);
    }
    return out;
}

static bool edit_note(const std::string& path, const std::string& initial) {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetGuideText(&kbd, "Write your note");
    swkbdConfigSetReturnButtonFlag(&kbd, 1);
    swkbdConfigSetStringLenMax(&kbd, MAX_CHARS);
    if (!initial.empty()) swkbdConfigSetInitialText(&kbd, initial.c_str());
    static char buf[4096];
    memset(buf, 0, sizeof buf);
    Result rc = swkbdShow(&kbd, buf, sizeof buf);
    swkbdClose(&kbd);
    if (R_FAILED(rc)) return false;
    return write_file(path, buf);
}

static void rrect(SDL_Renderer* r, int x, int y, int w, int h, int rad, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    if (rad * 2 > h) rad = h / 2;
    if (rad * 2 > w) rad = w / 2;
    for (int i = 0; i < h; i++) {
        int row = i < h - 1 - i ? i : h - 1 - i;
        int inset = 0;
        if (row < rad) {
            float dy = rad - (row + 0.5f);
            float dx = sqrtf((float)(rad * rad) - dy * dy);
            inset = (int)lroundf(rad - dx);
        }
        SDL_RenderDrawLine(r, x + inset, y + i, x + w - 1 - inset, y + i);
    }
}

static int text(SDL_Renderer* r, TTF_Font* f, const std::string& s, int x, int y, SDL_Color c, int wrap = 0) {
    if (s.empty()) return 0;
    SDL_Surface* surf = wrap > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, s.c_str(), c, wrap)
                                 : TTF_RenderUTF8_Blended(f, s.c_str(), c);
    if (!surf) return 0;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    int h = surf->h;
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
    return h;
}

static std::string fit(TTF_Font* f, std::string s, int maxw) {
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s.c_str(), &w, &h);
    if (w <= maxw) return s;
    while (!s.empty()) {
        while (!s.empty() && (s.back() & 0xC0) == 0x80) s.pop_back();
        if (!s.empty()) s.pop_back();
        std::string t = s + "...";
        TTF_SizeUTF8(f, t.c_str(), &w, &h);
        if (w <= maxw) return t;
    }
    return "";
}

static TTF_Font* open_font(PlFontData& shared, bool use_shared, int size) {
    if (!use_shared) return TTF_OpenFont(FONT_PATH, size);
    return TTF_OpenFontRW(SDL_RWFromMem(shared.address, shared.size), 1, size);
}

int main() {
    plInitialize(PlServiceType_User);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("Notes", 0, 0, 1280, 720, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, 1280, 720);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    mkdir("sdmc:/switch", 0777);
    mkdir(NOTES_DIR, 0777);
    mkdir("sdmc:/config", 0777);
    mkdir(CONFIG_DIR, 0777);

    PlFontData shared;
    bool use_shared = true;
    if (FILE* test = fopen(FONT_PATH, "rb")) { fclose(test); use_shared = false; }
    if (use_shared) plGetSharedFontByType(&shared, PlSharedFontType_Standard);

    TTF_Font* fTab   = open_font(shared, use_shared, 26);
    TTF_Font* fTitle = open_font(shared, use_shared, 24);
    TTF_Font* fBody  = open_font(shared, use_shared, 22);
    TTF_Font* fSmall = open_font(shared, use_shared, 18);
    if (!fTab || !fTitle || !fBody || !fSmall) {
        SDL_Quit();
        return 1;
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    int themeIdx = load_theme();
    int tab = 0;  // 0 = notes, 1 = settings
    std::vector<Note> notes = load_notes();
    int sel = 0, top = 0, bodyH = 0;
    float scroll = 0;
    bool confirm = false;
    const int ROWS = 6, CARD_H = 76, CARD_GAP = 12;

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        u64 held = padGetButtons(&pad);
        if (down & HidNpadButton_Plus) break;

        if ((down & HidNpadButton_L) && tab != 0) { tab = 0; confirm = false; }
        if ((down & HidNpadButton_R) && tab != 1) { tab = 1; confirm = false; }

        if (tab == 0) {
            int count = (int)notes.size();
            if ((down & HidNpadButton_Up) && sel > 0) { sel--; scroll = 0; confirm = false; }
            if ((down & HidNpadButton_Down) && sel + 1 < count) { sel++; scroll = 0; confirm = false; }
            if (held & HidNpadButton_ZL) scroll -= 10;
            if (held & HidNpadButton_ZR) scroll += 10;
            HidAnalogStickState rs = padGetStickPos(&pad, 1);
            if (abs(rs.y) > 4000) scroll -= (rs.y / 32767.0f) * 14.0f;

            if (down & HidNpadButton_X) {
                char name[64];
                snprintf(name, sizeof name, "note_%lld.txt", (long long)time(NULL));
                if (edit_note(std::string(NOTES_DIR) + name, "")) { notes = load_notes(); sel = 0; scroll = 0; }
                confirm = false;
            }
            if ((down & HidNpadButton_A) && count > 0) {
                std::string path = std::string(NOTES_DIR) + notes[sel].file;
                if (edit_note(path, notes[sel].text)) notes = load_notes();
                confirm = false;
            }
            if ((down & HidNpadButton_Y) && count > 0) {
                if (confirm) {
                    remove((std::string(NOTES_DIR) + notes[sel].file).c_str());
                    notes = load_notes();
                    if (sel >= (int)notes.size()) sel = (int)notes.size() - 1;
                    if (sel < 0) sel = 0;
                    scroll = 0;
                    confirm = false;
                } else confirm = true;
            }
            if ((down & HidNpadButton_B) && confirm) confirm = false;
        } else {
            int prev = themeIdx;
            if ((down & HidNpadButton_Up) && themeIdx > 0) themeIdx--;
            if ((down & HidNpadButton_Down) && themeIdx + 1 < THEME_COUNT) themeIdx++;
            if (themeIdx != prev) save_theme(themeIdx);
        }

        int count = (int)notes.size();
        if (sel < top) top = sel;
        if (sel >= top + ROWS) top = sel - ROWS + 1;
        float maxScroll = bodyH > 400 ? (float)(bodyH - 400) : 0;
        if (scroll < 0) scroll = 0;
        if (scroll > maxScroll) scroll = maxScroll;

        const Theme& t = THEMES[themeIdx];
        SDL_SetRenderDrawColor(ren, t.bg.r, t.bg.g, t.bg.b, 255);
        SDL_RenderClear(ren);

        const char* tabNames[2] = {"Notes", "Settings"};
        int tx = 40;
        for (int i = 0; i < 2; i++) {
            int w = 0, h = 0;
            TTF_SizeUTF8(fTab, tabNames[i], &w, &h);
            int pw = w + 44;
            if (i == tab) {
                rrect(ren, tx, 22, pw, 50, 25, t.sel);
                text(ren, fTab, tabNames[i], tx + 22, 22 + (50 - h) / 2, t.selText);
            } else {
                text(ren, fTab, tabNames[i], tx + 22, 22 + (50 - h) / 2, t.sub);
            }
            tx += pw + 8;
        }

        if (tab == 0) {
            for (int i = top; i < count && i < top + ROWS; i++) {
                int y = 100 + (i - top) * (CARD_H + CARD_GAP);
                bool on = i == sel;
                rrect(ren, 40, y, 480, CARD_H, 20, on ? t.sel : t.card);
                text(ren, fTitle, fit(fTitle, notes[i].title, 430), 64, y + 10, on ? t.selText : t.text);
                text(ren, fSmall, notes[i].stamp, 64, y + 44, on ? t.selSub : t.sub);
            }

            rrect(ren, 548, 100, 692, 540, 26, t.card);
            if (count == 0) {
                text(ren, fTitle, "No notes yet", 580, 130, t.text);
                text(ren, fSmall, "Press X to write your first one.", 580, 170, t.sub);
                bodyH = 0;
            } else {
                const Note& n = notes[sel];
                text(ren, fTitle, fit(fTitle, n.title, 628), 580, 126, t.text);
                text(ren, fSmall, n.stamp, 580, 162, t.sub);
                rrect(ren, 580, 196, 628, 2, 1, t.div);
                SDL_Rect clip = {580, 212, 628, 400};
                SDL_RenderSetClipRect(ren, &clip);
                bodyH = text(ren, fBody, n.text, 580, 212 - (int)scroll, t.text, 628);
                SDL_RenderSetClipRect(ren, NULL);
            }

            if (confirm) text(ren, fSmall, "Press Y again to delete this note, or B to cancel", 40, 668, t.red);
            else text(ren, fSmall, "A  Edit     X  New     Y  Delete     ZL / ZR  Scroll     L / R  Tabs     +  Quit", 40, 668, t.sub);
        } else {
            for (int i = 0; i < THEME_COUNT; i++) {
                int y = 100 + i * (CARD_H + CARD_GAP);
                bool on = i == themeIdx;
                if (on) {
                    rrect(ren, 40, y, 480, CARD_H, 20, t.sel);
                    rrect(ren, 43, y + 3, 474, CARD_H - 6, 17, t.card);
                } else {
                    rrect(ren, 40, y, 480, CARD_H, 20, t.card);
                }
                rrect(ren, 64, y + 18, 40, 40, 12, THEMES[i].sel);
                text(ren, fTitle, THEMES[i].name, 124, y + 24, t.text);
                if (on) text(ren, fSmall, "In use", 430, y + 28, t.sub);
            }

            rrect(ren, 548, 100, 692, 540, 26, t.card);
            text(ren, fSmall, "Preview", 580, 126, t.sub);
            rrect(ren, 580, 160, 628, 330, 22, t.bg);
            rrect(ren, 604, 184, 580, 76, 20, t.sel);
            text(ren, fTitle, "Shopping list", 628, 194, t.selText);
            text(ren, fSmall, "31 Oct 2026, 14:30", 628, 228, t.selSub);
            rrect(ren, 604, 272, 580, 76, 20, t.card);
            text(ren, fTitle, "Meeting notes", 628, 282, t.text);
            text(ren, fSmall, "30 Oct 2026, 09:12", 628, 316, t.sub);
            rrect(ren, 604, 360, 580, 76, 20, t.card);
            text(ren, fTitle, "Recipe: pancakes", 628, 370, t.text);
            text(ren, fSmall, "28 Oct 2026, 18:45", 628, 404, t.sub);
            text(ren, fSmall, "Themes apply instantly and are remembered.", 580, 510, t.sub);

            text(ren, fSmall, "Up / Down  Choose theme     L / R  Tabs     +  Quit", 40, 668, t.sub);
        }

        SDL_RenderPresent(ren);
    }

    TTF_CloseFont(fTab);
    TTF_CloseFont(fTitle);
    TTF_CloseFont(fBody);
    TTF_CloseFont(fSmall);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    plExit();
    return 0;
}
