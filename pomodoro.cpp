// pomodoro.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <json/json.h> // sudo apt-get install libjsoncpp-dev
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <map>

using namespace std;

const string RESET = "\033[0m";
const string GREEN = "\033[92m";
const string RED = "\033[91m";
const string YELLOW = "\033[93m";
const string BLUE = "\033[94m";
const string BOLD = "\033[1m";

string colorize(const string& text, const string& color) {
    return color + text + RESET;
}

string getHomeDir() {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    return string(home);
}

string CONFIG_FILE = getHomeDir() + "/.pomodoro.json";
Json::Value config;
bool running = false;

void loadConfig() {
    ifstream f(CONFIG_FILE);
    if (f) {
        f >> config;
    } else {
        config["work_time"] = 25;
        config["break_time"] = 5;
        config["long_break_time"] = 15;
        config["cycles_before_long"] = 4;
        config["stats"]["completed_pomodoros"] = 0;
        config["stats"]["total_work_seconds"] = 0;
        config["stats"]["total_breaks"] = 0;
        config["stats"]["current_streak"] = 0;
        config["stats"]["best_streak"] = 0;
        config["state"]["phase"] = "idle";
        config["state"]["remaining"] = 0;
        config["state"]["paused"] = false;
        config["state"]["cycle_count"] = 0;
        config["state"]["start_time"] = "";
    }
}

void saveConfig() {
    ofstream f(CONFIG_FILE);
    f << config.toStyledString();
}

string getPhaseName(const string& phase) {
    map<string, string> names = {{"idle", "Ожидание"}, {"work", "Работа"}, {"break", "Перерыв"}, {"long_break", "Длинный перерыв"}};
    return names[phase];
}

string getPhaseColor(const string& phase) {
    map<string, string> colors = {{"idle", BLUE}, {"work", GREEN}, {"break", YELLOW}, {"long_break", RED}};
    return colors[phase];
}

string formatTime(int seconds) {
    int m = seconds / 60;
    int s = seconds % 60;
    char buf[6];
    sprintf(buf, "%02d:%02d", m, s);
    return string(buf);
}

void playSound() {
    cout << '\a' << flush;
}

void updateDisplay() {
    string phase = config["state"]["phase"].asString();
    int remaining = config["state"]["remaining"].asInt();
    bool paused = config["state"]["paused"].asBool();
    string display = getPhaseName(phase);
    if (paused) display += " (пауза)";
    cout << "\r" << colorize(display, getPhaseColor(phase)) << " | " << colorize(formatTime(remaining), BOLD) << "    " << flush;
}

void tick() {
    Json::Value& state = config["state"];
    if (!state["paused"].asBool() && state["remaining"].asInt() > 0) {
        state["remaining"] = state["remaining"].asInt() - 1;
        if (state["remaining"].asInt() == 0) {
            // Конец интервала
            playSound();
            string phase = state["phase"].asString();
            Json::Value& stats = config["stats"];
            if (phase == "work") {
                stats["completed_pomodoros"] = stats["completed_pomodoros"].asInt() + 1;
                stats["total_work_seconds"] = stats["total_work_seconds"].asInt() + config["work_time"].asInt() * 60;
                stats["current_streak"] = stats["current_streak"].asInt() + 1;
                if (stats["current_streak"].asInt() > stats["best_streak"].asInt())
                    stats["best_streak"] = stats["current_streak"];
                state["cycle_count"] = state["cycle_count"].asInt() + 1;
                if (state["cycle_count"].asInt() % config["cycles_before_long"].asInt() == 0) {
                    state["phase"] = "long_break";
                    state["remaining"] = config["long_break_time"].asInt() * 60;
                } else {
                    state["phase"] = "break";
                    state["remaining"] = config["break_time"].asInt() * 60;
                }
            } else if (phase == "break" || phase == "long_break") {
                stats["total_breaks"] = stats["total_breaks"].asInt() + 1;
                state["phase"] = "work";
                state["remaining"] = config["work_time"].asInt() * 60;
            }
            state["paused"] = false;
            saveConfig();
        }
    }
    updateDisplay();
}

void timerLoop() {
    while (running) {
        tick();
        this_thread::sleep_for(chrono::seconds(1));
    }
}

int main(int argc, char* argv[]) {
    loadConfig();
    if (argc < 2) {
        cout << "Usage: pomodoro <start|pause|resume|stop|status|set|stats|reset|quit> [args...]" << endl;
        return 1;
    }
    string action = argv[1];
    Json::Value& state = config["state"];

    if (action == "start") {
        if (state["phase"].asString() == "idle") {
            state["phase"] = "work";
            state["remaining"] = config["work_time"].asInt() * 60;
            state["paused"] = false;
            saveConfig();
        } else if (state["paused"].asBool()) {
            state["paused"] = false;
            saveConfig();
        } else {
            cout << "Таймер уже работает." << endl;
            return 0;
        }
        if (!running) {
            running = true;
            thread timerThread(timerLoop);
            timerThread.detach();
        }
    } else if (action == "pause") {
        if (state["phase"].asString() == "idle") {
            cout << "Таймер не запущен." << endl;
        } else if (state["paused"].asBool()) {
            cout << "Таймер уже на паузе." << endl;
        } else {
            state["paused"] = true;
            saveConfig();
            updateDisplay();
            cout << endl << "Таймер приостановлен." << endl;
        }
    } else if (action == "resume") {
        if (state["phase"].asString() == "idle") {
            cout << "Таймер не запущен." << endl;
        } else if (!state["paused"].asBool()) {
            cout << "Таймер не на паузе." << endl;
        } else {
            state["paused"] = false;
            saveConfig();
            updateDisplay();
            cout << endl << "Таймер возобновлён." << endl;
        }
    } else if (action == "stop") {
        if (state["phase"].asString() == "idle") {
            cout << "Таймер не запущен." << endl;
        } else {
            running = false;
            state["phase"] = "idle";
            state["remaining"] = 0;
            state["paused"] = false;
            saveConfig();
            cout << endl << "Таймер остановлен." << endl;
        }
    } else if (action == "status") {
        if (state["phase"].asString() == "idle") {
            cout << "Таймер не запущен." << endl;
        } else {
            string phase = getPhaseName(state["phase"].asString());
            int remaining = state["remaining"].asInt();
            bool paused = state["paused"].asBool();
            string p = paused ? " (пауза)" : "";
            cout << colorize(phase, getPhaseColor(state["phase"].asString())) << p << " | Осталось: " << colorize(formatTime(remaining), BOLD) << endl;
        }
    } else if (action == "set") {
        if (argc < 4) {
            cout << "Использование: set <param> <value>" << endl;
            return 1;
        }
        string param = argv[2];
        string value = argv[3];
        if (param == "work_time" || param == "break_time" || param == "long_break_time" || param == "cycles_before_long") {
            config[param] = stoi(value);
            saveConfig();
            cout << "Параметр " << param << " установлен в " << value << endl;
        } else {
            cout << "Неизвестный параметр." << endl;
        }
    } else if (action == "stats") {
        Json::Value stats = config["stats"];
        cout << colorize("📊 Статистика:", BOLD) << endl;
        cout << "  Завершённых помидоров: " << stats["completed_pomodoros"].asInt() << endl;
        cout << "  Общее время работы: " << stats["total_work_seconds"].asInt() / 60 << " мин" << endl;
        cout << "  Количество перерывов: " << stats["total_breaks"].asInt() << endl;
        cout << "  Текущая серия: " << stats["current_streak"].asInt() << endl;
        cout << "  Лучшая серия: " << stats["best_streak"].asInt() << endl;
    } else if (action == "reset") {
        config["stats"]["completed_pomodoros"] = 0;
        config["stats"]["total_work_seconds"] = 0;
        config["stats"]["total_breaks"] = 0;
        config["stats"]["current_streak"] = 0;
        config["stats"]["best_streak"] = 0;
        saveConfig();
        cout << "Статистика сброшена." << endl;
    } else if (action == "quit") {
        cout << "Выход." << endl;
        running = false;
        return 0;
    } else {
        cout << "Неизвестная команда." << endl;
    }
    return 0;
}
