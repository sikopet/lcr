/*****************************************************************************\
**                                                                           **
** LCR                                                                       **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg                                              **
**                                                                           **
** mISDN gsm (MS mode)                                                       **
**                                                                           **
\*****************************************************************************/ 

#include "main.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
extern "C" {
#include <getopt.h>

#include <osmocore/select.h>
#include <osmocore/talloc.h>

#include <osmocom/osmocom_data.h>
#include <osmocom/logging.h>
#include <osmocom/l1l2_interface.h>
#include <osmocom/l23_app.h>
}

const char *openbsc_copyright = "";
short vty_port = 4247;

struct llist_head ms_list;
struct log_target *stderr_target;
void *l23_ctx = NULL;
int (*l23_app_work) (struct osmocom_ms *ms) = NULL;
int (*l23_app_exit) (struct osmocom_ms *ms) = NULL;

/*
 * constructor
 */
Pgsm_ms::Pgsm_ms(int type, struct mISDNport *mISDNport, char *portname, struct port_settings *settings, int channel, int exclusive, int mode) : Pgsm(type, mISDNport, portname, settings, channel, exclusive, mode)
{
	struct osmocom_ms *ms = NULL;
	char *ms_name = mISDNport->ifport->gsm_ms_name;

	p_m_g_instance = NULL;

	llist_for_each_entry(ms, &ms_list, entity) {
		if (!strcmp(ms->name, ms_name)) {
			p_m_g_instance = ms;
			break;
		}
	}

	if (!p_m_g_instance)
		FATAL("MS name %s does not exists. Please fix!");

	PDEBUG(DEBUG_GSM, "Created new GSMMSPort(%s %s).\n", portname, ms_name);
}

/*
 * destructor
 */
Pgsm_ms::~Pgsm_ms()
{
	PDEBUG(DEBUG_GSM, "Destroyed GSM MS process(%s).\n", p_name);
}

/*
 * handles all indications
 */
/* SETUP INDICATION */
void Pgsm_ms::setup_ind(unsigned int msg_type, unsigned int callref, struct gsm_mncc *mncc)
{
	int ret;
	class Endpoint *epoint;
	struct lcr_msg *message;
	int channel;
	struct gsm_mncc *mode, *proceeding, *frame;

	/* process given callref */
	l1l2l3_trace_header(p_m_mISDNport, this, L3_NEW_L3ID_IND, DIRECTION_IN);
	add_trace("callref", "new", "0x%x", callref);
	if (p_m_g_callref) {
		/* release in case the ID is already in use */
		add_trace("error", NULL, "callref already in use");
		end_trace();
		mncc = create_mncc(MNCC_REJ_REQ, callref);
		gsm_trace_header(p_m_mISDNport, this, MNCC_REJ_REQ, DIRECTION_OUT);
		mncc->fields |= MNCC_F_CAUSE;
		mncc->cause.coding = 3;
		mncc->cause.location = 1;
		mncc->cause.value = 47;
		add_trace("cause", "coding", "%d", mncc->cause.coding);
		add_trace("cause", "location", "%d", mncc->cause.location);
		add_trace("cause", "value", "%d", mncc->cause.value);
		add_trace("reason", NULL, "callref already in use");
		end_trace();
		send_and_free_mncc(p_m_g_instance, mncc->msg_type, mncc);
		new_state(PORT_STATE_RELEASE);
		trigger_work(&p_m_g_delete);
		return;
	}
	p_m_g_callref = callref;
	end_trace();

	/* if blocked, release call with MT_RELEASE_COMPLETE */
	if (p_m_mISDNport->ifport->block) {
		mncc = create_mncc(MNCC_REJ_REQ, p_m_g_callref);
		gsm_trace_header(p_m_mISDNport, this, MNCC_REJ_REQ, DIRECTION_OUT);
		mncc->fields |= MNCC_F_CAUSE;
		mncc->cause.coding = 3;
		mncc->cause.location = 1;
		mncc->cause.value = 27;
		add_trace("cause", "coding", "%d", mncc->cause.coding);
		add_trace("cause", "location", "%d", mncc->cause.location);
		add_trace("cause", "value", "%d", mncc->cause.value);
		add_trace("reason", NULL, "port is blocked");
		end_trace();
		send_and_free_mncc(p_m_g_instance, mncc->msg_type, mncc);
		new_state(PORT_STATE_RELEASE);
		trigger_work(&p_m_g_delete);
		return;
	}

	l1l2l3_trace_header(p_m_mISDNport, this, MNCC_SETUP_IND, DIRECTION_IN);
	/* caller information */
	p_callerinfo.ntype = INFO_NTYPE_NOTPRESENT;
	if (mncc->fields & MNCC_F_CALLING) {
		switch (mncc->calling.present) {
			case 1:
			p_callerinfo.present = INFO_PRESENT_RESTRICTED;
			break;
			case 2:
			p_callerinfo.present = INFO_PRESENT_NOTAVAIL;
			break;
			default:
			p_callerinfo.present = INFO_PRESENT_ALLOWED;
			break;
		}
		switch (mncc->calling.screen) {
			case 0:
			p_callerinfo.screen = INFO_SCREEN_USER;
			break;
			case 1:
			p_callerinfo.screen = INFO_SCREEN_USER_VERIFIED_PASSED;
			break;
			case 2:
			p_callerinfo.screen = INFO_SCREEN_USER_VERIFIED_FAILED;
			break;
			default:
			p_callerinfo.screen = INFO_SCREEN_NETWORK;
			break;
		}
		switch (mncc->calling.type) {
			case 0x0:
			p_callerinfo.ntype = INFO_NTYPE_UNKNOWN;
			break;
			case 0x1:
			p_callerinfo.ntype = INFO_NTYPE_INTERNATIONAL;
			break;
			case 0x2:
			p_callerinfo.ntype = INFO_NTYPE_NATIONAL;
			break;
			case 0x4:
			p_callerinfo.ntype = INFO_NTYPE_SUBSCRIBER;
			break;
			default:
			p_callerinfo.ntype = INFO_NTYPE_UNKNOWN;
			break;
		}
		SCPY(p_callerinfo.id, mncc->calling.number);
		add_trace("calling", "type", "%d", mncc->calling.type);
		add_trace("calling", "plan", "%d", mncc->calling.plan);
		add_trace("calling", "present", "%d", mncc->calling.present);
		add_trace("calling", "screen", "%d", mncc->calling.screen);
		add_trace("calling", "number", "%s", mncc->calling.number);
	}
	p_callerinfo.isdn_port = p_m_portnum;
	SCPY(p_callerinfo.interface, p_m_mISDNport->ifport->interface->name);
	/* dialing information */
	if (mncc->fields & MNCC_F_CALLED) {
		SCAT(p_dialinginfo.id, mncc->called.number);
		switch (mncc->called.type) {
			case 0x1:
			p_dialinginfo.ntype = INFO_NTYPE_INTERNATIONAL;
			break;
			case 0x2:
			p_dialinginfo.ntype = INFO_NTYPE_NATIONAL;
			break;
			case 0x4:
			p_dialinginfo.ntype = INFO_NTYPE_SUBSCRIBER;
			break;
			default:
			p_dialinginfo.ntype = INFO_NTYPE_UNKNOWN;
			break;
		}
		add_trace("dialing", "type", "%d", mncc->called.type);
		add_trace("dialing", "plan", "%d", mncc->called.plan);
		add_trace("dialing", "number", "%s", mncc->called.number);
	}
	/* redir info */
	p_redirinfo.ntype = INFO_NTYPE_NOTPRESENT;
	if (mncc->fields & MNCC_F_REDIRECTING) {
		switch (mncc->redirecting.present) {
			case 1:
			p_redirinfo.present = INFO_PRESENT_RESTRICTED;
			break;
			case 2:
			p_redirinfo.present = INFO_PRESENT_NOTAVAIL;
			break;
			default:
			p_redirinfo.present = INFO_PRESENT_ALLOWED;
			break;
		}
		switch (mncc->redirecting.screen) {
			case 0:
			p_redirinfo.screen = INFO_SCREEN_USER;
			break;
			case 1:
			p_redirinfo.screen = INFO_SCREEN_USER_VERIFIED_PASSED;
			break;
			case 2:
			p_redirinfo.screen = INFO_SCREEN_USER_VERIFIED_FAILED;
			break;
			default:
			p_redirinfo.screen = INFO_SCREEN_NETWORK;
			break;
		}
		switch (mncc->redirecting.type) {
			case 0x0:
			p_redirinfo.ntype = INFO_NTYPE_UNKNOWN;
			break;
			case 0x1:
			p_redirinfo.ntype = INFO_NTYPE_INTERNATIONAL;
			break;
			case 0x2:
			p_redirinfo.ntype = INFO_NTYPE_NATIONAL;
			break;
			case 0x4:
			p_redirinfo.ntype = INFO_NTYPE_SUBSCRIBER;
			break;
			default:
			p_redirinfo.ntype = INFO_NTYPE_UNKNOWN;
			break;
		}
		SCPY(p_redirinfo.id, mncc->redirecting.number);
		add_trace("redir", "type", "%d", mncc->redirecting.type);
		add_trace("redir", "plan", "%d", mncc->redirecting.plan);
		add_trace("redir", "present", "%d", mncc->redirecting.present);
		add_trace("redir", "screen", "%d", mncc->redirecting.screen);
		add_trace("redir", "number", "%s", mncc->redirecting.number);
		p_redirinfo.isdn_port = p_m_portnum;
	}
	/* bearer capability */
	if (mncc->fields & MNCC_F_BEARER_CAP) {
		switch (mncc->bearer_cap.transfer) {
			case 1:
			p_capainfo.bearer_capa = INFO_BC_DATAUNRESTRICTED;
			break;
			case 2:
			case 3:
			p_capainfo.bearer_capa = INFO_BC_AUDIO;
			p_capainfo.bearer_info1 = (options.law=='a')?3:2;
			break;
			default:
			p_capainfo.bearer_capa = INFO_BC_SPEECH;
			p_capainfo.bearer_info1 = (options.law=='a')?3:2;
			break;
		}
		switch (mncc->bearer_cap.mode) {
			case 1:
			p_capainfo.bearer_mode = INFO_BMODE_PACKET;
			break;
			default:
			p_capainfo.bearer_mode = INFO_BMODE_CIRCUIT;
			break;
		}
		add_trace("bearer", "transfer", "%d", mncc->bearer_cap.transfer);
		add_trace("bearer", "mode", "%d", mncc->bearer_cap.mode);
	} else {
		p_capainfo.bearer_capa = INFO_BC_SPEECH;
		p_capainfo.bearer_info1 = (options.law=='a')?3:2;
		p_capainfo.bearer_mode = INFO_BMODE_CIRCUIT;
	}
	/* if packet mode works some day, see dss1.cpp for conditions */
	p_capainfo.source_mode = B_MODE_TRANSPARENT;

	end_trace();

	/* hunt channel */
	ret = channel = hunt_bchannel();
	if (ret < 0)
		goto no_channel;

	/* open channel */
	ret = seize_bchannel(channel, 1);
	if (ret < 0) {
		no_channel:
		mncc = create_mncc(MNCC_REJ_REQ, p_m_g_callref);
		gsm_trace_header(p_m_mISDNport, this, MNCC_REJ_REQ, DIRECTION_OUT);
		mncc->fields |= MNCC_F_CAUSE;
		mncc->cause.coding = 3;
		mncc->cause.location = 1;
		mncc->cause.value = 34;
		add_trace("cause", "coding", "%d", mncc->cause.coding);
		add_trace("cause", "location", "%d", mncc->cause.location);
		add_trace("cause", "value", "%d", mncc->cause.value);
		add_trace("reason", NULL, "no channel");
		end_trace();
		send_and_free_mncc(p_m_g_instance, mncc->msg_type, mncc);
		new_state(PORT_STATE_RELEASE);
		trigger_work(&p_m_g_delete);
		return;
	}
	bchannel_event(p_m_mISDNport, p_m_b_index, B_EVENT_USE);
	if (bchannel_open(p_m_b_index))
		goto no_channel;

	/* what infos did we got ... */
	gsm_trace_header(p_m_mISDNport, this, msg_type, DIRECTION_IN);
	if (p_callerinfo.id[0])
		add_trace("calling", "number", "%s", p_callerinfo.id);
	else
		SPRINT(p_callerinfo.id, "imsi-%s", p_callerinfo.imsi);
	add_trace("calling", "imsi", "%s", p_callerinfo.imsi);
	add_trace("dialing", "number", "%s", p_dialinginfo.id);
	end_trace();

	/* create endpoint */
	if (p_epointlist)
		FATAL("Incoming call but already got an endpoint.\n");
	if (!(epoint = new Endpoint(p_serial, 0)))
		FATAL("No memory for Endpoint instance\n");
	if (!(epoint->ep_app = new DEFAULT_ENDPOINT_APP(epoint, 0))) //incoming
		FATAL("No memory for Endpoint Application instance\n");
	epointlist_new(epoint->ep_serial);

	/* modify lchan to GSM codec V1 */
	gsm_trace_header(p_m_mISDNport, this, MNCC_LCHAN_MODIFY, DIRECTION_OUT);
	mode = create_mncc(MNCC_LCHAN_MODIFY, p_m_g_callref);
	mode->lchan_mode = 0x01; /* GSM V1 */
	add_trace("mode", NULL, "0x%02x", mode->lchan_mode);
	end_trace();
	send_and_free_mncc(p_m_g_instance, mode->msg_type, mode);

	/* send call proceeding */
	gsm_trace_header(p_m_mISDNport, this, MNCC_CALL_PROC_REQ, DIRECTION_OUT);
	proceeding = create_mncc(MNCC_CALL_PROC_REQ, p_m_g_callref);
	if (p_m_mISDNport->tones) {
		proceeding->fields |= MNCC_F_PROGRESS;
		proceeding->progress.coding = 3; /* GSM */
		proceeding->progress.location = 1;
		proceeding->progress.descr = 8;
		add_trace("progress", "coding", "%d", proceeding->progress.coding);
		add_trace("progress", "location", "%d", proceeding->progress.location);
		add_trace("progress", "descr", "%d", proceeding->progress.descr);
	}
	end_trace();
	send_and_free_mncc(p_m_g_instance, proceeding->msg_type, proceeding);

	new_state(PORT_STATE_IN_PROCEEDING);

	if (p_m_mISDNport->tones && !p_m_g_tch_connected) { /* only if ... */
		gsm_trace_header(p_m_mISDNport, this, MNCC_FRAME_RECV, DIRECTION_OUT);
		end_trace();
		frame = create_mncc(MNCC_FRAME_RECV, p_m_g_callref);
		send_and_free_mncc(p_m_g_instance, frame->msg_type, frame);
		p_m_g_tch_connected = 1;
	}

	/* send setup message to endpoit */
	message = message_create(p_serial, ACTIVE_EPOINT(p_epointlist), PORT_TO_EPOINT, MESSAGE_SETUP);
	message->param.setup.isdn_port = p_m_portnum;
	message->param.setup.port_type = p_type;
//	message->param.setup.dtmf = 0;
	memcpy(&message->param.setup.dialinginfo, &p_dialinginfo, sizeof(struct dialing_info));
	memcpy(&message->param.setup.callerinfo, &p_callerinfo, sizeof(struct caller_info));
	memcpy(&message->param.setup.capainfo, &p_capainfo, sizeof(struct capa_info));
	SCPY((char *)message->param.setup.useruser.data, (char *)mncc->useruser.info);
	message->param.setup.useruser.len = strlen(mncc->useruser.info);
	message->param.setup.useruser.protocol = mncc->useruser.proto;
	message_put(message);
}

/*
 * MS sends message to port
 */
static int message_ms(struct osmocom_ms *ms, int msg_type, void *arg)
{
	struct gsm_mncc *mncc = (struct gsm_mncc *)arg;
	unsigned int callref = mncc->callref;
	class Port *port;
	class Pgsm_ms *pgsm_ms = NULL;
	char name[64];
	struct mISDNport *mISDNport;

	/* Special messages */
	if (msg_type) {
	}

	/* find callref */
	callref = mncc->callref;
	port = port_first;
	while(port) {
		if ((port->p_type & PORT_CLASS_GSM_MASK) == PORT_CLASS_GSM_MS) {
			pgsm_ms = (class Pgsm_ms *)port;
			if (pgsm_ms->p_m_g_callref == callref) {
				break;
			}
		}
		port = port->next;
	}

	if (msg_type == GSM_TCHF_FRAME) {
		if (port)
			pgsm_ms->frame_receive((struct gsm_trau_frame *)arg);
		return 0;
	}

	if (!port) {
		if (msg_type != MNCC_SETUP_IND)
			return(0);
		/* find gsm ms port */
		mISDNport = mISDNport_first;
		while(mISDNport) {
			if (mISDNport->gsm_ms && !strcmp(mISDNport->ifport->gsm_ms_name, ms->name))
				break;
			mISDNport = mISDNport->next;
		}
		if (!mISDNport) {
			struct gsm_mncc *rej;

			rej = create_mncc(MNCC_REJ_REQ, callref);
			rej->fields |= MNCC_F_CAUSE;
			rej->cause.coding = 3;
			rej->cause.location = 1;
			rej->cause.value = 27;
			gsm_trace_header(NULL, NULL, MNCC_REJ_REQ, DIRECTION_OUT);
			add_trace("cause", "coding", "%d", rej->cause.coding);
			add_trace("cause", "location", "%d", rej->cause.location);
			add_trace("cause", "value", "%d", rej->cause.value);
			end_trace();
			send_and_free_mncc(ms, rej->msg_type, rej);
			return 0;
		}
		/* creating port object, transparent until setup with hdlc */
		SPRINT(name, "%s-%d-in", mISDNport->ifport->interface->name, mISDNport->portnum);
		if (!(pgsm_ms = new Pgsm_ms(PORT_TYPE_GSM_MS_IN, mISDNport, name, NULL, 0, 0, B_MODE_TRANSPARENT)))

			FATAL("Cannot create Port instance.\n");
	}

	switch(msg_type) {
		case MNCC_SETUP_IND:
		pgsm_ms->setup_ind(msg_type, callref, mncc);
		break;

		case MNCC_ALERT_IND:
		pgsm_ms->alert_ind(msg_type, callref, mncc);
		break;

		case MNCC_SETUP_CNF:
		pgsm_ms->setup_cnf(msg_type, callref, mncc);
		break;

		case MNCC_SETUP_COMPL_IND:
		pgsm_ms->setup_compl_ind(msg_type, callref, mncc);
		break;

		case MNCC_DISC_IND:
		pgsm_ms->disc_ind(msg_type, callref, mncc);
		break;

		case MNCC_REL_IND:
		case MNCC_REL_CNF:
		case MNCC_REJ_IND:
		pgsm_ms->rel_ind(msg_type, callref, mncc);
		break;

		case MNCC_NOTIFY_IND:
		pgsm_ms->notify_ind(msg_type, callref, mncc);
		break;

		default:
		;
	}
	return(0);
}

/* MESSAGE_SETUP */
void Pgsm_ms::message_setup(unsigned int epoint_id, int message_id, union parameter *param)
{
	struct lcr_msg *message;
	int ret;
	struct epoint_list *epointlist;
	struct gsm_mncc *mncc;
	int channel;

	/* copy setup infos to port */
	memcpy(&p_callerinfo, &param->setup.callerinfo, sizeof(p_callerinfo));
	memcpy(&p_dialinginfo, &param->setup.dialinginfo, sizeof(p_dialinginfo));
	memcpy(&p_capainfo, &param->setup.capainfo, sizeof(p_capainfo));
	memcpy(&p_redirinfo, &param->setup.redirinfo, sizeof(p_redirinfo));

	/* no number */
	if (!p_dialinginfo.id[0]) {
		gsm_trace_header(p_m_mISDNport, this, MNCC_SETUP_REQ, DIRECTION_OUT);
		add_trace("failure", NULL, "No dialed subscriber given.");
		end_trace();
		message = message_create(p_serial, epoint_id, PORT_TO_EPOINT, MESSAGE_RELEASE);
		message->param.disconnectinfo.cause = 28;
		message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		message_put(message);
		new_state(PORT_STATE_RELEASE);
		trigger_work(&p_m_g_delete);
		return;
	}
	
	/* release if port is blocked */
	if (p_m_mISDNport->ifport->block) {
		gsm_trace_header(p_m_mISDNport, this, MNCC_SETUP_REQ, DIRECTION_OUT);
		add_trace("failure", NULL, "Port blocked.");
		end_trace();
		message = message_create(p_serial, epoint_id, PORT_TO_EPOINT, MESSAGE_RELEASE);
		message->param.disconnectinfo.cause = 27; // temp. unavail.
		message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		message_put(message);
		new_state(PORT_STATE_RELEASE);
		trigger_work(&p_m_g_delete);
		return;
	}

	/* hunt channel */
	ret = channel = hunt_bchannel();
	if (ret < 0)
		goto no_channel;
	/* open channel */
	ret = seize_bchannel(channel, 1);
	if (ret < 0) {
		no_channel:
		gsm_trace_header(p_m_mISDNport, this, MNCC_SETUP_REQ, DIRECTION_OUT);
		add_trace("failure", NULL, "No internal audio channel available.");
		end_trace();
		message = message_create(p_serial, epoint_id, PORT_TO_EPOINT, MESSAGE_RELEASE);
		message->param.disconnectinfo.cause = 34;
		message->param.disconnectinfo.location = LOCATION_PRIVATE_LOCAL;
		message_put(message);
		new_state(PORT_STATE_RELEASE);
		trigger_work(&p_m_g_delete);
		return;
	}
	bchannel_event(p_m_mISDNport, p_m_b_index, B_EVENT_USE);
	if (bchannel_open(p_m_b_index))
		goto no_channel;

	/* attach only if not already */
	epointlist = p_epointlist;
	while(epointlist) {
		if (epointlist->epoint_id == epoint_id)
			break;
		epointlist = epointlist->next;
	}
	if (!epointlist)
		epointlist_new(epoint_id);

	/* creating l3id */
	l1l2l3_trace_header(p_m_mISDNport, this, L3_NEW_L3ID_REQ, DIRECTION_OUT);
	p_m_g_callref = new_callref++;
	add_trace("callref", "new", "0x%x", p_m_g_callref);
	end_trace();

	gsm_trace_header(p_m_mISDNport, this, MNCC_SETUP_REQ, DIRECTION_OUT);
	mncc = create_mncc(MNCC_SETUP_REQ, p_m_g_callref);
	if (!strncasecmp(p_dialinginfo.id, "emerg", 5)) {
		mncc->emergency = 1;
	} else {
		/* caller info (only clir) */
		switch (p_callerinfo.present) {
			case INFO_PRESENT_ALLOWED:
			mncc->clir.inv = 1;
			break;
			default:
			mncc->clir.sup = 1;
		}
		/* dialing information (mandatory) */
		mncc->fields |= MNCC_F_CALLED;
		mncc->called.type = 0; /* unknown */
		mncc->called.plan = 1; /* isdn */
		SCPY(mncc->called.number, p_dialinginfo.id);
		add_trace("dialing", "number", "%s", mncc->called.number);
		
		/* bearer capability (mandatory) */
		mncc->fields |= MNCC_F_BEARER_CAP;
		mncc->bearer_cap.coding = 0;
		mncc->bearer_cap.radio = 1;
		mncc->bearer_cap.speech_ctm = 0;
		mncc->bearer_cap.speech_ver[0] = 0;
		mncc->bearer_cap.speech_ver[1] = -1; /* end of list */
		switch (p_capainfo.bearer_capa) {
			case INFO_BC_DATAUNRESTRICTED:
			case INFO_BC_DATARESTRICTED:
			mncc->bearer_cap.transfer = 1;
			break;
			case INFO_BC_SPEECH:
			mncc->bearer_cap.transfer = 0;
			break;
			default:
			mncc->bearer_cap.transfer = 2;
			break;
		}
		switch (p_capainfo.bearer_mode) {
			case INFO_BMODE_PACKET:
			mncc->bearer_cap.mode = 1;
			break;
			default:
			mncc->bearer_cap.mode = 0;
			break;
		}
	}

	end_trace();
	send_and_free_mncc(p_m_g_instance, mncc->msg_type, mncc);

	new_state(PORT_STATE_OUT_SETUP);

	message = message_create(p_serial, ACTIVE_EPOINT(p_epointlist), PORT_TO_EPOINT, MESSAGE_PROCEEDING);
	message_put(message);

	new_state(PORT_STATE_OUT_PROCEEDING);
}

/*
 * endpoint sends messages to the port
 */
int Pgsm_ms::message_epoint(unsigned int epoint_id, int message_id, union parameter *param)
{
	if (Pgsm::message_epoint(epoint_id, message_id, param))
		return(1);

	switch(message_id) {
		case MESSAGE_SETUP: /* dial-out command received from epoint */
		if (p_state!=PORT_STATE_IDLE)
			break;
		message_setup(epoint_id, message_id, param);
		break;

		default:
		PDEBUG(DEBUG_GSM, "Pgsm_ms(%s) gsm port with (caller id %s) received unhandled nessage: %d\n", p_name, p_callerinfo.id, message_id);
	}

	return(1);
}

int gsm_ms_exit(int rc)
{

	return(rc);
}

int gsm_ms_init(void)
{
	INIT_LLIST_HEAD(&ms_list);
	log_init(&log_info);
	stderr_target = log_target_create_stderr();
	log_add_target(stderr_target);
	log_set_all_filter(stderr_target, 1);

	l23_ctx = talloc_named_const(NULL, 1, "layer2 context");

	return 0;
}

/* add a new GSM mobile instance */
int gsm_ms_new(const char *name, const char *socket_path)
{
	struct osmocom_ms *ms = NULL;
	int rc;

	PDEBUG(DEBUG_GSM, "GSM: creating new instance '%s' on '%s'\n", name, socket_path);

	ms = talloc_zero(l23_ctx, struct osmocom_ms);
	if (!ms) {
		FATAL("Failed to allocate MS\n");
	}
	/* must add here, because other init processes may search in the list */
	llist_add_tail(&ms->entity, &ms_list);


	SPRINT(ms->name, name);

	rc = layer2_open(ms, socket_path);
	if (rc < 0) {
		FATAL("Failed during layer2_open()\n");
	}

	lapdm_init(&ms->l2_entity.lapdm_dcch, ms);
	lapdm_init(&ms->l2_entity.lapdm_acch, ms);

	rc = l23_app_init(ms);
	if (rc < 0) {
		FATAL("Failed to init layer23\n");
	}
	ms->cclayer.mncc_recv = message_ms;

	return 0;
}

int gsm_ms_delete(const char *name)
{
	struct osmocom_ms *ms = NULL;
	int found = 0;

	PDEBUG(DEBUG_GSM, "GSM: destroying instance '%s'\n", name);

	llist_for_each_entry(ms, &ms_list, entity) {
		if (!strcmp(ms->name, name)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		FATAL("Failed delete layer23, instance '%s' not found\n", name);
	}

	l23_app_exit(ms);
	lapdm_exit(&ms->l2_entity.lapdm_dcch);
	lapdm_exit(&ms->l2_entity.lapdm_acch);

	llist_del(&ms->entity);

	return 0;
}

/*
 * handles bsc select function within LCR's main loop
 */
int handle_gsm_ms(void)
{
	struct osmocom_ms *ms = NULL;
	int work = 0;

	llist_for_each_entry(ms, &ms_list, entity) {
		if (l23_app_work(ms))
			work = 1;
//		debug_reset_context();
		if (bsc_select_main(1)) /* polling */
			work = 1;
	}

	return work;
}

