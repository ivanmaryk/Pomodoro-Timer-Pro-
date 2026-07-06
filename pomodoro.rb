#!/usr/bin/env ruby
# pomodoro.rb
# encoding: UTF-8

require 'json'
require 'fileutils'
require 'time'

COLORS = {
  reset: "\e[0m",
  green: "\e[92m",
  red: "\e[91m",
  yellow: "\e[93m",
  blue: "\e[94m",
  bold: "\e[1m"
}

def colorize(text, color)
  "#{COLORS[color]}#{text}#{COLORS[:reset]}"
end

CONFIG_FILE = File.join(Dir.home, '.pomodoro.json')
$running = false
$timer_thread = nil

def load_config
  if File.exist?(CONFIG_FILE)
    JSON.parse(File.read(CONFIG_FILE))
  else
    {
      'work_time' => 25,
      'break_time' => 5,
      'long_break_time' => 15,
      'cycles_before_long' => 4,
      'stats' => {
        'completed_pomodoros' => 0,
        'total_work_seconds' => 0,
        'total_breaks' => 0,
        'current_streak' => 0,
        'best_streak' => 0
      },
      'state' => {
        'phase' => 'idle',
        'remaining' => 0,
        'paused' => false,
        'cycle_count' => 0,
        'start_time' => nil
      }
    }
  end
end

def save_config(config)
  File.write(CONFIG_FILE, JSON.pretty_generate(config))
end

$config = load_config

def phase_name(phase)
  { 'idle' => 'Ожидание', 'work' => 'Работа', 'break' => 'Перерыв', 'long_break' => 'Длинный перерыв' }[phase] || phase
end

def phase_color(phase)
  { 'idle' => 'blue', 'work' => 'green', 'break' => 'yellow', 'long_break' => 'red' }[phase] || 'reset'
end

def format_time(seconds)
  format('%02d:%02d', seconds/60, seconds%60)
end

def play_sound
  print "\a"
end

def update_display
  state = $config['state']
  phase = phase_name(state['phase'])
  phase += ' (пауза)' if state['paused']
  print "\r#{colorize(phase, phase_color(state['phase']))} | #{colorize(format_time(state['remaining']), 'bold')}    "
end

def tick
  state = $config['state']
  if !state['paused'] && state['remaining'] > 0
    state['remaining'] -= 1
    if state['remaining'] == 0
      play_sound
      phase = state['phase']
      if phase == 'work'
        $config['stats']['completed_pomodoros'] += 1
        $config['stats']['total_work_seconds'] += $config['work_time'] * 60
        $config['stats']['current_streak'] += 1
        if $config['stats']['current_streak'] > $config['stats']['best_streak']
          $config['stats']['best_streak'] = $config['stats']['current_streak']
        end
        state['cycle_count'] += 1
        if state['cycle_count'] % $config['cycles_before_long'] == 0
          state['phase'] = 'long_break'
          state['remaining'] = $config['long_break_time'] * 60
        else
          state['phase'] = 'break'
          state['remaining'] = $config['break_time'] * 60
        end
      elsif phase == 'break' || phase == 'long_break'
        $config['stats']['total_breaks'] += 1
        state['phase'] = 'work'
        state['remaining'] = $config['work_time'] * 60
      end
      state['paused'] = false
      save_config($config)
    end
  end
  update_display
end

def timer_loop
  while $running
    tick
    sleep 1
  end
end

def main
  if ARGV.empty?
    puts "Usage: pomodoro <start|pause|resume|stop|status|set|stats|reset|quit> [args...]"
    exit 1
  end
  action = ARGV[0]
  state = $config['state']

  case action
  when 'start'
    if state['phase'] == 'idle'
      state['phase'] = 'work'
      state['remaining'] = $config['work_time'] * 60
      state['paused'] = false
      save_config($config)
    elsif state['paused']
      state['paused'] = false
      save_config($config)
    else
      puts 'Таймер уже работает.'
      exit 0
    end
    unless $running
      $running = true
      $timer_thread = Thread.new { timer_loop }
    end
  when 'pause'
    if state['phase'] == 'idle'
      puts 'Таймер не запущен.'
    elsif state['paused']
      puts 'Таймер уже на паузе.'
    else
      state['paused'] = true
      save_config($config)
      update_display
      puts "\nТаймер приостановлен."
    end
  when 'resume'
    if state['phase'] == 'idle'
      puts 'Таймер не запущен.'
    elsif !state['paused']
      puts 'Таймер не на паузе.'
    else
      state['paused'] = false
      save_config($config)
      update_display
      puts "\nТаймер возобновлён."
    end
  when 'stop'
    if state['phase'] == 'idle'
      puts 'Таймер не запущен.'
    else
      $running = false
      $timer_thread.kill if $timer_thread
      state['phase'] = 'idle'
      state['remaining'] = 0
      state['paused'] = false
      save_config($config)
      puts "\nТаймер остановлен."
    end
  when 'status'
    if state['phase'] == 'idle'
      puts 'Таймер не запущен.'
    else
      phase = phase_name(state['phase'])
      phase += ' (пауза)' if state['paused']
      puts "#{colorize(phase, phase_color(state['phase']))} | Осталось: #{colorize(format_time(state['remaining']), 'bold')}"
    end
  when 'set'
    if ARGV.size < 3
      puts 'Использование: set <param> <value>'
      exit 1
    end
    param, value = ARGV[1], ARGV[2].to_i
    if ['work_time', 'break_time', 'long_break_time', 'cycles_before_long'].include?(param)
      $config[param] = value
      save_config($config)
      puts "Параметр #{param} установлен в #{value}"
    else
      puts 'Неизвестный параметр.'
    end
  when 'stats'
    stats = $config['stats']
    puts colorize('📊 Статистика:', 'bold')
    puts "  Завершённых помидоров: #{stats['completed_pomodoros']}"
    puts "  Общее время работы: #{stats['total_work_seconds'] / 60} мин"
    puts "  Количество перерывов: #{stats['total_breaks']}"
    puts "  Текущая серия: #{stats['current_streak']}"
    puts "  Лучшая серия: #{stats['best_streak']}"
  when 'reset'
    $config['stats'] = {
      'completed_pomodoros' => 0,
      'total_work_seconds' => 0,
      'total_breaks' => 0,
      'current_streak' => 0,
      'best_streak' => 0
    }
    save_config($config)
    puts 'Статистика сброшена.'
  when 'quit'
    puts 'Выход.'
    $running = false
    $timer_thread.kill if $timer_thread
    exit 0
  else
    puts 'Неизвестная команда.'
  end
end

main if __FILE__ == $0
