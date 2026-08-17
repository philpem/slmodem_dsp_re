###########################################################################
#
#
#       Makefile  --  modem application Makefile.
#
#       Copyright(c) 2003, Smart Link Ltd. (www.smlink.com)
#	All rights reserved.
#
#       Author: Sasha K (sashak@smlink.com)
#
#
###########################################################################
#
###########################################################################

ARCH64:=$(shell uname -m | sed -e '/amd64/s//x86_64/' | grep -c x86_64)

ifeq ($(ARCH64), 0)
CC:= gcc
else
# SUPPORT_ALSA:=1
CC:= gcc -m32
endif

RM:= rm -f

CFLAGS+= -Wall -g -O -I. -DCONFIG_DEBUG_MODEM

modem-objs:= \
	modem.o modem_datafile.o modem_at.o modem_timer.o \
	modem_pack.o modem_ec.o modem_comp.o \
	modem_param.o modem_debug.o homolog_data.o
dp-objs:= dp_sinus.o dp_dummy.o
sysdep-objs:= sysdep_common.o
re-base-objs:= modem_cmdline.o $(modem-objs) $(dp-objs) $(sysdep-objs)
all-objs:= $(re-base-objs) dsplibs.o
RE_DSP_RE_OBJS:= ../re/dsplibs_re.o ../re/sl_common.o ../re/sl_runtime.o ../re/sl_v8.o ../re/sl_v8_msg.o ../re/sl_v8_pcm.o ../re/sl_v8_seq.o ../re/sl_v8_fsk.o ../re/sl_v8_state.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o ../re/sl_v92_tx.o ../re/sl_vpcm.o ../re/sl_stubs.o
RE_DSP_OBJS?= dsplibs.o

all: slmodemd modem_test

slmodemd: modem_main.o $(all-objs)
modem_test: modem_test.o $(all-objs)

../re/dp_equiv: ../re/dp_equiv.o ../re/sl_v8_fsk.o $(re-base-objs) $(RE_DSP_OBJS)
	$(CC) -o $@ ../re/dp_equiv.o ../re/sl_v8_fsk.o $(re-base-objs) $(filter-out ../re/sl_v8_fsk.o,$(RE_DSP_OBJS)) $(LFLAGS)

../re/dsp_probe: ../re/dsp_probe.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/dsp_probe.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/dp_hybrid: ../re/dp_equiv.o ../re/dsplibs_wrap.o $(re-base-objs) dsplibs.o
	$(CC) -Wl,--allow-multiple-definition -o $@ ../re/dp_equiv.o ../re/dsplibs_wrap.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_msg_equiv: ../re/v8_msg_equiv.o ../re/sl_v8_msg.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_msg_equiv.o ../re/sl_v8_msg.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_fsk_equiv: ../re/v8_fsk_equiv.o ../re/sl_v8_fsk.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_fsk_equiv.o ../re/sl_v8_fsk.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_fsk_rx_equiv: ../re/v8_fsk_rx_equiv.o ../re/sl_v8_fsk.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_fsk_rx_equiv.o ../re/sl_v8_fsk.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_fsk_8k_selftest: ../re/v8_fsk_8k_selftest.o ../re/sl_v8_fsk.o
	$(CC) -o $@ ../re/v8_fsk_8k_selftest.o ../re/sl_v8_fsk.o $(LFLAGS)

../re/v8_seq_equiv: ../re/v8_seq_equiv.o ../re/sl_v8_seq.o ../re/sl_v8_msg.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_seq_equiv.o ../re/sl_v8_seq.o ../re/sl_v8_msg.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_init_probe: ../re/v8_init_probe.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_init_probe.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_handshake_trace: ../re/v8_handshake_trace.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_handshake_trace.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_public_pair: ../re/v8_public_pair.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_public_pair.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_update_probe: ../re/v8_update_probe.o ../re/sl_v8_state.o ../re/sl_v8_pcm.o ../re/sl_v8_fsk.o ../re/sl_v8_msg.o ../re/sl_common.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v8_update_probe.o ../re/sl_v8_state.o ../re/sl_v8_pcm.o ../re/sl_v8_fsk.o ../re/sl_v8_msg.o ../re/sl_common.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v8_pcm_state_probe: ../re/v8_pcm_state_probe.o ../re/sl_v8_state.o ../re/sl_v8_pcm.o ../re/sl_v8_fsk.o ../re/sl_v8_msg.o ../re/sl_common.o
	$(CC) -o $@ ../re/v8_pcm_state_probe.o ../re/sl_v8_state.o ../re/sl_v8_pcm.o ../re/sl_v8_fsk.o ../re/sl_v8_msg.o ../re/sl_common.o $(LFLAGS)

../re/v92_cp_probe: ../re/v92_cp_probe.o ../re/sl_v92_cp.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v92_cp_probe.o ../re/sl_v92_cp.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v92_filter_selftest: ../re/v92_filter_selftest.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o
	$(CC) -o $@ ../re/v92_filter_selftest.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o $(LFLAGS)

../re/v92_filter_equiv: ../re/v92_filter_equiv.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v92_filter_equiv.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o $(re-base-objs) dsplibs.o $(LFLAGS)

../re/v92_tx_selftest: ../re/v92_tx_selftest.o ../re/sl_v92_tx.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o
	$(CC) -o $@ ../re/v92_tx_selftest.o ../re/sl_v92_tx.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o $(LFLAGS)

../re/v92_tx_equiv: ../re/v92_tx_equiv.o ../re/sl_v92_tx.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o $(re-base-objs) dsplibs.o
	$(CC) -o $@ ../re/v92_tx_equiv.o ../re/sl_v92_tx.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o $(re-base-objs) dsplibs.o $(LFLAGS)

ifdef SUPPORT_ALSA
CFLAGS+= -DSUPPORT_ALSA=1
ifeq ($(ARCH64), 0)
LFLAGS+= /usr/lib/libasound.so
else
# LFLAGS+= /usr/lib32/libasound.so , replaced by -l commands search for asound
LFLAGS+= -lasound
endif
endif

slmodemd:
	$(CC) -o slmodemd modem_main.o $(all-objs) $(LFLAGS)

modem_test:
	$(CC) -o modem_test modem_test.o $(all-objs) $(LFLAGS)

clean:
	$(RM) slmodemd modem_test modem_main.o modem_cmdline.o modem_test.o $(modem-objs) $(dp-objs) $(sysdep-objs)
	$(RM) ../re/dp_equiv ../re/dp_equiv.o ../re/dp_hybrid ../re/dsp_probe ../re/dsp_probe.o ../re/v8_msg_equiv ../re/v8_msg_equiv.o ../re/v8_fsk_equiv ../re/v8_fsk_equiv.o ../re/v8_fsk_rx_equiv ../re/v8_fsk_rx_equiv.o ../re/v8_fsk_8k_selftest ../re/v8_fsk_8k_selftest.o ../re/v8_seq_equiv ../re/v8_seq_equiv.o ../re/v8_init_probe ../re/v8_init_probe.o ../re/v8_handshake_trace ../re/v8_handshake_trace.o ../re/dsplibs_re.o ../re/dsplibs_wrap.o
	$(RM) ../re/v8_public_pair ../re/v8_public_pair.o
	$(RM) ../re/v8_update_probe ../re/v8_update_probe.o
	$(RM) ../re/v8_pcm_state_probe ../re/v8_pcm_state_probe.o
	$(RM) ../re/v92_cp_probe ../re/v92_cp_probe.o ../re/sl_v92_cp.o
	$(RM) ../re/v92_filter_selftest ../re/v92_filter_selftest.o ../re/v92_filter_equiv ../re/v92_filter_equiv.o ../re/v92_tx_selftest ../re/v92_tx_selftest.o ../re/v92_tx_equiv ../re/v92_tx_equiv.o ../re/sl_v90_cp_payload.o ../re/sl_v92_filter.o ../re/sl_v92_tx.o
	$(RM) ../re/sl_common.o ../re/sl_runtime.o ../re/sl_v8.o ../re/sl_v8_msg.o ../re/sl_v8_pcm.o ../re/sl_v8_seq.o ../re/sl_v8_fsk.o ../re/sl_v8_state.o ../re/sl_vpcm.o ../re/sl_stubs.o
	$(RM) *~ *.orig *.rej

.PHONY: all dep generic-dep clean clean-build-profile


#
# rules
#

%.o: %.c .build_profile
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -o $@ -c $<

../re/%.o: ../re/%.c $(wildcard ../re/*.h) .build_profile
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -o $@ -c $<

dep: generic-dep
generic-dep:
	$(RM) .depend
	$(foreach src,$(wildcard *.c),$(CC) -M $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$(src:.c=.o)) -c $(src) >> .depend ;)

-include .depend

# build_profile
-include .build_profile

.build_profile::
ifneq ($(CFLAGS),$(PROFILE_CFLAGS))
	@if [ "$(CFLAGS) $(EXTRA_CFLAGS)" != "$(PROFILE_CFLAGS)" ] ; then \
	  echo "rebuild profile..." ; \
	  echo "PROFILE_CFLAGS=$(CFLAGS) $(EXTRA_CFLAGS)" > $(@) ; fi
endif

clean: clean-build-profile
clean-build-profile:
	@$(RM) -f .build_profile
