################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/power/Bms_PowerManager.c 

OBJS += \
./src/power/Bms_PowerManager.o 

C_DEPS += \
./src/power/Bms_PowerManager.d 


# Each subdirectory must supply rules for building sources it contributes
src/power/%.o: ../src/power/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/power/Bms_PowerManager.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


