################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/drivers/Bms_Can.c \
../src/drivers/Bms_Gpio.c \
../src/drivers/Bms_Led.c 

OBJS += \
./src/drivers/Bms_Can.o \
./src/drivers/Bms_Gpio.o \
./src/drivers/Bms_Led.o 

C_DEPS += \
./src/drivers/Bms_Can.d \
./src/drivers/Bms_Gpio.d \
./src/drivers/Bms_Led.d 


# Each subdirectory must supply rules for building sources it contributes
src/drivers/%.o: ../src/drivers/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/drivers/Bms_Can.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


