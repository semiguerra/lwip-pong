#  Modifications made by Jose Miguel Guerra Ramírez to support a TCP-based Pong server 
# using the netconn API in the LWIP-TAP environment.
# 
# Based on the original lwip-tap.c by Takayuki Usui and contributors to the lwIP project.
#
#
# Copyright (c) 2012-2013 Takayuki Usui
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
srcdir = .
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
sbindir = $(exec_prefix)/sbin
libexecdir = $(exec_prefix)/libexec
datadir = $(prefix)/share

IP_VERSION = 4
CC = gcc
CPPFLAGS = -DHAVE_CONFIG_H -I. -Ilwip-contrib/ports/unix/include \
  -Ilwip/src/include/ipv$(IP_VERSION) -Ilwip/src/include \
  -Ilwip-contrib/apps/chargen -Ilwip-contrib/apps/httpserver \
  -Ilwip-contrib/apps/tcpecho -Ilwip-contrib/apps/udpecho \
  -Ilwip-contrib/apps/pong
CFLAGS = -pthread -Wall -g -O2
LDFLAGS = -pthread 
LIBS = 
INSTALL = /usr/bin/install -c
SOURCES = \
  lwip/src/api/api_lib.c \
  lwip/src/api/api_msg.c \
  lwip/src/api/err.c \
  lwip/src/api/netbuf.c \
  lwip/src/api/netdb.c \
  lwip/src/api/netifapi.c \
  lwip/src/api/sockets.c \
  lwip/src/api/tcpip.c \
  lwip/src/core/def.c \
  lwip/src/core/dhcp.c \
  lwip/src/core/dns.c \
  lwip/src/core/init.c \
  lwip/src/core/mem.c \
  lwip/src/core/memp.c \
  lwip/src/core/netif.c \
  lwip/src/core/pbuf.c \
  lwip/src/core/raw.c \
  lwip/src/core/stats.c \
  lwip/src/core/sys.c \
  lwip/src/core/tcp.c \
  lwip/src/core/tcp_in.c \
  lwip/src/core/tcp_out.c \
  lwip/src/core/timers.c \
  lwip/src/core/udp.c \
  lwip/src/core/ipv4/autoip.c \
  lwip/src/core/ipv4/icmp.c \
  lwip/src/core/ipv4/igmp.c \
  lwip/src/core/ipv4/inet.c \
  lwip/src/core/ipv4/inet_chksum.c \
  lwip/src/core/ipv4/ip.c \
  lwip/src/core/ipv4/ip_addr.c \
  lwip/src/core/ipv4/ip_frag.c \
  lwip/src/core/snmp/asn1_dec.c \
  lwip/src/core/snmp/asn1_enc.c \
  lwip/src/core/snmp/mib2.c \
  lwip/src/core/snmp/mib_structs.c \
  lwip/src/core/snmp/msg_in.c \
  lwip/src/core/snmp/msg_out.c \
  lwip/src/netif/etharp.c \
  lwip-contrib/ports/unix/sys_arch.c \
  lwip-contrib/apps/chargen/chargen.c \
  lwip-contrib/apps/httpserver/httpserver-netconn.c \
  lwip-contrib/apps/tcpecho/tcpecho.c \
  lwip-contrib/apps/udpecho/udpecho.c \
  tapif.c \
  lwip-tap.c \
  lwip-contrib/apps/pong/pong.c

# Definir VPATH para encontrar los archivos fuente en sus directorios originales
VPATH = $(sort $(dir $(SOURCES)))

OBJS := $(notdir $(SOURCES:.c=.o))

# Regla de compilación corregida
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: all check-syntax depend dep clean distclean

all: lwip-tap

lwip-tap: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $^ -lm

check-syntax:
	$(CC) $(CFLAGS) $(CPPFLAGS) -fsyntax-only $(CHK_SOURCES)

depend dep:
	$(CC) $(CFLAGS) $(CPPFLAGS) -MM $(SOURCES) >.depend

clean:
	rm -f config.cache config.log
	rm -f lwip-tap $(OBJS) *~

distclean: clean
	rm -f Makefile config.h config.status
	rm -rf autom4te.cache

ifeq (.depend,$(wildcard .depend))
include .depend
endif
