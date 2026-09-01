################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/control/Bms_Contactor.c 

OBJS += \
./src/control/Bms_Contactor.o 

C_DEPS += \
./src/control/Bms_Contactor.d 


# Each subdirectory must supply rules for building sources it contributes
src/control/%.o: ../src/control/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/control/Bms_Contactor.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


