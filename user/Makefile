BUILD_DIR ?= ../build/user

U_FILES = $(wildcard *.txt)
U_FILE_TARGETS = $(U_FILES:%=$(BUILD_DIR)/%)

.PHONY: all clean

all: $(U_FILE_TARGETS)

$(BUILD_DIR)/%.txt: %.txt
	@mkdir -p $(dir $@)
	cp $< $@

clean:
	rm -rf $(BUILD_DIR)
