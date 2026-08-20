################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/safety/subdir.mk
-include src/drivers/subdir.mk
-include src/control/subdir.mk
-include src/communication/subdir.mk
-include src/battery/subdir.mk
-include src/app/subdir.mk
-include src/subdir.mk
-include generate/src/subdir.mk
-include board/subdir.mk
-include RTD/src/subdir.mk
-include Project_Settings/Startup_Code/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_SIZE += \
BMS_demo.siz \


# All Target
all: BMS_demo.elf secondary-outputs

# Tool invocations
BMS_demo.elf: $(OBJS) C:/S32K344/workspace/BMS_demo/Project_Settings/Linker_Files/linker_flash_s32k344.ld $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Standard S32DS C Linker'
	arm-none-eabi-gcc -o "BMS_demo.elf" "@BMS_demo.args"  $(USER_OBJS)
	@echo 'Finished building target: $@'
	@echo ' '

BMS_demo.siz: BMS_demo.elf
	@echo 'Invoking: Standard S32DS Print Size'
	arm-none-eabi-size --format=berkeley BMS_demo.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) ./*
	-@echo ' '

secondary-outputs: $(SECONDARY_SIZE)

.PHONY: all clean dependents

-include ../makefile.targets
