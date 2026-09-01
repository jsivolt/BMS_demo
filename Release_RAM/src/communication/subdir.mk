################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/communication/Bms_Can.c \
../src/communication/Bms_Spi.c 

OBJS += \
./src/communication/Bms_Can.o \
./src/communication/Bms_Spi.o 

C_DEPS += \
./src/communication/Bms_Can.d \
./src/communication/Bms_Spi.d 


# Each subdirectory must supply rules for building sources it contributes
src/communication/%.o: ../src/communication/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/communication/Bms_Can.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


