################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/battery/Battery_Monitor.c 

OBJS += \
./src/battery/Battery_Monitor.o 

C_DEPS += \
./src/battery/Battery_Monitor.d 


# Each subdirectory must supply rules for building sources it contributes
src/battery/%.o: ../src/battery/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/battery/Battery_Monitor.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


