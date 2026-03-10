flowchart TD
    Start([Start / Power On]) --> Load[Load Configs from SPIFFS]
    Load --> Init[Init LCD, RS485, Button]
    Init --> Loop(Main Loop)
    
    Loop --> Button{Button Pressed?}
    Button -- Short Press --> Toggle[Toggle Mode: Monitor <-> Configure]
    Button -- Long Press --> Reset[Reset Configs & Reboot]
    
    Toggle --> Loop
    Reset --> Start
    
    Button -- No --> ModeCheck{Current Mode?}
    
    ModeCheck -- Configure --> WebServer[Handle Web Server / WiFi AP]
    WebServer --> Loop
    
    ModeCheck -- Monitor --> Timer{Time to Measure?}
    
    Timer -- No --> Loop
    Timer -- Yes --> SendCmd[Enable TX -> Send 'M' -> Disable TX]
    
    SendCmd --> Wait[Wait for Response with Timeout]
    
    Wait --> Resp{Response Received?}
    
    Resp -- No (Timeout) --> LCDError[Display 'Comm Timeout' on LCD]
    LCDError --> Loop
    
    Resp -- Yes --> Parse{Valid Data 'D:'?}
    
    Parse -- No (Error/Junk) --> LCDSensErr[Display 'Sensor Error' on LCD]
    LCDSensErr --> Loop
    
    Parse -- Yes --> UpdateTank[Update Tank Object with Distance]
    UpdateTank --> UpdateLCD[Calculate Vol % and Update LCD]
    UpdateLCD --> Loop
