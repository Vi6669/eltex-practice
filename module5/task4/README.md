Царенкова В. А. АВТ-342

Модуль 5

Практика 4

make

<img src="./media/image1.png"
style="width:6.49653in;height:1.51944in" />

Рисунок 1 – Компиляция модуля

sudo insmod chardev.ko

lsmod \| grep chardev

sudo dmesg \| tail -n 10

cat /proc/devices \| grep chardev

<img src="./media/image2.png"
style="width:6.49653in;height:1.19792in" />

Рисунок 2 – Просмотр логов кольцевого буфера ядра

sudo chmod 666 /dev/chardev

echo "Hello Kernel, this is Eltex Student!" \> /dev/chardev

cat /dev/chardev

<img src="./media/image3.png"
style="width:6.49653in;height:0.55417in" />

Рисунок 3 – Проверка работы

sudo rmmod chardev
