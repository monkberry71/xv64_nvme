#include <stdint.h>
#include <kernel/x86_64.h>
#include <kernel/kbd.h>
#include <kernel/ioapic.h>
#include <kernel/intr.h>
#include <kernel/cpu.h>
#include <kernel/console.h>

// io ports
#define PS2_CONTROL_REG 0x64
#define PS2_STATUS_REG 0x64
#define KBD_W_REG 0x60
#define KBD_R_REG 0x60

// sc
#define SHIFT_L_PRESS   0x2A
#define SHIFT_R_PRESS   0x36
#define SHIFT_L_RELEASE 0xAA
#define SHIFT_R_RELEASE 0xB6

// Check whether there is a data in the ps2 output buf
// bit0 is 1 == kbd output buf has something
static inline int is_output_buf_full(void) {
    return inb(PS2_STATUS_REG) & 0x01;
}

// Check whether there is a data in the ps2 input buf, which means kbd havent fetch it yet
static inline int is_input_buf_full(void) {
    return inb(PS2_STATUS_REG) & 0x02;
}

int activate_kbd(void) {
    while(is_input_buf_full()); // wait until the ps2 input buf is empty
    outb(PS2_CONTROL_REG, 0xAE); // Tell PS2 controller : enable the kbd port

    while(is_input_buf_full()); // wait until the ps2 input buf is empty
    outb(KBD_W_REG, 0xF4); // Tell kbd : start scanning and sending scancode

    for(int i=0; i<100; i++) {
        // we want ACK, but other key can come, so we get 100 keys
        int timeout = 0xFFFF;
        while(!is_output_buf_full() && timeout--); 
        if(inb(KBD_R_REG) == 0xFA) return 0;
    }

    return -1;
}

// uint8_t get_scancode(void) {
//     while(!is_output_buf_full());
//     return inb(KBD_R_REG);
// }

// int
// kbdgetc(void)
// {
//   static uint shift;
//   static uchar *charcode[4] = {
//     normalmap, shiftmap, ctlmap, ctlmap
//   };
//   uint st, data, c;

//   st = inb(KBSTATP);
//   if((st & KBS_DIB) == 0)
//     return -1;
//   data = inb(KBDATAP);

//   if(data == 0xE0){
//     shift |= E0ESC;
//     return 0;
//   } else if(data & 0x80){
//     // Key released
//     data = (shift & E0ESC ? data : data & 0x7F);
//     shift &= ~(shiftcode[data] | E0ESC);
//     return 0;
//   } else if(shift & E0ESC){
//     // Last character was an E0 escape; or with 0x80
//     data |= 0x80;
//     shift &= ~E0ESC;
//   }

//   shift |= shiftcode[data];
//   shift ^= togglecode[data];
//   c = charcode[shift & (CTL | SHIFT)][data];
//   if(shift & CAPSLOCK){
//     if('a' <= c && c <= 'z')
//       c += 'A' - 'a';
//     else if('A' <= c && c <= 'Z')
//       c += 'a' - 'A';
//   }
//   return c;
// }

int kbd_getc(void) {
    static uint32_t kbd_status;
    // kbd_status 0 -->
    // SHIFT PRESSED | CTRL PRESSED | ALT_PRESSED | CAPS_LOCk_ON | NUM LOCK_ON | scroll lock on
    static uint8_t *char_code[4] = {
        normalmap, shiftmap, ctlmap, ctlmap
    };
    if(!is_output_buf_full()) return -1;
    uint8_t sc = inb(KBD_R_REG);
    
    if(sc == 0xE0) {
        // second sc coming through
        kbd_status |= E0ESC;
        return 0;
    } else if(sc & 0x80) {
        // sc 1000 0000 <<< released
        // this code is frustratingly confusing, i will give my best shot
        // ctrl press 0x1d, r-ctrl press e0 1d
        // ctrl rel   0x9d, r-ctrl rel   e0 9d
        // so rel codes will be here anyway, and if it was E0, (r-ctrl), it will keep as 9d, because xv6 uses 9d as a r-ctrl when indexing maps
        sc = (kbd_status & E0ESC ? sc : sc & 0x7F); 
        kbd_status &= ~(shiftcode[sc] | E0ESC); // off bit E0ESC and bit (CTRL or SHIFT or ALT)
        return 0;
    } else if(kbd_status & E0ESC) {
        // e0 escape
        sc |= 0x80; // yes, it will use 0b1000 0000 ORed as a indexing as I said
        kbd_status &= ~E0ESC;
    }

    kbd_status |= shiftcode[sc]; // change sc to bit index of kdb_status, 0x1d -> CTL(bit1)
    kbd_status ^= togglecode[sc]; // 
    uint8_t c = char_code[kbd_status & (CTL | SHIFT)][sc]; 
    // 10 11 -> ctrl pressed -> ctlmap anyway, 01 -> shift only pressed, not ctrl -> shiftmap, 00 -> none pressed -> normalmap
    if(kbd_status & CAPSLOCK) {
        //toggle
        if('a' <= c && c <= 'z') c = c + - 'a' + 'A' ; // make it upper
        else if('A' <= c && c <= 'Z') c = c  - 'A' + 'a'; // make it lower
    }
    return c;
}

int kbd_intr(void) {
    // // one sc -> this func exec
    // uint8_t sc = get_scancode();

    // switch(sc) {
    //     case SHIFT_L_PRESS:
    //     case SHIFT_R_PRESS:
    //         kbd_status.shift_down = 1;
    //         return 0;
        
    //     case SHIFT_L_RELEASE:
    //     case SHIFT_R_RELEASE:
    //         kbd_status.shift_down = 0;
    //         return 0;
    // }

    // if(sc & 0x80) return 0; // ignore release

    // char c = normal_map[sc];
    // if(c == 0) return 0; // wtf

    // if(kbd_status.shift_down && c >= 'a' && c <= 'z') c = c - 'a' + 'A';

    // enqueue(&ascii_q, c);
    // return 0;
    console_intr();
    return 0;
}

void kbd_init(void) {
    // ring_buf_init(&ascii_q, "ascii_q"); 
    activate_kbd();
    ioapic_enable(IRQ_KBD, mycpu()->lapic_id);
}
