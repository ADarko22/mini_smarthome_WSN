CONTIKI_PROJECT = CU Node1 Node2

all: $(CONTIKI_PROJECT)

CONTIKI = /home/user/contiki

CONTIKI_WITH_RIME = 1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

include $(CONTIKI)/Makefile.include