obj-m += nl_kernel.o

# Определяем переменную для компилятора
CC_COMPILER = x86_64-linux-gnu-gcc-13

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) CC=$(CC_COMPILER) modules
	$(CC_COMPILER) nl_user.c -o nl_user

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f nl_user



load:
	@echo "--- Загрузка модуля ядра ---"
	sudo insmod nl_kernel.ko

unload:
	@echo "--- Выгрузка модуля ядра ---"
	sudo rmmod nl_kernel

log-clear:
	@echo "--- Очистка логов ядра dmesg ---"
	sudo dmesg -C

log:
	@echo "--- Вывод логов dmesg (последние 15 строк) ---"
	sudo dmesg | tail -n 15

run:
	@echo "--- Запуск пользовательского приложения ---"
	./nl_user