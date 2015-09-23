prefix		= $(DESTDIR)/usr/local
exec_prefix	= $(DESTDIR)${prefix}
bindir		= $(DESTDIR)${exec_prefix}/bin
mandir		= $(DESTDIR)${prefix}/share/man
incdir		= $(DESTDIR)${prefix}/include

all: all-programs

CFLAGS = -O2 -Wall -Iinclude -g
CPPFLAGS = -DDEBUG_EXE -DRDS_VERSION=\"2.0.7\" -MD -MP -MF $(@D)/.$(basename $(@F)).d

HEADERS = kernel-list.h pfhack.h include/rds.h
COMMON_SOURCES = pfhack.c
SOURCES = $(addsuffix .c,$(PROGRAMS)) $(COMMON_SOURCES)
CLEAN_OBJECTS = $(addsuffix .o,$(PROGRAMS)) $(subst .c,.o,$(COMMON_SOURCES))

# This is the default
DYNAMIC_PF_RDS = true

ifneq ($(DYNAMIC_PF_RDS),)
CPPFLAGS += -DDYNAMIC_PF_RDS
COMMON_OBJECTS = $(subst .c,.o,$(COMMON_SOURCES))
else
COMMON_OBJECTS = $(subst .c,.o,$(filter-out pfhack.c,$(COMMON_SOURCES)))
endif

PROGRAMS = rds-info rds-stress rds-ping

all-programs: $(PROGRAMS)

install: $(PROGRAMS)
	install -d $(bindir)
	install -m 755 $(PROGRAMS) $(bindir)
	install -d $(mandir)/man1
	install -d $(mandir)/man7
	install -m 644 *.1 $(mandir)/man1
	install -m 644 *.7 $(mandir)/man7
	install -d $(incdir)/net
	install -m 444 include/rds.h $(incdir)/net

clean:
	rm -f $(PROGRAMS) $(CLEAN_OBJECTS)

distclean: clean
	rm -f .*.d



$(PROGRAMS) : % : %.o $(COMMON_OBJECTS)
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^

LOCAL_DFILES := $(wildcard .*.d)
ifneq ($(LOCAL_DFILES),)
.PHONY: $(LOCAL_DFILES)
-include $(LOCAL_DFILES)
endif

VERSION := 2.0.7
RELEASE := 1.2

TAR_PREFIX := rds-tools-$(VERSION)
TAR_FILE := $(TAR_PREFIX).tar.gz

EXTRA_DIST := 	rds-info.1 \
		rds-stress.1 \
		rds-ping.1 \
		rds.7 \
		rds-rdma.7 \
		Makefile.in \
		rds-tools.spec.in \
		configure.in \
		configure \
		README \
		rds-tools.txt \
		docs/rds-architecture.txt \
		examples/Makefile \
		examples/rds-sample.c \
		examples/README

DISTFILES := $(SOURCES) $(HEADERS) $(EXTRA_DIST)

$(TAR_FILE): Makefile rds-tools.spec
	@rm -rf $@ $(TAR_PREFIX) || :
	@mkdir $(TAR_PREFIX)
	for a in $^ $(DISTFILES); do    \
                if [ ! -f $$a ]; then                                   \
                        continue;                                       \
                fi;                                                     \
                targ=$(TAR_PREFIX)/$$(dirname $$a);                     \
                mkdir -p $$targ;                                        \
                cp $$a $$targ;                                          \
        done

	tar -zcf $@ $(TAR_PREFIX)

.PHONY: rpm
rpm: $(TAR_FILE)
	rpmbuild -ta $^

.PHONY: dist
dist: $(TAR_FILE)

