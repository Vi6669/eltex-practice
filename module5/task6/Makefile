obj-m += nf.o

# Сборка модуля ядра
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) CC=x86_64-linux-gnu-gcc-13 modules

# Очистка директории от файлов сборки
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# --- Команды для тестирования ---

# Показать список доступных команд и их описание
help:
	@echo "Доступные команды:"
	@echo "  make load    - Загрузить скомпилированный модуль в ядро"
	@echo "  make unload  - Выгрузить модуль из ядра"
	@echo "  make block   - Добавить тестовый IP (8.8.8.8) в черный список"
	@echo "  make unblock - Удалить тестовый IP (8.8.8.8) из черного списка"
	@echo "  make list    - Вывести текущее содержимое черного списка"
	@echo "  make ping    - Проверить отправку пакетов на 8.8.8.8"
	@echo "  make log     - Вывести последние 20 строк системного лога ядра (dmesg)"

# Загрузка модуля ядра (требуются права sudo)
load:
	sudo insmod nf.ko

# Выгрузка модуля ядра (требуются права sudo)
unload:
	sudo rmmod nf || true

# Добавление тестового адреса 8.8.8.8 в черный список
block:
	echo "+8.8.8.8" | sudo tee /proc/ip_blacklist
	echo "+9.9.9.9" | sudo tee /proc/ip_blacklist

# Удаление тестового адреса 8.8.8.8 из черного списка
unblock:
	echo "-8.8.8.8" | sudo tee /proc/ip_blacklist
	echo "-9.9.9.9" | sudo tee /proc/ip_blacklist

# Чтение текущего списка заблокированных IP
list:
	cat /proc/ip_blacklist

# Тестовый ping на адрес 8.8.8.8 для проверки блокировки (таймаут 1 сек)
ping:
	ping -c 2 -W 1 8.8.8.8 || true

# Вывод последних строк лога ядра для анализа работы хука
log:
	dmesg | tail -n 20