################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/battery/vPACK/Bms_Vpack.c 

OBJS += \
./src/battery/vPACK/Bms_Vpack.o 

C_DEPS += \
./src/battery/vPACK/Bms_Vpack.d 


# Each subdirectory must supply rules for building sources it contributes
src/battery/vPACK/%.o: ../src/battery/vPACK/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/battery/vPACK/Bms_Vpack.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


