/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <modem/at_monitor.h>
#include <modem/modem_info.h>

#define ENABLE 1
#define DISABLE 0

/* Network registration semaphore */
static K_SEM_DEFINE(cereg_sem, 0, 1);

/* AT monitor for network notifications */
AT_MONITOR(network, "CEREG", cereg_mon);
/* Monitors are enabled by default, but an initial state may be set optionally.
 * AT monitor for link quality notifications, paused.
 */
AT_MONITOR(link_quality, "CESQ", cesq_mon, PAUSED);

static int cereg_status;
enum cereg_status {
	NO_NETWORK = 0,
	HOME = 1,
	SEARCHING = 2,
	DENIED = 3,
	UNKNOWN = 4,
	ROAMING = 5,
	UICC_FAILURE = 90
};

static const char *cereg_str_get(enum cereg_status status)
{
	switch (status) {
	case NO_NETWORK:
		return "no network";
	case HOME:
		return "home";
	case SEARCHING:
		return "searching";
	case DENIED:
		return "denied";
	case UNKNOWN:
		return "unknown";
	case ROAMING:
		return "roaming";
	case UICC_FAILURE:
		return "UICC failure";
	default:
		return NULL;
	}
}

static int rsrp_status;
static char response[256];

static void cereg_mon(const char *notif)
{
	const char *cereg_status_str;

	cereg_status = atoi(notif + strlen("+CEREG: "));
	cereg_status_str = cereg_str_get(cereg_status);

	if (!cereg_status_str) {
		printk("Network registration status unknown: %d\n", cereg_status);
		return;
	}

	printk("Network registration status: %s\n", cereg_status_str);

	if (cereg_status == HOME) {
		k_sem_give(&cereg_sem);
	}
}

static void cesq_mon(const char *notif)
{
	rsrp_status = atoi(notif + strlen("%CESQ: "));

	printk("Link quality: %d dBm\n", RSRP_IDX_TO_DBM(rsrp_status));
}

static int psm_control(int enable)
{
	return nrf_modem_at_printf("AT+CPSMS=%d", enable ? 1 : 0);
}

static void psm_read(void)
{
	int ret;
	int psm_enabled;
	char request_periodic_tau[8];
	char request_active_time[8];

	printk("Reading PSM info...\n");
	ret = nrf_modem_at_scanf("AT+CPSMS?",
		"+CPSMS: "
			"%d"	/* enabled */
			","	/* Requested_Periodic-RAU, ignored */
			","	/* Requested_GPRS-READY-timer, ignored */
		",\"%8[0-1]\""	/* Requested_Periodic-TAU */
		",\"%8[0-1]\"",	/* Requested_Active-Time */
		&psm_enabled,
		&request_periodic_tau,
		&request_active_time
	);

	if (ret < 0) {
		printk("Could not parse PSM data, err %d\n", ret);
		return;
	}

	if (ret > 0) { /* One param matched */
		printk("  PSM: %s\n", psm_enabled ? "enabled" : "disabled");
	}
	if (ret > 1) { /* Two params matched */
		printk("  Periodic TAU string: %.*s\n",
		       sizeof(request_periodic_tau), request_periodic_tau);
	}
	if (ret > 2) {  /* Three params matched */
		printk("  Active time string: %.*s\n",
			sizeof(request_active_time), request_active_time);
	}
}

static void send_at(const char *mensaje){
	int error;
	error = nrf_modem_at_cmd(response, sizeof(response), mensaje);
	if (error) {
		printk(mensaje);
		printk("Failed to read, err %d\n", error);
	
	}
	printk(mensaje);
	printk(":\n%s", response);
}

int main(void)
{
	int err;

	printk("AT Monitor sample started\n");

	err = nrf_modem_lib_init();
	if (err) {
		printk("Modem library initialization failed, error: %d\n", err);
		return 0;
	}

	send_at("AT+CFUN=0");

	send_at("AT%%XSIM?");

	send_at("AT%%XSIM=1");

	send_at("AT+CFUN=1");
	
	send_at("AT%%XSIM?");

	send_at("AT%%XSIM=1");
	
	send_at("AT%%XSIM?");
	
	send_at("AT+CIMI");

	send_at("AT%%XICCID");

	send_at("AT+CNUM");


/*	
	printk("Subscribing to notifications\n");
	err = nrf_modem_at_printf("AT+CEREG=1");
	if (err) {
		printk("AT+CEREG failed\n");
		return 0;
	}

	err = nrf_modem_at_printf("AT%%CESQ=1");
	if (err) {
		printk("AT+CESQ failed\n");
		return 0;
	}

	//////////////////////////////////////////////
	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CFUN=0");
	if (err) {
		printk("Failed to read CFUN=0, err %d\n", err);
		return 0;
	}
	printk("CFUN=0:\n%s", response);

	send_at("AT+CFUN=1");

	send_at("AT+CFUN=0");

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG=5");
	if (err) {
		printk("Failed to read CEREG=5, err %d\n", err);
		return 0;
	}

	printk("CEREG=5:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+COPS=0");
	if (err) {
		printk("Failed to read COPS=0, err %d\n", err);
		return 0;
	}

	printk("COPS=0:\n%s", response);
//AT+COPS=1,2,"44010"

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XSYSTEMMODE=1,0,0,1");
	if (err) {
		printk("Failed to read AT%%XSYSTEMMODE=1,0,0,0, err %d\n", err);
		return 0;
	}

	printk("AT%%XSYSTEMMODE=1,0,0,0:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGDCONT=1,\"IP\",\"m2m.entel.cl\"");
	if (err) {
		printk("Failed to read AT+CGDCONT=1,\"IP\",\"m2m.entel.cl\", err %d\n", err);
		return 0;
	}

	printk("AT+CGDCONT=1,\"IP\",\"m2m.entel.cl\":\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGAUTH=1,2,\"entelpcs\",\"entelpcs\"");
	if (err) {
		printk("Failed to read AT+CGAUTH=1,2,\"entelpcs\",\"entelpcs\", err %d\n", err);
		return 0;
	}
	printk("AT+CGAUTH=1,2,\"entelpcs\",\"entelpcs\":\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGDCONT=0,\"IP\",\"m2m.entel.cl\"");
	if (err) {
		printk("Failed to read AT+CGDCONT=0,\"IP\",\"m2m.entel.cl\", err %d\n", err);
		return 0;
	}
	printk("AT+CGDCONT=0,\"IP\",\"m2m.entel.cl\":\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGAUTH=0,2,\"entelpcs\",\"entelpcs\"");
	if (err) {
		printk("Failed to read AT+CGAUTH=0,2,\"entelpcs\",\"entelpcs\", err %d\n", err);
		return 0;
	}
	printk("AT+CGAUTH=0,2,\"entelpcs\",\"entelpcs\":\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CFUN=1");
	if (err) {
		printk("Failed to read AT+CFUN=1, err %d\n", err);
		return 0;
	}
	printk("AT+CFUN=1:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGSN=1");
	if (err) {
		printk("Failed to read AT+CGSN=1, err %d\n", err);
		return 0;
	}
	printk("AT+CGSN=1:\n%s", response);


	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGDCONT?");
	if (err) {
		printk("Failed to read AT+CGDCONT?, err %d\n", err);
		return 0;
	}
	printk("AT+CGDCONT?:\n%s", response);
	

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGATT=1");
	if (err) {
		printk("Failed to read AT+CGATT=1, err %d\n", err);
	}
	printk("AT+CGATT=1:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CESQ");
	if (err) {
		printk("Failed to read AT+CESQ, err %d\n", err);
	}
	printk("AT+CESQ:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%CESQ=0");
	if (err) {
		printk("Failed to read AT%%CESQ=0, err %d\n", err);
	}
	printk("AT%%CESQ=0:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%CESQ=1");
	if (err) {
		printk("Failed to read AT%%CESQ=1, err %d\n", err);
	}
	printk("AT%%CESQ=1:\n%s", response);
	
	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XCBAND");
	if (err) {
		printk("Failed to read AT%%XCBAND, err %d\n", err);
	}
	printk("AT%%XCBAND:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CGACT=1,1");
	if (err) {
		printk("Failed to read AT+CGACT=1,1, err %d\n", err);
	}
	printk("AT+CGACT=1,1:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CPIN?");
	if (err) {
		printk("Failed to read AT+CPIN?, err %d\n", err);
	}
	printk("AT+CPIN?:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CFUN?");
	if (err) {
		printk("Failed to read AT+CFUN?, err %d\n", err);
	}
	printk("AT+CFUN?:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XANTFCG?");
	if (err) {
		printk("Failed to read AT%%XANTFCG?, err %d\n", err);
	}
	printk("AT%%XANTFCG?:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XCBAND=?");
	if (err) {
		printk("Failed to read AT%%XCBAND=?, err %d\n", err);
	}
	printk("AT%%XCBAND=?:\n%s", response);

	
	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XAPNSTATUS?");
	if (err) {
		printk("Failed to read AT%%XAPNSTATUS?, err %d\n", err);
	}
	printk("AT%%XAPNSTATUS?:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XMONITOR");
	if (err) {
		printk("Failed to read AT%%XMONITOR, err %d\n", err);
	}
	printk("AT%%XMONITOR:\n%s", response);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+COPS=?");
	if (err) {
		printk("Failed to read AT+COPS=?, err %d\n", err);
	}
	printk("AT+COPS=?:\n%s", response);

//AT+CGAUTH=0,1,"sora","sora"
//AT+CESQ=?
//AT+CFUN=1

*/

	printk("Connecting to network\n");
	err = nrf_modem_at_printf("AT+CFUN=1");
	if (err) {
		printk("AT+CFUN failed\n");
		return 0;
	}

	/* Let's monitor link quality while attempting to register to network */
	printk("Resuming link quality monitor for AT notifications\n");
	at_monitor_resume(&link_quality);

	printk("Waiting for network\n");
	err = k_sem_take(&cereg_sem, K_SECONDS(60));

	if (cereg_status == HOME) {
		printk("Network connection ready\n");
	} else {
		if (err == -EAGAIN) {
			printk("Network connection timed out\n");
		}
		printk("Continuing without network\n");
	}

	/* Monitors can be paused when necessary */
	printk("Pausing link quality monitor for AT notifications\n");
	at_monitor_pause(&link_quality);

	psm_read();

	printk("Enabling PSM\n");
	err = psm_control(ENABLE);
	if (err) {
		printk("Could not enable power saving mode\n");
		return 0;
	}

	psm_read();

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG?");
	if (err) {
		printk("Failed to read CEREG, err %d\n", err);
		return 0;
	}

	printk("Modem response:\n%s", response);

	printk("Shutting down modem\n");
	err = nrf_modem_at_printf("AT+CFUN=0");
	if (err) {
		printk("AT+CFUN failed\n");
		return 0;
	}
	nrf_modem_lib_shutdown();
	printk("Bye\n");
	return 0;
}
