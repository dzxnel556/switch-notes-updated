// Notes overlay for Tesla (libtesla). Read-only: browse and read the notes
// that the Switch Notes app saves in sdmc:/switch/notes/.
// Overlays can't open the system keyboard, so writing and editing stay in the app.

#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <dirent.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#define NOTES_DIR "sdmc:/switch/notes/"

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

static std::vector<std::string> list_files() {
    std::vector<std::string> v;
    if (DIR* d = opendir(NOTES_DIR)) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".txt") v.push_back(n);
        }
        closedir(d);
    }
    std::sort(v.begin(), v.end(), std::greater<std::string>());
    return v;
}

static std::string first_line(const std::string& text, size_t max = 30) {
    size_t nl = text.find('\n');
    std::string t = nl == std::string::npos ? text : text.substr(0, nl);
    if (t.size() > max) t = t.substr(0, max) + "...";
    return t.empty() ? "Empty note" : t;
}

// Word-wraps a note into short lines that fit the overlay's narrow width.
static std::vector<std::string> wrap(const std::string& text, size_t width = 34) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string para = text.substr(start, end - start);
        size_t before = out.size();
        std::string line, word;
        auto push = [&]() { if (!line.empty()) out.push_back(line); line.clear(); };
        for (size_t i = 0; i <= para.size(); i++) {
            if (i == para.size() || para[i] == ' ') {
                if (!word.empty()) {
                    while (word.size() > width) {
                        push();
                        out.push_back(word.substr(0, width));
                        word = word.substr(width);
                    }
                    if (line.empty()) line = word;
                    else if (line.size() + 1 + word.size() <= width) line += " " + word;
                    else { push(); line = word; }
                    word.clear();
                }
            } else if (para[i] != '\r') {
                word += para[i];
            }
        }
        push();
        if (out.size() == before) out.push_back(" ");
        start = end + 1;
    }
    return out;
}

class NoteGui : public tsl::Gui {
    std::string title, text;
public:
    NoteGui(std::string t, std::string x) : title(std::move(t)), text(std::move(x)) {}

    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame(title, "B to go back");
        auto list = new tsl::elm::List();
        for (auto& l : wrap(text)) list->addItem(new tsl::elm::ListItem(l));
        frame->setContent(list);
        return frame;
    }
};

class NotesGui : public tsl::Gui {
public:
    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("Notes", "A to open");
        auto list = new tsl::elm::List();
        auto files = list_files();
        if (files.empty()) list->addItem(new tsl::elm::ListItem("No notes yet"));
        for (auto& f : files) {
            std::string text = read_file(std::string(NOTES_DIR) + f);
            std::string title = first_line(text);
            auto item = new tsl::elm::ListItem(title);
            item->setClickListener([title, text](u64 keys) {
                if (keys & HidNpadButton_A) {
                    tsl::changeTo<NoteGui>(title, text);
                    return true;
                }
                return false;
            });
            list->addItem(item);
        }
        frame->setContent(list);
        return frame;
    }
};

class NotesOverlay : public tsl::Overlay {
public:
    virtual void initServices() override { fsdevMountSdmc(); }
    virtual void exitServices() override { fsdevUnmountDevice("sdmc"); }
    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<NotesGui>();
    }
};

int main(int argc, char** argv) {
    return tsl::loop<NotesOverlay>(argc, argv);
}
