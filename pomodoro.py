# pomodoro.py
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import json
import time
import threading
import argparse
from datetime import datetime

# ANSI-цвета
COLORS = {
    'reset': '\033[0m',
    'green': '\033[92m',
    'red': '\033[91m',
    'yellow': '\033[93m',
    'blue': '\033[94m',
    'bold': '\033[1m'
}

def colorize(text, color):
    return f"{COLORS.get(color, '')}{text}{COLORS['reset']}"

# Путь к файлу с настройками и статистикой
HOME = os.path.expanduser('~')
CONFIG_FILE = os.path.join(HOME, '.pomodoro.json')
DEFAULT_CONFIG = {
    'work_time': 25,      # минуты
    'break_time': 5,
    'long_break_time': 15,
    'cycles_before_long': 4,
    'stats': {
        'completed_pomodoros': 0,
        'total_work_seconds': 0,
        'total_breaks': 0,
        'current_streak': 0,
        'best_streak': 0
    },
    'state': {
        'phase': 'idle',  # idle, work, break, long_break
        'remaining': 0,
        'paused': False,
        'cycle_count': 0,
        'start_time': None
    }
}

class PomodoroTimer:
    def __init__(self):
        self.config = self.load_config()
        self.running = False
        self.thread = None
        self.lock = threading.Lock()

    def load_config(self):
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE, 'r') as f:
                return json.load(f)
        return DEFAULT_CONFIG.copy()

    def save_config(self):
        with open(CONFIG_FILE, 'w') as f:
            json.dump(self.config, f, indent=2)

    def get_phase_name(self):
        phase = self.config['state']['phase']
        names = {'idle': 'Ожидание', 'work': 'Работа', 'break': 'Перерыв', 'long_break': 'Длинный перерыв'}
        return names.get(phase, 'Неизвестно')

    def get_phase_color(self):
        phase = self.config['state']['phase']
        colors = {'idle': 'blue', 'work': 'green', 'break': 'yellow', 'long_break': 'red'}
        return colors.get(phase, 'reset')

    def format_time(self, seconds):
        m, s = divmod(seconds, 60)
        return f"{m:02d}:{s:02d}"

    def play_sound(self):
        # Системный звонок
        print('\a', end='', flush=True)

    def update_display(self):
        state = self.config['state']
        phase = self.get_phase_name()
        remaining = state['remaining']
        if state['paused']:
            phase += " (пауза)"
        sys.stdout.write(f"\r{colorize(phase, self.get_phase_color())} | {colorize(self.format_time(remaining), 'bold')}    ")
        sys.stdout.flush()

    def tick(self):
        state = self.config['state']
        if not state['paused'] and state['remaining'] > 0:
            state['remaining'] -= 1
            if state['remaining'] == 0:
                self.on_interval_end()
        self.update_display()

    def on_interval_end(self):
        self.play_sound()
        phase = self.config['state']['phase']
        stats = self.config['stats']
        if phase == 'work':
            stats['completed_pomodoros'] += 1
            stats['total_work_seconds'] += self.config['work_time'] * 60
            stats['current_streak'] += 1
            if stats['current_streak'] > stats['best_streak']:
                stats['best_streak'] = stats['current_streak']
            self.config['state']['cycle_count'] += 1
            # Переход к перерыву
            if self.config['state']['cycle_count'] % self.config['cycles_before_long'] == 0:
                self.start_phase('long_break')
            else:
                self.start_phase('break')
        elif phase == 'break' or phase == 'long_break':
            stats['total_breaks'] += 1
            self.start_phase('work')

    def start_phase(self, phase):
        state = self.config['state']
        if phase == 'work':
            state['remaining'] = self.config['work_time'] * 60
        elif phase == 'break':
            state['remaining'] = self.config['break_time'] * 60
        elif phase == 'long_break':
            state['remaining'] = self.config['long_break_time'] * 60
        else:
            return
        state['phase'] = phase
        state['paused'] = False
        state['start_time'] = datetime.now().isoformat()
        self.save_config()
        self.update_display()

    def start(self):
        if self.config['state']['phase'] == 'idle':
            self.start_phase('work')
        elif self.config['state']['paused']:
            self.resume()
        else:
            print("Таймер уже работает.")
            return
        if not self.running:
            self.running = True
            self.thread = threading.Thread(target=self._run_loop, daemon=True)
            self.thread.start()

    def _run_loop(self):
        while self.running:
            self.tick()
            time.sleep(1)

    def pause(self):
        if self.config['state']['phase'] == 'idle':
            print("Таймер не запущен.")
            return
        if self.config['state']['paused']:
            print("Таймер уже на паузе.")
            return
        self.config['state']['paused'] = True
        self.save_config()
        self.update_display()
        print("\nТаймер приостановлен.")

    def resume(self):
        if self.config['state']['phase'] == 'idle':
            print("Таймер не запущен.")
            return
        if not self.config['state']['paused']:
            print("Таймер не на паузе.")
            return
        self.config['state']['paused'] = False
        self.save_config()
        self.update_display()
        print("\nТаймер возобновлён.")

    def stop(self):
        if self.config['state']['phase'] == 'idle':
            print("Таймер не запущен.")
            return
        self.running = False
        self.config['state']['phase'] = 'idle'
        self.config['state']['remaining'] = 0
        self.config['state']['paused'] = False
        self.save_config()
        print("\nТаймер остановлен.")

    def status(self):
        state = self.config['state']
        if state['phase'] == 'idle':
            print("Таймер не запущен.")
            return
        phase = self.get_phase_name()
        remaining = state['remaining']
        paused = " (пауза)" if state['paused'] else ""
        print(f"{colorize(phase, self.get_phase_color())}{paused} | Осталось: {colorize(self.format_time(remaining), 'bold')}")

    def set_param(self, param, value):
        if param in ['work_time', 'break_time', 'long_break_time', 'cycles_before_long']:
            self.config[param] = int(value)
            self.save_config()
            print(f"Параметр {param} установлен в {value}")
        else:
            print("Неизвестный параметр.")

    def stats(self):
        stats = self.config['stats']
        print(colorize("📊 Статистика:", 'bold'))
        print(f"  Завершённых помидоров: {stats['completed_pomodoros']}")
        print(f"  Общее время работы: {stats['total_work_seconds'] // 60} мин")
        print(f"  Количество перерывов: {stats['total_breaks']}")
        print(f"  Текущая серия: {stats['current_streak']}")
        print(f"  Лучшая серия: {stats['best_streak']}")

    def reset_stats(self):
        self.config['stats'] = DEFAULT_CONFIG['stats']
        self.save_config()
        print("Статистика сброшена.")

def main():
    parser = argparse.ArgumentParser(description="Pomodoro Timer")
    parser.add_argument('action', nargs='?', default='status',
                        choices=['start', 'pause', 'resume', 'stop', 'status', 'set', 'stats', 'reset', 'quit'],
                        help='Действие')
    parser.add_argument('args', nargs='*', help='Аргументы для set')
    args = parser.parse_args()

    timer = PomodoroTimer()
    if args.action == 'start':
        timer.start()
    elif args.action == 'pause':
        timer.pause()
    elif args.action == 'resume':
        timer.resume()
    elif args.action == 'stop':
        timer.stop()
    elif args.action == 'status':
        timer.status()
    elif args.action == 'set':
        if len(args.args) >= 2:
            timer.set_param(args.args[0], args.args[1])
        else:
            print("Использование: set <param> <value>")
    elif args.action == 'stats':
        timer.stats()
    elif args.action == 'reset':
        timer.reset_stats()
    elif args.action == 'quit':
        print("Выход.")
        timer.running = False
        sys.exit(0)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\nВыход.")
        sys.exit(0)
