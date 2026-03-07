# Тесты System Monitor Server

## 📋 Описание

Этот каталог содержит тесты для System Monitor Server.

## 🚀 Запуск тестов

### C тесты (серверная часть)

```bash
# Компиляция тестов
make -f Makefile.test run

# Запуск с проверкой памяти
make -f Makefile.test valgrind

# Очистка
make -f Makefile.test clean