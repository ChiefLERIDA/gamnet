################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Handler_HeartBeat.cpp \
../Handler_Login.cpp \
../Handler_Reconnect.cpp \
../Manager_Session.cpp \
../Session.cpp \
../main.cpp 

OBJS += \
./Handler_HeartBeat.o \
./Handler_Login.o \
./Handler_Reconnect.o \
./Manager_Session.o \
./Session.o \
./main.o 

CPP_DEPS += \
./Handler_HeartBeat.d \
./Handler_Login.d \
./Handler_Reconnect.d \
./Manager_Session.d \
./Session.d \
./main.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D_DEBUG -I/usr/include/mysql -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


