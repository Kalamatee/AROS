SRCS = main.c task_page.c memory_page.c misc_page.c globals.c mui.c config.c\
       aloha.c menus.c cat/VMM_Cat.c

OBJS = main.o task_page.o memory_page.o misc_page.o globals.o mui.o config.o\
       aloha.o menus.o cat/VMM_Cat.o

HDRS = defs.h protos.h cat/VMM_Cat.h /shared_defs.h

C_DEFINES=
CFLAGS = NOICONS CPU 68030 ANSI STRICT NOSTACKCHECK COMMENTNEST \
         UTILLIB $(C_DEFINES) GST VMM.gst nodebug data faronly \
         idir=MUI:Developer/C/Include

CATALOGS = LOCALE:catalogs/deutsch/VMM.catalog \
           LOCALE:catalogs/français/VMM.catalog \
           LOCALE:catalogs/dansk/VMM.catalog \
           LOCALE:catalogs/svenska/VMM.catalog \
           LOCALE:catalogs/italiano/VMM.catalog \
           LOCALE:catalogs/español/VMM.catalog \
           LOCALE:catalogs/russian/VMM.catalog

TARGETS = VMM.gst VMM $(CATALOGS)

.c.o:
  sc nolink $(CFLAGS) $*.c

all: $(TARGETS)

VMM: $(OBJS)
  sc link data faronly pname VMM $(OBJS)
  copy VMM dh0:bin

VMM.gst : dmp.c
  gst >nil: VMM.gst unload
  sc MGST=VMM.gst $(CFLAGS) dmp.c
  delete >NIL: dmp.o

$(OBJS) : $(HDRS)

cat/VMM_Cat.o : cat/VMM_Cat.h cat/VMM_Cat.h
  sc nolink $(CFLAGS) cat/VMM_Cat.c

cat/VMM_Cat.h cat/VMM_Cat.c : cat/VMM.cd 
  flexcat cat/VMM.cd cat/VMM_Cat.c=C_c_V21.sd cat/VMM_Cat.h=C_h.sd

LOCALE:catalogs/deutsch/VMM.catalog : cat/deutsch.ct cat/VMM.cd
  flexcat cat/VMM.cd cat/deutsch.ct catalog LOCALE:catalogs/deutsch/VMM.catalog \
       newctfile cat/deutsch.ct

LOCALE:catalogs/français/VMM.catalog : cat/français.ct cat/VMM.cd
  flexcat cat/VMM.cd cat/français.ct catalog LOCALE:catalogs/français/VMM.catalog \
       newctfile cat/français.ct

LOCALE:catalogs/dansk/VMM.catalog : cat/dansk.ct cat/VMM.cd
  flexcat cat/VMM.cd cat/dansk.ct catalog LOCALE:catalogs/dansk/VMM.catalog \
       newctfile cat/dansk.ct

LOCALE:catalogs/svenska/VMM.catalog : cat/svenska.ct cat/VMM.cd
  flexcat cat/VMM.cd cat/svenska.ct catalog LOCALE:catalogs/svenska/VMM.catalog \
       newctfile cat/svenska.ct

LOCALE:catalogs/italiano/VMM.catalog : cat/italiano.ct cat/VMM.cd
  flexcat cat/VMM.cd cat/italiano.ct catalog LOCALE:catalogs/italiano/VMM.catalog \
       newctfile cat/italiano.ct

LOCALE:catalogs/español/VMM.catalog : cat/español.ct cat/VMM.cd
  flexcat cat/VMM.cd cat/español.ct catalog LOCALE:catalogs/español/VMM.catalog \
       newctfile cat/español.ct

LOCALE:catalogs/russian/VMM.catalog : cat/russian.ct cat/VMM.cd
  flexcat cat/VMM.cd cat/russian.ct catalog LOCALE:catalogs/russian/VMM.catalog \
       newctfile cat/russian.ct

clean:
  rm -f $(OBJS)
  rm -f $(TARGETS)
