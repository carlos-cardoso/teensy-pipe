#include <Arduino.h>
#include <array>
#include "notes.h"
#include <EEPROM.h>
#include <touchablePin.h>

// Activate debug mode (serial prints)
bool DEBUG{false};

// Helper to get size of array
template<class T, size_t N>
constexpr size_t size(T (&)[N]) { return N; }

template<class T>
inline Print &operator<<(Print &obj, T arg) {
    obj.print(arg);
    return obj;
}

touchablePin tpins[] = {23, 22, 19, 18, 4, 3, 1, 0}; //pins of teensy LC

bool ready{true};
bool loaded_eeprom{false};

// store how much each finger hole is covered from 0 to 255
std::array<int, 8> play_flags{0, 0, 0, 0, 0, 0, 0, 0};
//current midi note and pitch bend
//{note_table[note_idx], get_pitch(play_flags, note_idx)};
std::array<int, 2> current_note = {0, 0};

//store read values
int reads[] = {0, 0, 0, 0, 0, 0, 0, 0};
//low pass filtered values
double lp_reads[] = {0, 0, 0, 0, 0, 0, 0, 0};

//global max/min for each finger
uint32_t mins[] = {10000, 10000, 10000,10000,10000,10000,10000,10000};
uint32_t maxs[] = {0, 0, 0, 0, 0, 0, 0, 0};

//max/mins for each finger for each possible note
uint8_t maxxs[256][8]={0};//2048 bytes
uint8_t minns[256][8]={0};//2048 bytes


//number of consecutive samples of the same midi note
uint32_t count_same_note_idx{0};
int prev_note_idx{0};//previous note

int led_status = HIGH;
uint32_t last_loop{0}; //store last loop miliseconds

void setup() {
    // initialize note specifix max/mins
    for(int i =0 ; i<255;i++){
        for(int j=0;j<8;j++){
            minns[i][j]=255;
            maxxs[i][j]=0;
        }
    }
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, led_status);

    for (auto pin : tpins) {
        pin.initUntouched();
    }

    if (DEBUG) {
        //SerialUSB.begin(500000); //https://www.pjrc.com/teensy/td_uart.html
        SerialUSB.begin(115200); //https://www.pjrc.com/teensy/td_uart.html
        while (!SerialUSB.available()) {}
    }

    //if we want to read settings from eeprom memory
    //const auto check = (recalibrate) ? EEPROM.read(start_address) : 'r';

    led_status = (led_status == HIGH ? LOW : HIGH);
    digitalWrite(LED_BUILTIN, led_status);
}

//get_flats(play_flags, note)
//16,384
int pitchbend(float perc) {
    return round(log2(perc) * 12 * 8192 / 2);
}

constexpr int max_pitch{16384 / 2};
constexpr int semitone{16384 / 4};
constexpr int no_pitch{0};

int cs(int n) {
    if (n < 45)
        return 0;
    return (1.7/2.5) * (n - 45);
}

int pitch(int n, int n2, int n3) {
    n = cs(n);
    n2 = cs(n2);
    n3 = cs(n3);
    int out = no_pitch;
    out += (n * max_pitch) / 100;
    out += (n2 * semitone) / 200;
    out += (n3 * semitone) / 400;

    if (out >= semitone)
        out = semitone;

    return -out;
}

int get_note_idx(std::array<int, 8> _play_flags) {
    int bplay_flag = 0;

    for (int i = 0; i < 8; i++) {
        if (_play_flags[i] == 100) {
            int mask = 1;
            mask = mask << i;
            bplay_flag |= mask;
        }
    }
    return bplay_flag;
}

int get_pitch(std::array<int, 8> _play_flags, const int bplay_flag) {
    int pitch_bend0 = 0;
    int pitch_bend1 = 0;
    int pitch_bend2 = 0;

    for (int i = 7; i >= 0; --i) {
        if (bitRead(bplay_flag, i) == 0) {
            if (pitch_bend0 == 0)
                pitch_bend0 = _play_flags[i];
            else if (pitch_bend1 == 0)
                pitch_bend1 = _play_flags[i];
            else if (pitch_bend2 == 0)
                pitch_bend2 = _play_flags[i];
        }
    }
    return pitch(pitch_bend0, pitch_bend1, pitch_bend2);
}

bool read_sensors(){
    bool changed{false};
    for (int i = 0; i < size(tpins); i++) {
        reads[i] = tpins[i].isTouched_orVal();

        if (DEBUG) {
          if(i==4)
            SerialUSB << reads[i] << ",";
        }


        int new_pflag{0};
        if (reads[i] != -1) {
            if(lp_reads[i]==0)
              lp_reads[i]=reads[i];
            lp_reads[i] = 0.1 * lp_reads[i] + 0.9*reads[i];

            if (lp_reads[i] > maxs[i])
                maxs[i] = lp_reads[i];
            if (lp_reads[i] < mins[i])
                mins[i] = lp_reads[i];
            new_pflag = 0;
        } else {
            new_pflag = 100;
        }

        if (play_flags[i] != new_pflag) {
            changed = true;
            play_flags[i] = new_pflag;
        }
        if (DEBUG) {
            //SerialUSB << play_flags[i] << ",";
        }
    }
    return changed;
}


void loop() {

    bool changed = read_sensors();

    if (DEBUG)
        SerialUSB << "\nt: " <<  millis() - last_loop << "\n";

    const int note_idx = get_note_idx(play_flags);



    if(note_idx == prev_note_idx){
      count_same_note_idx += 1;
    }
    else{
        count_same_note_idx = 0;
    }

    prev_note_idx = note_idx;

    for (int i = 0; i < size(tpins); i++) {
        if (reads[i] != -1) {

            if (maxs[i] - mins[i] > 0) {
                play_flags[i] = 255.0 * static_cast<double>(lp_reads[i] - mins[i]) / static_cast<double>(maxs[i] - mins[i]);
            }
            else
                play_flags[i] = 0;
        } else {
            play_flags[i] = 255;
        }

    //update note specific max/min
        if (play_flags[i] != 255 && count_same_note_idx > 0){
            if( play_flags[i] > maxxs[note_idx][i] ){
                maxxs[note_idx][i]=play_flags[i];
                changed = true;
            }
            if( play_flags[i] < minns[note_idx][i] ){
                minns[note_idx][i] = play_flags[i];
                changed = true;
            }
        }
        if(maxxs[note_idx][i]-minns[note_idx][i] > 50)
            play_flags[i] =  255.0*static_cast<double>(play_flags[i]-minns[note_idx][i]) / static_cast<double>(maxxs[note_idx][i]-minns[note_idx][i]);

        if(DEBUG && i==1){
            SerialUSB << "play_flags:" << play_flags[i] << "\n";
            SerialUSB << "maxxs:" << maxxs[note_idx][i] << "\n";
            SerialUSB << "minns:" << minns[note_idx][i] << "\n";
        }


    if (DEBUG)
            SerialUSB << play_flags[i] << ",";
    }
    if (DEBUG) {
        SerialUSB << "\n";
        SerialUSB << "reads:";
        for (int i = 0; i < size(tpins); i++) {
            SerialUSB << lp_reads[i] << ",";
        }
        SerialUSB << "\nmaxs:";
        for (int i = 0; i < size(tpins); i++) {
            SerialUSB << maxs[i] << ",";
        }
        SerialUSB << "\nmins:";
        for (int i = 0; i < size(tpins); i++) {
            SerialUSB << mins[i] << ",";
        }
        SerialUSB << "\n";
    }


    if (ready && changed) {
        const std::array<int, 2> prev_note = current_note;
        current_note=std::array<int, 2>{note_table[note_idx], get_pitch(play_flags, note_idx)};

        if (current_note != prev_note) {
            if (!DEBUG) usbMIDI.sendNoteOff(prev_note[0], 0, 1);

            if (current_note[0] != 0) {
                if (!DEBUG) {
                    usbMIDI.sendNoteOn(current_note[0], 99, 1);
                    usbMIDI.sendPitchBend(current_note[1], 1);
                }
            }
        }
    }


    if (!DEBUG) {
        while (usbMIDI.read()) {}//avoid blocking
    }
    last_loop = millis();
}
