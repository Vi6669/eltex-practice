#!/bin/bash

PIPE="/tmp/run/cuckoo.pid"
LOG_FILE="cuckoo.log"

cleanup() {
    
    echo "$(date '+%Y-%m-%d %H:%M:%S') Shutdown!" >> "$LOG_FILE"
    
    rm -f "$PIPE"
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT

mkdir -p /tmp/run/
if [ ! -p "$PIPE" ]; then
    mkfifo "$PIPE"
fi

echo "$(date '+%Y-%m-%d %H:%M:%S') Startup!" >> "$LOG_FILE"

while true; do
  
    if read line < "$PIPE"; then
        
        if [[ "$line" =~ ^([a-zA-Z0-9_\.-]+)\[([0-9]+)\]:\ how\ much\ time\ do\ I\ have\? ]]; then
            
            NAME="${BASH_REMATCH[1]}"
            PID="${BASH_REMATCH[2]}"
            
            N=$((2 + RANDOM % 9))
            
            echo "$(date '+%Y-%m-%d %H:%M:%S') $NAME[$PID] $N" >> "$LOG_FILE"
            
            CLIENT_PIPE="/tmp/run/cuckoo_$PID"
            
            if [ -p "$CLIENT_PIPE" ]; then
                echo "$N" > "$CLIENT_PIPE" &
            fi
        fi
    fi
done


