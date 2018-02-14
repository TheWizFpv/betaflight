/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "platform.h"

#include "common/axis.h"
#include "common/color.h"
#include "common/maths.h"
#include "common/printf.h"

#include "drivers/nvic.h"

#include "drivers/sensor.h"
#include "drivers/system.h"
#include "drivers/dma.h"
#include "drivers/light_led.h"
#include "drivers/sound_beeper.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "drivers/serial_softserial.h"
#include "drivers/serial_uart.h"
#include "drivers/pwm_output.h"
#include "drivers/adc.h"
#include "drivers/bus_i2c.h"
#include "drivers/bus_spi.h"
#include "drivers/inverter.h"
#include "drivers/flash_m25p16.h"
#include "drivers/sonar_hcsr04.h"
#include "drivers/sdcard.h"
#include "drivers/usb_io.h"
#include "drivers/transponder_ir.h"
#include "drivers/io.h"
#include "drivers/exti.h"

#ifdef USE_BST
#include "bus_bst.h"
#endif

#include "rx/rx.h"
#include "rx/spektrum.h"

#include "io/beeper.h"
#include "io/serial.h"
#include "io/flashfs.h"
#include "io/gps.h"
#include "io/gimbal.h"
#include "io/ledstrip.h"
#include "io/asyncfatfs/asyncfatfs.h"
#include "io/transponder_ir.h"
#include "io/osd.h"

#include "scheduler/scheduler.h"

#include "sensors/sensors.h"
#include "sensors/sonar.h"
#include "sensors/barometer.h"
#include "sensors/compass.h"
#include "sensors/acceleration.h"
#include "sensors/gyro.h"
#include "sensors/battery.h"
#include "sensors/boardalignment.h"
#include "sensors/initialisation.h"

#include "telemetry/telemetry.h"
#include "blackbox/blackbox.h"

#include "flight/pid.h"
#include "flight/imu.h"
#include "flight/mixer.h"
#include "flight/failsafe.h"
#include "flight/navigation.h"


#include "fpga_drv.h"
#include "brainfpv/ir_transponder.h"
#include "brainfpv/brainfpv_osd.h"



const timerHardware_t timerHardware[USABLE_TIMER_CHANNEL_COUNT] = {
    { TIM12, IO_TAG(PB14), TIM_Channel_1, TIM_USE_PPM,   0, GPIO_AF_TIM12, NULL,  0, 0, },    // PPM_IN
    { TIM5,  IO_TAG(PA2),  TIM_Channel_3, TIM_USE_MOTOR, 1, GPIO_AF_TIM5,  DMA1_Stream0, DMA_Channel_6, DMA1_ST0_HANDLER}, // S1_OUT
    { TIM5,  IO_TAG(PA3),  TIM_Channel_4, TIM_USE_MOTOR, 1, GPIO_AF_TIM5,  DMA1_Stream1, DMA_Channel_6, DMA1_ST1_HANDLER}, // S2_OUT
    { TIM1,  IO_TAG(PA10), TIM_Channel_3, TIM_USE_MOTOR, 1, GPIO_AF_TIM1,  DMA2_Stream6, DMA_Channel_0, DMA2_ST6_HANDLER}, // S3_OUT
    { TIM2,  IO_TAG(PA15), TIM_Channel_1, TIM_USE_MOTOR, 1, GPIO_AF_TIM2,  DMA1_Stream5, DMA_Channel_3, DMA1_ST5_HANDLER}, // S4_OUT
    { TIM8,  IO_TAG(PC8),  TIM_Channel_3, TIM_USE_MOTOR, 1, GPIO_AF_TIM8,  DMA2_Stream2, DMA_Channel_0, DMA2_ST2_HANDLER}, // S5_OUT
    { TIM3,  IO_TAG(PB0),  TIM_Channel_3, TIM_USE_MOTOR, 1, GPIO_AF_TIM3,  DMA1_Stream7, DMA_Channel_5, DMA1_ST7_HANDLER}, // S6_OUT
    { TIM11, IO_TAG(PB9),  TIM_Channel_1, TIM_USE_ANY, 1,   GPIO_AF_TIM11, NULL,         NULL,          NULL},             // Camera Control
};

bool brainfpv_settings_updated = true;
bool brainfpv_settings_updated_from_cms = false;

extern bfOsdConfig_t bfOsdConfigCms;

void brainFPVUpdateSettings(void) {
    const bfOsdConfig_t * bfOsdConfigUse;

    if (brainfpv_settings_updated_from_cms)
        bfOsdConfigUse = &bfOsdConfigCms;
    else
        bfOsdConfigUse = bfOsdConfig();


    BRAINFPVFPGA_SetBwLevels(bfOsdConfigUse->black_level, bfOsdConfigUse->white_level);
    BRAINFPVFPGA_SetSyncThreshold(bfOsdConfigUse->sync_threshold);
    BRAINFPVFPGA_SetXOffset(bfOsdConfigUse->x_offset);
    BRAINFPVFPGA_SetXScale(bfOsdConfigUse->x_scale);
    BRAINFPVFPGA_Set3DConfig(bfOsdConfigUse->sbs_3d_enabled, bfOsdConfigUse->sbs_3d_right_eye_offset);

    if (bfOsdConfigUse->ir_system == 1) {
        uint8_t ir_data[6];
        ir_generate_ilap_packet(bfOsdConfigUse->ir_ilap_id, ir_data, 6);
        BRAINFPVFPGA_SetIRData(ir_data, 6);
        BRAINFPVFPGA_SetIRProtocol(BRAINFPVFPGA_IR_PROTOCOL_ILAP);
    }

    if (bfOsdConfigUse->ir_system == 2) {
        uint8_t ir_data[4];
        ir_generate_trackmate_packet(bfOsdConfigUse->ir_trackmate_id, ir_data, 6);
        BRAINFPVFPGA_SetIRData(ir_data, 4);
        BRAINFPVFPGA_SetIRProtocol(BRAINFPVFPGA_IR_PROTOCOL_TRACKMATE);
    }
    brainfpv_settings_updated_from_cms = false;
}