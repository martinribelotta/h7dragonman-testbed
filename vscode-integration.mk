
COMMA:=,
QUOTE:="
EMPTY:=
OBRACK:={
CBRACK:=}
IDENT:=$(EMPTY)				$(EMPTY)
IDENT2:=$(EMPTY)        $(EMPTY)
SPACE:=$(EMPTY) $(EMPTY)
NL:=\n

define DOT_VSCODE_C_CPP_PROPERTIES
{
    "configurations": [
        {
            "name": "$(TARGET)",
            "includePath": [
$(IDENT)$(subst $(SPACE),$(COMMA)$(NL)$(IDENT),$(addsuffix $(QUOTE), $(addprefix $(QUOTE)$${workspaceFolder}/, $(C_INCLUDES))))
            ],
            "defines": [
$(IDENT)$(subst $(SPACE),$(COMMA)$(NL)$(IDENT),$(addsuffix $(QUOTE), $(addprefix $(QUOTE), $(C_DEFS))))
            ],
            "compilerArgs": [
$(IDENT)$(subst $(SPACE),$(COMMA)$(NL)$(IDENT),$(addsuffix $(QUOTE), $(addprefix $(QUOTE), $(COMMON_FLAGS) $(OPT_FLAGS))))
            ],
            "compilerPath": "$(CC)",
            "cStandard": "c11",
            "cppStandard": "c++17",
            "intelliSenseMode": "gcc-arm"
        }
    ],
    "version": 4
}
endef
export DOT_VSCODE_C_CPP_PROPERTIES

define TASK_TEMPLATE
{
            "problemMatcher": "$$gcc",
            "type": "shell",
            "group": "build",
            "label": "make $1",
            "command": "make $1"
        }
endef

define DOT_VSCODE_TASKS
{
    "version": "2.0.0",
    "tasks": [
$(IDENT2)$(subst $(CBRACK) $(OBRACK),$(CBRACK)$(COMMA)\n$(IDENT2)$(OBRACK),$(foreach t,$(PHONY_TARGETS),$(call TASK_TEMPLATE,$(t))))
    ]
}
endef
export DOT_VSCODE_TASKS

define DOT_VSCODE_LAUNCH
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug demo",
            "cwd": "$${workspaceRoot}",
            "executable": "$${workspaceFolder}/$(BUILD_DIR)/$(TARGET).elf",
            "request": "launch",
            "type": "cortex-debug",
            "servertype": "openocd",
            "configFiles": [
                "interface/cmsis-dap.cfg",
                "target/stm32h7x.cfg"
            ],
            "runToMain": true,
            "preLaunchTask": "Make all"
        }
    ]
}
endef
export DOT_VSCODE_LAUNCH

.vscode-integration:
	@mkdir -p .vscode
	@echo -n "$$DOT_VSCODE_C_CPP_PROPERTIES" > .vscode/c_cpp_properties.json
	@echo -n "$$DOT_VSCODE_TASKS" > .vscode/tasks.json
	@echo -n "$$DOT_VSCODE_LAUNCH" > .vscode/launch.json
