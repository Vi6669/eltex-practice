
#!/bin/bash
SCRIPT_NAME=$(basename "$0")
if [ "$SCRIPT_NAME" = "template_task.sh" ]; then
    echo "я бригадир, сам не работаю"
    exit 1
fi

LOG_FILE="report_${SCRIPT_NAME}.log"
PID=$$  # PID текущего запущенного процесса-клиента

echo "$(date '+%Y-%m-%d %H:%M:%S') [$PID] Скрипт запущен" >> "$LOG_FILE"

SERVER_PIPE="/tmp/run/cuckoo.pid"
CLIENT_PIPE="/tmp/run/cuckoo_$PID"

mkfifo "$CLIENT_PIPE"

echo "${SCRIPT_NAME}[$PID]: how much time do I have?" > "$SERVER_PIPE"

read N < "$CLIENT_PIPE"

rm -f "$CLIENT_PIPE"
if ! [[ "$N" =~ ^[0-9]+$ ]]; then
    N=5
fi

sleep "$N"

echo "$(date '+%Y-%m-%d %H:%M:%S') [$PID] Скрипт завершился, работал $N секунд." >> "$LOG_FILE"
