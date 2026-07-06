// pomodoro.js
#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const readline = require('readline');

const COLORS = {
    reset: '\x1b[0m',
    green: '\x1b[92m',
    red: '\x1b[91m',
    yellow: '\x1b[93m',
    blue: '\x1b[94m',
    bold: '\x1b[1m'
};

function colorize(text, color) {
    return COLORS[color] + text + COLORS.reset;
}

const CONFIG_FILE = path.join(os.homedir(), '.pomodoro.json');

function loadConfig() {
    try {
        return JSON.parse(fs.readFileSync(CONFIG_FILE, 'utf8'));
    } catch {
        return {
            work_time: 25,
            break_time: 5,
            long_break_time: 15,
            cycles_before_long: 4,
            stats: {
                completed_pomodoros: 0,
                total_work_seconds: 0,
                total_breaks: 0,
                current_streak: 0,
                best_streak: 0
            },
            state: {
                phase: 'idle',
                remaining: 0,
                paused: false,
                cycle_count: 0,
                start_time: null
            }
        };
    }
}

function saveConfig(config) {
    fs.writeFileSync(CONFIG_FILE, JSON.stringify(config, null, 2));
}

let config = loadConfig();
let running = false;
let timer = null;

function getPhaseName(phase) {
    const names = { idle: 'Ожидание', work: 'Работа', break: 'Перерыв', long_break: 'Длинный перерыв' };
    return names[phase] || phase;
}

function getPhaseColor(phase) {
    const colors = { idle: 'blue', work: 'green', break: 'yellow', long_break: 'red' };
    return colors[phase] || 'reset';
}

function formatTime(seconds) {
    const m = Math.floor(seconds / 60);
    const s = seconds % 60;
    return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
}

function playSound() {
    process.stdout.write('\x07');
}

function updateDisplay() {
    const state = config.state;
    const phase = getPhaseName(state.phase);
    const remaining = state.remaining;
    const paused = state.paused ? ' (пауза)' : '';
    process.stdout.write(`\r${colorize(phase + paused, getPhaseColor(state.phase))} | ${colorize(formatTime(remaining), 'bold')}    `);
}

function tick() {
    const state = config.state;
    if (!state.paused && state.remaining > 0) {
        state.remaining--;
        if (state.remaining === 0) {
            playSound();
            const phase = state.phase;
            if (phase === 'work') {
                config.stats.completed_pomodoros++;
                config.stats.total_work_seconds += config.work_time * 60;
                config.stats.current_streak++;
                if (config.stats.current_streak > config.stats.best_streak) {
                    config.stats.best_streak = config.stats.current_streak;
                }
                state.cycle_count++;
                if (state.cycle_count % config.cycles_before_long === 0) {
                    state.phase = 'long_break';
                    state.remaining = config.long_break_time * 60;
                } else {
                    state.phase = 'break';
                    state.remaining = config.break_time * 60;
                }
            } else if (phase === 'break' || phase === 'long_break') {
                config.stats.total_breaks++;
                state.phase = 'work';
                state.remaining = config.work_time * 60;
            }
            state.paused = false;
            saveConfig(config);
        }
    }
    updateDisplay();
}

function startTimer() {
    if (config.state.phase === 'idle') {
        config.state.phase = 'work';
        config.state.remaining = config.work_time * 60;
        config.state.paused = false;
        saveConfig(config);
    } else if (config.state.paused) {
        config.state.paused = false;
        saveConfig(config);
    } else {
        console.log('Таймер уже работает.');
        return;
    }
    if (!running) {
        running = true;
        timer = setInterval(tick, 1000);
    }
}

function pauseTimer() {
    if (config.state.phase === 'idle') {
        console.log('Таймер не запущен.');
    } else if (config.state.paused) {
        console.log('Таймер уже на паузе.');
    } else {
        config.state.paused = true;
        saveConfig(config);
        updateDisplay();
        console.log('\nТаймер приостановлен.');
    }
}

function resumeTimer() {
    if (config.state.phase === 'idle') {
        console.log('Таймер не запущен.');
    } else if (!config.state.paused) {
        console.log('Таймер не на паузе.');
    } else {
        config.state.paused = false;
        saveConfig(config);
        updateDisplay();
        console.log('\nТаймер возобновлён.');
    }
}

function stopTimer() {
    if (config.state.phase === 'idle') {
        console.log('Таймер не запущен.');
    } else {
        if (timer) clearInterval(timer);
        running = false;
        config.state.phase = 'idle';
        config.state.remaining = 0;
        config.state.paused = false;
        saveConfig(config);
        console.log('\nТаймер остановлен.');
    }
}

function showStatus() {
    if (config.state.phase === 'idle') {
        console.log('Таймер не запущен.');
    } else {
        const phase = getPhaseName(config.state.phase);
        const remaining = config.state.remaining;
        const paused = config.state.paused ? ' (пауза)' : '';
        console.log(`${colorize(phase, getPhaseColor(config.state.phase))}${paused} | Осталось: ${colorize(formatTime(remaining), 'bold')}`);
    }
}

function setParam(param, value) {
    const val = parseInt(value, 10);
    if (['work_time', 'break_time', 'long_break_time', 'cycles_before_long'].includes(param)) {
        config[param] = val;
        saveConfig(config);
        console.log(`Параметр ${param} установлен в ${value}`);
    } else {
        console.log('Неизвестный параметр.');
    }
}

function showStats() {
    const stats = config.stats;
    console.log(colorize('📊 Статистика:', 'bold'));
    console.log(`  Завершённых помидоров: ${stats.completed_pomodoros}`);
    console.log(`  Общее время работы: ${Math.floor(stats.total_work_seconds / 60)} мин`);
    console.log(`  Количество перерывов: ${stats.total_breaks}`);
    console.log(`  Текущая серия: ${stats.current_streak}`);
    console.log(`  Лучшая серия: ${stats.best_streak}`);
}

function resetStats() {
    config.stats.completed_pomodoros = 0;
    config.stats.total_work_seconds = 0;
    config.stats.total_breaks = 0;
    config.stats.current_streak = 0;
    config.stats.best_streak = 0;
    saveConfig(config);
    console.log('Статистика сброшена.');
}

function main() {
    const args = process.argv.slice(2);
    if (args.length === 0) {
        console.log('Usage: node pomodoro.js <start|pause|resume|stop|status|set|stats|reset|quit> [args...]');
        return;
    }
    const action = args[0];
    switch (action) {
        case 'start':
            startTimer();
            break;
        case 'pause':
            pauseTimer();
            break;
        case 'resume':
            resumeTimer();
            break;
        case 'stop':
            stopTimer();
            break;
        case 'status':
            showStatus();
            break;
        case 'set':
            if (args.length < 3) {
                console.log('Использование: set <param> <value>');
                return;
            }
            setParam(args[1], args[2]);
            break;
        case 'stats':
            showStats();
            break;
        case 'reset':
            resetStats();
            break;
        case 'quit':
            console.log('Выход.');
            if (timer) clearInterval(timer);
            process.exit(0);
            break;
        default:
            console.log('Неизвестная команда.');
    }
}

main();
