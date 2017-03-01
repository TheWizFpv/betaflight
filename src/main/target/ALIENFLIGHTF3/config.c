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

#include <platform.h>

#ifdef TARGET_CONFIG
#include "common/axis.h"

#include "drivers/compass.h"
#include "drivers/io.h"
#include "drivers/pwm_esc_detect.h"
#include "drivers/pwm_output.h"
#include "drivers/sensor.h"

#include "fc/rc_controls.h"

#include "flight/failsafe.h"
#include "flight/mixer.h"
#include "flight/pid.h"

#include "rx/rx.h"

#include "sensors/sensors.h"
#include "sensors/compass.h"

#include "config/config_profile.h"
#include "config/config_master.h"

#include "hardware_revision.h"

#define BRUSHED_MOTORS_PWM_RATE 32000           // 32kHz

// alternative defaults settings for AlienFlight targets
void targetConfiguration(master_t *config)
{
    /* depending on revision ... depends on the LEDs to be utilised. */
    if (hardwareRevision == AFF3_REV_2) {
        config->statusLedConfig.polarity = 0
#ifdef LED0_A_INVERTED
            | BIT(0)
#endif
#ifdef LED1_A_INVERTED
            | BIT(1)
#endif
#ifdef LED2_A_INVERTED
            | BIT(2)
#endif
            ;

        for (int i = 0; i < LED_NUMBER; i++) {
            config->statusLedConfig.ledTags[i] = IO_TAG_NONE;
        }
#ifdef LED0_A
        config->statusLedConfig.ledTags[0] = IO_TAG(LED0_A);
#endif
#ifdef LED1_A
        config->statusLedConfig.ledTags[1] = IO_TAG(LED1_A);
#endif
#ifdef LED2_A
        config->statusLedConfig.ledTags[2] = IO_TAG(LED2_A);
#endif
    } else {
        config->gyroConfig.gyro_sync_denom = 2;
        config->pidConfig.pid_process_denom = 2;
    }

    config->rxConfig.spektrum_sat_bind = 5;
    config->rxConfig.spektrum_sat_bind_autoreset = 1;
    config->compassConfig.mag_hardware = MAG_NONE;            // disabled by default

    if (hardwareMotorType == MOTOR_BRUSHED) {
        config->motorConfig.motorPwmRate = BRUSHED_MOTORS_PWM_RATE;
        config->pidConfig.pid_process_denom = 1;
    }

    config->profile[0].pidProfile.P8[ROLL] = 90;
    config->profile[0].pidProfile.I8[ROLL] = 44;
    config->profile[0].pidProfile.D8[ROLL] = 60;
    config->profile[0].pidProfile.P8[PITCH] = 90;
    config->profile[0].pidProfile.I8[PITCH] = 44;
    config->profile[0].pidProfile.D8[PITCH] = 60;

    config->customMotorMixer[0] = (motorMixer_t){ 1.0f, -0.414178f,  1.0f, -1.0f };    // REAR_R
    config->customMotorMixer[1] = (motorMixer_t){ 1.0f, -0.414178f, -1.0f,  1.0f };    // FRONT_R
    config->customMotorMixer[2] = (motorMixer_t){ 1.0f,  0.414178f,  1.0f,  1.0f };    // REAR_L
    config->customMotorMixer[3] = (motorMixer_t){ 1.0f,  0.414178f, -1.0f, -1.0f };    // FRONT_L
    config->customMotorMixer[4] = (motorMixer_t){ 1.0f, -1.0f, -0.414178f, -1.0f };    // MIDFRONT_R
    config->customMotorMixer[5] = (motorMixer_t){ 1.0f,  1.0f, -0.414178f,  1.0f };    // MIDFRONT_L
    config->customMotorMixer[6] = (motorMixer_t){ 1.0f, -1.0f,  0.414178f,  1.0f };    // MIDREAR_R
    config->customMotorMixer[7] = (motorMixer_t){ 1.0f,  1.0f,  0.414178f, -1.0f };    // MIDREAR_L
}
#endif
