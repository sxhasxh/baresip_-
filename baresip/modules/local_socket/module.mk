#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= local_socket
$(MOD)_SRCS	+= local_socket.c
$(MOD)_LFLAGS	+=

include mk/mod.mk
