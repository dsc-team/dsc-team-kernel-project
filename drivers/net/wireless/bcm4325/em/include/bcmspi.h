/*
 * Broadcom SPI Low-Level Hardware Driver API
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcmspi.h,v 13.3.10.2 2008/06/30 21:09:40 Exp $
 */

extern void spi_devintr_off(sdioh_info_t *sd);
extern void spi_devintr_on(sdioh_info_t *sd);
extern bool spi_start_clock(sdioh_info_t *sd, uint16 new_sd_divisor);
extern bool spi_controller_highspeed_mode(sdioh_info_t *sd, bool hsmode);
extern bool spi_check_client_intr(sdioh_info_t *sd, int *is_dev_intr);
extern bool spi_hw_attach(sdioh_info_t *sd);
extern bool spi_hw_detach(sdioh_info_t *sd);
extern void spi_sendrecv(sdioh_info_t *sd, uint8 *msg_out, uint8 *msg_in, int msglen);
extern void spi_spinbits(sdioh_info_t *sd);
extern void spi_waitbits(sdioh_info_t *sd, bool yield);
