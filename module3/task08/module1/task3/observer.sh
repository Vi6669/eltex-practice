
#!/bin/bash
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
CONF_FILE="/home/user1/observer.conf"
LOG_FILE="/home/user1/observer.log"
if [ ! -f "$CONF_FILE" ]; then
    echo "Файл конфигурации $CONF_FILE не найден!"
    exit 1
fi

check_process_in_proc() {
    local script_path="$1"
    local script_name=$(basename "$script_path")
    
    for pid_dir in /proc/[0-9]*/; do
        if [ -r "${pid_dir}cmdline" ]; then
            local cmdline
            cmdline=$(tr '\0' ' ' < "${pid_dir}cmdline" 2>/dev/null)
            
            if [[ "$cmdline" == *"$script_name"* ]]; then
                return 0 # Процесс найден (запущен)
            fi
        fi
    done
    return 1 # Процесс не найден (выключен)
}

while IFS= read -r script_path || [ -n "$script_path" ]; do
    [[ -z "$script_path" || "$script_path" =~ ^# ]] && continue
    
    if ! check_process_in_proc "$script_path"; then
        
       nohup /bin/bash "$script_path" > /dev/null 2>&1 &
        
        echo "$(date '+%Y-%m-%d %H:%M:%S') Перезапуск скрипта: $script_path" >> "$LOG_FILE"
    fi
done < "$CONF_FILE"
