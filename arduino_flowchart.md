flowchart TD
    Start([Start / Power On]) --> Init[Initialize: Serial, RS485, Sensors]
    Init --> Idle{RS485 Data Available?}
    
    Idle -- Yes --> CheckCmd{Command == 'M'?}
    
    CheckCmd -- No --> Idle
    CheckCmd -- Yes --> ReadEnv[Read DHT11 Temp & Humidity]
    
    ReadEnv --> Trig[Trigger HC-SR04 Pulse]
    Trig --> Measure[Measure Echo Duration]
    Measure --> CalcSOS[Calculate Sound Speed based on Temp/Hum]
    CalcSOS --> CalcDist[Calculate Distance in cm]
    
    CalcDist --> Format[Format String: 'D:xx.xx']
    
    Format --> EnableTX[Set RS485 DE/RE = HIGH]
    EnableTX --> Send[Serial Print Data]
    Send --> WaitTx[Wait for Transmission to Finish]
    WaitTx --> DisableTX[Set RS485 DE/RE = LOW]
    
    DisableTX --> Idle
