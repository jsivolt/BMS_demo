################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/app/Bms_Adc.c \
../src/app/Bms_App.c \
../src/app/Bms_Scheduler.c \
../src/app/Bms_StateMachine.c 

OBJS += \
./src/app/Bms_Adc.o \
./src/app/Bms_App.o \
./src/app/Bms_Scheduler.o \
./src/app/Bms_StateMachine.o 

C_DEPS += \
./src/app/Bms_Adc.d \
./src/app/Bms_App.d \
./src/app/Bms_Scheduler.d \
./src/app/Bms_StateMachine.d 


# Each subdirectory must supply rules for building sources it contributes
src/app/%.o: ../src/app/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/app/Bms_Adc.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


