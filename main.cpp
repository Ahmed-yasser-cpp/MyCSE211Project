#include "mbed.h"

// --- Shift Register Pins ---
// These DigitalOut pins are connected to the 74HC595 shift register
DigitalOut latchPin(D4);  // ST_CP - Latch pin: updates the output when toggled
DigitalOut clockPin(D7);  // SH_CP - Clock pin: triggers data shifting
DigitalOut dataPin(D8);   // DS    - Data pin: sends bits to the register

// --- Button Inputs ---
// Buttons are active LOW, meaning they read 0 when pressed
DigitalIn s1(A1), s2(A2), s3(A3);

// --- Analog Input ---
// Potentiometer connected to analog pin A0 for voltage measurement
AnalogIn potentiometer(A0);

// --- 7-Segment Display Digit Patterns ---
// Each byte represents the pattern for a digit on a 7-segment display
// These are for a **common anode** display, so bits are inverted (0 = ON)
const uint8_t digitPattern[10] = {
    static_cast<uint8_t>(~0x3F), // 0 → a, b, c, d, e, f
    static_cast<uint8_t>(~0x06), // 1 → b, c
    static_cast<uint8_t>(~0x5B), // 2 → a, b, d, e, g
    static_cast<uint8_t>(~0x4F), // 3 → a, b, c, d, g
    static_cast<uint8_t>(~0x66), // 4 → b, c, f, g
    static_cast<uint8_t>(~0x6D), // 5 → a, c, d, f, g
    static_cast<uint8_t>(~0x7D), // 6 → a, c, d, e, f, g
    static_cast<uint8_t>(~0x07), // 7 → a, b, c
    static_cast<uint8_t>(~0x7F), // 8 → all segments
    static_cast<uint8_t>(~0x6F)  // 9 → a, b, c, d, f, g
};

// --- Digit Position Selector ---
// Determines which of the 4 digits is currently active (multiplexing)
const uint8_t digitPos[4] = {
    0x01, // Leftmost digit
    0x02, // Second digit from left
    0x04, // Third digit
    0x08  // Rightmost digit
};

// --- Time Keeping Variables ---
volatile int seconds = 0, minutes = 0; // Volatile as they are modified in an interrupt
Ticker timerTicker; // Used to call a function periodically (every 1 second)

// --- Timer Callback Function ---
// Increments time every second
void updateTime() {
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes++;
        if (minutes >= 100) minutes = 0; // Roll over after 99:59
    }
}

// --- Bit-Banging Function to Send One Byte ---
// Sends a byte to the shift register, MSB first
void shiftOutMSBFirst(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        dataPin = (value & (1 << i)) ? 1 : 0; // Output bit value
        clockPin = 1; // Pulse clock to shift in the bit
        clockPin = 0;
    }
}

// --- Send Segment and Digit Data to Shift Register ---
// Displays one digit at a time using 2-byte shift (segment + digit)
void writeToShiftRegister(uint8_t segments, uint8_t digit) {
    latchPin = 0;                  // Disable output while updating
    shiftOutMSBFirst(segments);   // Send segment pattern
    shiftOutMSBFirst(digit);      // Send digit position
    latchPin = 1;                  // Latch to display data
}

// --- Display a 4-Digit Number with Optional Decimal Point ---
// `number`: integer to display
// `showDecimalPoint`: whether to show decimal point
// `decimalPosition`: index (0-3) where decimal point should appear
void displayNumber(int number, bool showDecimalPoint = false, int decimalPosition = 1) {
    // Extract each digit
    int digits[4] = {
        (number / 1000) % 10,
        (number / 100) % 10,
        (number / 10) % 10,
        number % 10
    };
    
    for (int i = 0; i < 4; i++) {
        uint8_t segments = digitPattern[digits[i]];
        
        // Turn ON decimal point if required (bit 7 = decimal point)
        if (showDecimalPoint && i == decimalPosition) {
            segments &= ~0x80; // Clear bit 7 to enable decimal (0 = ON)
        }
        
        writeToShiftRegister(segments, digitPos[i]); // Output digit
        ThisThread::sleep_for(2ms); // Short delay for multiplexing
    }
}

// --- Main Program ---
int main() {
    // Initialize output pins to LOW
    latchPin = 0;
    clockPin = 0;
    dataPin = 0;
    
    // Enable internal pull-up resistors for buttons
    s1.mode(PullUp);
    s2.mode(PullUp);
    s3.mode(PullUp);
    
    // Attach timer interrupt to updateTime every 1 second
    timerTicker.attach(&updateTime, 10ms);
    
    while(1) {
        // --- Button S1: Reset Clock ---
        if (!s1) { // If S1 is pressed
            seconds = minutes = 0;             // Reset time
            ThisThread::sleep_for(200ms);      // Debounce delay
        }
        
        // --- Button S3: Show Voltage ---
        if (!s3) { // If S3 is pressed
            // Read analog voltage from potentiometer (0 to 3.3V)
            float voltage = potentiometer.read_u16() / 65535.0f * 3.3f;
            
            // Convert to millivolts and display (e.g., 2.75V → 2750)
            int voltageValue = (int)(voltage * 1000);
            
            // Display with decimal after first digit (e.g., 2.750)
            displayNumber(voltageValue, true, 0);
        } else {
            // --- Default Display: Show Time in MM:SS ---
            // Combine minutes and seconds into one number (e.g., 05:30 → 530)
            int timeValue = minutes * 100 + seconds;
            
            // Show decimal between minutes and seconds (e.g., 05.30)
            displayNumber(timeValue, true, 1);
        }
    }
}
