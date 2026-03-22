TARGET = test
SOURCES = unit_tests.c

#INCLUDE_DIRS +=

CC ?= gcc
AR ?= ar
CFLAGS ?= -O0 -g -DEBUG
OBJ_DIR ?= obj
BIN_DIR ?= bin
LIB_DIR ?= lib
ARFLAGS ?= rcs

#SYSTEM_LIBS +=

# All libs below will be searched at one level up directory (../)
# if found then symlink to this directory
# if not - will be cloned (git clone)

# Compileable and downloadable libs
#LIBS_URLS +=

# Downloadable header-only libs
LIBS_HEADERS_URLS += https://github.com/sheredom/utest.h.git
LIBS_HEADERS_URLS += https://github.com/AIG-Livny/c-vector.git

# Compileable libs
#LIBS_DIRS +=

# Lib-headers only
#LIBS_HEADERS_DIRS +=

###

#MAKEFLAGS=--no-print-directory -e -s

OBJECTS=$(SOURCES:%.c=$(OBJ_DIR)/%.o)
DEPS=$(OBJECTS:.o=.d)
DEPFLAGS=-MMD -MP

LIBS_NAMES += $(foreach url,$(LIBS_URLS),$(shell basename $(url) .git))
LIBS_HEADERS_NAMES += $(foreach url,$(LIBS_HEADERS_URLS),$(shell basename $(url) .git))

LIBS_DIRS = $(LIBS_NAMES:%=$(LIB_DIR)/%)
LIBS_HEADERS_DIRS = $(LIBS_HEADERS_NAMES:%=$(LIB_DIR)/%)

SPACE=$() $()
export PKG_CONFIG_PATH =$(subst $(SPACE),:,$(LIBS_DIRS) $(LIBS_HEADERS_DIRS))

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(LIBS_NAMES) $(LIBS_HEADERS_NAMES) $(SYSTEM_LIBS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(LIBS_NAMES) $(LIBS_HEADERS_NAMES) $(SYSTEM_LIBS))

$(foreach url,$(LIBS_HEADERS_URLS),$(eval URL_$(LIB_DIR)/$(shell basename $(url) .git) := $(url)))
$(foreach url,$(LIBS_URLS),$(eval URL_$(LIB_DIR)/$(shell basename $(url) .git) := $(url)))

.PHONY: all target run test clean cleanall

all: target

target: $(LIBS_DIRS) $(LIBS_HEADERS_DIRS) $(BIN_DIR)/$(TARGET)

run: target
	$(BIN_DIR)/$(TARGET)

clean: subprojects.clean
	rm -rf $(OBJ_DIR)

cleanall: subprojects.cleanall clean
	rm -rf $(BIN_DIR) $(LIB_DIR)

subprojects.%:
	-$(LIBS_DIRS:%=$(MAKE) -C % $* ;)
	-$(LIBS_HEADERS_DIRS:%=$(MAKE) -C % $* ;)

$(LIBS_DIRS):
	@mkdir -p $(dir $@)
	$(if $(wildcard ../$(notdir $@)), ln -sr ../$(notdir $@) $@, git clone $(URL_$@) $@)
	$(MAKE) -C $@
	$(MAKE)

$(LIBS_HEADERS_DIRS):
	@mkdir -p $(dir $@)
	$(if $(wildcard ../$(notdir $@)), ln -sr ../$(notdir $@) $@, git clone $(URL_$@) $@)
	$(MAKE)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(CACHED_CFLAGS) -c $< -o $@

$(BIN_DIR)/lib%.a: $(OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^

$(BIN_DIR)/%: $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

-include $(DEPS)