// pomodoro.go
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

const (
	reset  = "\033[0m"
	green  = "\033[92m"
	red    = "\033[91m"
	yellow = "\033[93m"
	blue   = "\033[94m"
	bold   = "\033[1m"
)

func colorize(text, color string) string {
	return color + text + reset
}

type Config struct {
	WorkTime          int `json:"work_time"`
	BreakTime         int `json:"break_time"`
	LongBreakTime     int `json:"long_break_time"`
	CyclesBeforeLong  int `json:"cycles_before_long"`
	Stats             struct {
		CompletedPomodoros int `json:"completed_pomodoros"`
		TotalWorkSeconds   int `json:"total_work_seconds"`
		TotalBreaks        int `json:"total_breaks"`
		CurrentStreak      int `json:"current_streak"`
		BestStreak         int `json:"best_streak"`
	} `json:"stats"`
	State struct {
		Phase      string `json:"phase"`
		Remaining  int    `json:"remaining"`
		Paused     bool   `json:"paused"`
		CycleCount int    `json:"cycle_count"`
		StartTime  string `json:"start_time"`
	} `json:"state"`
}

var config Config
var running bool
var configFile string

func loadConfig() {
	home, _ := os.UserHomeDir()
	configFile = filepath.Join(home, ".pomodoro.json")
	data, err := os.ReadFile(configFile)
	if err != nil {
		// Defaults
		config.WorkTime = 25
		config.BreakTime = 5
		config.LongBreakTime = 15
		config.CyclesBeforeLong = 4
		config.State.Phase = "idle"
		return
	}
	json.Unmarshal(data, &config)
}

func saveConfig() {
	data, _ := json.MarshalIndent(config, "", "  ")
	os.WriteFile(configFile, data, 0644)
}

func getPhaseName(phase string) string {
	names := map[string]string{"idle": "Ожидание", "work": "Работа", "break": "Перерыв", "long_break": "Длинный перерыв"}
	return names[phase]
}

func getPhaseColor(phase string) string {
	colors := map[string]string{"idle": blue, "work": green, "break": yellow, "long_break": red}
	return colors[phase]
}

func formatTime(seconds int) string {
	return fmt.Sprintf("%02d:%02d", seconds/60, seconds%60)
}

func playSound() {
	fmt.Print("\a")
}

func updateDisplay() {
	phase := config.State.Phase
	remaining := config.State.Remaining
	paused := config.State.Paused
	disp := getPhaseName(phase)
	if paused {
		disp += " (пауза)"
	}
	fmt.Printf("\r%s | %s    ", colorize(disp, getPhaseColor(phase)), colorize(formatTime(remaining), bold))
}

func tick() {
	if !config.State.Paused && config.State.Remaining > 0 {
		config.State.Remaining--
		if config.State.Remaining == 0 {
			playSound()
			phase := config.State.Phase
			if phase == "work" {
				config.Stats.CompletedPomodoros++
				config.Stats.TotalWorkSeconds += config.WorkTime * 60
				config.Stats.CurrentStreak++
				if config.Stats.CurrentStreak > config.Stats.BestStreak {
					config.Stats.BestStreak = config.Stats.CurrentStreak
				}
				config.State.CycleCount++
				if config.State.CycleCount%config.CyclesBeforeLong == 0 {
					config.State.Phase = "long_break"
					config.State.Remaining = config.LongBreakTime * 60
				} else {
					config.State.Phase = "break"
					config.State.Remaining = config.BreakTime * 60
				}
			} else if phase == "break" || phase == "long_break" {
				config.Stats.TotalBreaks++
				config.State.Phase = "work"
				config.State.Remaining = config.WorkTime * 60
			}
			config.State.Paused = false
			saveConfig()
		}
	}
	updateDisplay()
}

func timerLoop() {
	for running {
		tick()
		time.Sleep(1 * time.Second)
	}
}

func main() {
	loadConfig()
	if len(os.Args) < 2 {
		fmt.Println("Usage: pomodoro <start|pause|resume|stop|status|set|stats|reset|quit> [args...]")
		os.Exit(1)
	}
	action := os.Args[1]

	switch action {
	case "start":
		if config.State.Phase == "idle" {
			config.State.Phase = "work"
			config.State.Remaining = config.WorkTime * 60
			config.State.Paused = false
			saveConfig()
		} else if config.State.Paused {
			config.State.Paused = false
			saveConfig()
		} else {
			fmt.Println("Таймер уже работает.")
			return
		}
		if !running {
			running = true
			go timerLoop()
		}
	case "pause":
		if config.State.Phase == "idle" {
			fmt.Println("Таймер не запущен.")
		} else if config.State.Paused {
			fmt.Println("Таймер уже на паузе.")
		} else {
			config.State.Paused = true
			saveConfig()
			updateDisplay()
			fmt.Println("\nТаймер приостановлен.")
		}
	case "resume":
		if config.State.Phase == "idle" {
			fmt.Println("Таймер не запущен.")
		} else if !config.State.Paused {
			fmt.Println("Таймер не на паузе.")
		} else {
			config.State.Paused = false
			saveConfig()
			updateDisplay()
			fmt.Println("\nТаймер возобновлён.")
		}
	case "stop":
		if config.State.Phase == "idle" {
			fmt.Println("Таймер не запущен.")
		} else {
			running = false
			config.State.Phase = "idle"
			config.State.Remaining = 0
			config.State.Paused = false
			saveConfig()
			fmt.Println("\nТаймер остановлен.")
		}
	case "status":
		if config.State.Phase == "idle" {
			fmt.Println("Таймер не запущен.")
		} else {
			phase := getPhaseName(config.State.Phase)
			remaining := config.State.Remaining
			paused := config.State.Paused
			p := ""
			if paused {
				p = " (пауза)"
			}
			fmt.Printf("%s%s | Осталось: %s\n", colorize(phase, getPhaseColor(config.State.Phase)), p, colorize(formatTime(remaining), bold))
		}
	case "set":
		if len(os.Args) < 4 {
			fmt.Println("Использование: set <param> <value>")
			return
		}
		param := os.Args[2]
		value := os.Args[3]
		val, _ := strconv.Atoi(value)
		switch param {
		case "work_time":
			config.WorkTime = val
		case "break_time":
			config.BreakTime = val
		case "long_break_time":
			config.LongBreakTime = val
		case "cycles_before_long":
			config.CyclesBeforeLong = val
		default:
			fmt.Println("Неизвестный параметр.")
			return
		}
		saveConfig()
		fmt.Printf("Параметр %s установлен в %s\n", param, value)
	case "stats":
		fmt.Println(colorize("📊 Статистика:", bold))
		fmt.Printf("  Завершённых помидоров: %d\n", config.Stats.CompletedPomodoros)
		fmt.Printf("  Общее время работы: %d мин\n", config.Stats.TotalWorkSeconds/60)
		fmt.Printf("  Количество перерывов: %d\n", config.Stats.TotalBreaks)
		fmt.Printf("  Текущая серия: %d\n", config.Stats.CurrentStreak)
		fmt.Printf("  Лучшая серия: %d\n", config.Stats.BestStreak)
	case "reset":
		config.Stats.CompletedPomodoros = 0
		config.Stats.TotalWorkSeconds = 0
		config.Stats.TotalBreaks = 0
		config.Stats.CurrentStreak = 0
		config.Stats.BestStreak = 0
		saveConfig()
		fmt.Println("Статистика сброшена.")
	case "quit":
		fmt.Println("Выход.")
		running = false
		os.Exit(0)
	default:
		fmt.Println("Неизвестная команда.")
	}
}
