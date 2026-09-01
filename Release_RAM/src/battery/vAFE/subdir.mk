################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/battery/vAFE/Bms_Vafe.c 

OBJS += \
./src/battery/vAFE/Bms_Vafe.o 

C_DEPS += \
./src/battery/vAFE/Bms_Vafe.d 


# Each subdirectory must supply rules for building sources it contributes
src/battery/vAFE/%.o: ../src/battery/vAFE/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/battery/vAFE/Bms_Vafe.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


