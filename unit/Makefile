# -*- Mode: makefile-gmake -*-

all:
%:
	@$(MAKE) -C unit_bits $*
	@$(MAKE) -C unit_encode $*

clean: unitclean
	rm -f coverage/*.gcov
	rm -fr coverage/report
