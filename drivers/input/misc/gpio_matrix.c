/* drivers/input/misc/gpio_matrix.c
 *
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/gpio_event.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <mach/gpio.h>
#include <mach/austin_hwid.h>

#ifdef CONFIG_QSD_ARM9_CRASH_FUNCTION
static int resetmodem_keycode = 0;
#endif

#if defined (CONFIG_HW_AUSTIN) || defined (CONFIG_HW_TOUCAN)
extern atomic_t psensor_approached;
#endif

struct gpio_kp {
	struct gpio_event_input_devs *input_devs;
	struct gpio_event_matrix_info *keypad_info;
	struct hrtimer timer;
	struct wake_lock wake_lock;
	int current_output;
	unsigned int use_irq:1;
	unsigned int key_state_changed:1;
	unsigned int last_key_state_changed:1;
	unsigned int some_keys_pressed:2;
	unsigned long keys_pressed[0];
};

static void clear_phantom_key(struct gpio_kp *kp, int out, int in)
{
	struct gpio_event_matrix_info *mi = kp->keypad_info;
	int key_index = mi->ninputs + in;
	unsigned short keyentry = mi->keymap[key_index];
	unsigned short keycode = keyentry & MATRIX_KEY_MASK;
	unsigned short dev = keyentry >> MATRIX_CODE_BITS;

	if (!test_bit(keycode, kp->input_devs->dev[dev]->key)) {
		if (mi->flags & GPIOKPF_PRINT_PHANTOM_KEYS)
			pr_info("gpiomatrix: phantom key %x, %d-%d (%d-%d) "
				"cleared\n", keycode, out, in,
				mi->output_gpios[out], mi->input_gpios[in]);
		__clear_bit(key_index, kp->keys_pressed);
	} else {
		if (mi->flags & GPIOKPF_PRINT_PHANTOM_KEYS)
			pr_info("gpiomatrix: phantom key %x, %d-%d (%d-%d) "
				"not cleared\n", keycode, out, in,
				mi->output_gpios[out], mi->input_gpios[in]);
	}
}

static int restore_keys_for_input(struct gpio_kp *kp, int out, int in)
{
	int rv = 0;
	int key_index;

	key_index = kp->keypad_info->ninputs + in;

		if (test_bit(key_index, kp->keys_pressed)) {
			rv = 1;
			clear_phantom_key(kp, out, in);
		}
		key_index ++;
		out++;

	return rv;
}

static void remove_phantom_keys(struct gpio_kp *kp)
{
	int out=0, in, inp;
	int key_index;

	if (kp->some_keys_pressed < 3)
		return;

		inp = -1;
		key_index = kp->keypad_info->ninputs;
		for (in = 0; in < kp->keypad_info->ninputs; in++, key_index++) {
			if (test_bit(key_index, kp->keys_pressed)) {
				if (inp == -1) {
					inp = in;
					continue;
				}
				if (inp >= 0) {
					if (!restore_keys_for_input(kp, out + 1,
									inp))
						break;
					clear_phantom_key(kp, out, inp);
					inp = -2;
				}
				restore_keys_for_input(kp, out, in);
			}
		}
}

static void report_key(struct gpio_kp *kp, int key_index, int out, int in)
{
	struct gpio_event_matrix_info *mi = kp->keypad_info;
	int pressed = test_bit(key_index, kp->keys_pressed);
	unsigned short keyentry = mi->keymap[key_index];
	unsigned short keycode = keyentry & MATRIX_KEY_MASK;
	unsigned short dev = keyentry >> MATRIX_CODE_BITS;

	if (pressed != test_bit(keycode, kp->input_devs->dev[dev]->key)) {
		if (keycode == KEY_RESERVED) {
			if (mi->flags & GPIOKPF_PRINT_UNMAPPED_KEYS)
				pr_info("gpiomatrix: unmapped key, %d-%d "
					"(%d-%d) changed to %d\n",
					out, in, mi->output_gpios[out],
					mi->input_gpios[in], pressed);
		} else {
			if (mi->flags & GPIOKPF_PRINT_MAPPED_KEYS)
				pr_info("gpiomatrix: key %x, %d-%d (%d-%d) "
					"changed to %d\n", keycode,
					out, in, mi->output_gpios[out],
					mi->input_gpios[in], pressed);
			#ifdef CONFIG_QSD_ARM9_CRASH_FUNCTION
			resetmodem_keycode = keycode;
			#endif

#ifdef CONFIG_HW_AUSTIN
			if(atomic_read(&psensor_approached) && ((keycode==KEY_F1)||(keycode==KEY_CAMERA)))
				pr_info("## [Jackie] ## Bypass report keycode to AP, key=%d, pressed=%d\n", keycode, pressed);
			else
			{
				input_report_key(kp->input_devs->dev[dev], keycode, pressed);
				printk("k[%d]p[%d]\n",keycode,pressed);
			}
#else
			if(atomic_read(&psensor_approached) && ((keycode==KEY_F1)||(keycode==KEY_CAMERA)))
			{
				pr_info("## [Emily] ## Bypass report keycode to AP, key=%d, pressed=%d\n", keycode, pressed);
			}	
			else
			{
				input_report_key(kp->input_devs->dev[dev], keycode, pressed);
				printk("k[%d]p[%d]\n",keycode,pressed);
			}	
#endif
		}
	}
}

static enum hrtimer_restart gpio_keypad_timer_func(struct hrtimer *timer)
{
	int out=0, in;
	int key_index;
	int gpio;
	struct gpio_kp *kp = container_of(timer, struct gpio_kp, timer);
	struct gpio_event_matrix_info *mi = kp->keypad_info;
	unsigned gpio_keypad_flags = mi->flags;
	unsigned polarity = !!(gpio_keypad_flags & GPIOKPF_ACTIVE_HIGH);

		kp->last_key_state_changed = kp->key_state_changed;
		kp->key_state_changed = 0;
		kp->some_keys_pressed = 0;
		key_index = 0;
		for (in = 0; in < mi->ninputs; in++, key_index++) {
			gpio = mi->input_gpios[in];
			if (gpio_get_value(gpio) ^ !polarity) {
				if (kp->some_keys_pressed < 3)
					kp->some_keys_pressed++;
				kp->key_state_changed |= !__test_and_set_bit(
						key_index, kp->keys_pressed);
			} else
				kp->key_state_changed |= __test_and_clear_bit(
						key_index, kp->keys_pressed);
		}

	if (gpio_keypad_flags & GPIOKPF_DEBOUNCE) {
		if (kp->key_state_changed) {
			hrtimer_start(&kp->timer, mi->debounce_delay,
				      HRTIMER_MODE_REL);
			return HRTIMER_NORESTART;
		}
		kp->key_state_changed = kp->last_key_state_changed;
	}
	if (kp->key_state_changed) {
		if (gpio_keypad_flags & GPIOKPF_REMOVE_SOME_PHANTOM_KEYS)
			remove_phantom_keys(kp);
		key_index = 0;
			for (in = 0; in < mi->ninputs; in++, key_index++)
				report_key(kp, key_index, out, in);
	}
	if (!kp->use_irq || kp->some_keys_pressed) {
		hrtimer_start(timer, mi->poll_time, HRTIMER_MODE_REL);
		return HRTIMER_NORESTART;
	}

	/* No keys are pressed, reenable interrupt */
	for (in = 0; in < mi->ninputs; in++)
		enable_irq(gpio_to_irq(mi->input_gpios[in]));
	wake_unlock(&kp->wake_lock);
	return HRTIMER_NORESTART;
}
static int start_to_setup_irq = 1;
static int irq_disable = 0;
static irqreturn_t gpio_keypad_irq_handler(int irq_in, void *dev_id)
{
	int i;
	struct gpio_kp *kp = dev_id;
	struct gpio_event_matrix_info *mi = kp->keypad_info;

	if(start_to_setup_irq == 1)
	{
		printk("%s %d\n",__func__,__LINE__);
		disable_irq_nosync(irq_in);
		irq_disable = 1;
		return IRQ_HANDLED;
	}

	if (!kp->use_irq) /* ignore interrupt while registering the handler */
		return IRQ_HANDLED;

	for (i = 0; i < mi->ninputs; i++)
		disable_irq_nosync(gpio_to_irq(mi->input_gpios[i]));
	wake_lock(&kp->wake_lock);
	hrtimer_start(&kp->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
	return IRQ_HANDLED;
}

static DEFINE_SPINLOCK(gpio_keypad_lock);
static int gpio_keypad_request_irqs(struct gpio_kp *kp)
{
	int i;
	int err;
	unsigned int irq;
	unsigned long request_flags;
	struct gpio_event_matrix_info *mi = kp->keypad_info;
	unsigned long flags;

	switch (mi->flags & (GPIOKPF_ACTIVE_HIGH|GPIOKPF_LEVEL_TRIGGERED_IRQ)) {
	default:
		request_flags = IRQF_TRIGGER_FALLING;
		break;
	case GPIOKPF_ACTIVE_HIGH:
		request_flags = IRQF_TRIGGER_RISING;
		break;
	case GPIOKPF_LEVEL_TRIGGERED_IRQ:
		request_flags = IRQF_TRIGGER_LOW;
		break;
	case GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_ACTIVE_HIGH:
		request_flags = IRQF_TRIGGER_HIGH;
		break;
	}

	start_to_setup_irq = 1;
	for (i = 0; i < mi->ninputs; i++) {
		err = irq = gpio_to_irq(mi->input_gpios[i]);
		if (err < 0)
			goto err_gpio_get_irq_num_failed;
		irq_disable = 0;
		err = request_irq(irq, gpio_keypad_irq_handler, request_flags,
				  "gpio_kp", kp);
		if (err) {
			pr_err("gpiomatrix: request_irq failed for input %d, "
				"irq %d\n", mi->input_gpios[i], irq);
			goto err_request_irq_failed;
		}
		err = set_irq_wake(irq, 1);
		if (err) {
			pr_err("gpiomatrix: set_irq_wake failed for input %d, "
				"irq %d\n", mi->input_gpios[i], irq);
		}
		spin_lock_irqsave(&gpio_keypad_lock,flags);
		if(irq_disable == 0) disable_irq(irq);
		spin_unlock_irqrestore(&gpio_keypad_lock,flags);
	}
	start_to_setup_irq = 0;
	return 0;

	for (i = mi->ninputs - 1; i >= 0; i--) {
		free_irq(gpio_to_irq(mi->input_gpios[i]), kp);
err_request_irq_failed:
err_gpio_get_irq_num_failed:
		;
	}
	return err;
}

int gpio_event_matrix_func(struct gpio_event_input_devs *input_devs,
	struct gpio_event_info *info, void **data, int func)
{
	int i;
	int err;
	int key_count;
	struct gpio_kp *kp;
	struct gpio_event_matrix_info *mi;

	mi = container_of(info, struct gpio_event_matrix_info, info);
	if (func == GPIO_EVENT_FUNC_SUSPEND || func == GPIO_EVENT_FUNC_RESUME) {
		/* TODO: disable scanning */
		return 0;
	}

	if (func == GPIO_EVENT_FUNC_INIT) {
		if (mi->keymap == NULL ||
		   mi->input_gpios == NULL ||
		   mi->output_gpios == NULL) {
			err = -ENODEV;
			pr_err("gpiomatrix: Incomplete pdata\n");
			goto err_invalid_platform_data;
		}
		key_count = mi->ninputs;
#ifdef CONFIG_HW_TOUCAN
		if (system_rev >= TOUCAN_EVT3_Band125 && system_rev < HWID_UNKNOWN) 
		{
			mi->input_gpios[1] = 51;
		}	
#endif
		*data = kp = kzalloc(sizeof(*kp) + sizeof(kp->keys_pressed[0]) *
				     BITS_TO_LONGS(key_count), GFP_KERNEL);
		if (kp == NULL) {
			err = -ENOMEM;
			pr_err("gpiomatrix: Failed to allocate private data\n");
			goto err_kp_alloc_failed;
		}
		kp->input_devs = input_devs;
		kp->keypad_info = mi;
		for (i = 0; i < key_count; i++) {
			unsigned short keyentry = mi->keymap[i];
			unsigned short keycode = keyentry & MATRIX_KEY_MASK;
			unsigned short dev = keyentry >> MATRIX_CODE_BITS;
			if (dev >= input_devs->count) {
				pr_err("gpiomatrix: bad device index %d >= "
					"%d for key code %d\n",
					dev, input_devs->count, keycode);
				err = -EINVAL;
				goto err_bad_keymap;
			}
			if (keycode && keycode <= KEY_MAX)
				input_set_capability(input_devs->dev[dev],
							EV_KEY, keycode);
		}

		for (i = 0; i < mi->ninputs; i++) {
			gpio_tlmm_config(GPIO_CFG(mi->input_gpios[i], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

			err = gpio_request(mi->input_gpios[i], "gpio_kp_in");
			if (err) {
				pr_err("gpiomatrix: gpio_request failed for "
					"input %d\n", mi->input_gpios[i]);
				goto err_request_input_gpio_failed;
			}
			err = gpio_direction_input(mi->input_gpios[i]);
			if (err) {
				pr_err("gpiomatrix: gpio_direction_input failed"
					" for input %d\n", mi->input_gpios[i]);
				goto err_gpio_direction_input_failed;
			}
			printk(KERN_ERR "Jackie: gpio_direction_input for input %d\n", mi->input_gpios[i]);
		}
		kp->current_output = mi->noutputs;
		kp->key_state_changed = 1;

		hrtimer_init(&kp->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		kp->timer.function = gpio_keypad_timer_func;
		wake_lock_init(&kp->wake_lock, WAKE_LOCK_SUSPEND, "gpio_kp");
		err = gpio_keypad_request_irqs(kp);
		kp->use_irq = err == 0;

		pr_info("GPIO Matrix Keypad Driver: Start keypad matrix for "
			"%s%s in %s mode\n", input_devs->dev[0]->name,
			(input_devs->count > 1) ? "..." : "",
			kp->use_irq ? "interrupt" : "polling");

		if (kp->use_irq)
			wake_lock(&kp->wake_lock);
		hrtimer_start(&kp->timer, ktime_set(0, 0), HRTIMER_MODE_REL);

		return 0;
	}

	err = 0;
	kp = *data;

	if (kp->use_irq)
		for (i = mi->ninputs - 1; i >= 0; i--)
			free_irq(gpio_to_irq(mi->input_gpios[i]), kp);

	hrtimer_cancel(&kp->timer);
	wake_lock_destroy(&kp->wake_lock);
	for (i = mi->ninputs - 1; i >= 0; i--) {
err_gpio_direction_input_failed:
		gpio_free(mi->input_gpios[i]);
err_request_input_gpio_failed:
		;
	}
	for (i = mi->noutputs - 1; i >= 0; i--) {
		gpio_free(mi->output_gpios[i]);
	}
err_bad_keymap:
	kfree(kp);
err_kp_alloc_failed:
err_invalid_platform_data:
	return err;
}


#ifdef CONFIG_QSD_ARM9_CRASH_FUNCTION
int gpio_keypad_detect_reset_modem(void)
{
	int ret=0;

	ret = resetmodem_keycode;
	if(resetmodem_keycode != 0)
		resetmodem_keycode=0;
	return ret;
}
#endif
