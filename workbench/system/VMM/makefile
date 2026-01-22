SRCS = startup.asm sv_regs30.asm sv_regs40.asm sv_regs60.asm traphandler30.asm \
       traphandler40.asm traphandler60.asm parthandler.asm mem_trace.asm switch_patch.asm \
       loadseg_patch.asm dma_patch.asm wb_patch.asm sv_regs851.asm \
       pagehandler.c globals.c pageio.c find_dev_params.c debug.c \
       prepager.c forbidden_tasks.c timer.c fault.c VM_Manager.c \
       config.c stat.c mmu_table.c bitmap.c reset_handler.c ffs.c cache.c \
       mem_tracking.c VM_Manager_Init.c rexx.c cat/VMM_Cat.c

C_HDRS = cat/VMM_Cat.h defs.h shared_defs.h protos.h errors.h 
ASM_HDRS = shared_defs.i macros.i

ASMOBJS = startup.o sv_regs30.o sv_regs40.o sv_regs60.o sv_regs851.o traphandler30.o\
		traphandler40.o traphandler60.o parthandler.o mem_trace.o switch_patch.o \
          loadseg_patch.o dma_patch.o wb_patch.o dyn_mmu_trap40.o \
          dyn_mmu_trap30.o dyn_mmu_trap60.o

COBJS = pagehandler.o globals.o pageio.o find_dev_params.o debug.o prepager.o \
        forbidden_tasks.o timer.o fault.o config.o VM_Manager.o stat.o \
        mmu_table.o bitmap.o reset_handler.o ffs.o cache.o mem_tracking.o \
        VM_Manager_Init.o rexx.o cat/VMM_Cat.o

CATALOGS = LOCALE:catalogs/deutsch/VMM-Handler.catalog \
           LOCALE:catalogs/français/VMM-Handler.catalog \
           LOCALE:catalogs/dansk/VMM-Handler.catalog \
           LOCALE:catalogs/svenska/VMM-Handler.catalog \
           LOCALE:catalogs/italiano/VMM-Handler.catalog \
           LOCALE:catalogs/español/VMM-Handler.catalog \
           LOCALE:catalogs/russian/VMM-Handler.catalog

OBJS = $(ASMOBJS) $(COBJS)

TARGETS = VMM-Handler $(CATALOGS)

# possible defines are:
# DEBUG
# TRACE_MEM
# PAGE4K or PAGE8K
# NUM_PTR_TABLES = 1 - ??
# MAX_FAULTS = ??
# USE_OWN_SEMAPHORES

ASM_DEFINES = SET "PAGE4K=1,DEBUG"
ASM2_DEFINES = SET "PAGE4K=1,DEBUG=1,DYN_MMU_SETUP"
C_DEFINES = DEFINE PAGE4K DEFINE DEBUG

CFLAGS = NOICONS CPU 68020     ANSI STRICT NOSTACKCHECK NOSTANDARDIO COMMENTNEST \
         UTILLIB $(C_DEFINES) GST VMM.gst nodebug data faronly

AFLAGS =   $(ASM_DEFINES) NOEXE QUIET LARGE
A2FLAGS =   $(ASM2_DEFINES) NOEXE QUIET LARGE
LFLAGS = smallcode smalldata
# stripdebug

.c.o:
  sc nolink $(CFLAGS) $*.c

.asm.o:
  phxass $(AFLAGS) $*.asm TO $*.o 

all: VMM.gst $(TARGETS) 

VMM-Handler : $(OBJS) makefile 
  bumprev 22 VMM
  sc nolink $(CFLAGS) version.c
  slink FROM $(OBJS) version.o lib/string.o TO VMM-Handler LIB LIB:amiga.lib $(LFLAGS)
  copy VMM-Handler L:

VMM.gst : dmp.c
  gst >nil: VMM.gst unload
  sc MGST=VMM.gst dmp.c
  delete >NIL: dmp.o

$(COBJS) : $(C_HDRS)
$(ASMOBJS) : $(ASM_HDRS)

dyn_mmu_trap60.o : traphandler60.asm
  phxass $(A2FLAGS) to dyn_mmu_trap60.o from traphandler60.asm

dyn_mmu_trap40.o : traphandler40.asm
  phxass $(A2FLAGS) to dyn_mmu_trap40.o from traphandler40.asm

dyn_mmu_trap30.o : traphandler30.asm
  phxass $(A2FLAGS) to dyn_mmu_trap30.o from traphandler30.asm

clean:
  rm -f $(ASMOBJS)
  rm -f $(COBJS)
  rm -f $(TARGETS)
  rm -f VMM.gst

backup:
  lha a VMM_bak $(SRCS) $(C_HDRS) $(ASM_HDRS) makefile makefile.gcc ffs.h

revision:
  ci -u $(SRCS) $(C_HDRS) $(ASM_HDRS)

release:
  ci -u -f -m"Release 3.3" -nRel3_3 -sRel $(SRCS) $(C_HDRS) $(ASM_HDRS)

ffs.o : ffs.h
VM_Manager.o VM_Manager_Init.o rexx.o : Manager_priv.h
mmu_table.o : mmu_bits30.h

cat/VMM_Cat.o : cat/VMM_Cat.c cat/VMM_Cat.h
  sc nolink $(CFLAGS) cat/VMM_Cat.c

cat/VMM_Cat.h cat/VMM_Cat.c : cat/VMM-Handler.cd  cat/VMM_Cat_c.sd cat/VMM_Cat_h.sd
  flexcat cat/VMM-Handler.cd cat/VMM_Cat.h=cat/VMM_Cat_h.sd cat/VMM_Cat.c=cat/VMM_Cat_c.sd 

LOCALE:catalogs/deutsch/VMM-Handler.catalog : cat/deutsch.ct cat/VMM-Handler.cd
  flexcat cat/VMM-Handler.cd cat/deutsch.ct catalog LOCALE:catalogs/deutsch/VMM-Handler.catalog \
       newctfile cat/deutsch.ct

LOCALE:catalogs/français/VMM-Handler.catalog : cat/français.ct cat/VMM-Handler.cd
  flexcat cat/VMM-Handler.cd cat/français.ct catalog LOCALE:catalogs/français/VMM-Handler.catalog \
       newctfile cat/français.ct

LOCALE:catalogs/dansk/VMM-Handler.catalog : cat/dansk.ct cat/VMM-Handler.cd
  flexcat cat/VMM-Handler.cd cat/dansk.ct catalog LOCALE:catalogs/dansk/VMM-Handler.catalog \
       newctfile cat/dansk.ct

LOCALE:catalogs/svenska/VMM-Handler.catalog : cat/svenska.ct cat/VMM-Handler.cd
  flexcat cat/VMM-Handler.cd cat/svenska.ct catalog LOCALE:catalogs/svenska/VMM-Handler.catalog \
       newctfile cat/svenska.ct

LOCALE:catalogs/italiano/VMM-Handler.catalog : cat/italiano.ct cat/VMM-Handler.cd
  flexcat cat/VMM-Handler.cd cat/italiano.ct catalog LOCALE:catalogs/italiano/VMM-Handler.catalog \
       newctfile cat/italiano.ct

LOCALE:catalogs/español/VMM-Handler.catalog : cat/español.ct cat/VMM-Handler.cd
  flexcat cat/VMM-Handler.cd cat/español.ct catalog LOCALE:catalogs/español/VMM-Handler.catalog \
       newctfile cat/español.ct

LOCALE:catalogs/russian/VMM-Handler.catalog : cat/russian.ct cat/VMM-Handler.cd
  flexcat cat/VMM-Handler.cd cat/russian.ct catalog LOCALE:catalogs/russian/VMM-Handler.catalog \
       newctfile cat/russian.ct
