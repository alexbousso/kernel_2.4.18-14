/*
 * llc_actn.c - Implementation of actions of station component of LLC
 *
 * Description :
 *   Functions in this module are implementation of station component actions.
 *   Details of actions can be found in IEEE-802.2 standard document.
 *   All functions have one station and one event as input argument. All of
 *   them return 0 On success and 1 otherwise.
 *
 * Copyright (c) 1997 by Procom Technology, Inc.
 * 		 2001 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License for more details.
 */
#include <linux/netdevice.h>
#include <net/llc_if.h>
#include <net/llc_main.h>
#include <net/llc_evnt.h>
#include <net/llc_pdu.h>
#include <net/llc_mac.h>

static void llc_station_ack_tmr_callback(unsigned long timeout_data);

int llc_station_ac_start_ack_timer(struct llc_station *station,
				   struct llc_station_state_ev *ev)
{
	del_timer(&station->ack_timer);
	station->ack_timer.expires  = jiffies + LLC_ACK_TIME * HZ;
	station->ack_timer.data     = (unsigned long)station;
	station->ack_timer.function = llc_station_ack_tmr_callback;
	add_timer(&station->ack_timer);
	station->ack_tmr_running = 1;
	return 0;
}

int llc_station_ac_set_retry_cnt_0(struct llc_station *station,
				   struct llc_station_state_ev *ev)
{
	station->retry_count = 0;
	return 0;
}

int llc_station_ac_inc_retry_cnt_by_1(struct llc_station *station,
				      struct llc_station_state_ev *ev)
{
	station->retry_count++;
	return 0;
}

int llc_station_ac_set_xid_r_cnt_0(struct llc_station *station,
				   struct llc_station_state_ev *ev)
{
	station->xid_r_count = 0;
	return 0;
}

int llc_station_ac_inc_xid_r_cnt_by_1(struct llc_station *station,
				      struct llc_station_state_ev *ev)
{
	station->xid_r_count++;
	return 0;
}

int llc_station_ac_send_null_dsap_xid_c(struct llc_station *station,
					struct llc_station_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (!skb)
		goto out;
	rc = 0;
	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, 0, 0, LLC_PDU_CMD);
	llc_pdu_init_as_xid_cmd(skb, LLC_XID_NULL_CLASS_2, 127);
	lan_hdrs_init(skb, station->mac_sa, station->mac_sa);
	llc_station_send_pdu(station, skb);
out:	return rc;
}

int llc_station_ac_send_xid_r(struct llc_station *station,
			      struct llc_station_state_ev *ev)
{
	u8 mac_da[ETH_ALEN], dsap;
	int rc = 1;
	struct sk_buff *ev_skb;
	struct sk_buff* skb = llc_alloc_frame();

	if (!skb)
		goto out;
	rc = 0;
	ev_skb = ev->data.pdu.skb;
	skb->dev = ev_skb->dev;
	llc_pdu_decode_sa(ev_skb, mac_da);
	llc_pdu_decode_ssap(ev_skb, &dsap);
	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, 0, dsap, LLC_PDU_RSP);
	llc_pdu_init_as_xid_rsp(skb, LLC_XID_NULL_CLASS_2, 127);
	lan_hdrs_init(skb, station->mac_sa, mac_da);
	llc_station_send_pdu(station, skb);
out:	return rc;
}

int llc_station_ac_send_test_r(struct llc_station *station,
			       struct llc_station_state_ev *ev)
{
	u8 mac_da[ETH_ALEN], dsap;
	int rc = 1;
	struct sk_buff *ev_skb;
	struct sk_buff *skb = llc_alloc_frame();

	if (!skb)
		goto out;
	rc = 0;
	ev_skb = ev->data.pdu.skb;
	skb->dev = ev_skb->dev;
	llc_pdu_decode_sa(ev_skb, mac_da);
	llc_pdu_decode_ssap(ev_skb, &dsap);
	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, 0, dsap, LLC_PDU_RSP);
       	llc_pdu_init_as_test_rsp(skb, ev_skb);
	lan_hdrs_init(skb, station->mac_sa, mac_da);
	llc_station_send_pdu(station, skb);
out:	return rc;
}

int llc_station_ac_report_status(struct llc_station *station,
				 struct llc_station_state_ev *ev)
{
	return 0;
}

static void llc_station_ack_tmr_callback(unsigned long timeout_data)
{
	struct llc_station *station = (struct llc_station *)timeout_data;
	struct llc_station_state_ev *ev;

	station->ack_tmr_running = 0;
	ev = llc_station_alloc_ev(station);
	if (ev) {
		ev->type = LLC_STATION_EV_TYPE_ACK_TMR;
		ev->data.tmr.timer_specific = NULL;
		llc_station_send_ev(station, ev);
	}
}
