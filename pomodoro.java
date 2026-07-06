// pomodoro.java
import java.io.*;
import java.nio.file.*;
import java.time.*;
import java.util.*;
import com.google.gson.*; // install gson

public class pomodoro {
    private static final String RESET = "\u001B[0m";
    private static final String GREEN = "\u001B[92m";
    private static final String RED = "\u001B[91m";
    private static final String YELLOW = "\u001B[93m";
    private static final String BLUE = "\u001B[94m";
    private static final String BOLD = "\u001B[1m";

    private static String colorize(String text, String color) {
        return color + text + RESET;
    }

    private static class Stats {
        int completed_pomodoros;
        int total_work_seconds;
        int total_breaks;
        int current_streak;
        int best_streak;
    }

    private static class State {
        String phase;
        int remaining;
        boolean paused;
        int cycle_count;
        String start_time;
    }

    private static class Config {
        int work_time;
        int break_time;
        int long_break_time;
        int cycles_before_long;
        Stats stats;
        State state;
    }

    private static Config config;
    private static boolean running = false;
    private static Thread timerThread;
    private static String configFile = System.getProperty("user.home") + "/.pomodoro.json";

    private static void loadConfig() throws IOException {
        Gson gson = new Gson();
        Path path = Paths.get(configFile);
        if (Files.exists(path)) {
            String json = new String(Files.readAllBytes(path));
            config = gson.fromJson(json, Config.class);
        } else {
            config = new Config();
            config.work_time = 25;
            config.break_time = 5;
            config.long_break_time = 15;
            config.cycles_before_long = 4;
            config.stats = new Stats();
            config.state = new State();
            config.state.phase = "idle";
            config.state.remaining = 0;
            config.state.paused = false;
            config.state.cycle_count = 0;
        }
    }

    private static void saveConfig() throws IOException {
        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        String json = gson.toJson(config);
        Files.write(Paths.get(configFile), json.getBytes());
    }

    private static String getPhaseName(String phase) {
        switch (phase) {
            case "idle": return "Ожидание";
            case "work": return "Работа";
            case "break": return "Перерыв";
            case "long_break": return "Длинный перерыв";
            default: return phase;
        }
    }

    private static String getPhaseColor(String phase) {
        switch (phase) {
            case "idle": return BLUE;
            case "work": return GREEN;
            case "break": return YELLOW;
            case "long_break": return RED;
            default: return RESET;
        }
    }

    private static String formatTime(int seconds) {
        return String.format("%02d:%02d", seconds / 60, seconds % 60);
    }

    private static void playSound() {
        System.out.print("\007");
    }

    private static void updateDisplay() {
        State state = config.state;
        String phase = getPhaseName(state.phase);
        if (state.paused) phase += " (пауза)";
        System.out.printf("\r%s | %s    ", colorize(phase, getPhaseColor(state.phase)), colorize(formatTime(state.remaining), BOLD));
    }

    private static void tick() throws IOException {
        State state = config.state;
        if (!state.paused && state.remaining > 0) {
            state.remaining--;
            if (state.remaining == 0) {
                playSound();
                String phase = state.phase;
                if (phase.equals("work")) {
                    config.stats.completed_pomodoros++;
                    config.stats.total_work_seconds += config.work_time * 60;
                    config.stats.current_streak++;
                    if (config.stats.current_streak > config.stats.best_streak)
                        config.stats.best_streak = config.stats.current_streak;
                    state.cycle_count++;
                    if (state.cycle_count % config.cycles_before_long == 0) {
                        state.phase = "long_break";
                        state.remaining = config.long_break_time * 60;
                    } else {
                        state.phase = "break";
                        state.remaining = config.break_time * 60;
                    }
                } else if (phase.equals("break") || phase.equals("long_break")) {
                    config.stats.total_breaks++;
                    state.phase = "work";
                    state.remaining = config.work_time * 60;
                }
                state.paused = false;
                saveConfig();
            }
        }
        updateDisplay();
    }

    private static void timerLoop() {
        while (running) {
            try {
                tick();
                Thread.sleep(1000);
            } catch (InterruptedException | IOException e) {
                break;
            }
        }
    }

    public static void main(String[] args) throws IOException {
        loadConfig();
        if (args.length == 0) {
            System.out.println("Usage: java pomodoro <start|pause|resume|stop|status|set|stats|reset|quit> [args...]");
            return;
        }
        String action = args[0];
        State state = config.state;

        switch (action) {
            case "start":
                if (state.phase.equals("idle")) {
                    state.phase = "work";
                    state.remaining = config.work_time * 60;
                    state.paused = false;
                    saveConfig();
                } else if (state.paused) {
                    state.paused = false;
                    saveConfig();
                } else {
                    System.out.println("Таймер уже работает.");
                    break;
                }
                if (!running) {
                    running = true;
                    timerThread = new Thread(() -> timerLoop());
                    timerThread.start();
                }
                break;
            case "pause":
                if (state.phase.equals("idle"))
                    System.out.println("Таймер не запущен.");
                else if (state.paused)
                    System.out.println("Таймер уже на паузе.");
                else {
                    state.paused = true;
                    saveConfig();
                    updateDisplay();
                    System.out.println("\nТаймер приостановлен.");
                }
                break;
            case "resume":
                if (state.phase.equals("idle"))
                    System.out.println("Таймер не запущен.");
                else if (!state.paused)
                    System.out.println("Таймер не на паузе.");
                else {
                    state.paused = false;
                    saveConfig();
                    updateDisplay();
                    System.out.println("\nТаймер возобновлён.");
                }
                break;
            case "stop":
                if (state.phase.equals("idle"))
                    System.out.println("Таймер не запущен.");
                else {
                    running = false;
                    if (timerThread != null) timerThread.interrupt();
                    state.phase = "idle";
                    state.remaining = 0;
                    state.paused = false;
                    saveConfig();
                    System.out.println("\nТаймер остановлен.");
                }
                break;
            case "status":
                if (state.phase.equals("idle"))
                    System.out.println("Таймер не запущен.");
                else {
                    String phase = getPhaseName(state.phase);
                    if (state.paused) phase += " (пауза)";
                    System.out.printf("%s | Осталось: %s\n", colorize(phase, getPhaseColor(state.phase)), colorize(formatTime(state.remaining), BOLD));
                }
                break;
            case "set":
                if (args.length < 3) {
                    System.out.println("Использование: set <param> <value>");
                    return;
                }
                String param = args[1];
                int val = Integer.parseInt(args[2]);
                switch (param) {
                    case "work_time": config.work_time = val; break;
                    case "break_time": config.break_time = val; break;
                    case "long_break_time": config.long_break_time = val; break;
                    case "cycles_before_long": config.cycles_before_long = val; break;
                    default: System.out.println("Неизвестный параметр."); return;
                }
                saveConfig();
                System.out.printf("Параметр %s установлен в %d\n", param, val);
                break;
            case "stats":
                Stats stats = config.stats;
                System.out.println(colorize("📊 Статистика:", BOLD));
                System.out.printf("  Завершённых помидоров: %d\n", stats.completed_pomodoros);
                System.out.printf("  Общее время работы: %d мин\n", stats.total_work_seconds / 60);
                System.out.printf("  Количество перерывов: %d\n", stats.total_breaks);
                System.out.printf("  Текущая серия: %d\n", stats.current_streak);
                System.out.printf("  Лучшая серия: %d\n", stats.best_streak);
                break;
            case "reset":
                config.stats = new Stats();
                saveConfig();
                System.out.println("Статистика сброшена.");
                break;
            case "quit":
                System.out.println("Выход.");
                running = false;
                if (timerThread != null) timerThread.interrupt();
                System.exit(0);
                break;
            default:
                System.out.println("Неизвестная команда.");
        }
    }
}
