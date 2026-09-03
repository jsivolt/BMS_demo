################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/communication/xcp/Xcp.c \
../src/communication/xcp/Xcp_Can.c 

OBJS += \
./src/communication/xcp/Xcp.o \
./src/communication/xcp/Xcp_Can.o 

C_DEPS += \
./src/communication/xcp/Xcp.d \
./src/communication/xcp/Xcp_Can.d 


# Each subdirectory must supply rules for building sources it contributes
src/communication/xcp/%.o: ../src/communication/xcp/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/communication/xcp/Xcp.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


