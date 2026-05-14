#!/bin/bash
echo "╔══════════════════════════════════════════╗"
echo "║      System Monitor Server v2.0         ║"
echo "╚══════════════════════════════════════════╝"
echo ""

# Проверка порта
PORT=8080
if [ ! -z "$1" ]; then
    PORT=$1
fi

echo "🚀 Запуск сервера на порту $PORT..."
echo "📊 API: http://localhost:$PORT/api/system"
echo "📈 История: http://localhost:$PORT/api/history"
echo "🏥 Health: http://localhost:$PORT/api/health"
echo ""

# Запуск сервера
./monitor_server $PORT
