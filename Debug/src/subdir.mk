################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/DualImageCapture.cpp 

OBJS += \
./src/DualImageCapture.o 

CPP_DEPS += \
./src/DualImageCapture.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I"/home/zkirkendoll/workspace/DualImageCaptureWithOpenCV_ARM/mvIMPACT_CPP" -I/usr/local/include -I"/home/zkirkendoll/workspace/DualImageCaptureWithOpenCV_ARM/DriverBase/Include" -I"/home/zkirkendoll/workspace/DualImageCaptureWithOpenCV_ARM/mvPropHandling/Include" -I"/home/zkirkendoll/workspace/DualImageCaptureWithOpenCV_ARM/mvDeviceManager/Include" -O0 -g3 -pedantic -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -std=gnu++11 -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


