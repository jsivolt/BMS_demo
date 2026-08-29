################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/storage/Bms_Nvm.c 

OBJS += \
./src/storage/Bms_Nvm.o 

C_DEPS += \
./src/storage/Bms_Nvm.d 


# Each subdirectory must supply rules for building sources it contributes
src/storage/%.o: ../src/storage/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/storage/Bms_Nvm.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


