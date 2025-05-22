# Timer + Servo Control Device

## Overview
This device lets you set a timer and control a 360° servo motor.  
It also has a debug mode for direct servo testing.

---

## **Button Functions (Normal Mode)**

- **Button 0**: Add 30 minutes to the timer
- **Button 1**: Add 1 hour to the timer
- **Button 2**: Start the timer (when time > 0)
- **Button 3**: Reset timer (hold for 3 seconds to enter debug mode)

---

## **How to Use**

1. **Setting the Timer**  
   - Press **Button 0** or **Button 1** to add time.
   - The LCD shows the total time set.

2. **Starting the Timer**  
   - Press **Button 2** (Start) to begin countdown.

3. **During Countdown**  
   - The LCD shows the remaining time.
   - Press **Button 3** (Reset) to stop and reset the timer.

4. **When Time is Up**  
   - The servo will run (e.g., to wind something).
   - After the action, the device shows a menu for further options.

5. **Post-Timer Options**
   - **Button 2**: Run the servo again (rewind)
   - **Button 3**: Reset the device

---

## **Debug Mode (Servo Test Mode)**

**To Enter Debug Mode:**  
- Hold **Button 4** for 3 seconds while in normal mode.

**Debug Controls:**  
- **Button 1**: Servo spins full speed in reverse
- **Button 2**: Servo spins full speed forward
- **Button 3**: Servo stops spinning
- **Button 4**: Exit debug mode and return to timer

*Use debug mode to check servo direction, speed, and connections.*

---

## **Serial Commands (Optional, via USB/Serial Monitor)**

| Command      | What it does                       |
|--------------|------------------------------------|
| help         | List all commands                  |
| status       | Show current status                |
| reset        | Reset timer and servo              |
| start        | Start timer                        |
| pause        | Pause timer                        |
| resume       | Resume timer                       |
| add30        | Add 30 minutes                     |
| add1h        | Add 1 hour                         |
| add10        | Add 10 seconds                     |
| sub10        | Subtract 10 seconds                |
| debug        | Toggle debug mode                  |
| servo on     | Attach servo                       |
| servo off    | Detach servo                       |

---

## **Display Messages**

- Device will show its current mode and instructions on the LCD.
- If the display dims, just press any button to wake it up.

---

## **Wiring & Power Tips**

- **Servo Pin**: Connect signal wire to pin 7.
- **Power**: Use a bread/circuit board and pull the 5 volt to the board connecting it to both the lcd and servo same with ground.
- **Buttons**: Connect your Olimex16x2 LCD shield to thr ardionu through a breadboard or circuit board.

---

## **Troubleshooting**

- If servo jitters: Check power supply or connections.
- If nothing displays: Check LCD wiring and power.
- If buttons don’t work: Check connections and button assignment in code.

---

## **About the Device**

- Designed for general-purpose timed mechanical actuation (e.g., turning, releasing, or winding something after a set time).
- Easy debug mode for quick servo direction/speed tests.

---


