################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/safety/Fault_Manager.c 

OBJS += \
./src/safety/Fault_Manager.o 

C_DEPS += \
./src/safety/Fault_Manager.d 


# Each subdirectory must supply rules for building sources it contributes
src/safety/%.o: ../src/safety/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/safety/Fault_Manager.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


