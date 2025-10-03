import sys
from datetime import datetime

# Color codes matching utils/logger.py
COLORS = {
    'DEBUG':    '\033[94m',  # Blue
    'INFO':     '\033[92m',  # Green
    'WARNING':  '\033[93m',  # Yellow
    'ERROR':    '\033[91m',  # Red
    'CRITICAL': '\033[95m',  # Magenta
    'RESET':    '\033[0m',   # Reset color
}

def colorize(levelname, text):
    color = COLORS.get(levelname, COLORS['RESET'])
    reset = COLORS['RESET']
    return f"{color}{text}{reset}"

# Important log patterns to show
IMPORTANT_PATTERNS = [
    'starting',
    'running',
    'Config loaded',
    'Opening ipv4 listen socket',
    'Error',
    'Warning',
    'Failed',
    'Certificate',
    'SSL',
    'TLS',
    'closed its connection',
    'disconnected',
    'Will message',
    'Last will'
]

# Patterns to filter out (noise)
FILTER_OUT_PATTERNS = [
    'Received PINGREQ',
    'Sending PINGRESP',
    'Received SUBSCRIBE',
    'Sending SUBACK',
    'Received PUBLISH',
    'Sending PUBLISH',
    'QoS 0',
    'CONNACK',
    'No will message specified',
    'Sending CONNACK',
    '0 ControlDevice',  # Topic subscription details
    'ControlDevice/Uplink/+',
    'ControlDevice/Status/+',
    'New connection from',
    'New client connected'
]

def should_show_log(message):
    """Determine if log message should be shown based on importance"""
    msg_lower = message.lower()
    
    # Filter out noisy patterns first
    for pattern in FILTER_OUT_PATTERNS:
        if pattern.lower() in msg_lower:
            return False
    
    # Always show if it contains important patterns
    for pattern in IMPORTANT_PATTERNS:
        if pattern.lower() in msg_lower:
            return True
    
    # For connection events, only show disconnections and errors
    if 'connection' in msg_lower or 'client' in msg_lower:
        return 'closed' in msg_lower or 'disconnect' in msg_lower or 'error' in msg_lower
    
    # Show by default if no specific pattern matches
    return True

for line in sys.stdin:
    try:
        if ':' in line:
            ts, msg = line.split(':', 1)
            msg = msg.strip()
            
            # Skip if message should be filtered
            if not should_show_log(msg):
                continue
                
            # Convert epoch to human-readable time
            try:
                ts = int(ts.strip())
                time_str = datetime.fromtimestamp(ts).strftime('%H:%M:%S')
            except Exception:
                time_str = ts.strip()
            
            # Format fields to match logger.py
            name = 'Mosquitto'
            level = 'INFO'
            thread = '-'
            
            # Set appropriate log level based on content
            msg_lower = msg.lower()
            if any(word in msg_lower for word in ['error', 'failed', 'cannot', 'unable']):
                level = 'ERROR'
            elif any(word in msg_lower for word in ['warning', 'warn']):
                level = 'WARNING'
            elif any(word in msg_lower for word in ['connection', 'connected', 'disconnected']):
                level = 'INFO'
            
            # Colorize levelname
            level_col = colorize(level, f"{level:<7}")
            
            # Compose log line
            print(f"{time_str} | {name:<15} | {level_col} | {thread:<18}  | {msg}")
        else:
            # For lines without timestamp, check if important
            if should_show_log(line.strip()):
                print(line.strip())
    except Exception:
        print(line.strip())