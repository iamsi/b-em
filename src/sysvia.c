/*B-em v2.2 by Tom Walker
  System VIA + keyboard emulation*/

#include "b-em.h"
#include "model.h"
#include "cmos.h"
#include "compactcmos.h"
#include "keyboard.h"
#include "led.h"
#include "via.h"
#include "sysvia.h"
#include "sn76489.h"
#include "video.h"

VIA sysvia;

#define KB_CAPSLOCK_FLAG 0x0400
#define KB_SCROLOCK_FLAG 0x0100

void sysvia_set_ca1(int level)
{
        via_set_ca1(&sysvia,level);
}
void sysvia_set_ca2(int level)
{
        if (OS01) level = !level; /*OS 0.1 programs CA2 to interrupt on negative edge and expects the keyboard to still work*/
        via_set_ca2(&sysvia,level);
}
void sysvia_set_cb1(int level)
{
        via_set_cb1(&sysvia,level);
}
void sysvia_set_cb2(int level)
{
        via_set_cb2(&sysvia,level);
}

void sysvia_via_set_cb2(int level)
{
        if (level && !sysvia.cb2) /*Low -> high*/
           crtc_latchpen();
}

/*Slow data bus

  Port A is the slow data bus, and is connected to

    Keyboard
    SN76489
    Speech chip (B/B+ only, not emulated)
    CMOS RAM    (Master 128 only)

  Port B bits 0-3 control the bus, and are connected on a model B to IC32, a
  74LS249 latch. This also controls screen size (for hardware scrolling) and the
  caps and scroll lock LEDs.

  This code emulates bus contention, which is entirely possible if developing
  software under emulation and inadvertently enabling multiple bus masters*/


/*Current state of IC32 output*/
uint8_t IC32=0;
/*Current effective state of the slow data bus*/
uint8_t sdbval;
/*What the System VIA is actually outputting to the slow data bus
  For use when contending with whatever else is outputting to the bus*/
static uint8_t sysvia_sdb_out;

int scrsize;
int kbdips;

/*Calculate current state of slow data bus
  B-em emulates three bus masters - the System VIA itself, the keyboard (bit 7
  only) and the CMOS RAM (Master 128 only)*/
static void sysvia_update_sdb()
{
        sdbval = sysvia_sdb_out;
        if (MASTER && !compactcmos) sdbval &= cmos_read();

        key_scan((sdbval >> 4) & 7, sdbval & 0xF);
        if (!(IC32 & 8) && !key_is_down())
            sdbval &= 0x7f;
}

static void sysvia_write_IC32(uint8_t val)
{
        if (val & 8)
           IC32 |=  (1 << (val & 7));
        else
           IC32 &= ~(1 << (val & 7));

        sysvia_update_sdb();

        scrsize = ((IC32 & 0x10) ? 2 : 0) | ((IC32 & 0x20) ? 1 : 0);

    log_debug("sysvia: IC32=%02X", IC32);
    led_update(LED_CAPS_LOCK, !(IC32 & 0x40), 0);
    led_update(LED_SHIFT_LOCK, !(IC32 & 0x80), 0);
    if (MASTER && !compactcmos)
        cmos_update(IC32, sdbval);
}

void sysvia_write_portA(uint8_t val)
{
        sysvia_sdb_out = val;

        sysvia_update_sdb();

        if (MASTER && !compactcmos) cmos_update(IC32, sdbval);
}

void sysvia_write_portB(uint8_t val)
{
        sysvia_write_IC32(val);
        /*Master 128 reuses the speech processor inputs*/
        if (MASTER && !compactcmos)
           cmos_writeaddr(val);
        /*Master Compact reuses the joystick fire inputs*/
        if (compactcmos)
           compactcmos_i2cchange(val & 0x20, val & 0x10);
}

uint8_t sysvia_read_portA()
{
        sysvia_update_sdb();

        return sdbval;
}

uint8_t sysvia_read_portB()
{
        uint8_t temp = 0xFF;
        if (compactcmos)
        {
                temp &= ~0x30;
                if (i2c_clock) temp |= 0x20;
                if (i2c_data)  temp |= 0x10;
        }
        else
        {
                temp |= 0xF0;
                if (joybutton[0]) temp &= ~0x10;
                if (joybutton[1]) temp &= ~0x20;
        }
        return temp;
}

void sysvia_write(uint16_t addr, uint8_t val)
{
//        log_debug("SYSVIA write %04X %02X\n",addr,val);
        via_write(&sysvia, addr, val);
}

uint8_t sysvia_read(uint16_t addr)
{
        uint8_t temp = via_read(&sysvia, addr);
//        log_debug("SYSVIA read  %04X %02X\n",addr,temp);
        return temp;
}

void sysvia_reset()
{
        via_reset(&sysvia);

        sysvia.read_portA = sysvia_read_portA;
        sysvia.read_portB = sysvia_read_portB;

        sysvia.write_portA = sysvia_write_portA;
        sysvia.write_portB = sysvia_write_portB;

        sysvia.set_cb2 = sysvia_via_set_cb2; /*Lightpen*/
        sysvia.timer_expire1 = key_paste_poll;

        sysvia.intnum = 1;
}


void sysvia_savestate(FILE *f)
{
        via_savestate(&sysvia, f);

        putc(IC32,f);
}

void sysvia_loadstate(FILE *f)
{
        via_loadstate(&sysvia, f);

        IC32=getc(f);
        scrsize=((IC32&16)?2:0)|((IC32&32)?1:0);
}

int sysvia_get_sn_data(uint8_t *data)
{
        *data = sdbval;

        return !(IC32 & 1);
}
