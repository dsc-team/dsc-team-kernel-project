/*
 * BT-AMP (BlueTooth Alternate Mac and Phy) HCI (Host/Controller Interface)
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bt_amp_hci.h,v 9.4.4.2 2009/02/18 02:53:44 Exp $
*/

#ifndef _bt_amp_hci_h
#define _bt_amp_hci_h

/* enable structure packing */
#if defined(__GNUC__)
#define	PACKED	__attribute__((packed))
#else
#pragma pack(1)
#define	PACKED
#endif

/* AMP HCI CMD packet format */
typedef struct amp_hci_cmd {
	uint16 opcode;
	uint8 plen;
	uint8 parms[1];
} PACKED amp_hci_cmd_t;

#define HCI_CMD_PREAMBLE_SIZE		OFFSETOF(amp_hci_cmd_t, parms)
#define HCI_CMD_DATA_SIZE		255

/* AMP HCI CMD opcode layout */
#define HCI_CMD_OPCODE(ogf, ocf)	((((ogf) & 0x3F) << 10) | ((ocf) & 0x03FF))
#define HCI_CMD_OGF(opcode)		((uint8)(((opcode) >> 10) & 0x3F))
#define HCI_CMD_OCF(opcode)		((opcode) & 0x03FF)

/* AMP HCI command opcodes */
#define HCI_Read_Link_Quality			HCI_CMD_OPCODE(0x05, 0x0003)
#define HCI_Read_Local_AMP_Info			HCI_CMD_OPCODE(0x05, 0x0009)
#define HCI_Read_Local_AMP_ASSOC		HCI_CMD_OPCODE(0x05, 0x000A)
#define HCI_Write_Remote_AMP_ASSOC		HCI_CMD_OPCODE(0x05, 0x000B)
#define HCI_Create_Physical_Link		HCI_CMD_OPCODE(0x01, 0x0035)
#define HCI_Accept_Physical_Link_Request	HCI_CMD_OPCODE(0x01, 0x0036)
#define HCI_Disconnect_Physical_Link		HCI_CMD_OPCODE(0x01, 0x0037)
#define HCI_Create_Logical_Link			HCI_CMD_OPCODE(0x01, 0x0038)
#define HCI_Accept_Logical_Link			HCI_CMD_OPCODE(0x01, 0x0039)
#define HCI_Disconnect_Logical_Link		HCI_CMD_OPCODE(0x01, 0x003A)
#define HCI_Logical_Link_Cancel			HCI_CMD_OPCODE(0x01, 0x003B)
#define HCI_Write_Flow_Control_Mode		HCI_CMD_OPCODE(0x01, 0x0067)
#define HCI_Read_Best_Effort_Flush_Timeout	HCI_CMD_OPCODE(0x01, 0x0069)
#define HCI_Write_Best_Effort_Flush_Timeout	HCI_CMD_OPCODE(0x01, 0x006A)
#define HCI_Short_Range_Mode			HCI_CMD_OPCODE(0x01, 0x006B)
#define HCI_Reset				HCI_CMD_OPCODE(0x03, 0x0003)
#define HCI_Read_Connection_Accept_Timeout	HCI_CMD_OPCODE(0x03, 0x0015)
#define HCI_Write_Connection_Accept_Timeout	HCI_CMD_OPCODE(0x03, 0x0016)
#define HCI_Read_Link_Supervision_Timeout	HCI_CMD_OPCODE(0x03, 0x0036)
#define HCI_Write_Link_Supervision_Timeout	HCI_CMD_OPCODE(0x03, 0x0037)
#define HCI_Enhanced_Flush			HCI_CMD_OPCODE(0x03, 0x005F)
#define HCI_Read_Logical_Link_Accept_Timeout	HCI_CMD_OPCODE(0x03, 0x0061)
#define HCI_Write_Logical_Link_Accept_Timeout	HCI_CMD_OPCODE(0x03, 0x0062)
#define HCI_Read_Buffer_Size			HCI_CMD_OPCODE(0x04, 0x0005)
#define HCI_Read_Data_Block_Size		HCI_CMD_OPCODE(0x04, 0x000A)

/* AMP HCI command parameters */
typedef struct read_local_cmd_parms {
	uint8 plh;
	uint8 offset[2];			/* length so far */
	uint8 max_remote[2];
} PACKED read_local_cmd_parms_t;

typedef struct write_remote_cmd_parms {
	uint8 plh;
	uint8 offset[2];
	uint8 len[2];
	uint8 frag[1];
} PACKED write_remote_cmd_parms_t;

typedef struct phy_link_cmd_parms {
	uint8 plh;
	uint8 key_length;
	uint8 key_type;
	uint8 key[1];
} PACKED phy_link_cmd_parms_t;

typedef struct dis_phy_link_cmd_parms {
	uint8 plh;
	uint8 reason;
} PACKED dis_phy_link_cmd_parms_t;

typedef struct log_link_cmd_parms {
	uint8 plh;
	uint8 txflow[16];
	uint8 rxflow[16];
} PACKED log_link_cmd_parms_t;

typedef struct ext_flow_spec {
	uint8 id;
	uint8 service_type;
	uint8 max_sdu[2];
	uint8 sdu_ia_time[4];
	uint8 access_latency[4];
	uint8 flush_timeout[4];
} PACKED ext_flow_spec_t;

typedef struct log_link_cancel_cmd_parms {
	uint8 plh;
	uint8 tx_fs_ID;
} PACKED log_link_cancel_cmd_parms_t;

typedef struct plh_pad {
	uint8 plh;
	uint8 pad;
} PACKED plh_pad_t;

typedef union hci_handle {
	uint16 bredr;
	plh_pad_t amp;
} hci_handle_t;

typedef struct ls_to_cmd_parms {
	hci_handle_t handle;
	uint8 timeout[2];
} PACKED ls_to_cmd_parms_t;

typedef struct befto_cmd_parms {
	uint8 llh[2];
	uint8 befto[4];
} PACKED befto_cmd_parms_t;

typedef struct srm_cmd_parms {
	uint8 plh;
	uint8 srm;
} PACKED srm_cmd_parms_t;

typedef struct eflush_cmd_parms {
	uint8 llh[2];
	uint8 packet_type;
} PACKED eflush_cmd_parms_t;

/* Generic AMP extended flow spec service types */
#define EFS_SVCTYPE_NO_TRAFFIC		0
#define EFS_SVCTYPE_BEST_EFFORT		1
#define EFS_SVCTYPE_GUARANTEED		2

/* AMP HCI event packet format */
typedef struct amp_hci_event {
	uint8 ecode;
	uint8 plen;
	uint8 parms[1];
} PACKED amp_hci_event_t;

#define HCI_EVT_PREAMBLE_SIZE			OFFSETOF(amp_hci_event_t, parms)

/* AMP HCI event codes */
#define HCI_Command_Complete			0x0E
#define HCI_Command_Status			0x0F
#define HCI_Flush_Occurred			0x11
#define HCI_Enhanced_Flush_Complete		0x39
#define HCI_Physical_Link_Complete		0x40
#define HCI_Channel_Select			0x41
#define HCI_Disconnect_Physical_Link_Complete	0x42
#define HCI_Logical_Link_Complete		0x45
#define HCI_Disconnect_Logical_Link_Complete	0x46
#define HCI_Number_of_Completed_Data_Blocks	0x48
#define HCI_Short_Range_Mode_Change_Complete	0x4C
#define HCI_Vendor_Specific			0xFF

/* AMP HCI event parameters */
typedef struct cmd_status_parms {
	uint8 status;
	uint8 cmdpkts;
	uint16 opcode;
} PACKED cmd_status_parms_t;

typedef struct cmd_complete_parms {
	uint8 cmdpkts;
	uint16 opcode;
	uint8 parms[1];
} PACKED cmd_complete_parms_t;

typedef struct flush_occurred_evt_parms {
	uint16 handle;
} PACKED flush_occurred_evt_parms_t;

typedef struct write_remote_evt_parms {
	uint8 status;
	uint8 plh;
} PACKED write_remote_evt_parms_t;

typedef struct read_local_evt_parms {
	uint8 status;
	uint8 plh;
	uint16 len;
	uint8 frag[1];
} PACKED read_local_evt_parms_t;

typedef struct read_local_info_evt_parms {
	uint8 status;
	uint8 AMP_status;
	uint32 bandwidth;
	uint32 gbandwidth;
	uint32 latency;
	uint32 PDU_size;
	uint8 ctrl_type;
	uint16 PAL_cap;
	uint16 AMP_ASSOC_len;
	uint32 max_flush_timeout;
	uint32 be_flush_timeout;
} PACKED read_local_info_evt_parms_t;

typedef struct log_link_evt_parms {
	uint8 status;
	uint16 llh;
	union {
		uint8 plh;
		uint8 reason;
	} u;
} PACKED log_link_evt_parms_t;

typedef struct phy_link_evt_parms {
	uint8 status;
	uint8 plh;
} PACKED phy_link_evt_parms_t;

typedef struct dis_phy_link_evt_parms {
	uint8 status;
	uint8 plh;
	uint8 reason;
} PACKED dis_phy_link_evt_parms_t;

typedef struct read_ls_to_evt_parms {
	uint8 status;
	hci_handle_t handle;
	uint16 timeout;
} PACKED read_ls_to_evt_parms_t;

typedef struct read_lla_ca_to_evt_parms {
	uint8 status;
	uint16 timeout;
} PACKED read_lla_ca_to_evt_parms_t;

typedef struct read_data_block_size_evt_parms {
	uint8 status;
	uint16 ACL_pkt_len;
	uint16 data_block_len;
	uint16 data_block_num;
} PACKED read_data_block_size_evt_parms_t;

typedef struct data_blocks {
	uint16 handle;
	uint16 pkts;
	uint16 blocks;
} PACKED data_blocks_t;

typedef struct num_completed_data_blocks_evt_parms {
	uint16 num_blocks;
	uint8 num_handles;
	data_blocks_t completed[1];
} PACKED num_completed_data_blocks_evt_parms_t;

typedef struct befto_evt_parms {
	uint8 status;
	uint32 befto;
} PACKED befto_evt_parms_t;

typedef struct srm_evt_parms {
	uint8 status;
	uint8 plh;
	uint8 srm;
} PACKED srm_evt_parms_t;

typedef struct read_linkq_evt_parms {
	uint8 status;
	hci_handle_t handle;
	uint8 link_quality;
} PACKED read_linkq_evt_parms_t;

typedef struct eflush_complete_evt_parms {
	uint16 handle;
} PACKED eflush_complete_evt_parms_t;

typedef struct vendor_specific_evt_parms {
	uint8 len;
	uint8 parms[1];
} vendor_specific_evt_parms_t;

/* AMP HCI error codes */
#define HCI_SUCCESS				0x00
#define HCI_ERR_ILLEGAL_COMMAND			0x01
#define HCI_ERR_NO_CONNECTION			0x02
#define HCI_ERR_MEMORY_FULL			0x07
#define HCI_ERR_CONNECTION_TIMEOUT		0x08
#define HCI_ERR_MAX_NUM_OF_CONNECTIONS		0x09
#define HCI_ERR_CONNECTION_EXISTS		0x0B
#define HCI_ERR_CONNECTION_ACCEPT_TIMEOUT	0x10
#define HCI_ERR_UNSUPPORTED_VALUE		0x11
#define HCI_ERR_ILLEGAL_PARAMETER_FMT		0x12
#define HCI_ERR_CONN_TERM_BY_LOCAL_HOST		0x16
#define HCI_ERR_UNSPECIFIED			0x1F
#define HCI_ERR_UNIT_KEY_USED			0x26
#define HCI_ERR_QOS_REJECTED			0x2D
#define HCI_ERR_PARAM_OUT_OF_RANGE		0x30

/* AMP HCI ACL Data packet format */
typedef struct amp_hci_ACL_data {
	uint16	handle;			/* 12-bit connection handle + 2-bit PB and 2-bit BC flags */
	uint16	dlen;			/* data total length */
	uint8 data[1];
} PACKED amp_hci_ACL_data_t;

#define HCI_ACL_DATA_PREAMBLE_SIZE	OFFSETOF(amp_hci_ACL_data_t, data)

#define HCI_ACL_DATA_BC_FLAGS		(0x0 << 14)
#define HCI_ACL_DATA_PB_FLAGS		(0x3 << 12)

#define HCI_ACL_DATA_HANDLE(handle)	((handle) & 0x0fff)
#define HCI_ACL_DATA_FLAGS(handle)	((handle) >> 12)

/* AMP Activity Report packet formats */
typedef struct amp_hci_activity_report {
	uint8	ScheduleKnown;
	uint8	NumReports;
	uint8	data[1];
} PACKED amp_hci_activity_report_t;

typedef struct amp_hci_activity_report_triple {
	uint32	StartTime;
	uint32	Duration;
	uint32	Periodicity;
} PACKED amp_hci_activity_report_triple_t;

#define HCI_AR_SCHEDULE_KNOWN		0x01

#undef PACKED
#if !defined(__GNUC__)
#pragma pack()
#endif

#endif /* _bt_amp_hci_h_ */
