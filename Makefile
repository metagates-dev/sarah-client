# Makefile for compiling sarahos and all its extensions
# @Author	grandhelmsman<desupport@grandhelmsman.com>

INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin
INSTALL_INC= $(INSTALL_TOP)/include
INSTALL_LIB= $(INSTALL_TOP)/lib

# Other utilities.
MKDIR= mkdir -p
RM= rm -f

# all available targets including for main sources
SRC_TARGETS= debug release ghost install

# Targets start here.
all:
	cd main && $(MAKE)

$(SRC_TARGETS) clean:
	cd main && $(MAKE) $@

%-compile:
	cd ext/$* && $(MAKE)

ext-%:
	cd ext/$* && $(MAKE)

%-install:
	cd ext/$* && $(MAKE) install

%-clean:
	cd ext/$* && $(MAKE) clean

%-reload:
	cd ext/$* && $(MAKE) clean && $(MAKE) && sudo $(MAKE) install

# echo config parameters
echo:
	@echo "INSTALL_TOP= $(INSTALL_TOP)"
	@echo "INSTALL_BIN= $(INSTALL_BIN)"
	@echo "INSTALL_INC= $(INSTALL_INC)"
	@echo "INSTALL_LIB= $(INSTALL_LIB)"
	@echo "INSTALL_EXEC= $(INSTALL_EXEC)"
	@echo "INSTALL_DATA= $(INSTALL_DATA)"

# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all clean echo

# (end of Makefile)
