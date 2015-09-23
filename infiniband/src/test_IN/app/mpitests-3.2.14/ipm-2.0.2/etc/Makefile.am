
ipmetcdir = $(prefix)/etc

WRAPPER_TEMPLATES = fake_*.c wrap_*.c

WRAPPER_HELPER =  ipm_key_*
             
EXTRA_DIST = $(WRAPPER_HELPER) $(WRAPPER_TEMPLATES)
             
install-exec-hook:
	mkdir -p $(DESTDIR)$(prefix)/etc
	for i in $(WRAPPER_HELPER); do \
		cp $$i $(DESTDIR)$(prefix)/etc; \
	done

uninstall-local:
	for i in $(WRAPPER_HELPER); do \
		rm -f $(DESTDIR)$(prefix)/etc/$$i; \
	done



             
