/*
 * v8spandsp.c -- SpanDSP's end of the interop calls.  See v8spandsp.h.
 */

#include <string.h>

#include "v8spandsp.h"

void
v8neg_spandsp_parms(v8_parms_t *parms, uint32_t modulations)
{
	memset(parms, 0, sizeof(*parms));
	parms->modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
	parms->send_ci = false;
	parms->v92 = -1;
	parms->jm_cm.call_function = V8_CALL_V_SERIES;
	parms->jm_cm.modulations = modulations;
	parms->jm_cm.protocols = V8_PROTOCOL_LAPM_V42;
}

const char *
v8neg_status_name(int st)
{
	switch (st) {
	case V8_STATUS_IN_PROGRESS:	return "in progress";
	case V8_STATUS_V8_OFFERED:	return "V.8 offered";
	case V8_STATUS_V8_CALL:		return "V.8 negotiated";
	case V8_STATUS_NON_V8_CALL:	return "not a V.8 call";
	case V8_STATUS_FAILED:		return "failed";
	case V8_STATUS_CALL_FUNCTION_RECEIVED: return "call function seen";
	case V8_STATUS_CALLING_TONE_RECEIVED: return "calling tone";
	case V8_STATUS_FAX_CNG_TONE_RECEIVED: return "CNG";
	default:			return "nothing yet";
	}
}

