BUILD_DIR:=build
BIN_DIR:=bin

SIM_SRC:=$(wildcard src/modbussim/*.c)
SIM_OBJ:=$(addprefix $(BUILD_DIR)/, $(SIM_SRC:.c=.o))
SIM_BIN:=modbus_simulator

CLI_SRC:=$(wildcard src/modbuscli/*.c)
CLI_OBJ:=$(addprefix $(BUILD_DIR)/, $(CLI_SRC:.c=.o))
CLI_BIN:=modbus_cli

CPPFLAGS:=$(shell pkg-config --cflags libmodbus)
CFLAGS:=-MMD -g -pipe -O2 -Wall -Wextra --pedantic
LDFLAGS:=$(shell pkg-config --libs-only-L libmodbus)
LDLIBS:=$(shell pkg-config --libs-only-l libmodbus) -s

.phony: all clean

all: $(BIN_DIR)/$(SIM_BIN) $(BIN_DIR)/$(CLI_BIN)

$(BIN_DIR)/$(SIM_BIN): $(SIM_OBJ)
	mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN_DIR)/$(CLI_BIN): $(CLI_OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

clean:
	rm -rf $(BIN)/$(SIM_BIN) $(BIN)/$(CLI_BIN) $(BUILD_DIR)/*

-include $(SIM_OBJ:.o=.d)
-include $(CLI_OBJ:.o=.d)
