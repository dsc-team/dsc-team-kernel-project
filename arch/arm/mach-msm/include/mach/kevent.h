#ifndef __KEVENT_H_
#define	__KEVENT_H_

enum {
	KEVENT_OOPS = 0,
	KEVENT_MODEM_CRASH,
	KEVENT_ADSP_CRASH,
	KEVENT_DUMP_WAKELOCKS,
	KEVENT_COUNT
};

#ifdef CONFIG_ANDROID_KERNEL_EVENT_DRIVER
int kevent_trigger(unsigned event_id);
#else
#define kevent_trigger
#endif

#endif
