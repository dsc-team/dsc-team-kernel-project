
struct hdmi_cmd {
	unsigned char cmd;
};

enum {
	HDMI_ON,
	HDMI_OFF,
	HDMI_GET_CABLE_STATUS,
	HDMI_CMD_MAX
};
