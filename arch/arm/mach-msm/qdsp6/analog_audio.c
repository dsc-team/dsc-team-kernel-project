/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/gpio.h>
#include <mach/pmic.h>
#include <mach/msm_qdsp6_audio.h>
#include <mach/pm_log.h>
#include <linux/moduleparam.h>
#include <mach/austin_hwid.h>

#define DEBUG
#ifdef DEBUG
#define D(fmt, args...) printk(KERN_INFO "%s, tid = %d: " fmt ,__FUNCTION__,  current->pid, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

int Is_first_boot_flag = 1;
int g_pmic_mute_status = 0;

#define GPIO_HEADSET_AMP 109

void analog_init(void)
{
	D("+analog_init+\n");
	/* stereo pmic init */
	pmic_spkr_set_gain(LEFT_SPKR, SPKR_GAIN_PLUS12DB);
	pmic_spkr_set_gain(RIGHT_SPKR, SPKR_GAIN_PLUS12DB);
	pmic_mic_set_volt(MIC_VOLT_2_00V);

	pmic_speaker_cmd(SPKR_OFF);
	pmic_spkr_en_left_chan(0);
	pmic_spkr_en_right_chan(0);
	pmic_spkr_en_hpf(0);
	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUDIO_SPK);
	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUDIO_HS);
	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUTIO_MIC);

	pmic_spkr_set_mux_hpf_corner_freq(SPKR_FREQ_0_76KHZ);

	gpio_request(GPIO_HEADSET_AMP, "headset amp");

	gpio_direction_output(GPIO_HEADSET_AMP, 1);
	gpio_set_value(GPIO_HEADSET_AMP, 0);
	D("-analog_init-: GPIO_HEADSET_AMP = %d\n", GPIO_HEADSET_AMP);
}

void analog_headset_enable(int en)
{
	D("en = %d\n", en);
	/* enable audio amp */
	gpio_set_value(GPIO_HEADSET_AMP, !!en);
	if (en)
		PM_LOG_EVENT(PM_LOG_ON, PM_LOG_AUDIO_HS);
	else
		PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUDIO_HS);	
}

void analog_speaker_enable(int en)
{
	struct spkr_config_mode scm;
	memset(&scm, 0, sizeof(scm));
	D("en = %d\n", en);
	if( g_pmic_mute_status == en )
	{
		D("g_pmic_mute_status == en,  needn't config\n");
		return;
	}
	if (en) {
		scm.is_right_chan_en = 1;
		scm.is_left_chan_en = 1;
		scm.is_stereo_en = 0;
		scm.is_hpf_en = 1;
		scm.is_right_left_chan_added =1;

		pmic_spkr_en_mute(LEFT_SPKR, 0);
		pmic_spkr_en_mute(RIGHT_SPKR, 0);
		pmic_set_spkr_configuration(&scm);
		pmic_spkr_en(LEFT_SPKR, 1);
		pmic_spkr_en(RIGHT_SPKR, 1);
		pmic_spkr_en_left_chan(1);
		pmic_spkr_en_right_chan(1);
		pmic_spkr_en_hpf(1);
		if (1 == Is_first_boot_flag)
		{
			Is_first_boot_flag = 0;	
		}
		else
		{
			/* unmute */
			pmic_spkr_en_mute(LEFT_SPKR, 1);
			pmic_spkr_en_mute(RIGHT_SPKR, 1);
		}
		PM_LOG_EVENT(PM_LOG_ON, PM_LOG_AUDIO_SPK);		
	} else {
		pmic_spkr_en_mute(LEFT_SPKR, 0);
		pmic_spkr_en_mute(RIGHT_SPKR, 0);

		pmic_spkr_en(LEFT_SPKR, 0);
		pmic_spkr_en(RIGHT_SPKR, 0);
		pmic_set_spkr_configuration(&scm);
		pmic_spkr_en_left_chan(0);
		pmic_spkr_en_right_chan(0);
		pmic_spkr_en_hpf(0);
		PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUDIO_SPK);		

	}
	g_pmic_mute_status = en;
}

void analog_mic_enable(int en)
{
	pmic_mic_en(en);

	if (en)
		PM_LOG_EVENT(PM_LOG_ON, PM_LOG_AUTIO_MIC);
	else
		PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUTIO_MIC);		
}

static struct q6audio_analog_ops ops = {
	.init = analog_init,
	.speaker_enable = analog_speaker_enable,
	.headset_enable = analog_headset_enable,
	.int_mic_enable = analog_mic_enable,
	.ext_mic_enable = analog_mic_enable,
};

static int __init init(void)
{
	q6audio_register_analog_ops(&ops);
	return 0;
}

device_initcall(init);
